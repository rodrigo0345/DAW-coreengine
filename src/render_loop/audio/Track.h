//
// Created by rodrigo0345 on 3/7/26.
//

#ifndef DAWCOREENGINE_TRACK_H
#define DAWCOREENGINE_TRACK_H

#include <array>
#include <atomic>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include "Instrument.h"
#include "TimelineEvent.h"
#include "EffectChain.h"
#include "simd_ops.h"

namespace coreengine {

    /**
     * Represents a track in the DAW timeline.
     * Each track has an instrument and a list of events.
     */
    class Track {
    public:
        Track(int id, std::string name, std::unique_ptr<Instrument> inst)
            : trackId(id), trackName(std::move(name)), instrument(std::move(inst))
        {
            // Pre-wire scratch AudioBuffer — channels point into fixed storage, never re-allocated
            scratchBuf_.sampleRate = ENGINE_SAMPLE_RATE;
            scratchBuf_.numSamples = MAX_BLOCK_SIZE;
            scratchBuf_.channels.resize(MAX_CHANNELS);
            for (size_t i = 0; i < MAX_CHANNELS; ++i)
                scratchBuf_.channels[i] = scratchStorage_[i].data();
        }

        int getTrackId() const { return trackId; }
        const std::string& getName() const { return trackName; }
        Instrument* getInstrument() { return instrument.get(); }

        /** Replace the instrument (e.g. when synth type changes). Keeps notes. */
        void replaceInstrument(std::unique_ptr<Instrument> newInst) {
            if (instrument) instrument->allNotesOff();
            instrument = std::move(newInst);
            markDirty("replaceInstrument");
        }

        void setMuted(bool muted) { isMuted = muted; markDirty("setMuted"); }
        bool getMuted() const { return isMuted; }

        void setSolo(bool solo) { isSolo = solo; markDirty("setSolo"); }
        bool getSolo() const { return isSolo; }

        void setVolume(float vol) { volume = vol; markDirty("setVolume"); }
        float getVolume() const { return volume; }

        /** Called by external events (noteOn/Off, param changes, replaceInstrument).
         *  Guarantees at least one fresh render on the next block regardless of
         *  how many silent blocks have accumulated.
         *  Pass a short tag string to identify the caller in logs. */
        void markDirty(const char* caller = "unknown") {
            lastDirtyCaller_ = caller;
            ++wakeCount_;
            silentBlocks_ = 0;
            dirty_.store(true, std::memory_order_release);
        }

        // Add a note to this track
        void addNote(uint64_t startSample, uint64_t durationSamples, int midiNote, float velocity) {
            events.emplace_back(startSample,                   EventType::NoteOn,  trackId, midiNote, velocity);
            events.emplace_back(startSample + durationSamples, EventType::NoteOff, trackId, midiNote, 0.0f);
            markDirty("addNote");
        }

        const std::vector<TimelineEvent>& getEvents() const { return events; }

        void clearEvents() { events.clear(); markDirty("clearEvents"); }

        // ── Effect chain API ───────────────────────────────────────────────
        void addEffect(std::unique_ptr<AudioEffect> effect) {
            effectChain_.addEffect(std::move(effect));
        }

        void removeEffect(const std::string& name) {
            effectChain_.removeEffectByName(name);
        }

        AudioEffect* getEffect(const std::string& name) {
            return effectChain_.getEffectByName(name);
        }

        EffectChain& getEffectChain() { return effectChain_; }

        // ── Cache state & logging ─────────────────────────────────────────────
        enum class CacheState : uint8_t { Hot, Cold };

        [[nodiscard]] CacheState getCacheState() const noexcept {
            const bool hasVoices = instrument && instrument->hasActiveVoices();
            return (!hasVoices &&
                    silentBlocks_ >= SILENT_SKIP_BLOCKS &&
                    !dirty_.load(std::memory_order_relaxed))
                   ? CacheState::Cold : CacheState::Hot;
        }

        // ── processBlock / renderToScratch / mixDown ──────────────────────────
        //
        // Cache logic:
        //   dirty_        = set ONLY by external events (noteOn/Off, param changes).
        //                   Consumed (cleared) once per block. Forces re-render.
        //   silentBlocks_ = consecutive blocks with peak < threshold AND !dirty.
        //                   Once >= SILENT_SKIP_BLOCKS: skip instrument entirely.
        //
        // Logs (stderr) only on state transitions: Hot→Cold or Cold→Hot.
        //
        void processBlock(AudioBuffer& output) {
            if (!instrument || isMuted) return;

            const size_t ns  = output.numSamples;
            const size_t nch = std::min(output.channels.size(), MAX_CHANNELS);

            scratchBuf_.sampleRate = output.sampleRate;
            scratchBuf_.numSamples = ns;
            if (scratchBuf_.channels.size() != nch) {
                scratchBuf_.channels.resize(nch);
                for (size_t i = 0; i < nch; ++i)
                    scratchBuf_.channels[i] = scratchStorage_[i].data();
            }

            const bool wasDirty = dirty_.exchange(false, std::memory_order_acq_rel);
            const bool hasVoices = instrument->hasActiveVoices();

            if (!wasDirty && !hasVoices && silentBlocks_ >= SILENT_SKIP_BLOCKS) {
                logTransition(CacheState::Cold, "");
                return;   // cache hit — nothing to mix
            }

            for (size_t i = 0; i < nch; ++i)
                simd::clear(scratchStorage_[i].data(), ns);

            instrument->processBlock(scratchBuf_);
            effectChain_.process(scratchBuf_);

            float peak = 0.f;
            for (size_t c = 0; c < nch; ++c) {
                const float cp = simd::peak_abs(scratchStorage_[c].data(), ns);
                if (cp > peak) peak = cp;
            }

            if (peak < 1e-7f && !hasVoices) ++silentBlocks_;
            else                             silentBlocks_ = 0;

            logTransition(CacheState::Hot, wasDirty ? "dirty(external event)" : "active(producing audio)");

            for (size_t c = 0; c < nch; ++c)
                simd::accumulate_scaled(output.channels[c], scratchStorage_[c].data(), volume, ns);
        }

        void mixDown(AudioBuffer& output) const {
            if (!instrument || isMuted) return;
            if (silentBlocks_ >= SILENT_SKIP_BLOCKS) return;   // cached — nothing to add
            const size_t ns  = output.numSamples;
            const size_t nch = std::min(output.channels.size(), MAX_CHANNELS);
            for (size_t c = 0; c < nch; ++c)
                simd::accumulate_scaled(output.channels[c], scratchStorage_[c].data(), volume, ns);
        }

        void renderToScratch() {
            if (!instrument || isMuted) return;

            const bool wasDirty = dirty_.exchange(false, std::memory_order_acq_rel);
            const bool hasVoices = instrument->hasActiveVoices();

            // Never cache-skip when the instrument has active voices
            // (Lua double-buffer may be 1 block behind — zeros are transient)
            if (!wasDirty && !hasVoices && silentBlocks_ >= SILENT_SKIP_BLOCKS) {
                logTransition(CacheState::Cold, "");
                return;   // cache hit
            }

            const size_t ns  = scratchBuf_.numSamples;
            const size_t nch = scratchBuf_.channels.size();

            for (size_t i = 0; i < nch; ++i)
                simd::clear(scratchStorage_[i].data(), ns);

            instrument->processBlock(scratchBuf_);
            effectChain_.process(scratchBuf_);

            float peak = 0.f;
            for (size_t c = 0; c < nch; ++c) {
                const float cp = simd::peak_abs(scratchStorage_[c].data(), ns);
                if (cp > peak) peak = cp;
            }

            if (peak < 1e-7f && !hasVoices) ++silentBlocks_;
            else                             silentBlocks_ = 0;

            logTransition(CacheState::Hot, wasDirty ? "dirty(external event)" : "active(producing audio)");
        }

        void prepareScratch(const AudioBuffer& ref) {
            const size_t nch = std::min(ref.channels.size(), MAX_CHANNELS);
            scratchBuf_.sampleRate = ref.sampleRate;
            scratchBuf_.numSamples = ref.numSamples;
            if (scratchBuf_.channels.size() != nch) {
                scratchBuf_.channels.resize(nch);
                for (size_t i = 0; i < nch; ++i)
                    scratchBuf_.channels[i] = scratchStorage_[i].data();
            }
        }

    private:
        // ── logTransition ─────────────────────────────────────────────────────
        // Prints to stderr ONLY when the cache state changes (Hot→Cold or Cold→Hot).
        // Calling this every block is safe: the branch is predicted perfectly after
        // the first few blocks in a given state, and the print itself is gated.
        void logTransition(CacheState next, const char* reason) {
            if (next == lastCacheState_) return;
            lastCacheState_ = next;
            if (next == CacheState::Cold) {
                std::fprintf(stderr,
                    "[Track cache] id=%-3d  %-20s  → COLD  (silentBlocks=%u, totalWakes=%u)\n",
                    trackId, trackName.c_str(), silentBlocks_, wakeCount_);
            } else {
                std::fprintf(stderr,
                    "[Track cache] id=%-3d  %-20s  → HOT   (%s, dirtiedBy='%s', totalWakes=%u)\n",
                    trackId, trackName.c_str(), reason, lastDirtyCaller_, wakeCount_);
            }
            std::fflush(stderr);
        }

        static constexpr uint32_t SILENT_SKIP_BLOCKS = 4;

        int         trackId;
        std::string trackName;
        std::unique_ptr<Instrument> instrument;
        std::vector<TimelineEvent>  events;
        EffectChain effectChain_;

        bool  isMuted = false;
        bool  isSolo  = false;
        float volume  = 1.0f;

        // dirty_ is set ONLY by external events; consumed once per block.
        std::atomic<bool> dirty_{true};
        uint32_t          silentBlocks_{0};
        uint32_t          wakeCount_{0};
        const char*       lastDirtyCaller_{"init"};

        // Tracks the last logged cache state to avoid printing every block.
        CacheState lastCacheState_{CacheState::Hot};

        std::array<std::array<float, MAX_BLOCK_SIZE>, MAX_CHANNELS> scratchStorage_{};
        AudioBuffer scratchBuf_;
    };
}

#endif //DAWCOREENGINE_TRACK_H

