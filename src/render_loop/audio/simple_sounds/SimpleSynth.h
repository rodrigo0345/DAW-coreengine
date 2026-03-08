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
#include "../ADSR.h"

namespace coreengine {
    /**
     * A polyphonic synthesizer that can be configured with different oscillator types.
     * This is a flexible synth implementation that supports multiple voices with ADSR envelopes.
     */
    class SimpleSynth : public Instrument {
    public:
        /**
         * Create a synth with a specific number of voices and oscillator type
         * @param numVoices Number of simultaneous voices (polyphony)
         * @param oscillatorCreator Function that creates oscillator instances
         * @param sampleRate Sample rate in Hz
         * @param adsrParams ADSR parameters for all voices
         */
        SimpleSynth(int numVoices,
                   VoiceFactory::OscillatorCreator oscillatorCreator,
                   double sampleRate = 48000.0,
                   const ADSR::Parameters& adsrParams = ADSR::Parameters())
            : oscillatorCreator_(std::move(oscillatorCreator))
            , sampleRate_(sampleRate)
            , adsrParameters_(adsrParams)
        {
            voices.reserve(numVoices);
            for (int i = 0; i < numVoices; ++i) {
                voices.push_back(VoiceFactory::createVoice(
                    oscillatorCreator_,
                    sampleRate_,
                    adsrParameters_
                ));
            }
        }

        /**
         * Create a synth with default sine wave oscillators
         * @param numVoices Number of simultaneous voices (default: 8)
         * @param sampleRate Sample rate in Hz
         */
        explicit SimpleSynth(int numVoices = 8, double sampleRate = 48000.0)
            : SimpleSynth(numVoices,
                []() -> std::unique_ptr<Oscillator> {
                    return std::make_unique<SineOscillator>();
                },
                sampleRate) {}

        void noteOn(int midiNote, float velocity) override {
            // 1. Convert MIDI note to Frequency: f = 440 * 2^((n-69)/12)
            const float freq = 440.0f * std::pow(2.0f, (static_cast<float>(midiNote) - 69.0f) / 12.0f);
            const float amp = velocity / 127.0f * 0.2f; // Scale volume and keep it low

            // 2. Find an idle voice
            for (const auto& voice : voices) {
                if (!voice->isActive) {
                    voice->midiNote = midiNote;
                    voice->start(freq, amp);
                    return;
                }
            }

            // 3. Voice stealing: steal the oldest releasing voice, or the first voice
            Voice* stealVoice = nullptr;
            for (const auto& voice : voices) {
                if (voice->isReleasing()) {
                    stealVoice = voice.get();
                    break;
                }
            }

            // If no releasing voice, steal the first one
            if (!stealVoice && !voices.empty()) {
                stealVoice = voices[0].get();
            }

            if (stealVoice) {
                stealVoice->midiNote = midiNote;
                stealVoice->start(freq, amp);
            }
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
                if (voice->isActive) {
                    voice->stop();
                }
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

        /**
         * Set ADSR parameters for all voices
         */
        void setADSRParameters(const ADSR::Parameters& params) {
            adsrParameters_ = params;
            for (const auto& voice : voices) {
                voice->setADSRParameters(params);
            }
        }

        /**
         * Get current ADSR parameters
         */
        [[nodiscard]] const ADSR::Parameters& getADSRParameters() const {
            return adsrParameters_;
        }

        /**
         * Get a specific voice for fine-tuning
         */
        [[nodiscard]] Voice* getVoice(size_t index) {
            return (index < voices.size()) ? voices[index].get() : nullptr;
        }

        /**
         * Get number of voices
         */
        [[nodiscard]] size_t getVoiceCount() const {
            return voices.size();
        }

        /**
         * Update sample rate for all voices
         */
        void setSampleRate(double sampleRate) {
            sampleRate_ = sampleRate;
            for (const auto& voice : voices) {
                voice->setSampleRate(sampleRate);
            }
        }

        /**
         * Set number of polyphonic voices at runtime (voices are rebuilt)
         */
        void setVoiceCount(int numVoices) {
            numVoices = std::max(1, numVoices);
            for (const auto& v : voices) v->stop();
            voices.clear();
            voices.reserve(numVoices);
            for (int i = 0; i < numVoices; ++i) {
                voices.push_back(VoiceFactory::createVoice(oscillatorCreator_, sampleRate_, adsrParameters_));
            }
        }

    private:
        std::vector<std::unique_ptr<Voice>> voices;
        VoiceFactory::OscillatorCreator oscillatorCreator_;
        double sampleRate_;
        ADSR::Parameters adsrParameters_;
    };
}
#endif //DAWCOREENGINE_SIMPLESYNTH_H