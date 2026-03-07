//
// Created by rodrigo0345 on 3/7/26.
//

#ifndef DAWCOREENGINE_SQUAREOSCILLATOR_H
#define DAWCOREENGINE_SQUAREOSCILLATOR_H

#include <memory>
#include <cmath>
#include "../Oscillator.h"
#include "../AudioBuffer.h"

namespace coreengine {

/**
 * Square wave oscillator implementation.
 * Generates a square wave with adjustable pulse width.
 */
class SquareOscillator: public Oscillator {
public:
    explicit SquareOscillator(float pulseWidth = 0.5f)
        : pulseWidth_(pulseWidth) {}

    void generate(std::shared_ptr<AudioBuffer> buffer,
                 float frequency,
                 float amplitude,
                 float& phase) override {
        const auto sampleRate = buffer->sampleRate;
        const auto phaseIncrement = static_cast<float>(2.0f * M_PI * frequency / sampleRate);

        for (size_t s = 0; s < buffer->numSamples; ++s) {
            // Normalize phase to 0-1 range
            const float normalizedPhase = phase / (2.0f * M_PI);
            const float sample = (normalizedPhase < pulseWidth_ ? 1.0f : -1.0f) * amplitude;

            for (const auto& channel : buffer->channels) {
                channel[s] += sample;
            }

            phase += phaseIncrement;
            if (phase >= 2.0f * M_PI) phase -= 2.0f * M_PI;
        }
    }

    void reset() override {}

    void setPulseWidth(float width) {
        pulseWidth_ = std::clamp(width, 0.0f, 1.0f);
    }

private:
    float pulseWidth_; // 0.0 to 1.0, where 0.5 is a perfect square wave
};

}

#endif //DAWCOREENGINE_SQUAREOSCILLATOR_H

