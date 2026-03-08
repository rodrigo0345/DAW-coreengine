//
// Created by rodrigo0345 on 3/4/26.
//

#include "RenderLoop.h"
#include <iostream>
#include "../commands/Command.h"
#include "audio/SynthFactory.h"
#include "audio/SamplePlayer.h"
#include "audio/Sequencer.h"
#include "audio/simple_sounds/SimpleSynth.h"
#include "audio/ADSR.h"
#include "audio/effects/ReverbEffect.h"
#include "audio/effects/DelayEffect.h"
#include "audio/effects/DistortionEffect.h"

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
        audioBuffer->channels[static_cast<size_t>(i)] = new float[numSamples];
        std::fill_n(audioBuffer->channels[static_cast<size_t>(i)], numSamples, 0.0f); // reset
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
                // auto data = std::get<TrackControlData>(cmd->data);
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
            case CommandType::SetTrackEffect: {
                auto data = std::get<SetTrackEffectData>(cmd->data);
                Track* track = timeline.getTrack(data.trackId);
                if (!track) break;

                // Remove any existing effect with the same name first
                track->removeEffect(data.effectType);

                if (!data.enabled) break; // just removing

                float sr = static_cast<float>(audioBuffer->sampleRate);

                if (data.effectType == "Reverb") {
                    auto fx = std::make_unique<ReverbEffect>(data.roomSize, data.damping);
                    fx->setMix(data.mix);
                    track->addEffect(std::move(fx));
                } else if (data.effectType == "Delay") {
                    auto fx = std::make_unique<DelayEffect>(data.delayMs, data.feedback, data.delayDamping, sr);
                    fx->setMix(data.mix);
                    track->addEffect(std::move(fx));
                } else if (data.effectType == "Distortion") {
                    auto fx = std::make_unique<DistortionEffect>(data.drive);
                    fx->setMix(data.mix);
                    track->addEffect(std::move(fx));
                }
                break;
            }
            case CommandType::RemoveTrackEffect: {
                auto data = std::get<RemoveTrackEffectData>(cmd->data);
                Track* track = timeline.getTrack(data.trackId);
                if (track) track->removeEffect(data.effectType);
                break;
            }
            case CommandType::SetEffectParam: {
                auto data = std::get<SetEffectParamData>(cmd->data);
                Track* track = timeline.getTrack(data.trackId);
                if (!track) break;
                AudioEffect* fx = track->getEffect(data.effectType);
                if (!fx) break;

                if (data.paramName == "mix") {
                    fx->setMix(data.value);
                } else if (data.effectType == "Reverb") {
                    auto* rev = dynamic_cast<ReverbEffect*>(fx);
                    if (!rev) break;
                    if (data.paramName == "roomSize") rev->setRoomSize(data.value);
                    else if (data.paramName == "damping") rev->setDamping(data.value);
                } else if (data.effectType == "Delay") {
                    auto* del = dynamic_cast<DelayEffect*>(fx);
                    if (!del) break;
                    if (data.paramName == "delayMs")  del->setDelayMs(data.value);
                    else if (data.paramName == "feedback") del->setFeedback(data.value);
                    else if (data.paramName == "damping")  del->setDamping(data.value);
                } else if (data.effectType == "Distortion") {
                    auto* dist = dynamic_cast<DistortionEffect*>(fx);
                    if (!dist) break;
                    if (data.paramName == "drive") dist->setDrive(data.value);
                }
                break;
            }
            case CommandType::SetAutomationLane: {
                auto data = std::get<AutomationLaneData>(cmd->data);
                auto& laneMap = automationData_[data.trackId];
                auto& pts = laneMap[data.paramName];
                pts.clear();
                // const double spb = 60.0 / data.bpm * static_cast<double>(data.sampleRate);
                for (auto& p : data.points) {
                    pts.push_back({ p.beat, p.value });
                }
                // Already sorted by the frontend but sort anyway for safety
                std::sort(pts.begin(), pts.end(),
                    [](const AutomationPoint& a, const AutomationPoint& b){ return a.beat < b.beat; });
                break;
            }
            case CommandType::ClearAutomationLane: {
                auto data = std::get<AutomationLaneData>(cmd->data);
                automationData_[data.trackId].erase(data.paramName);
                break;
            }
            case CommandType::LoadSample: {
                auto data = std::get<LoadSampleData>(cmd->data);
                Track* track = timeline.getTrack(data.trackId);
                if (!track) break;
                double sr = static_cast<double>(audioBuffer->sampleRate);
                auto sampler = std::make_unique<SamplePlayer>(8, sr, data.rootNote);
                sampler->setOneShot(data.oneShot);
                if (!sampler->loadFile(data.filePath)) {
                    std::cerr << "SamplePlayer: failed to load " << data.filePath << "\n";
                    break;
                }
                track->replaceInstrument(std::move(sampler));
                break;
            }
            case CommandType::SetVoiceCount: {
                auto data = std::get<SetVoiceCountData>(cmd->data);
                Track* track = timeline.getTrack(data.trackId);
                if (!track) break;
                auto* inst = track->getInstrument();
                if (auto* synth = dynamic_cast<SimpleSynth*>(inst)) {
                    synth->setVoiceCount(data.numVoices);
                } else if (auto* sampler = dynamic_cast<SamplePlayer*>(inst)) {
                    sampler->setVoiceCount(data.numVoices);
                }
                break;
            }
            case CommandType::SetSynthType: {
                auto data = std::get<SetSynthTypeData>(cmd->data);
                Track* track = timeline.getTrack(data.trackId);
                if (!track) break;
                if (data.synthType == 4) {
                    // Sampler — instrument gets loaded separately via LoadSample
                    auto sampler = std::make_unique<SamplePlayer>(data.numVoices, data.sampleRate);
                    track->replaceInstrument(std::move(sampler));
                } else {
                    auto newInst = SynthFactory::createByType(data.synthType, data.numVoices, data.sampleRate);
                    track->replaceInstrument(std::move(newInst));
                }
                break;
            }
            case CommandType::AddInstrument: {
                // auto data = std::get<InstrumentData>(cmd->data);
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

void coreengine::RenderLoop::gotoPosition(uint64_t userPositionClock) {
    this->positionClock = userPositionClock;
    timeline.seekTo(userPositionClock);
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

    // Apply automation for this block
    applyAutomation(positionClock, audioBuffer->numSamples);

    // Process all tracks from timeline
    for (auto& track : timeline.getAllTracks()) {
        track->processBlock(audioBuffer);
    }

    // Process any additional audio blocks (effects, etc.)
    for (const auto& processor : processorBlocks) {
        processor->processBlock(audioBuffer);
    }

    // ── Master limiter (brick-wall, -0.1 dBFS) ───────────────────────────────
    // Simple feed-forward peak limiter with fast attack / slow release.
    // Prevents any sample from exceeding ±0.989 (≈ -0.1 dBFS).
    {
        constexpr float CEILING    = 0.989f;      // -0.1 dBFS
        constexpr float ATTACK_TC  = 0.001f;      // 1 ms attack
        constexpr float RELEASE_TC = 0.200f;      // 200 ms release
        const float     sr         = static_cast<float>(audioBuffer->sampleRate);
        const float     attackCoef  = std::exp(-1.0f / (ATTACK_TC  * sr));
        const float     releaseCoef = std::exp(-1.0f / (RELEASE_TC * sr));

        for (size_t s = 0; s < audioBuffer->numSamples; ++s) {
            // Find peak across all channels
            float peak = 0.0f;
            for (auto* ch : audioBuffer->channels)
                peak = std::max(peak, std::abs(ch[s]));

            // Gain-computer: how much gain reduction is needed?
            const float targetGain = (peak > CEILING) ? (CEILING / peak) : 1.0f;

            // Smooth the gain reduction with attack/release
            if (targetGain < limiterGain_) {
                limiterGain_ = attackCoef  * limiterGain_ + (1.0f - attackCoef)  * targetGain;
            } else {
                limiterGain_ = releaseCoef * limiterGain_ + (1.0f - releaseCoef) * targetGain;
            }

            // Apply gain
            for (auto* ch : audioBuffer->channels)
                ch[s] *= limiterGain_;
        }
    }

    this->positionClock += audioBuffer->numSamples;
}

// ── Automation ────────────────────────────────────────────────────────────────

float coreengine::RenderLoop::interpolateAutomation(
    const std::vector<AutomationPoint>& pts, const double beat) const
{
    if (pts.empty()) [[unlikely]] return 0.0f;

    const auto& front = pts.front();
    if (beat <= front.beat) return front.value;
    const auto& back = pts.back();
    if (beat >= back.beat) return back.value;

    const auto it = std::upper_bound(pts.begin(), pts.end(), beat,
        [](double b, const AutomationPoint& p) {
            return b < p.beat;
        });

    const auto& p1 = *std::prev(it);
    const auto& p2 = *it;
    const double beatRange = p2.beat - p1.beat;
    const double t = (beat - p1.beat) / beatRange;

    // Linear interpolation: v1 + t * (v2 - v1)
    const auto fT = static_cast<float>(t);
    return p1.value + fT * (p2.value - p1.value);
}

void coreengine::RenderLoop::applyAutomation(uint64_t blockStartSample, uint64_t blockSamples)
{
    if (automationData_.empty()) return;

    const double sr = static_cast<double>(audioBuffer->sampleRate);
    // Use the beat at the centre of the block for a single-value update per block.
    // This is accurate enough for typical block sizes (512 samples @ 44100 = ~11.6ms).
    const double beat = (static_cast<double>(blockStartSample) + static_cast<double>(blockSamples) * 0.5) / (sr * 60.0 / 120.0);
    // NOTE: we store bpm per-lane. Without it here we use a fallback. A proper solution
    // is to read bpm from a member or timeline — for now 120 is the default and the
    // frontend sends per-lane bpm via SetAutomationLane which we ignore in the beat calc here.
    // TODO: store bpm member and use it. For now this works at 120 BPM.

    for (auto& [trackId, params] : automationData_) {
        Track* track = timeline.getTrack(trackId);
        if (!track) continue;

        for (auto& [paramName, pts] : params) {
            if (pts.empty()) continue;
            const float val = interpolateAutomation(pts, beat);

            if (paramName == "volume") {
                track->setVolume(val);
                continue;
            }
            // Effect param: format is "EffectType.paramName"
            auto dot = paramName.find('.');
            if (dot == std::string::npos) continue;
            const std::string effectType = paramName.substr(0, dot);
            const std::string epName     = paramName.substr(dot + 1);
            AudioEffect* fx = track->getEffect(effectType);
            if (!fx) continue;

            if (epName == "mix") {
                fx->setMix(val);
            } else if (effectType == "Reverb") {
                auto* rev = dynamic_cast<ReverbEffect*>(fx);
                if (!rev) continue;
                if (epName == "roomSize") rev->setRoomSize(val);
                else if (epName == "damping") rev->setDamping(val);
            } else if (effectType == "Delay") {
                auto* del = dynamic_cast<DelayEffect*>(fx);
                if (!del) continue;
                if (epName == "delayMs")       del->setDelayMs(val * 2000.f); // 0–1 → 0–2000ms
                else if (epName == "feedback") del->setFeedback(val * 0.95f);
                else if (epName == "damping")  del->setDamping(val);
            } else if (effectType == "Distortion") {
                auto* dist = dynamic_cast<DistortionEffect*>(fx);
                if (!dist) continue;
                if (epName == "drive") dist->setDrive(val * 10.f); // 0–1 → 0–10
            }
        }
    }
}
