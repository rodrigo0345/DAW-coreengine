//
// Created by rodrigo0345 on 3/7/26.
//

#ifndef DAWCOREENGINE_SIMPLESYNTH_H
#define DAWCOREENGINE_SIMPLESYNTH_H

#include <vector>
#include <cmath>
#include <memory>
#include <functional>

#include "SineOscillator.h"
#include "../Instrument.h"
#include "../VoiceFactory.h"

namespace coreengine {
    /**
     * A polyphonic synthesizer that can be configured with different oscillator types.
     * This is a flexible synth implementation that supports multiple voices.
     */
    class SimpleSynth : public Instrument {
    public:
        /**
         * Create a synth with a specific number of voices and oscillator type
         * @param numVoices Number of simultaneous voices (polyphony)
         * @param oscillatorCreator Function that creates oscillator instances
         */
        SimpleSynth(int numVoices, VoiceFactory::OscillatorCreator oscillatorCreator)
            : oscillatorCreator_(std::move(oscillatorCreator)) {
            voices.reserve(numVoices);
            for (int i = 0; i < numVoices; ++i) {
                voices.push_back(VoiceFactory::createVoice(oscillatorCreator_));
            }
        }

        /**
         * Create a synth with default sine wave oscillators
         * @param numVoices Number of simultaneous voices (default: 8)
         */
        explicit SimpleSynth(int numVoices = 8)
            : SimpleSynth(numVoices, []() -> std::unique_ptr<Oscillator> {
                return std::make_unique<SineOscillator>();
            }) {}

        void noteOn(int midiNote, float velocity) override {
            // 1. Convert MIDI note to Frequency: f = 440 * 2^((n-69)/12)
            const float freq = 440.0f * std::pow(2.0f, (midiNote - 69.0f) / 12.0f);
            const float amp = velocity / 127.0f * 0.2f; // Scale volume and keep it low

            // 2. Find an idle voice
            for (const auto& voice : voices) {
                if (!voice->isActive) {
                    voice->midiNote = midiNote;
                    voice->start(freq, amp);
                    return;
                }
            }

            // If no idle voice found, voice stealing could be implemented here
            // For now, we simply ignore the note if all voices are busy
        }

        void noteOff(int midiNote) override {
            for (const auto& voice : voices) {
                if (voice->isActive && voice->midiNote == midiNote) {
                    voice->stop();
                }
            }
        }

        void allNotesOff() override {
            for (const auto& voice : voices) {
                voice->stop();
            }
        }

        void processBlock(std::shared_ptr<AudioBuffer> buffer) override {
            for (const auto& voice : voices) {
                voice->processBlock(buffer);
            }
        }

        void releaseResources() override {
            for (const auto& voice : voices) {
                voice->releaseResources();
            }
        }

    private:
        std::vector<std::unique_ptr<Voice>> voices;
        VoiceFactory::OscillatorCreator oscillatorCreator_;
    };
}
#endif //DAWCOREENGINE_SIMPLESYNTH_H