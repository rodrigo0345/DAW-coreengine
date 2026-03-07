//
// Created by rodrigo0345 on 3/4/26.
//

#ifndef DAWCOREENGINE_RENDERLOOP_H
#define DAWCOREENGINE_RENDERLOOP_H
#include <memory>
#include <vector>

#include "../commands/CommandQueue.h"
#include "../configs/EngineConfig.h"
#include "audio/AudioBlock.h"
#include "audio/AudioBuffer.h"
#include "audio/Timeline.h"

namespace coreengine {

    class RenderLoop {
    public:
        RenderLoop(const coreengine::EngineConfig& config);

        void processCommands();
        void play();
        void pause();
        void stop();
        void reset();
        void gotoPosition(uint64_t positionClock);
        void addProcessor(std::unique_ptr<coreengine::AudioBlock> block);

        [[nodiscard]]
        std::shared_ptr<coreengine::AudioBuffer> getBuffer() const { return audioBuffer; }
        void processNextBlock();

        // Access to command queue for pushing commands from UI/main thread
        CommandQueue& getCommandQueue() { return commandQueue; }

        // Access to timeline for scheduling events
        Timeline& getTimeline() { return timeline; }

        // Get current playback position in samples
        uint64_t getCurrentPosition() const { return positionClock; }

        // Get sample rate for time conversions
        uint64_t getSampleRate() const { return audioBuffer->sampleRate; }

    private:
        CommandQueue commandQueue;
        Timeline timeline;
        const uint32_t numSamples = 512; // TODO: needs to stop be static
        std::shared_ptr<coreengine::AudioBuffer> audioBuffer;
        uint64_t positionClock; // Global timeline position
        bool isPlaying;

        std::vector<std::unique_ptr<coreengine::AudioBlock>> processorBlocks;
    };

}


#endif //DAWCOREENGINE_RENDERLOOP_H