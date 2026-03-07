//
// Created by rodrigo0345 on 3/7/26.
//

#ifndef DAWCOREENGINE_OSCILLATOR_H
#define DAWCOREENGINE_OSCILLATOR_H

#include <memory>
#include "AudioBuffer.h"

namespace coreengine {
    /**
     * Abstract base class for oscillators.
     * Defines the interface for generating audio waveforms.
     */
    class Oscillator {
    public:
        virtual ~Oscillator() = default;

        /**
         * Generate audio samples and add them to the buffer
         * @param buffer The audio buffer to write to
         * @param frequency The frequency in Hz
         * @param amplitude The amplitude (0.0 to 1.0)
         * @param phase The current phase (will be updated)
         */
        virtual void generate(std::shared_ptr<AudioBuffer> buffer,
                            float frequency,
                            float amplitude,
                            float& phase) = 0;

        /**
         * Reset the oscillator state
         */
        virtual void reset() {}
    };
}

#endif //DAWCOREENGINE_OSCILLATOR_H

