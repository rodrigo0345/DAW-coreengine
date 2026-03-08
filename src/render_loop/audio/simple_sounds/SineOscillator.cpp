//
// Created by rodrigo0345 on 3/4/26.
//

#include "SineOscillator.h"
#include <cmath>
#include <memory>

void coreengine::SineOscillator::generate(std::shared_ptr<coreengine::AudioBuffer> buffer,
                                          const float frequency,
                                          const float amplitude,
                                          float& phase) {
    const auto sampleRate = static_cast<float>(buffer->sampleRate);
    constexpr float pi2 = 2.0f * std::numbers::pi_v<float>;
    const float phaseInc = (pi2 * frequency) / sampleRate;

    const size_t numSamples = buffer->numSamples;
    const auto& channels = buffer->channels;

    for (size_t s = 0; s < numSamples; ++s) {
        const float sample = std::sin(phase) * amplitude;

        // Compiler can vectorize this inner loop
        for (const auto& channel : channels) {
            channel[s] += sample;
        }

        phase += phaseInc;
    }

    // Wrap phase once per block instead of once per sample
    phase = std::fmod(phase, pi2);
}

