#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include "../AudioEffect.h"

namespace coreengine {

/**
 * Stereo tape-style delay with feedback and high-frequency damping.
 * Parameters:
 *   delayMs    0–2000 ms
 *   feedback   0–0.95
 *   damping    0–1  (0 = bright, 1 = dark)
 *   mix        0–1  (dry/wet, inherited)
 */
class DelayEffect : public AudioEffect {
public:
    explicit DelayEffect(float delayMs = 300.f, float feedback = 0.4f, float damping = 0.3f,
                         float sampleRate = 44100.f)
        : sampleRate_(sampleRate)
    {
        mix_      = 0.3f;
        setDelayMs(delayMs);
        setFeedback(feedback);
        setDamping(damping);
    }

    void setSampleRate(float sr) {
        sampleRate_ = sr;
        // Rebuild buffer at new rate
        setDelayMs(delayMs_);
    }

    void setDelayMs(float ms) {
        delayMs_ = std::clamp(ms, 1.f, 2000.f);
        const size_t newSize = static_cast<size_t>(delayMs_ * sampleRate_ / 1000.f) + 1;
        bufL_.assign(newSize, 0.f);
        bufR_.assign(newSize, 0.f);
        writePos_ = 0;
    }
    [[nodiscard]] float getDelayMs()  const { return delayMs_; }

    void setFeedback(float f) { feedback_ = std::clamp(f, 0.f, 0.98f); }
    [[nodiscard]] float getFeedback() const { return feedback_; }

    void setDamping(float d) { damping_ = std::clamp(d, 0.f, 1.f); }
    [[nodiscard]] float getDamping() const { return damping_; }

    [[nodiscard]] std::string getName() const override { return "Delay"; }

    void reset() override {
        std::fill(bufL_.begin(), bufL_.end(), 0.f);
        std::fill(bufR_.begin(), bufR_.end(), 0.f);
        filterL_ = filterR_ = 0.f;
        writePos_ = 0;
    }

    void process(std::shared_ptr<AudioBuffer> buffer) override {
        if (!enabled_ || !buffer || mix_ == 0.f || bufL_.empty()) return;

        const size_t n   = buffer->numSamples;
        const size_t ch  = buffer->channels.size();
        if (ch == 0) return;

        const size_t bufSize = bufL_.size();

        for (size_t s = 0; s < n; ++s) {
            float inL = buffer->channels[0][s];
            float inR = (ch > 1) ? buffer->channels[1][s] : inL;

            // Read delay
            float delayedL = bufL_[writePos_];
            float delayedR = bufR_[writePos_];

            // 1-pole LP filter for damping (simulate tape saturation)
            filterL_ = filterL_ * damping_ + delayedL * (1.f - damping_);
            filterR_ = filterR_ * damping_ + delayedR * (1.f - damping_);

            // Write: input + feedback * filtered output
            bufL_[writePos_] = inL + filterL_ * feedback_;
            bufR_[writePos_] = inR + filterR_ * feedback_;

            if (++writePos_ >= bufSize) writePos_ = 0;

            // Mix
            float dry = 1.f - mix_;
            buffer->channels[0][s] = inL * dry + delayedL * mix_;
            if (ch > 1)
                buffer->channels[1][s] = inR * dry + delayedR * mix_;
        }
    }

private:
    float sampleRate_;
    float delayMs_  = 300.f;
    float feedback_ = 0.4f;
    float damping_  = 0.3f;
    float filterL_  = 0.f;
    float filterR_  = 0.f;
    size_t writePos_ = 0;
    std::vector<float> bufL_, bufR_;
};

} // namespace coreengine

