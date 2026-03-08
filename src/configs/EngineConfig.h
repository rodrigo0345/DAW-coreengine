//
// Created by rodrigo0345 on 3/4/26.
//

#ifndef DAWCOREENGINE_CORESERVICEENGINECONFIG_H
#define DAWCOREENGINE_CORESERVICEENGINECONFIG_H
#include <cstdint>

namespace coreengine {
    enum class SampleRate {
        CD       = 44100,
        VIDEO    = 48000,
        HIGHRES  = 192000,
        STUDIO   = 196000,
        LOSSLESS = 0,
        DEV      = 22000,
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
        SampleRate sampleRate;
        DspFormat dspFormat;
        Channels channels;
        const uint32_t sampleBlockSize = 512; // TODO: make this dynamic

        [[nodiscard]] uint8_t getChannelsVal() const {
            return static_cast<uint8_t>(channels);
        }

        [[nodiscard]] uint32_t getSampleRateVal() const {
            return static_cast<uint32_t>(sampleRate);
        }
    };

}

#endif //DAWCOREENGINE_CORESERVICEENGINECONFIG_H