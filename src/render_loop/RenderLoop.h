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
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <latch>
#include "../commands/CommandQueue.h"
#include "../configs/EngineConfig.h"
#include "audio/AudioBlock.h"
#include "audio/AudioBuffer.h"
#include "audio/Timeline.h"

namespace coreengine {
    class PluginManager;

    // ── Minimal lock-free work queue for the track thread pool ───────────────
    // Fixed capacity — no heap allocation after construction.
    class TrackThreadPool {
    public:
        explicit TrackThreadPool(size_t numThreads);
        ~TrackThreadPool();

        // Submit N independent tasks, block until all are done.
        // tasks must remain valid until dispatch() returns.
        void dispatch(std::function<void()>* tasks, size_t count);

    private:
        void workerLoop();

        struct alignas(64) WorkItem {
            std::function<void()>* fn   = nullptr;
            std::latch*            done = nullptr;
        };

        static constexpr size_t QUEUE_CAP = 64; // max concurrent tasks
        std::array<WorkItem, QUEUE_CAP> queue_{};
        std::atomic<size_t> head_{0}, tail_{0};

        std::vector<std::thread> workers_;
        std::mutex               mtx_;
        std::condition_variable  cv_;
        std::atomic<bool>        stop_{false};
    };

    class RenderLoop {
    public:
        explicit RenderLoop(const coreengine::EngineConfig& config);
        ~RenderLoop();

        void processCommands();
        void play();
        void pause();
        void stop();
        void reset();
        void gotoPosition(uint64_t positionClock);
        void addProcessor(std::unique_ptr<coreengine::AudioBlock> block);

        [[nodiscard]] AudioBuffer& getBuffer() { return audioBuffer_; }
        [[nodiscard]] const AudioBuffer& getBuffer() const { return audioBuffer_; }
        void processNextBlock();

        CommandQueue& getCommandQueue() { return commandQueue; }
        Timeline& getTimeline() { return timeline; }
        uint64_t getCurrentPosition() const { return positionClock; }
        uint64_t getSampleRate() const { return audioBuffer_.sampleRate; }

        void setPluginManager(PluginManager* pm) { pluginManager_ = pm; }

        [[nodiscard]] double getBpm() const { return bpm_; }

        /** Convert a beat position to a sample offset at the current BPM. */
        [[nodiscard]] uint64_t beatsToSamples(double beats) const {
            const double samplesPerBeat = 60.0 / bpm_ * static_cast<double>(audioBuffer_.sampleRate);
            return static_cast<uint64_t>(beats * samplesPerBeat);
        }
        /** Convert a sample offset to a beat position at the current BPM. */
        [[nodiscard]] double samplesToBeats(uint64_t samples) const {
            const double samplesPerBeat = 60.0 / bpm_ * static_cast<double>(audioBuffer_.sampleRate);
            return static_cast<double>(samples) / samplesPerBeat;
        }

    private:
        CommandQueue commandQueue;
        Timeline     timeline;
        const uint32_t numSamples = 512;

        AudioBuffer audioBuffer_;
        uint64_t positionClock = 0;
        bool isPlaying = false;
        double bpm_ = 120.0;   // master BPM — single source of truth

        PluginManager* pluginManager_ = nullptr;

        std::vector<std::unique_ptr<coreengine::AudioBlock>> processorBlocks;

        float limiterGain_ = 1.0f;

        // ── Thread pool ───────────────────────────────────────────────────────
        // Only used when there are ≥2 audible tracks (otherwise serial is faster).
        static constexpr size_t MIN_TRACKS_FOR_PARALLEL = 2;
        std::unique_ptr<TrackThreadPool> threadPool_;
        // Pre-allocated task slots — one per track, reused every block (no alloc).
        static constexpr size_t MAX_PARALLEL_TASKS = 64;
        std::array<std::function<void()>, MAX_PARALLEL_TASKS> taskSlots_;

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

