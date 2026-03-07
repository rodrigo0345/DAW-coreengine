//
// Created by rodrigo0345 on 3/7/26.
//

#ifndef DAWCOREENGINE_EFFECTCHAIN_H
#define DAWCOREENGINE_EFFECTCHAIN_H

#include <vector>
#include <memory>
#include <algorithm>
#include "AudioEffect.h"
#include "AudioBuffer.h"

namespace coreengine {
    /**
     * Manages a chain of audio effects.
     * Effects are processed in order, allowing for complex signal routing.
     */
    class EffectChain {
    public:
        /**
         * Add an effect to the end of the chain
         */
        void addEffect(std::unique_ptr<AudioEffect> effect) {
            if (effect) {
                effects_.push_back(std::move(effect));
            }
        }

        /**
         * Insert an effect at a specific position
         * @param index Position to insert (0 = first)
         * @param effect Effect to insert
         */
        void insertEffect(size_t index, std::unique_ptr<AudioEffect> effect) {
            if (effect && index <= effects_.size()) {
                effects_.insert(effects_.begin() + index, std::move(effect));
            }
        }

        /**
         * Remove an effect at a specific position
         */
        void removeEffect(size_t index) {
            if (index < effects_.size()) {
                effects_.erase(effects_.begin() + index);
            }
        }

        /**
         * Remove an effect by name
         */
        void removeEffectByName(const std::string& name) {
            effects_.erase(
                std::remove_if(effects_.begin(), effects_.end(),
                    [&name](const std::unique_ptr<AudioEffect>& effect) {
                        return effect->getName() == name;
                    }),
                effects_.end()
            );
        }

        /**
         * Get an effect by index
         */
        [[nodiscard]] AudioEffect* getEffect(size_t index) const {
            if (index < effects_.size()) {
                return effects_[index].get();
            }
            return nullptr;
        }

        /**
         * Get an effect by name
         */
        [[nodiscard]] AudioEffect* getEffectByName(const std::string& name) const {
            auto it = std::find_if(effects_.begin(), effects_.end(),
                [&name](const std::unique_ptr<AudioEffect>& effect) {
                    return effect->getName() == name;
                });

            return (it != effects_.end()) ? it->get() : nullptr;
        }

        /**
         * Get number of effects in chain
         */
        [[nodiscard]] size_t size() const {
            return effects_.size();
        }

        /**
         * Check if chain is empty
         */
        [[nodiscard]] bool isEmpty() const {
            return effects_.empty();
        }

        /**
         * Process buffer through all effects in chain
         */
        void process(std::shared_ptr<AudioBuffer> buffer) {
            if (!buffer) return;

            for (auto& effect : effects_) {
                if (effect && effect->isEnabled()) {
                    effect->process(buffer);
                }
            }
        }

        /**
         * Reset all effects in chain
         */
        void reset() {
            for (auto& effect : effects_) {
                if (effect) {
                    effect->reset();
                }
            }
        }

        /**
         * Clear all effects from chain
         */
        void clear() {
            effects_.clear();
        }

        /**
         * Enable/disable all effects
         */
        void setAllEnabled(bool enabled) {
            for (auto& effect : effects_) {
                if (effect) {
                    effect->setEnabled(enabled);
                }
            }
        }

    private:
        std::vector<std::unique_ptr<AudioEffect>> effects_;
    };
}

#endif //DAWCOREENGINE_EFFECTCHAIN_H

