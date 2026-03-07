//
// Created by rodrigo0345 on 3/7/26.
//

#ifndef DAWCOREENGINE_SYNTHFACTORY_H
#define DAWCOREENGINE_SYNTHFACTORY_H

#include <memory>
#include "Instrument.h"
#include "ADSR.h"
#include "simple_sounds/SimpleSynth.h"
#include "simple_sounds/SineOscillator.h"
#include "simple_sounds/SquareOscillator.h"
#include "simple_sounds/SawtoothOscillator.h"

namespace coreengine {
    /**
     * Factory class for creating pre-configured synthesizers.
     * Every factory method accepts sampleRate so ADSR timing is correct.
     */
    class SynthFactory {
    public:
        /**
         * Create a sine wave synthesizer
         * @param numVoices Number of polyphonic voices (default: 8)
         * @return A unique pointer to the synth
         */
        static std::unique_ptr<Instrument> createSineSynth(int numVoices = 8, double sampleRate = 44100.0) {
            return std::make_unique<SimpleSynth>(
                numVoices,
                []() -> std::unique_ptr<Oscillator> { return std::make_unique<SineOscillator>(); },
                sampleRate
            );
        }

        /**
         * Create a square wave synthesizer
         * @param numVoices Number of polyphonic voices (default: 8)
         * @param pulseWidth Pulse width ratio 0.0-1.0 (default: 0.5 for perfect square)
         * @return A unique pointer to the synth
         */
        static std::unique_ptr<Instrument> createSquareSynth(int numVoices = 8, double sampleRate = 44100.0, float pulseWidth = 0.5f) {
            return std::make_unique<SimpleSynth>(
                numVoices,
                [pulseWidth]() -> std::unique_ptr<Oscillator> { return std::make_unique<SquareOscillator>(pulseWidth); },
                sampleRate
            );
        }

        /**
         * Create a sawtooth wave synthesizer
         * @param numVoices Number of polyphonic voices (default: 8)
         * @return A unique pointer to the synth
         */
        static std::unique_ptr<Instrument> createSawtoothSynth(int numVoices = 8, double sampleRate = 44100.0) {
            return std::make_unique<SimpleSynth>(
                numVoices,
                []() -> std::unique_ptr<Oscillator> { return std::make_unique<SawtoothOscillator>(); },
                sampleRate
            );
        }

        /**
         * Create a PWM (Pulse Width Modulation) synthesizer with narrow pulse
         * Good for creating buzzy, thin sounds
         * @param numVoices Number of polyphonic voices (default: 8)
         * @return A unique pointer to the synth
         */
        static std::unique_ptr<Instrument> createPWMSynth(int numVoices = 8, double sampleRate = 44100.0) {
            return std::make_unique<SimpleSynth>(
                numVoices,
                []() -> std::unique_ptr<Oscillator> { return std::make_unique<SquareOscillator>(0.1f); },
                sampleRate
            );
        }

        /** Create any synth type by index. 0=Sine, 1=Square, 2=Saw, 3=PWM */
        static std::unique_ptr<Instrument> createByType(int synthType, int numVoices = 8, double sampleRate = 44100.0) {
            switch (synthType) {
                case 1:  return createSquareSynth(numVoices, sampleRate);
                case 2:  return createSawtoothSynth(numVoices, sampleRate);
                case 3:  return createPWMSynth(numVoices, sampleRate);
                default: return createSineSynth(numVoices, sampleRate);
            }
        }
    };
}

#endif //DAWCOREENGINE_SYNTHFACTORY_H

