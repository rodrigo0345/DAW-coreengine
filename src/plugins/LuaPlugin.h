//
// Created by rodrigo0345 on 3/9/26.
//

#ifndef DAWCOREENGINE_LUAPLUGIN_H
#define DAWCOREENGINE_LUAPLUGIN_H
#include "../render_loop/audio/AudioBuffer.h"
#include "../render_loop/audio/simd_ops.h"

extern "C" {
    #include <lua.h>
    #include <lualib.h>
    #include <lauxlib.h>
}
#include <sol/sol.hpp>
#include <string>
#include <string_view>
#include <stdexcept>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <array>
#include "Plugin.h"

namespace coreengine {

    inline int luaPanicHandler(lua_State* L) {
        const char* msg = lua_tostring(L, -1);
        std::fprintf(stderr, "Unprotected Lua error: %s\n", msg ? msg : "unknown");
        throw std::runtime_error(msg ? msg : "Lua panic");
    }

    // ── FloatArray: zero-overhead per-sample Lua access ───────────────────────
    //
    // Pushed as lightuserdata (just a raw pointer — no GC, no allocation).
    // A metatable named "FloatArray" provides __index/__newindex as bare
    // lua_CFunctions so each ch[i] read/write is ~50 ns instead of ~3 µs.
    //
    // Usage in Lua:
    //   local ch = buffer:getChannel(0)   -- returns FloatArray for channel 0
    //   local s  = ch[i]                  -- __index: direct pointer read
    //   ch[i]    = s * 0.5                -- __newindex: direct pointer write

    static constexpr char kFloatArrayMeta[] = "FloatArray";

    // Thin wrapper pushed as full userdata so we can store both pointer + length
    struct FloatArrayUD {
        float*  data;
        size_t  len;
    };

    inline void lua_pushfloatarray(lua_State* L, float* ptr, size_t len) {
        auto* ud = static_cast<FloatArrayUD*>(lua_newuserdata(L, sizeof(FloatArrayUD)));
        ud->data = ptr;
        ud->len  = len;
        luaL_getmetatable(L, kFloatArrayMeta);
        lua_setmetatable(L, -2);
    }

    // __index(arr, i)  — Lua is 1-based by convention; we keep 0-based to match C
    inline int floatarray_index(lua_State* L) {
        auto* ud = static_cast<FloatArrayUD*>(luaL_checkudata(L, 1, kFloatArrayMeta));
        auto  i  = static_cast<size_t>(luaL_checkinteger(L, 2));
        if (i >= ud->len) { lua_pushnumber(L, 0); return 1; }
        lua_pushnumber(L, static_cast<lua_Number>(ud->data[i]));
        return 1;
    }

    // __newindex(arr, i, v)
    inline int floatarray_newindex(lua_State* L) {
        auto* ud = static_cast<FloatArrayUD*>(luaL_checkudata(L, 1, kFloatArrayMeta));
        auto  i  = static_cast<size_t>(luaL_checkinteger(L, 2));
        auto  v  = static_cast<float>(luaL_checknumber(L, 3));
        if (i < ud->len) ud->data[i] = v;
        return 0;
    }

    // __len(arr) — supports #ch
    inline int floatarray_len(lua_State* L) {
        auto* ud = static_cast<FloatArrayUD*>(luaL_checkudata(L, 1, kFloatArrayMeta));
        lua_pushinteger(L, static_cast<lua_Integer>(ud->len));
        return 1;
    }

    inline void lua_register_floatarray(lua_State* L) {
        luaL_newmetatable(L, kFloatArrayMeta);
        lua_pushcfunction(L, floatarray_index);    lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, floatarray_newindex); lua_setfield(L, -2, "__newindex");
        lua_pushcfunction(L, floatarray_len);      lua_setfield(L, -2, "__len");
        lua_pop(L, 1);
    }

    /**
     * A Lua-scripted plugin that IS an Instrument.
     *
     * Required Lua function:
     *   function processBlock(buffer)
     *
     * buffer members exposed to Lua:
     *   buffer.numSamples        (integer)
     *   buffer.sampleRate        (integer)
     *   buffer:getChannel(n)     → FloatArray  -- zero-copy, direct pointer
     *   -- Legacy compat (slower, kept for simple scripts):
     *   buffer:getSample(ch, i)  → number
     *   buffer:setSample(ch, i, v)
     *
     * Optional Lua functions for instrument plugins:
     *   function noteOn(midiNote, velocity)
     *   function noteOff(midiNote)
     *   function allNotesOff()
     */
    class LuaPlugin : public Plugin {
    public:
        LuaPlugin(const int pluginId, const std::string_view pluginName, const std::string_view source)
            : Plugin(pluginId, pluginName, source)
        {
            lua_atpanic(lua_.lua_state(), luaPanicHandler);
            // Pre-zero both double-buffers
            for (auto& buf : dblBuf_)
                for (auto& ch : buf)
                    ch.fill(0.f);
        }

        ~LuaPlugin() override {
            stopWorker();
        }

        std::expected<void, std::string> updateSource(const std::string_view newSource) {
            stopWorker();
            isReady.store(false, std::memory_order_release);
            sourceCode = newSource;
            lua_ = sol::state{};
            channelUDs_ = {};
            lua_atpanic(lua_.lua_state(), luaPanicHandler);
            return compileAndOptimize();
        }

        std::expected<void, std::string> compileAndOptimize() override {
            try {
                lua_.open_libraries(sol::lib::base, sol::lib::math);
                lua_register_floatarray(lua_.lua_state());

                lua_State* L = lua_.lua_state();
                for (size_t ch = 0; ch < MAX_CHANNELS; ++ch) {
                    auto* ud = static_cast<FloatArrayUD*>(lua_newuserdata(L, sizeof(FloatArrayUD)));
                    ud->data = nullptr; ud->len = 0;
                    luaL_getmetatable(L, kFloatArrayMeta);
                    lua_setmetatable(L, -2);
                    channelRefs_[ch] = luaL_ref(L, LUA_REGISTRYINDEX);
                    channelUDs_[ch]  = ud;
                }

                lua_.new_usertype<AudioBuffer>("AudioBuffer",
                    "numSamples", &AudioBuffer::numSamples,
                    "sampleRate", &AudioBuffer::sampleRate,
                    "getChannel", [this](AudioBuffer& /*b*/, size_t ch, sol::this_state ts) -> sol::stack_object {
                        lua_State* Ls = ts.lua_state();
                        if (ch >= MAX_CHANNELS) { lua_pushnil(Ls); return sol::stack_object(Ls, -1); }
                        lua_rawgeti(Ls, LUA_REGISTRYINDEX, static_cast<lua_Integer>(channelRefs_[ch]));
                        return sol::stack_object(Ls, -1);
                    },
                    "getSample", [](AudioBuffer& b, size_t ch, size_t s) -> float {
                        return (ch < b.channels.size() && s < b.numSamples) ? b.channels[ch][s] : 0.f;
                    },
                    "setSample", [](AudioBuffer& b, size_t ch, size_t s, float v) {
                        if (ch < b.channels.size() && s < b.numSamples) b.channels[ch][s] = v;
                    }
                );

                if (const auto result = lua_.safe_script(sourceCode, sol::script_pass_on_error);
                    !result.valid()) return std::unexpected(sol::error(result).what());

                sol::optional<sol::protected_function> fn = lua_["processBlock"];
                if (!fn) return std::unexpected("Missing processBlock function");
                processFunc_ = *fn;

                if (sol::optional<sol::protected_function> f = lua_["noteOn"])      noteOnFunc_  = *f;
                if (sol::optional<sol::protected_function> f = lua_["noteOff"])     noteOffFunc_ = *f;
                if (sol::optional<sol::protected_function> f = lua_["allNotesOff"]) allOffFunc_  = *f;

                isReady.store(true, std::memory_order_release);
                startWorker();
                return {};
            } catch (const std::exception& e) {
                return std::unexpected(e.what());
            }
        }

        // ── Called from the audio thread ──────────────────────────────────────
        // Never runs Lua. Copies the last completed Lua buffer into `buffer`
        // using SIMD accumulate. If Lua is still rendering, reuses previous result.
        void processBlock(AudioBuffer& buffer) override {
            if (!processFunc_.has_value()) return;

            const size_t nch = std::min(buffer.channels.size(), MAX_CHANNELS);
            const size_t ns  = buffer.numSamples;

            // 1. Deliver new params (sampleRate/numSamples) to worker non-blockingly
            {
                std::unique_lock lk(paramMtx_, std::try_to_lock);
                if (lk.owns_lock()) {
                    pendingSR_ = buffer.sampleRate;
                    pendingNS_ = ns;
                    pendingNch_ = nch;
                    paramsReady_.store(true, std::memory_order_release);
                }
            }

            // 2. Copy the read buffer (last completed render) into output
            const size_t readBuf = readIdx_.load(std::memory_order_acquire);
            for (size_t c = 0; c < nch; ++c)
                simd::accumulate_scaled(buffer.channels[c], dblBuf_[readBuf][c].data(), 1.0f, ns);

            // 3. Signal worker to start next block
            {
                std::lock_guard lk(workMtx_);
                workReady_ = true;
            }
            workCv_.notify_one();
        }

        void noteOn(int midiNote, float velocity) override {
            activeVoices_.fetch_add(1, std::memory_order_relaxed);
            std::lock_guard lk(eventMtx_);
            pendingEvents_.push_back({EventKind::NoteOn, midiNote, velocity});
        }

        void noteOff(int midiNote) override {
            auto prev = activeVoices_.load(std::memory_order_relaxed);
            if (prev > 0) activeVoices_.fetch_sub(1, std::memory_order_relaxed);
            std::lock_guard lk(eventMtx_);
            pendingEvents_.push_back({EventKind::NoteOff, midiNote, 0.f});
        }

        void allNotesOff() override {
            activeVoices_.store(0, std::memory_order_relaxed);
            std::lock_guard lk(eventMtx_);
            pendingEvents_.push_back({EventKind::AllOff, 0, 0.f});
        }

        [[nodiscard]] bool hasActiveVoices() const override {
            return activeVoices_.load(std::memory_order_relaxed) > 0;
        }

        [[nodiscard]] bool isInstrument() const { return noteOnFunc_.has_value(); }

    private:
        // ── Double-buffer storage (2 buffers × MAX_CHANNELS × MAX_BLOCK_SIZE) ──
        static constexpr size_t N_BUFS = 2;
        std::array<std::array<std::array<float, MAX_BLOCK_SIZE>, MAX_CHANNELS>, N_BUFS> dblBuf_{};
        std::atomic<size_t> readIdx_{0};
        size_t writeBuf_{1};

        // ── Active voice tracking (C++ side, not Lua) ─────────────────────────
        std::atomic<int> activeVoices_{0};

        // ── Worker thread state ───────────────────────────────────────────────
        std::thread         worker_;
        std::mutex          workMtx_;
        std::condition_variable workCv_;
        bool                workReady_{false};
        std::atomic<bool>   workerStop_{false};

        // ── Pending audio params (sent non-blockingly from audio thread) ───────
        std::mutex   paramMtx_;
        uint64_t     pendingSR_{ENGINE_SAMPLE_RATE};
        size_t       pendingNS_{ENGINE_BLOCK_SIZE};
        size_t       pendingNch_{MAX_CHANNELS};
        std::atomic<bool> paramsReady_{false};

        // ── Pending MIDI events ───────────────────────────────────────────────
        enum class EventKind : uint8_t { NoteOn, NoteOff, AllOff };
        struct PendingEvent { EventKind kind; int note; float vel; };
        std::mutex                  eventMtx_;
        std::vector<PendingEvent>   pendingEvents_;
        std::vector<PendingEvent>   localEvents_;   // swap target — no alloc in worker

        // ── Lua state (lives on worker thread only after startWorker()) ────────
        sol::state lua_;
        std::array<FloatArrayUD*, MAX_CHANNELS> channelUDs_{};
        std::array<int, MAX_CHANNELS> channelRefs_{};
        sol::optional<sol::protected_function> processFunc_;
        sol::optional<sol::protected_function> noteOnFunc_;
        sol::optional<sol::protected_function> noteOffFunc_;
        sol::optional<sol::protected_function> allOffFunc_;

        // ── Internal AudioBuffer wired into dblBuf_ write slot ────────────────
        AudioBuffer workerBuf_;

        void startWorker() {
            workerStop_.store(false, std::memory_order_release);
            worker_ = std::thread([this]{ workerLoop(); });
        }

        void stopWorker() {
            {
                std::lock_guard lk(workMtx_);
                workerStop_.store(true, std::memory_order_release);
                workReady_ = true;
            }
            workCv_.notify_one();
            if (worker_.joinable()) worker_.join();
        }

        void workerLoop() {
            // Wire workerBuf_ channels to the write double-buffer slot
            workerBuf_.channels.resize(MAX_CHANNELS);
            workerBuf_.numSamples  = ENGINE_BLOCK_SIZE;
            workerBuf_.sampleRate  = ENGINE_SAMPLE_RATE;
            for (size_t c = 0; c < MAX_CHANNELS; ++c)
                workerBuf_.channels[c] = dblBuf_[writeBuf_][c].data();

            while (true) {
                // Wait for a trigger — either audio thread signal OR active voices
                {
                    std::unique_lock lk(workMtx_);
                    workCv_.wait(lk, [this]{
                        return workReady_
                            || workerStop_.load(std::memory_order_acquire)
                            || activeVoices_.load(std::memory_order_relaxed) > 0;
                    });
                    workReady_ = false;
                }
                if (workerStop_.load(std::memory_order_acquire)) return;

                // Pick up latest audio params
                if (paramsReady_.exchange(false, std::memory_order_acq_rel)) {
                    std::lock_guard lk(paramMtx_);
                    workerBuf_.sampleRate = pendingSR_;
                    workerBuf_.numSamples = pendingNS_;
                    const size_t nch = std::min(pendingNch_, MAX_CHANNELS);
                    if (workerBuf_.channels.size() != nch) {
                        workerBuf_.channels.resize(nch);
                        for (size_t c = 0; c < nch; ++c)
                            workerBuf_.channels[c] = dblBuf_[writeBuf_][c].data();
                    }
                }

                const size_t ns  = workerBuf_.numSamples;
                const size_t nch = workerBuf_.channels.size();

                // Zero write buffer
                for (size_t c = 0; c < nch; ++c)
                    simd::clear(dblBuf_[writeBuf_][c].data(), ns);

                // Drain pending MIDI events into Lua before rendering
                {
                    std::lock_guard lk(eventMtx_);
                    localEvents_.swap(pendingEvents_);
                }
                for (auto& ev : localEvents_) {
                    switch (ev.kind) {
                        case EventKind::NoteOn:
                            if (noteOnFunc_) {
                                (*noteOnFunc_)(ev.note, ev.vel);
                            }
                            break;
                        case EventKind::NoteOff:
                            if (noteOffFunc_) {
                                (*noteOffFunc_)(ev.note);
                            }
                            break;
                        case EventKind::AllOff:
                            if (allOffFunc_) {
                                (*allOffFunc_)();
                            }
                            break;
                    }
                }
                localEvents_.clear();

                // Wire FloatArray pointers to the write buffer
                for (size_t c = 0; c < nch; ++c) {
                    channelUDs_[c]->data = dblBuf_[writeBuf_][c].data();
                    channelUDs_[c]->len  = ns;
                }

                // Run Lua processBlock — fully off the audio thread
                try {
                    (*processFunc_)(workerBuf_);
                } catch (const std::exception& e) {
                    std::fprintf(stderr, "[LuaPlugin] processBlock error: %s\n", e.what());
                }

                // Swap buffers: publish write buffer as the new read buffer
                const size_t justWritten = writeBuf_;
                writeBuf_ = readIdx_.load(std::memory_order_relaxed);
                readIdx_.store(justWritten, std::memory_order_release);
            }
        }
    };
}

#endif

