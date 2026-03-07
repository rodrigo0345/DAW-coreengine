//
// Created by rodrigo0345 on 3/4/26.
//

#ifndef DAWCOREENGINE_SINEOSCILLATOR_H
#define DAWCOREENGINE_SINEOSCILLATOR_H

#include <memory>
#include <cmath>

#include "../AudioBuffer.h"
#include "../Oscillator.h"


namespace coreengine {

/**
 * Sine wave oscillator implementation.
 * Generates a pure sine wave tone.
 */
class SineOscillator: public coreengine::Oscillator {
public:
    void generate(std::shared_ptr<AudioBuffer> buffer,
                 float frequency,
                 float amplitude,
                 float& phase) override;

    void reset() override {
        // Nothing specific to reset for sine oscillator
    }
};

}

#endif //DAWCOREENGINE_SINEOSCILLATOR_H