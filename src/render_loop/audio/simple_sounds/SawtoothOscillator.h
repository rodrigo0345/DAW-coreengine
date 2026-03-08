//
// Created by rodrigo0345 on 3/7/26.
//

#ifndef DAWCOREENGINE_SAWTOOTHOSCILLATOR_H
#define DAWCOREENGINE_SAWTOOTHOSCILLATOR_H

#include <memory>
#include <cmath>
#include "../Oscillator.h"
#include "../AudioBuffer.h"

namespace coreengine {

/**
 * Sawtooth wave oscillator implementation.
 * Generates a sawtooth wave (linear ramp from -1 to 1).
 */
class SawtoothOscillator: public Oscillator {
public:
    void generate(std::shared_ptr<AudioBuffer> buffer,
                 const float frequency,
                 const float amplitude,
                 float& phase) override {
        const auto sr = static_cast<float>(buffer->sampleRate);
        const float phaseInc = frequency / sr;

        const size_t numSamples = buffer->numSamples;
        const auto& channels = buffer->channels;

        for (size_t s = 0; s < numSamples; ++s) {
            // Map normalized phase [0, 1) to waveform range [-1, 1]
            // Calculation: (Phase * 2) - 1
            const float sample = (2.0f * phase - 1.0f) * amplitude;
            for (auto& channel : channels) {
                channel[s] += sample;
            }
            phase += phaseInc;

            if (phase >= 1.0f) [[unlikely]] {
                phase -= 1.0f;
            }
        }
    }

    void reset() override {}
};

}

#endif //DAWCOREENGINE_SAWTOOTHOSCILLATOR_H

