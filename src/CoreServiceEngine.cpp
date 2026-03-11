//
// Created by rodrigo0345 on 3/4/26.
//

#include "CoreServiceEngine.h"
#include "render_loop/audio/AudioBuffer.h"
#include <pulse/error.h>
#include <iostream>

coreengine::CoreServiceEngine::CoreServiceEngine(const EngineConfig& engineConfig)
    : renderLoop(engineConfig), config(engineConfig) {

    // Wire the plugin manager so it runs after each audio block
    renderLoop.setPluginManager(&pluginManager);

    pa_sample_spec ss;
    ss.format = PA_SAMPLE_FLOAT32LE;
    ss.rate = config.getSampleRateVal();
    ss.channels = config.getChannelsVal();

    int error;
    pulseHandle = pa_simple_new(nullptr, "DAW_Core", PA_STREAM_PLAYBACK, nullptr, "Audio", &ss, nullptr, nullptr, &error);

    if (!pulseHandle) {
        throw std::runtime_error(pa_strerror(error));
    }

    // Pre-allocate buffer for interleaving
    interleavingBuffer.resize(config.sampleBlockSize * config.getChannelsVal());
}

coreengine::CoreServiceEngine::~CoreServiceEngine() {
    stop();
    if (pulseHandle) pa_simple_free(pulseHandle);
}

void coreengine::CoreServiceEngine::start() {
    if (isRunning) return;
    isRunning = true;
    audioThread = std::thread(&CoreServiceEngine::audioThreadLoop, this);
}

void coreengine::CoreServiceEngine::stop() {
    isRunning = false;
    if (audioThread.joinable()) audioThread.join();
}

void coreengine::CoreServiceEngine::audioThreadLoop() {
    int error;
    const size_t numSamples  = config.sampleBlockSize;
    AudioBuffer& buffer      = renderLoop.getBuffer();   // reference — no copy, no heap

    while (isRunning) {
        renderLoop.processNextBlock();

        const size_t nch = buffer.channels.size();
        for (size_t i = 0; i < numSamples; ++i) {
            for (size_t ch = 0; ch < nch; ++ch) {
                interleavingBuffer[i * nch + ch] = buffer.channels[ch][i];
            }
        }

        if (pa_simple_write(pulseHandle, interleavingBuffer.data(),
                           interleavingBuffer.size() * sizeof(float), &error) < 0) {
            isRunning = false;
        }
    }
}