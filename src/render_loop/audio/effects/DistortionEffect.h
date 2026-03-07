//
// Created by rodrigo0345 on 3/7/26.
//

#ifndef DAWCOREENGINE_DISTORTIONEFFECT_H
#define DAWCOREENGINE_DISTORTIONEFFECT_H

#include <cmath>
#include <algorithm>
#include "../AudioEffect.h"

namespace coreengine {
    /**
     * Simple distortion effect using soft clipping/tanh waveshaping
     */
    class DistortionEffect : public AudioEffect {
    public:
        explicit DistortionEffect(float drive = 1.0f)
            : drive_(std::clamp(drive, 0.1f, 10.0f))
        {}

        void process(std::shared_ptr<AudioBuffer> buffer) override {
            if (!enabled_ || !buffer) return;

            for (size_t s = 0; s < buffer->numSamples; ++s) {
                for (const auto& channel : buffer->channels) {
                    // Soft clipping using tanh
                    const float input = channel[s];
                    const float distorted = std::tanh(input * drive_) / std::tanh(drive_);

                    // Mix dry/wet
                    channel[s] = input * (1.0f - mix_) + distorted * mix_;
                }
            }
        }

        void reset() override {
            // No state to reset for this effect
        }

        [[nodiscard]] std::string getName() const override {
            return "Distortion";
        }

        /**
         * Set drive amount (gain before clipping)
         * @param drive Amount of drive (0.1 to 10.0)
         */
        void setDrive(float drive) {
            drive_ = std::clamp(drive, 0.1f, 10.0f);
        }

        [[nodiscard]] float getDrive() const {
            return drive_;
        }

    private:
        float drive_;
    };
}

#endif //DAWCOREENGINE_DISTORTIONEFFECT_H

