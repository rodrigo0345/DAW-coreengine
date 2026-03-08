//
// Created by rodrigo0345 on 3/7/26.
//

#ifndef DAWCOREENGINE_ADSR_H
#define DAWCOREENGINE_ADSR_H

#include <algorithm>
#include <cmath>

namespace coreengine {
    /**
     * ADSR Envelope Generator
     * Implements Attack, Decay, Sustain, Release envelope shaping
     * to eliminate clicks and provide expressive sound shaping.
     */
    class ADSR {
    public:
        enum class Stage {
            Idle,
            Attack,
            Decay,
            Sustain,
            Release
        };

        /**
         * ADSR Parameters structure for easy configuration
         */
        struct Parameters {
            float attackTime;   // Attack time in seconds
            float decayTime;    // Decay time in seconds
            float sustainLevel; // Sustain level (0.0 to 1.0)
            float releaseTime;  // Release time in seconds

            // Default parameters that prevent clicks
            Parameters()
                : attackTime(0.005f)   // 5ms attack – eliminates onset click
                , decayTime(0.05f)     // 50ms decay
                , sustainLevel(0.7f)   // 70% sustain level
                , releaseTime(0.05f)   // 50ms release – eliminates offset click
            {}

            Parameters(float attack, float decay, float sustain, float release)
                : attackTime(attack)
                , decayTime(decay)
                , sustainLevel(std::clamp(sustain, 0.0f, 1.0f))
                , releaseTime(release)
            {}
        };

        explicit ADSR(double sampleRate, const Parameters& params = Parameters())
            : sampleRate_(sampleRate)
            , parameters_(params)
            , currentStage_(Stage::Idle)
            , currentLevel_(0.0f)
            , currentSample_(0)
            , releaseStartLevel_(0.0f)
            , attackSamples_(0)
            , decaySamples_(0)
            , releaseSamples_(0)
        {
            calculateStageSamples();
        }

        /**
         * Trigger the envelope (note on)
         */
        void trigger() {
            currentStage_ = Stage::Attack;
            currentSample_ = 0;
            // Don't reset currentLevel to avoid clicks on retriggering
        }

        /**
         * Release the envelope (note off)
         */
        void release() {
            if (currentStage_ != Stage::Idle && currentStage_ != Stage::Release) {
                currentStage_ = Stage::Release;
                currentSample_ = 0;
                releaseStartLevel_ = currentLevel_; // Start release from current level
            }
        }

        /**
         * Process and return the next envelope value
         * @return Envelope value between 0.0 and 1.0
         */
        float process() {
            switch (currentStage_) {
                case Stage::Idle:
                    currentLevel_ = 0.0f;
                    break;

                case Stage::Attack:
                    if (attackSamples_ > 0) {
                        // Linear attack (can be changed to exponential)
                        currentLevel_ = static_cast<float>(currentSample_) / static_cast<float>(attackSamples_);
                        currentSample_++;
                        if (currentSample_ >= attackSamples_) {
                            currentStage_ = Stage::Decay;
                            currentSample_ = 0;
                            currentLevel_ = 1.0f;
                        }
                    } else {
                        // Instant attack
                        currentLevel_ = 1.0f;
                        currentStage_ = Stage::Decay;
                        currentSample_ = 0;
                    }
                    break;

                case Stage::Decay:
                    if (decaySamples_ > 0) {
                        // Exponential decay for more natural sound
                        const float t = static_cast<float>(currentSample_) / static_cast<float>(decaySamples_);
                        currentLevel_ = parameters_.sustainLevel +
                                      (1.0f - parameters_.sustainLevel) * (1.0f - t);
                        currentSample_++;
                        if (currentSample_ >= decaySamples_) {
                            currentStage_ = Stage::Sustain;
                            currentLevel_ = parameters_.sustainLevel;
                        }
                    } else {
                        // Instant decay
                        currentLevel_ = parameters_.sustainLevel;
                        currentStage_ = Stage::Sustain;
                    }
                    break;

                case Stage::Sustain:
                    currentLevel_ = parameters_.sustainLevel;
                    break;

                case Stage::Release:
                    if (releaseSamples_ > 0) {
                        // Exponential release for natural fade
                        const float t = static_cast<float>(currentSample_) / static_cast<float>(releaseSamples_);
                        currentLevel_ = releaseStartLevel_ * (1.0f - t);
                        currentSample_++;
                        if (currentSample_ >= releaseSamples_) {
                            currentStage_ = Stage::Idle;
                            currentLevel_ = 0.0f;
                        }
                    } else {
                        // Instant release
                        currentStage_ = Stage::Idle;
                        currentLevel_ = 0.0f;
                    }
                    break;
            }

            return currentLevel_;
        }

        /**
         * Check if envelope is active (not idle)
         */
        [[nodiscard]] bool isActive() const {
            return currentStage_ != Stage::Idle;
        }

        /**
         * Check if envelope is in release stage
         */
        [[nodiscard]] bool isReleasing() const {
            return currentStage_ == Stage::Release;
        }

        /**
         * Get current envelope stage
         */
        [[nodiscard]] Stage getCurrentStage() const {
            return currentStage_;
        }

        /**
         * Get current envelope level
         */
        [[nodiscard]] float getCurrentLevel() const {
            return currentLevel_;
        }

        /**
         * Update ADSR parameters
         */
        void setParameters(const Parameters& params) {
            parameters_ = params;
            parameters_.sustainLevel = std::clamp(parameters_.sustainLevel, 0.0f, 1.0f);
            calculateStageSamples();
        }

        /**
         * Get current parameters
         */
        [[nodiscard]] const Parameters& getParameters() const {
            return parameters_;
        }

        /**
         * Reset the envelope to idle state
         */
        void reset() {
            currentStage_ = Stage::Idle;
            currentLevel_ = 0.0f;
            currentSample_ = 0;
            releaseStartLevel_ = 0.0f;
        }

        /**
         * Update sample rate (recalculates stage durations)
         */
        void setSampleRate(double sampleRate) {
            sampleRate_ = sampleRate;
            calculateStageSamples();
        }

    private:
        void calculateStageSamples() {
            const auto sr = static_cast<float>(sampleRate_);
            attackSamples_  = static_cast<int>(parameters_.attackTime  * sr);
            decaySamples_   = static_cast<int>(parameters_.decayTime   * sr);
            releaseSamples_ = static_cast<int>(parameters_.releaseTime * sr);
        }

        double sampleRate_;
        Parameters parameters_;

        Stage currentStage_;
        float currentLevel_;
        int currentSample_;
        float releaseStartLevel_;

        // Cached sample counts for each stage
        int attackSamples_;
        int decaySamples_;
        int releaseSamples_;
    };
}

#endif //DAWCOREENGINE_ADSR_H





