//
// Created by rodrigo0345 on 3/7/26.
//

#ifndef DAWCOREENGINE_VOICE_H
#define DAWCOREENGINE_VOICE_H

#include <array>
#include <atomic>
#include <memory>
#include "AudioBlock.h"
#include "AudioBuffer.h"
#include "Oscillator.h"
#include "ADSR.h"
#include "EffectChain.h"
#include "simd_ops.h"
#include "../../configs/EngineConfig.h"

namespace coreengine {

class Voice : public AudioBlock {
public:
    explicit Voice(std::unique_ptr<Oscillator> osc,
                   double sampleRate = static_cast<double>(ENGINE_SAMPLE_RATE))
        : frequency_(0.f), phase_(0.f), amplitude_(0.f)
        , sampleRate_(sampleRate)
        , oscillator(std::move(osc))
        , adsr_(sampleRate)
    {
        isActive = false;
        midiNote = -1;
        // Pre-wire monoScratch once — never re-allocated on hot path
        monoScratch_.sampleRate = ENGINE_SAMPLE_RATE;
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

    // ── Zero-alloc processBlock (SIMD-accelerated) ─────────────────────────
    void processBlock(AudioBuffer& buffer) override {
        if (!isActive || !oscillator) return;

        const size_t numSamples  = buffer.numSamples;
        const size_t numChannels = buffer.channels.size();

        simd::clear(scratch_.data(), numSamples);

        monoScratch_.sampleRate = buffer.sampleRate;
        monoScratch_.numSamples = numSamples;

        // 1. Generate oscillator output into scratch_ (mono)
        oscillator->generate(monoScratch_, frequency_, amplitude_, phase_);

        // 2. Render the ADSR envelope into envBuf_ — one scalar process() per
        //    sample (ADSR has state transitions so we can't fully vectorise it,
        //    but we batch the output and use SIMD for the multiply step).
        size_t activeSamples = numSamples;
        for (size_t s = 0; s < numSamples; ++s) {
            envBuf_[s] = adsr_.process();
            if (!adsr_.isActive()) {
                // Zero remaining envelope samples so SIMD multiply is correct
                simd::clear(envBuf_.data() + s + 1, numSamples - s - 1);
                activeSamples = s + 1;
                isActive = false;
                break;
            }
        }

        // 3. scratch_[i] *= envBuf_[i]  — in-place multiply (no separate dst needed)
        //    Then accumulate into each output channel: out[ch][i] += scratch_[i]
        //    We do the multiply once and reuse the result for all channels.
        //
        //    Use AVX2: scratch_[i] = scratch_[i] * envBuf_[i]  (in-place)
        //    Then accumulate_scaled(out, scratch_, 1.0f) per channel.
        {
#if defined(__AVX2__)
            size_t i = 0;
            for (; i + 8 <= activeSamples; i += 8) {
                __m256 vs = _mm256_loadu_ps(scratch_.data() + i);
                __m256 ve = _mm256_loadu_ps(envBuf_.data()  + i);
                _mm256_storeu_ps(scratch_.data() + i, _mm256_mul_ps(vs, ve));
            }
            for (; i < activeSamples; ++i) scratch_[i] *= envBuf_[i];
#else
            for (size_t i = 0; i < activeSamples; ++i) scratch_[i] *= envBuf_[i];
#endif
            // accumulate into all output channels — gain=1 so just add
            for (size_t ch = 0; ch < numChannels; ++ch)
                simd::accumulate_scaled(buffer.channels[ch], scratch_.data(), 1.0f, activeSamples);
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

    // Fixed-size scratch: oscillator output + ADSR envelope — zero heap per block
    std::array<float, MAX_BLOCK_SIZE> scratch_{};
    std::array<float, MAX_BLOCK_SIZE> envBuf_{};   // ADSR envelope rendered per block
    AudioBuffer                       monoScratch_;   // channels[0] → scratch_.data(), set in ctor
};

} // namespace coreengine
#endif //DAWCOREENGINE_VOICE_H




