//
// Created by rodrigo0345 on 3/7/26.
//

#ifndef DAWCOREENGINE_VOICE_H
#define DAWCOREENGINE_VOICE_H

#include <memory>
#include <vector>
#include "AudioBlock.h"
#include "Oscillator.h"
#include "ADSR.h"
#include "EffectChain.h"
#include "../../configs/EngineConfig.h"

namespace coreengine {
    /**
     * Represents a single voice in a synthesizer.
     * Each voice can play one note at a time using a specific oscillator,
     * with professional ADSR envelope shaping and optional effects chain.
     */
    class Voice: public coreengine::AudioBlock {
    public:
        explicit Voice(std::unique_ptr<Oscillator> osc, const double sampleRate = static_cast<double>(SampleRate::STUDIO))
        : frequency_(0.0f)
            , phase_(0.0f)
            , amplitude_(0.0f)
            , sampleRate_(sampleRate)
            , oscillator(std::move(osc))
            , adsr_(sampleRate)
            , effectChain_()
            , scratchBuf_()
        {
            isActive = false;
            midiNote = -1;
        }

        bool isActive;
        int midiNote;

        /**
         * Start playing a note
         * @param freq Frequency in Hz
         * @param amp Amplitude (0.0 to 1.0)
         */
        void start(const float freq, const float amp) {
            frequency_ = freq;
            amplitude_ = amp;
            phase_ = 0.0f;
            isActive = true;

            if (oscillator) {
                oscillator->reset();
            }

            adsr_.trigger();
            effectChain_.reset();
        }

        /**
         * Stop playing (trigger release)
         */
        void stop() {
            adsr_.release();
        }

        /**
         * Process audio block with ADSR envelope and effects.
         * Uses a private scratch buffer so the envelope and effects
         * don't corrupt samples already accumulated from other voices.
         */
        void processBlock(std::shared_ptr<AudioBuffer> buffer) override {
            if (!isActive || !oscillator || !buffer) return;

            const auto numSamples = buffer->numSamples;
            const auto numChannels = buffer->channels.size();

            // Allocate/reuse a scratch buffer (one channel is enough, we mono-generate)
            if (scratchBuf_.size() < numSamples)
                scratchBuf_.resize(numSamples, 0.0f);

            // Clear scratch
            std::fill_n(scratchBuf_.data(), numSamples, 0.0f);

            // Wrap scratch in a temporary AudioBuffer for the oscillator
            auto tmpBuf = std::make_shared<AudioBuffer>();
            tmpBuf->sampleRate = buffer->sampleRate;
            tmpBuf->numSamples = numSamples;
            tmpBuf->channels.resize(1);
            tmpBuf->channels[0] = scratchBuf_.data();

            // Generate raw oscillator audio into scratch
            oscillator->generate(tmpBuf, frequency_, amplitude_, phase_);

            // Apply ADSR envelope per-sample and accumulate into the real buffer
            for (size_t s = 0; s < numSamples; ++s) {
                const float env = adsr_.process();
                const float sample = scratchBuf_[s] * env;

                for (size_t ch = 0; ch < numChannels; ++ch) {
                    buffer->channels[ch][s] += sample;
                }

                if (!adsr_.isActive()) {
                    isActive = false;
                    break;  // remaining samples stay 0 – nothing to add
                }
            }

            // Process through effect chain (operates on the main buffer)
            if (!effectChain_.isEmpty()) {
                effectChain_.process(buffer);
            }
        }

        void releaseResources() override {
            effectChain_.clear();
        }

        /**
         * Set ADSR parameters
         */
        void setADSRParameters(const ADSR::Parameters& params) {
            adsr_.setParameters(params);
        }

        /**
         * Get ADSR parameters
         */
        [[nodiscard]] const ADSR::Parameters& getADSRParameters() const {
            return adsr_.getParameters();
        }

        /**
         * Get reference to ADSR envelope
         */
        [[nodiscard]] ADSR& getADSR() {
            return adsr_;
        }

        /**
         * Get reference to effect chain
         */
        [[nodiscard]] EffectChain& getEffectChain() {
            return effectChain_;
        }

        /**
         * Update sample rate
         */
        void setSampleRate(double sampleRate) {
            sampleRate_ = sampleRate;
            adsr_.setSampleRate(sampleRate);
        }

        /**
         * Check if voice is in release phase
         */
        [[nodiscard]] bool isReleasing() const {
            return adsr_.isReleasing();
        }

    private:
        float frequency_;
        float phase_;
        float amplitude_;
        double sampleRate_;

        std::unique_ptr<Oscillator> oscillator;
        ADSR adsr_;
        EffectChain effectChain_;
        std::vector<float> scratchBuf_;   // per-voice scratch to avoid corrupting shared buffer
    };
}
#endif //DAWCOREENGINE_VOICE_H