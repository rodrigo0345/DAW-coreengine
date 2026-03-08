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

    void generate(AudioBuffer& buffer,
                 const float frequency,
                 const float amplitude,
                 float& phase) override {
        const auto sr = static_cast<float>(buffer.sampleRate);
        const float phaseInc    = frequency / sr;
        const size_t numSamples = buffer.numSamples;
        const float  pw         = pulseWidth_;
        const auto&  channels   = buffer.channels;

        for (size_t s = 0; s < numSamples; ++s) {
            const float sample = ((phase < pw) ? 1.0f : -1.0f) * amplitude;
            for (const auto& channel : channels)
                channel[s] += sample;
            phase += phaseInc;
            if (phase >= 1.0f) [[unlikely]] phase -= 1.0f;
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

