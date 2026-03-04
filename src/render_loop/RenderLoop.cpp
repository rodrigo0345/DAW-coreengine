//
// Created by rodrigo0345 on 3/4/26.
//

#include "RenderLoop.h"

coreengine::RenderLoop::RenderLoop(const coreengine::EngineConfig& config)
        : positionClock(0), isPlaying(false) {

    this->audioBuffer = std::make_shared<coreengine::AudioBuffer>(
        coreengine::AudioBuffer{
            .channels = {},
            .sampleRate = config.getSampleRateVal(),
            .numSamples = numSamples,
        }
    );

    const auto numChannels = config.getChannelsVal();
    audioBuffer->channels.resize(numChannels);
    for (int i = 0; i < numChannels; ++i) {
        audioBuffer->channels[i] = new float[numSamples];
        std::fill_n(audioBuffer->channels[i], numSamples, 0.0f); // reset
    }
}

void coreengine::RenderLoop::play() {
    isPlaying = true;
}

void coreengine::RenderLoop::pause() {
    isPlaying = false;
}

void coreengine::RenderLoop::stop() {
    isPlaying = false;
    this->reset();
}

void coreengine::RenderLoop::reset() {
    this->positionClock = 0;
        for (const auto& channelPtr : audioBuffer->channels) {
            std::fill_n(channelPtr, audioBuffer->numSamples, 0.0f);
        }
}

void coreengine::RenderLoop::gotoPosition(uint64_t positionClock) {
    this->positionClock = positionClock;
}

void coreengine::RenderLoop::addProcessor(std::unique_ptr<AudioBlock> block) {
    processorBlocks.push_back(std::move(block));
}

void coreengine::RenderLoop::processNextBlock() {
    for (const auto& channelPtr : audioBuffer->channels) {
        std::fill_n(channelPtr, audioBuffer->numSamples, 0.0f);
    }

    if (!isPlaying) return;
    for (const auto& processor : processorBlocks) {
        processor->processBlock(audioBuffer);
    }
    this->positionClock += audioBuffer->numSamples;
}

