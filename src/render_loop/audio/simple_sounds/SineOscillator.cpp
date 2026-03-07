//
// Created by rodrigo0345 on 3/4/26.
//

#include "SineOscillator.h"
#include <cmath>
#include <memory>

void coreengine::SineOscillator::generate(std::shared_ptr<coreengine::AudioBuffer> buffer,
                                          float frequency,
                                          float amplitude,
                                          float& phase) {
    const auto sampleRate = buffer->sampleRate;
    const auto phaseIncrement = static_cast<float>(2.0f * M_PI * frequency / sampleRate);

    for (size_t s = 0; s < buffer->numSamples; ++s) {
        const float sample = std::sin(phase) * amplitude;
        for (const auto& channel : buffer->channels) {
            channel[s] += sample;
        }
        phase += phaseIncrement;
        if (phase >= 2.0f * M_PI) phase -= 2.0f * M_PI;
    }
}

