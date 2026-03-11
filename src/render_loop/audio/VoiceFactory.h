//
// Created by rodrigo0345 on 3/7/26.
//

#ifndef DAWCOREENGINE_VOICEFACTORY_H
#define DAWCOREENGINE_VOICEFACTORY_H

#include <memory>
#include <functional>
#include "Voice.h"
#include "Oscillator.h"
#include "ADSR.h"

namespace coreengine {
    /**
     * Factory for creating Voice instances with different oscillator types.
     * This allows easy configuration of synths with different sound generators.
     */
    class VoiceFactory {
    public:
        using OscillatorCreator = std::function<std::unique_ptr<Oscillator>()>;

        /**
         * Create a voice with a specific oscillator type and sample rate
         * @param createOscillator Function that creates an oscillator instance
         * @param sampleRate Sample rate in Hz (default: 48000)
         * @param adsrParams Optional ADSR parameters
         * @return A new Voice instance
         */
        static std::unique_ptr<Voice> createVoice(
            const OscillatorCreator& createOscillator,
            double sampleRate = static_cast<double>(ENGINE_SAMPLE_RATE),
            const ADSR::Parameters& adsrParams = ADSR::Parameters()
        ) {
            auto voice = std::make_unique<Voice>(createOscillator(), sampleRate);
            voice->setADSRParameters(adsrParams);
            return voice;
        }
    };
}

#endif //DAWCOREENGINE_VOICEFACTORY_H

