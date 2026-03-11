//
// Created by rodrigo0345 on 3/9/26.
//

#ifndef DAWCOREENGINE_PLUGIN_H
#define DAWCOREENGINE_PLUGIN_H
#include <atomic>
#include <expected>
#include <string>
#include <string_view>
#include "../render_loop/audio/Instrument.h"

namespace coreengine {

    /**
     * Base class for all user-authored plugins.
     *
     * A Plugin IS an Instrument — it can be assigned to a Track and will
     * receive MIDI events (noteOn/noteOff/allNotesOff) from the timeline,
     * then write audio into the AudioBuffer via processBlock().
     *
     * If the Lua script only implements processBlock() (no note handlers),
     * the plugin acts as a master-bus effect instead; noteOn/Off are no-ops.
     */
    class Plugin : public Instrument {
    public:
        ~Plugin() override = default;

        const int         id;
        const std::string name;
        std::string       sourceCode;   // kept for hot-reload / listing
        std::atomic<bool> isReady{false};

        Plugin(int pluginId, std::string_view pluginName, std::string_view src = "")
            : id(pluginId), name(pluginName), sourceCode(src) {}

        // AudioBlock / Instrument — must be implemented by subclasses
        void processBlock(AudioBuffer& buffer) override = 0;
        void releaseResources() override {}      // optional override

        // Default MIDI stubs — override in subclasses that generate audio
        void noteOn(int /*midiNote*/, float /*velocity*/) override {}
        void noteOff(int /*midiNote*/) override {}
        void allNotesOff() override {}

        virtual std::expected<void, std::string> compileAndOptimize() = 0;
    };
}

#endif //DAWCOREENGINE_PLUGIN_H

