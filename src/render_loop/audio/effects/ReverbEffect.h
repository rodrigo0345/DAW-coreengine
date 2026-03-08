#pragma once
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>
#include "../AudioEffect.h"

namespace coreengine {

/**
 * Freeverb-inspired plate reverb using Schroeder all-pass + comb filters.
 * Parameters:
 *   roomSize  0–1   (larger = longer decay)
 *   damping   0–1   (higher = duller tail)
 *   mix       0–1   (dry/wet, inherited from AudioEffect)
 */
class ReverbEffect : public AudioEffect {
public:
    static constexpr float kSampleRate = static_cast<float>(SampleRate::VIDEO);

    explicit ReverbEffect(float roomSize = 0.5f, float damping = 0.5f)
    {
        setRoomSize(roomSize);
        setDamping(damping);
        mix_ = 0.3f;   // default 30 % wet for reverb
        initBuffers();
    }

    void setRoomSize(float r) {
        roomSize_ = std::clamp(r, 0.0f, 1.0f);
        updateCoefficients();
    }
    [[nodiscard]] float getRoomSize() const { return roomSize_; }

    void setDamping(float d) {
        damping_ = std::clamp(d, 0.0f, 1.0f);
        updateCoefficients();
    }
    [[nodiscard]] float getDamping() const { return damping_; }

    [[nodiscard]] std::string getName() const override { return "Reverb"; }

    void reset() override {
        for (auto& b : combL_)    std::fill(b.buf.begin(), b.buf.end(), 0.f);
        for (auto& b : combR_)    std::fill(b.buf.begin(), b.buf.end(), 0.f);
        for (auto& b : allpassL_) std::fill(b.buf.begin(), b.buf.end(), 0.f);
        for (auto& b : allpassR_) std::fill(b.buf.begin(), b.buf.end(), 0.f);
    }

    void process(std::shared_ptr<AudioBuffer> buffer) override {
        if (!enabled_ || !buffer || mix_ == 0.f) return;

        const size_t n  = buffer->numSamples;
        const size_t ch = buffer->channels.size();
        if (ch == 0) return;

        for (size_t s = 0; s < n; ++s) {
            float inL = buffer->channels[0][s];
            float inR = (ch > 1) ? buffer->channels[1][s] : inL;

            float input = (inL + inR) * 0.015f; // scale down to prevent saturation

            float outL = 0.f, outR = 0.f;

            // ── 8 parallel comb filters ──────────────────────────────────
            for (auto& c : combL_) outL += processComb(c, input);
            for (auto& c : combR_) outR += processComb(c, input);

            // ── 4 series all-pass filters ────────────────────────────────
            for (auto& a : allpassL_) outL = processAllpass(a, outL);
            for (auto& a : allpassR_) outR = processAllpass(a, outR);

            float dry = 1.f - mix_;
            buffer->channels[0][s] = inL * dry + outL * mix_;
            if (ch > 1)
                buffer->channels[1][s] = inR * dry + outR * mix_;
        }
    }

private:
    // ── Freeverb tuning (samples at 44100 Hz, scaled for other rates) ─────
    static constexpr std::array<int,8> kCombTuning   = {1116,1188,1277,1356,1422,1491,1557,1617};
    static constexpr std::array<int,4> kAllpassTuning = {556, 441, 341, 225};
    static constexpr float kScaleRoom  = 0.28f;
    static constexpr float kOffsetRoom = 0.7f;
    static constexpr float kFixedGain  = 0.015f;

    struct CombFilter {
        std::vector<float> buf;
        size_t pos  = 0;
        float store = 0.f;
        float feedback = 0.5f;
        float damp1    = 0.5f;
        float damp2    = 0.5f;
    };

    struct AllpassFilter {
        std::vector<float> buf;
        size_t pos     = 0;
        float feedback = 0.5f;
    };

    std::array<CombFilter,8>    combL_, combR_;
    std::array<AllpassFilter,4> allpassL_, allpassR_;
    float roomSize_ = 0.5f;
    float damping_  = 0.5f;

    void initBuffers() {
        constexpr auto sr = static_cast<float>(kSampleRate);
        constexpr float tuningScale = sr / 44100.0f;

        // Use size_t for counters to match buffer indexing and avoid signed-to-unsigned promotion
        for (size_t i = 0uz; i < 8uz; ++i) {
            // static_cast the array access if kCombTuning isn't already float
            const float combSizeBase = static_cast<float>(kCombTuning[i]) * tuningScale;

            const auto szL = static_cast<size_t>(combSizeBase);
            const size_t szR = szL + 23uz;

            combL_[i].buf.assign(szL, 0.0f);
            combR_[i].buf.assign(szR, 0.0f);
        }

        for (size_t i = 0uz; i < 4uz; ++i) {
            const float allpassSizeBase = static_cast<float>(kAllpassTuning[i]) * tuningScale;

            const auto szL = static_cast<size_t>(allpassSizeBase);
            const size_t szR = szL + 23uz;

            allpassL_[i].buf.assign(szL, 0.0f);
            allpassR_[i].buf.assign(szR, 0.0f);

            allpassL_[i].feedback = 0.5f;
            allpassR_[i].feedback = 0.5f;
        }

        updateCoefficients();
    }

    void updateCoefficients() {
        float fb   = roomSize_ * kScaleRoom + kOffsetRoom;
        float d1   = damping_;
        float d2   = 1.f - d1;
        for (auto& c : combL_) { c.feedback = fb; c.damp1 = d1; c.damp2 = d2; }
        for (auto& c : combR_) { c.feedback = fb; c.damp1 = d1; c.damp2 = d2; }
    }

    static float processComb(CombFilter& c, float input) {
        float output  = c.buf[c.pos];
        c.store       = output * c.damp2 + c.store * c.damp1;
        c.buf[c.pos]  = input + c.store * c.feedback;
        if (++c.pos >= c.buf.size()) c.pos = 0;
        return output;
    }

    static float processAllpass(AllpassFilter& a, float input) {
        float bufout   = a.buf[a.pos];
        float output   = -input + bufout;
        a.buf[a.pos]   = input + bufout * a.feedback;
        if (++a.pos >= a.buf.size()) a.pos = 0;
        return output;
    }
};

} // namespace coreengine

