//
// Created by rodrigo0345 on 3/4/26.
//

#ifndef DAWCOREENGINE_CORESERVICEENGINE_H
#define DAWCOREENGINE_CORESERVICEENGINE_H

#include <thread>
#include <atomic>
#include <pulse/simple.h>
#include "render_loop/RenderLoop.h"
#include "plugins/PluginManager.h"

namespace coreengine {

    class CoreServiceEngine {
    public:
        CoreServiceEngine(const EngineConfig& config);
        ~CoreServiceEngine();

        void start();
        void stop();

        // Interface to the underlying RenderLoop for UI control
        coreengine::RenderLoop& getRenderLoop() { return renderLoop; }
        coreengine::PluginManager& getPluginManager() { return pluginManager; }

    private:
        void audioThreadLoop();

        coreengine::PluginManager pluginManager;
        coreengine::RenderLoop renderLoop;
        const EngineConfig& config;

        std::atomic<bool> isRunning{false};
        std::thread audioThread;

        pa_simple* pulseHandle = nullptr;
        std::vector<float> interleavingBuffer;
    };
}

#endif //DAWCOREENGINE_CORESERVICEENGINE_H