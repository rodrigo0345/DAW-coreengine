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
    // Allocate backing store once — no per-block heap allocation
    const auto numChannels = static_cast<size_t>(config.getChannelsVal());
    audioBuffer_.initStorage(numChannels, numSamples, config.getSampleRateVal());
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
                auto sr = static_cast<double>(audioBuffer_.sampleRate);
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
                track->removeEffect(data.effectType);
                if (!data.enabled) break;
                float sr = static_cast<float>(audioBuffer_.sampleRate);
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
                double sr = static_cast<double>(audioBuffer_.sampleRate);
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
    positionClock = 0;
    timeline.reset();
    for (const auto& channelPtr : audioBuffer_.channels)
        std::fill_n(channelPtr, audioBuffer_.numSamples, 0.0f);
}

void coreengine::RenderLoop::gotoPosition(uint64_t userPositionClock) {
    positionClock = userPositionClock;
    timeline.seekTo(userPositionClock);
}

void coreengine::RenderLoop::addProcessor(std::unique_ptr<AudioBlock> block) {
    processorBlocks.push_back(std::move(block));
}

void coreengine::RenderLoop::processNextBlock() {
    using clock = std::chrono::steady_clock;
    using ns    = long long;
    auto t0 = clock::now();

    processCommands();
    auto t1 = clock::now();

    for (const auto& channelPtr : audioBuffer_.channels)
        std::fill_n(channelPtr, audioBuffer_.numSamples, 0.0f);
    auto t2 = clock::now();

    if (!isPlaying) {
        timing_.total += std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t0).count();
        timing_.count++;
        return;
    }

    timeline.processEventsForBlock(positionClock, audioBuffer_.numSamples);
    auto t3 = clock::now();

    applyAutomation(positionClock, audioBuffer_.numSamples);
    auto t4 = clock::now();

    for (auto& track : timeline.getAllTracks())
        track->processBlock(audioBuffer_);
    for (const auto& processor : processorBlocks)
        processor->processBlock(audioBuffer_);
    auto t5 = clock::now();

    // ── Master limiter ────────────────────────────────────────────────────
    {
        constexpr float CEILING    = 0.989f;
        constexpr float ATTACK_TC  = 0.001f;
        constexpr float RELEASE_TC = 0.200f;
        const float sr         = static_cast<float>(audioBuffer_.sampleRate);
        const float attackCoef  = std::exp(-1.0f / (ATTACK_TC  * sr));
        const float releaseCoef = std::exp(-1.0f / (RELEASE_TC * sr));

        for (size_t s = 0; s < audioBuffer_.numSamples; ++s) {
            float peak = 0.0f;
            for (auto* ch : audioBuffer_.channels)
                peak = std::max(peak, std::abs(ch[s]));
            const float targetGain = (peak > CEILING) ? (CEILING / peak) : 1.0f;
            if (targetGain < limiterGain_)
                limiterGain_ = attackCoef  * limiterGain_ + (1.0f - attackCoef)  * targetGain;
            else
                limiterGain_ = releaseCoef * limiterGain_ + (1.0f - releaseCoef) * targetGain;
            for (auto* ch : audioBuffer_.channels)
                ch[s] *= limiterGain_;
        }
    }
    auto t6 = clock::now();

    auto dur = [](auto a, auto b) -> ns {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(b - a).count();
    };
    timing_.commands       += dur(t0, t1);
    timing_.clearBuf       += dur(t1, t2);
    timing_.timelineEvents += dur(t2, t3);
    timing_.automation     += dur(t3, t4);
    timing_.trackProcess   += dur(t4, t5);
    timing_.limiter        += dur(t5, t6);
    timing_.total          += dur(t0, t6);
    timing_.count++;

    if (timing_.count >= TIMING_REPORT_INTERVAL) {
        printTimingReport();
        timing_.reset();
    }

    positionClock += audioBuffer_.numSamples;
}

void coreengine::RenderLoop::printTimingReport() const {
    const double n  = static_cast<double>(timing_.count);
    const double us = 1000.0;
    const double totalUs = static_cast<double>(timing_.total) / us / n;
    auto pct = [&](long long v) -> double {
        return timing_.total > 0
            ? 100.0 * static_cast<double>(v) / static_cast<double>(timing_.total) : 0.0;
    };
    const double budgetUs = (static_cast<double>(audioBuffer_.numSamples) /
                             static_cast<double>(audioBuffer_.sampleRate)) * 1e6;
    const double cpuLoad  = totalUs / budgetUs * 100.0;
    std::fprintf(stderr,
        "\n╔══════════════════════════════════════════════════════════════╗\n"
        "║  RenderLoop Block Timing  (avg over %4llu blocks)            ║\n"
        "╠══════════════════════════╦══════════╦══════════╦═════════════╣\n"
        "║  Section                 ║  avg µs  ║   %% tot  ║  CPU load   ║\n"
        "╠══════════════════════════╬══════════╬══════════╬═════════════╣\n"
        "║  1. processCommands      ║ %8.2f ║ %7.1f%% ║             ║\n"
        "║  2. clearBuffer          ║ %8.2f ║ %7.1f%% ║             ║\n"
        "║  3. timelineEvents       ║ %8.2f ║ %7.1f%% ║             ║\n"
        "║  4. automation           ║ %8.2f ║ %7.1f%% ║             ║\n"
        "║  5. trackProcess+effects ║ %8.2f ║ %7.1f%% ║             ║\n"
        "║  6. masterLimiter        ║ %8.2f ║ %7.1f%% ║             ║\n"
        "╠══════════════════════════╬══════════╬══════════╬═════════════╣\n"
        "║  TOTAL per block         ║ %8.2f ║  100.0%%  ║   %6.2f%%   ║\n"
        "║  Block budget @ %6u Hz ║ %8.2f ║          ║             ║\n"
        "╚══════════════════════════╩══════════╩══════════╩═════════════╝\n",
        static_cast<unsigned long long>(timing_.count),
        static_cast<double>(timing_.commands)       / us / n,  pct(timing_.commands),
        static_cast<double>(timing_.clearBuf)        / us / n,  pct(timing_.clearBuf),
        static_cast<double>(timing_.timelineEvents)  / us / n,  pct(timing_.timelineEvents),
        static_cast<double>(timing_.automation)      / us / n,  pct(timing_.automation),
        static_cast<double>(timing_.trackProcess)    / us / n,  pct(timing_.trackProcess),
        static_cast<double>(timing_.limiter)         / us / n,  pct(timing_.limiter),
        totalUs, cpuLoad,
        static_cast<unsigned>(audioBuffer_.sampleRate), budgetUs
    );
    std::fflush(stderr);
}

float coreengine::RenderLoop::interpolateAutomation(
    const std::vector<AutomationPoint>& pts, const double beat) const
{
    if (pts.empty()) [[unlikely]] return 0.0f;
    if (beat <= pts.front().beat) return pts.front().value;
    if (beat >= pts.back().beat)  return pts.back().value;
    const auto it = std::upper_bound(pts.begin(), pts.end(), beat,
        [](double b, const AutomationPoint& p) { return b < p.beat; });
    const auto& p1 = *std::prev(it);
    const auto& p2 = *it;
    const auto fT = static_cast<float>((beat - p1.beat) / (p2.beat - p1.beat));
    return p1.value + fT * (p2.value - p1.value);
}

void coreengine::RenderLoop::applyAutomation(uint64_t blockStartSample, uint64_t blockSamples)
{
    if (automationData_.empty()) return;
    const double sr   = static_cast<double>(audioBuffer_.sampleRate);
    const double beat = (static_cast<double>(blockStartSample) +
                         static_cast<double>(blockSamples) * 0.5) / (sr * 60.0 / 120.0);

    for (auto& [trackId, params] : automationData_) {
        Track* track = timeline.getTrack(trackId);
        if (!track) continue;
        for (auto& [paramName, pts] : params) {
            if (pts.empty()) continue;
            const float val = interpolateAutomation(pts, beat);
            if (paramName == "volume") { track->setVolume(val); continue; }
            const auto dot = paramName.find('.');
            if (dot == std::string::npos) continue;
            const std::string effectType = paramName.substr(0, dot);
            const std::string epName     = paramName.substr(dot + 1);
            AudioEffect* fx = track->getEffect(effectType);
            if (!fx) continue;
            if (epName == "mix") { fx->setMix(val); }
            else if (effectType == "Reverb") {
                if (auto* r = dynamic_cast<ReverbEffect*>(fx)) {
                    if (epName == "roomSize") r->setRoomSize(val);
                    else if (epName == "damping") r->setDamping(val);
                }
            } else if (effectType == "Delay") {
                if (auto* d = dynamic_cast<DelayEffect*>(fx)) {
                    if      (epName == "delayMs")   d->setDelayMs(val * 2000.f);
                    else if (epName == "feedback")  d->setFeedback(val * 0.95f);
                    else if (epName == "damping")   d->setDamping(val);
                }
            } else if (effectType == "Distortion") {
                if (auto* d = dynamic_cast<DistortionEffect*>(fx))
                    if (epName == "drive") d->setDrive(val * 10.f);
            }
        }
    }
}
