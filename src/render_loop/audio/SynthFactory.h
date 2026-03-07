//
// Created by rodrigo0345 on 3/7/26.
//

#ifndef DAWCOREENGINE_SYNTHFACTORY_H
#define DAWCOREENGINE_SYNTHFACTORY_H

#include <memory>
#include "Instrument.h"
#include "simple_sounds/SimpleSynth.h"
#include "simple_sounds/SineOscillator.h"
#include "simple_sounds/SquareOscillator.h"
#include "simple_sounds/SawtoothOscillator.h"

namespace coreengine {
    /**
     * Factory class for creating pre-configured synthesizers.
     * Provides convenient methods to create common synth types.
     */
    class SynthFactory {
    public:
        /**
         * Create a sine wave synthesizer
         * @param numVoices Number of polyphonic voices (default: 8)
         * @return A unique pointer to the synth
         */
        static std::unique_ptr<Instrument> createSineSynth(int numVoices = 8) {
            return std::make_unique<SimpleSynth>(
                numVoices,
                []() -> std::unique_ptr<Oscillator> {
                    return std::make_unique<SineOscillator>();
                }
            );
        }

        /**
         * Create a square wave synthesizer
         * @param numVoices Number of polyphonic voices (default: 8)
         * @param pulseWidth Pulse width ratio 0.0-1.0 (default: 0.5 for perfect square)
         * @return A unique pointer to the synth
         */
        static std::unique_ptr<Instrument> createSquareSynth(int numVoices = 8, float pulseWidth = 0.5f) {
            return std::make_unique<SimpleSynth>(
                numVoices,
                [pulseWidth]() -> std::unique_ptr<Oscillator> {
                    return std::make_unique<SquareOscillator>(pulseWidth);
                }
            );
        }

        /**
         * Create a sawtooth wave synthesizer
         * @param numVoices Number of polyphonic voices (default: 8)
         * @return A unique pointer to the synth
         */
        static std::unique_ptr<Instrument> createSawtoothSynth(int numVoices = 8) {
            return std::make_unique<SimpleSynth>(
                numVoices,
                []() -> std::unique_ptr<Oscillator> {
                    return std::make_unique<SawtoothOscillator>();
                }
            );
        }

        /**
         * Create a PWM (Pulse Width Modulation) synthesizer with narrow pulse
         * Good for creating buzzy, thin sounds
         * @param numVoices Number of polyphonic voices (default: 8)
         * @return A unique pointer to the synth
         */
        static std::unique_ptr<Instrument> createPWMSynth(int numVoices = 8) {
            return std::make_unique<SimpleSynth>(
                numVoices,
                []() -> std::unique_ptr<Oscillator> {
                    return std::make_unique<SquareOscillator>(0.1f); // 10% pulse width
                }
            );
        }
    };
}

#endif //DAWCOREENGINE_SYNTHFACTORY_H

