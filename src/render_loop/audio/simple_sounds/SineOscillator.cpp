//
// Created by rodrigo0345 on 3/4/26.
//

#include "SineOscillator.h"
#include <cmath>
#include <memory>

void coreengine::SineOscillator::processBlock(std::shared_ptr<coreengine::AudioBuffer> buffer) {
    const auto sampleRate = buffer->sampleRate;
    const auto phaseIncrement = static_cast<float>(2.0f * M_PI * frequency / sampleRate);

    for (size_t sample = 0; sample < buffer->numSamples; ++sample) {
        const float currentSample = std::sin(phase) * this->amplitude;

        for (const auto & channel : buffer->channels) {
            channel[sample] += currentSample;
        }

        phase += phaseIncrement;
        if (phase >= 2.0f * M_PI) phase -= 2.0f * M_PI;
    }
}

void coreengine::SineOscillator::releaseResources() {
}
