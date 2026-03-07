//
// Created by rodrigo0345 on 3/7/26.
//

#ifndef DAWCOREENGINE_AUDIOEFFECT_H
#define DAWCOREENGINE_AUDIOEFFECT_H

#include <memory>
#include <string>
#include "AudioBuffer.h"

namespace coreengine {
    /**
     * Abstract base class for audio effects.
     * All effects (reverb, delay, distortion, etc.) should inherit from this.
     */
    class AudioEffect {
    public:
        virtual ~AudioEffect() = default;

        /**
         * Process audio buffer through the effect
         * @param buffer The audio buffer to process (in-place processing)
         */
        virtual void process(std::shared_ptr<AudioBuffer> buffer) = 0;

        /**
         * Reset the effect state (clear delays, reverb tails, etc.)
         */
        virtual void reset() = 0;

        /**
         * Enable or disable the effect
         */
        virtual void setEnabled(bool enabled) {
            enabled_ = enabled;
        }

        /**
         * Check if effect is enabled
         */
        [[nodiscard]] virtual bool isEnabled() const {
            return enabled_;
        }

        /**
         * Get effect name for identification
         */
        [[nodiscard]] virtual std::string getName() const = 0;

        /**
         * Set mix level (dry/wet)
         * @param mix 0.0 = fully dry, 1.0 = fully wet
         */
        virtual void setMix(float mix) {
            mix_ = std::clamp(mix, 0.0f, 1.0f);
        }

        /**
         * Get current mix level
         */
        [[nodiscard]] virtual float getMix() const {
            return mix_;
        }

    protected:
        bool enabled_ = true;
        float mix_ = 1.0f; // Default to fully wet
    };
}

#endif //DAWCOREENGINE_AUDIOEFFECT_H

