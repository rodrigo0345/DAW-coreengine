//
// Created by rodrigo0345 on 3/4/26.
//

#ifndef DAWCOREENGINE_CORESERVICEENGINECONFIG_H
#define DAWCOREENGINE_CORESERVICEENGINECONFIG_H
#include <cstdint>

namespace coreengine {

    // ── Single source of truth ────────────────────────────────────────────────
    // Change only these two lines to reconfigure the entire engine.
    inline constexpr uint32_t ENGINE_SAMPLE_RATE = 44100;   // Hz
    inline constexpr uint32_t ENGINE_BLOCK_SIZE  = 512;     // samples per block

    enum class SampleRate : uint32_t {
        CD       = 44100,
        VIDEO    = 48000,
        HIGHRES  = 192000,
        STUDIO   = 196000,
        DEV      = 22000,
        Default  = ENGINE_SAMPLE_RATE,   // alias — always matches the constant above
    };

    enum class DspFormat {
        FLOAT32,
        FLOAT64,
    };

    enum class Channels {
        MONO = 1,
        STEREO = 2,
        SURROUND_5_1 = 6,
        SURROUND_7_1 = 8,
    };

    class EngineConfig {
    public:
        SampleRate sampleRate = SampleRate::Default;
        DspFormat  dspFormat  = DspFormat::FLOAT32;
        Channels   channels   = Channels::STEREO;
        uint32_t   sampleBlockSize = ENGINE_BLOCK_SIZE;

        [[nodiscard]] uint8_t  getChannelsVal()   const { return static_cast<uint8_t>(channels); }
        [[nodiscard]] uint32_t getSampleRateVal() const { return static_cast<uint32_t>(sampleRate); }
    };

}

#endif //DAWCOREENGINE_CORESERVICEENGINECONFIG_H

