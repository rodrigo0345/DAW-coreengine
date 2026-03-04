//
// Created by rodrigo0345 on 3/4/26.
//

#ifndef DAWCOREENGINE_RENDERLOOP_H
#define DAWCOREENGINE_RENDERLOOP_H
#include <memory>
#include <vector>

#include "../configs/EngineConfig.h"
#include "audio/AudioBlock.h"
#include "audio/AudioBuffer.h"

namespace coreengine {

    class RenderLoop {
    public:
        RenderLoop(const coreengine::EngineConfig& config);
        void play();
        void pause();
        void stop();
        void reset();
        void gotoPosition(uint64_t positionClock);
        void addProcessor(std::unique_ptr<coreengine::AudioBlock> block);

        [[nodiscard]]
        std::shared_ptr<coreengine::AudioBuffer> getBuffer() const { return audioBuffer; }
        void processNextBlock();

    private:
        const uint32_t numSamples = 512; // TODO: needs to stop be static
        std::shared_ptr<coreengine::AudioBuffer> audioBuffer;
        uint64_t positionClock; // Global timeline position
        bool isPlaying;

        std::vector<std::unique_ptr<coreengine::AudioBlock>> processorBlocks;
    };

}


#endif //DAWCOREENGINE_RENDERLOOP_H