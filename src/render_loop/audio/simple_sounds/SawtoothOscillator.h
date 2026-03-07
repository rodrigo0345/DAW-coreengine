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
                 float frequency,
                 float amplitude,
                 float& phase) override {
        const auto sampleRate = buffer->sampleRate;
        const auto phaseIncrement = static_cast<float>(2.0f * M_PI * frequency / sampleRate);

        for (size_t s = 0; s < buffer->numSamples; ++s) {
            // Normalize phase to 0-1 range, then scale to -1 to 1
            const float normalizedPhase = phase / (2.0f * M_PI);
            const float sample = (2.0f * normalizedPhase - 1.0f) * amplitude;

            for (const auto& channel : buffer->channels) {
                channel[s] += sample;
            }

            phase += phaseIncrement;
            if (phase >= 2.0f * M_PI) phase -= 2.0f * M_PI;
        }
    }

    void reset() override {}
};

}

#endif //DAWCOREENGINE_SAWTOOTHOSCILLATOR_H

