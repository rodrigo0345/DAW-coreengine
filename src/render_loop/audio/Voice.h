//
// Created by rodrigo0345 on 3/7/26.
//

#ifndef DAWCOREENGINE_VOICE_H
#define DAWCOREENGINE_VOICE_H

#include <array>
#include <memory>
#include "AudioBlock.h"
#include "AudioBuffer.h"
#include "Oscillator.h"
#include "ADSR.h"
#include "EffectChain.h"
#include "../../configs/EngineConfig.h"

namespace coreengine {

class Voice : public AudioBlock {
public:
    explicit Voice(std::unique_ptr<Oscillator> osc,
                   double sampleRate = static_cast<double>(SampleRate::STUDIO))
        : frequency_(0.f), phase_(0.f), amplitude_(0.f)
        , sampleRate_(sampleRate)
        , oscillator(std::move(osc))
        , adsr_(sampleRate)
    {
        isActive = false;
        midiNote = -1;
        // Pre-wire monoScratch once — never re-allocated on hot path
        monoScratch_.sampleRate = static_cast<uint64_t>(sampleRate);
        monoScratch_.numSamples = MAX_BLOCK_SIZE;
        monoScratch_.channels.resize(1);
        monoScratch_.channels[0] = scratch_.data();
    }

    bool isActive;
    int  midiNote;

    void start(float freq, float amp) {
        frequency_ = freq;
        amplitude_ = amp;
        phase_     = 0.f;
        isActive   = true;
        if (oscillator) oscillator->reset();
        adsr_.trigger();
        effectChain_.reset();
    }

    void stop() { adsr_.release(); }

    // ── Zero-alloc processBlock ───────────────────────────────────────────
    void processBlock(AudioBuffer& buffer) override {
        if (!isActive || !oscillator) return;

        const size_t numSamples  = buffer.numSamples;
        const size_t numChannels = buffer.channels.size();

        std::fill_n(scratch_.data(), numSamples, 0.f);

        // Update only the fields that change per block — no allocation
        monoScratch_.sampleRate = buffer.sampleRate;
        monoScratch_.numSamples = numSamples;

        oscillator->generate(monoScratch_, frequency_, amplitude_, phase_);

        for (size_t s = 0; s < numSamples; ++s) {
            const float env = adsr_.process();
            const float sample = scratch_[s] * env;
            for (size_t ch = 0; ch < numChannels; ++ch)
                buffer.channels[ch][s] += sample;
            if (!adsr_.isActive()) {
                isActive = false;
                break;
            }
        }

        if (!effectChain_.isEmpty())
            effectChain_.process(buffer);
    }

    void releaseResources() override { effectChain_.clear(); }

    void setADSRParameters(const ADSR::Parameters& params) { adsr_.setParameters(params); }
    [[nodiscard]] const ADSR::Parameters& getADSRParameters() const { return adsr_.getParameters(); }
    [[nodiscard]] ADSR& getADSR() { return adsr_; }
    [[nodiscard]] EffectChain& getEffectChain() { return effectChain_; }
    void setSampleRate(double sr) { sampleRate_ = sr; adsr_.setSampleRate(sr); }
    [[nodiscard]] bool isReleasing() const { return adsr_.isReleasing(); }

    // Pre-allocate the mono scratch channel vector once (called after construction)
    void preallocate() {
        // nothing more needed — scratch_ is a std::array, already allocated
    }

private:
    float  frequency_, phase_, amplitude_;
    double sampleRate_;

    std::unique_ptr<Oscillator>                    oscillator;
    ADSR                                           adsr_;
    EffectChain                                    effectChain_;

    // Fixed-size scratch + pre-wired mono AudioBuffer — zero heap per block
    std::array<float, MAX_BLOCK_SIZE> scratch_{};
    AudioBuffer                       monoScratch_;   // channels[0] → scratch_.data(), set in ctor
};

} // namespace coreengine
#endif //DAWCOREENGINE_VOICE_H




