//
// Created by rodrigo0345 on 3/7/26.
//

#ifndef DAWCOREENGINE_VOICE_H
#define DAWCOREENGINE_VOICE_H

#include <memory>
#include "AudioBlock.h"
#include "Oscillator.h"

namespace coreengine {
    /**
     * Represents a single voice in a synthesizer.
     * Each voice can play one note at a time using a specific oscillator.
     */
    class Voice: public coreengine::AudioBlock {
    public:
        explicit Voice(std::unique_ptr<Oscillator> osc)
            : oscillator(std::move(osc)) {}

        bool isActive = false;
        int midiNote = -1;

        void start(const float freq, const float amp) {
            frequency = freq;
            amplitude = amp;
            phase = 0.0f;
            isActive = true;
            if (oscillator) {
                oscillator->reset();
            }
        }

        void stop() {
            isActive = false;
        }

        void processBlock(std::shared_ptr<AudioBuffer> buffer) override {
            if (!isActive || !oscillator) return;
            oscillator->generate(buffer, frequency, amplitude, phase);
        }

        void releaseResources() override {}

    protected:
        std::unique_ptr<Oscillator> oscillator;
        float frequency = 0.0f;
        float phase = 0.0f;
        float amplitude = 0.0f;
    };
}
#endif //DAWCOREENGINE_VOICE_H