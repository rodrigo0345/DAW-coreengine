//
// Created by rodrigo0345 on 3/4/26.
//

#ifndef DAWCOREENGINE_RENDERLOOP_H
#define DAWCOREENGINE_RENDERLOOP_H
#include <array>
#include <map>
#include <cmath>
#include <chrono>
#include <string>
#include <vector>
#include <memory>
#include "../commands/CommandQueue.h"
#include "../configs/EngineConfig.h"
#include "audio/AudioBlock.h"
#include "audio/AudioBuffer.h"
#include "audio/Timeline.h"

namespace coreengine {

    class RenderLoop {
    public:
        explicit RenderLoop(const coreengine::EngineConfig& config);

        void processCommands();
        void play();
        void pause();
        void stop();
        void reset();
        void gotoPosition(uint64_t positionClock);
        void addProcessor(std::unique_ptr<coreengine::AudioBlock> block);

        // Return by reference — caller must not outlive RenderLoop
        [[nodiscard]] AudioBuffer& getBuffer() { return audioBuffer_; }
        [[nodiscard]] const AudioBuffer& getBuffer() const { return audioBuffer_; }
        void processNextBlock();

        CommandQueue& getCommandQueue() { return commandQueue; }
        Timeline& getTimeline() { return timeline; }
        uint64_t getCurrentPosition() const { return positionClock; }
        uint64_t getSampleRate() const { return audioBuffer_.sampleRate; }

    private:
        CommandQueue commandQueue;
        Timeline     timeline;
        const uint32_t numSamples = 512;

        // Owning audio buffer — allocated once at startup via initStorage()
        AudioBuffer audioBuffer_;
        uint64_t positionClock = 0;
        bool isPlaying = false;

        std::vector<std::unique_ptr<coreengine::AudioBlock>> processorBlocks;

        float limiterGain_ = 1.0f;

        // ── Per-block timing ──────────────────────────────────────────────────
        struct BlockTimingStats {
            using ns = long long;
            ns commands{0}, clearBuf{0}, timelineEvents{0},
               automation{0}, trackProcess{0}, limiter{0}, total{0};
            uint64_t count{0};
            void reset() { *this = {}; }
        };
        BlockTimingStats timing_;
        static constexpr uint64_t TIMING_REPORT_INTERVAL = 500;
        void printTimingReport() const;

        // ── Automation ────────────────────────────────────────────────────────
        struct AutomationPoint { double beat; float value; };
        std::map<int, std::map<std::string, std::vector<AutomationPoint>>> automationData_;
        void applyAutomation(uint64_t blockStartSample, uint64_t blockSamples);
        float interpolateAutomation(const std::vector<AutomationPoint>& pts, double beat) const;
    };

}

#endif //DAWCOREENGINE_RENDERLOOP_H

