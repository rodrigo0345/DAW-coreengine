//
// Created by rodrigo0345 on 3/4/26.
//

#include "RenderLoop.h"

#include "../commands/Command.h"

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

void coreengine::RenderLoop::processCommands() {
    while (const auto cmd = commandQueue.pop()) {
        switch (cmd->type) {
            case CommandType::NoteOn: {
                const auto data = std::get<NoteData>(cmd->data);
                if (data.synth) {
                    data.synth->noteOn(data.midiNote, data.velocity);
                }
                break;
            }
            case CommandType::NoteOff: {
                const auto data = std::get<NoteData>(cmd->data);
                if (data.synth) {
                    data.synth->noteOff(data.midiNote);
                }
                break;
            }
            case CommandType::AllNotesOff: {
                // Turn off all notes on all instruments
                for (const auto& processor : processorBlocks) {
                    if (auto* instrument = dynamic_cast<Instrument*>(processor.get())) {
                        instrument->allNotesOff();
                    }
                }
                break;
            }
            case CommandType::AddInstrument: {
                auto data = std::get<InstrumentData>(cmd->data);
                // Logic to instantiate a new AudioBlock and add to DAG
                // This would require a plugin registry/factory system
                break;
            }
            case CommandType::Play: {
                this->play();
                break;
            }
            case CommandType::Stop: {
                this->stop();
                break;
            }
            case CommandType::SetTimestamp: {
                auto data = std::get<TimestampData>(cmd->data);
                this->positionClock = data.samples;
                break;
            }
            default: ;
        }
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
    // Process any pending commands first
    processCommands();

    // Clear the buffer
    for (const auto& channelPtr : audioBuffer->channels) {
        std::fill_n(channelPtr, audioBuffer->numSamples, 0.0f);
    }

    if (!isPlaying) return;

    // Process all audio blocks
    for (const auto& processor : processorBlocks) {
        processor->processBlock(audioBuffer);
    }
    this->positionClock += audioBuffer->numSamples;
}

