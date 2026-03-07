//
// Created by rodrigo0345 on 3/4/26.
//

#include "RenderLoop.h"

#include "../commands/Command.h"
#include "audio/SynthFactory.h"
#include "audio/Sequencer.h"
#include "audio/simple_sounds/SimpleSynth.h"
#include "audio/ADSR.h"

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
            // Real-time playback commands
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
                for (const auto& processor : processorBlocks) {
                    if (auto* instrument = dynamic_cast<Instrument*>(processor.get())) {
                        instrument->allNotesOff();
                    }
                }
                for (auto& track : timeline.getAllTracks()) {
                    if (track->getInstrument()) {
                        track->getInstrument()->allNotesOff();
                    }
                }
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
            case CommandType::Pause: {
                this->pause();
                break;
            }
            case CommandType::Reset: {
                this->reset();
                break;
            }
            case CommandType::Seek: {
                auto data = std::get<SeekData>(cmd->data);
                this->gotoPosition(data.samplePosition);
                break;
            }

            // Timeline editing commands
            case CommandType::AddTrack: {
                auto data = std::get<AddTrackData>(cmd->data);
                double sr = static_cast<double>(audioBuffer->sampleRate);
                auto instrument = SynthFactory::createByType(data.synthType, data.numVoices, sr);
                timeline.addTrackWithId(data.trackId, data.trackName, std::move(instrument));
                break;
            }
            case CommandType::RemoveTrack: {
                auto data = std::get<TrackControlData>(cmd->data);
                // TODO: Implement track removal in Timeline class
                break;
            }
            case CommandType::AddNote: {
                if (std::holds_alternative<NoteEventData>(cmd->data)) {
                    auto data = std::get<NoteEventData>(cmd->data);
                    timeline.addNote(data.trackId, data.startSample, data.durationSamples,
                                   data.midiNote, data.velocity);
                } else if (std::holds_alternative<NoteEventMusicalData>(cmd->data)) {
                    auto data = std::get<NoteEventMusicalData>(cmd->data);
                    timeline.addNoteMusical(data.trackId, data.startBeat, data.durationBeats,
                                          data.midiNote, data.velocity, data.bpm, data.sampleRate);
                }
                break;
            }
            case CommandType::AddChord: {
                auto data = std::get<ChordData>(cmd->data);
                Sequencer::addChord(timeline, data.trackId, data.notes, data.startBeat,
                                  data.durationBeats, data.velocity, data.bpm, data.sampleRate);
                break;
            }
            case CommandType::AddMelody: {
                auto data = std::get<MelodyData>(cmd->data);
                std::vector<Note> notes;
                for (size_t i = 0; i < data.midiNotes.size(); ++i) {
                    notes.emplace_back(data.midiNotes[i], data.startBeats[i],
                                     data.durationBeats[i], data.velocities[i]);
                }
                Sequencer::addMelody(timeline, data.trackId, notes, data.bpm, data.sampleRate);
                break;
            }
            case CommandType::AddArpeggio: {
                auto data = std::get<ArpeggioData>(cmd->data);
                Sequencer::addArpeggio(timeline, data.trackId, data.notes, data.startBeat,
                                     data.noteLength, data.repetitions, data.velocity,
                                     data.bpm, data.sampleRate);
                break;
            }
            case CommandType::ClearTrack: {
                auto data = std::get<TrackControlData>(cmd->data);
                Track* track = timeline.getTrack(data.trackId);
                if (track) {
                    track->clearEvents();
                }
                break;
            }
            case CommandType::SetTrackVolume: {
                auto data = std::get<TrackControlData>(cmd->data);
                Track* track = timeline.getTrack(data.trackId);
                if (track) {
                    track->setVolume(data.value);
                }
                break;
            }
            case CommandType::SetTrackMute: {
                auto data = std::get<TrackControlData>(cmd->data);
                Track* track = timeline.getTrack(data.trackId);
                if (track) {
                    track->setMuted(data.value > 0.5f);
                }
                break;
            }
            case CommandType::SetTrackSolo: {
                auto data = std::get<TrackControlData>(cmd->data);
                Track* track = timeline.getTrack(data.trackId);
                if (track) {
                    track->setSolo(data.value > 0.5f);
                }
                break;
            }
            case CommandType::SetADSR: {
                auto data = std::get<ADSRData>(cmd->data);
                Track* track = timeline.getTrack(data.trackId);
                if (track) {
                    auto* inst = track->getInstrument();
                    if (auto* synth = dynamic_cast<SimpleSynth*>(inst)) {
                        ADSR::Parameters params(data.attack, data.decay, data.sustain, data.release);
                        synth->setADSRParameters(params);
                    }
                }
                break;
            }
            case CommandType::RebuildTimeline: {
                timeline.rebuildEventQueue();
                break;
            }

            // Legacy commands
            case CommandType::AddInstrument: {
                auto data = std::get<InstrumentData>(cmd->data);
                // Logic to instantiate a new AudioBlock and add to DAG
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
    timeline.reset();
    for (const auto& channelPtr : audioBuffer->channels) {
        std::fill_n(channelPtr, audioBuffer->numSamples, 0.0f);
    }
}

void coreengine::RenderLoop::gotoPosition(uint64_t positionClock) {
    this->positionClock = positionClock;
    timeline.seekTo(positionClock);
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

    // Process timeline events for this block
    timeline.processEventsForBlock(positionClock, audioBuffer->numSamples);

    // Process all tracks from timeline
    for (auto& track : timeline.getAllTracks()) {
        track->processBlock(audioBuffer);
    }

    // Process any additional audio blocks (effects, etc.)
    for (const auto& processor : processorBlocks) {
        processor->processBlock(audioBuffer);
    }

    this->positionClock += audioBuffer->numSamples;
}

