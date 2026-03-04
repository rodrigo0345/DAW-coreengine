//
// Created by rodrigo0345 on 3/4/26.
//

#include "CoreServiceEngine.h"
#include <pulse/error.h>
#include <iostream>

coreengine::CoreServiceEngine::CoreServiceEngine(const EngineConfig& config)
    : renderLoop(config), config(config) {

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
    const size_t numSamples = config.sampleBlockSize;
    const auto buffer = renderLoop.getBuffer();

    while (isRunning) {
        renderLoop.processNextBlock();

        for (size_t i = 0; i < numSamples; ++i) {
            for (size_t ch = 0; ch < buffer->channels.size(); ++ch) {
                interleavingBuffer[i * buffer->channels.size() + ch] = buffer->channels[ch][i];
            }
        }

        if (pa_simple_write(pulseHandle, interleavingBuffer.data(),
                           interleavingBuffer.size() * sizeof(float), &error) < 0) {
            isRunning = false;
                           }
    }
}