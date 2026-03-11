//
// Created by rodrigo0345 on 3/7/26.
//

#ifndef DAWCOREENGINE_COMMANDBUILDER_H
#define DAWCOREENGINE_COMMANDBUILDER_H

#include "Command.h"
#include "CommandQueue.h"
#include "../plugins/PluginManager.h"

namespace coreengine {

    /**
     * Helper class for building and sending commands to the audio engine.
     * This provides a clean API for frontend applications.
     */
    class CommandBuilder {
    public:
        explicit CommandBuilder(CommandQueue& queue, PluginManager& pm) : commandQueue(queue), pluginManager(pm) {}

        // ============================================================
        // Playback Control Commands
        // ============================================================

        bool play() {
            return commandQueue.push(Command(CommandType::Play));
        }

        bool stop() {
            return commandQueue.push(Command(CommandType::Stop));
        }

        bool pause() {
            return commandQueue.push(Command(CommandType::Pause));
        }

        bool reset() {
            return commandQueue.push(Command(CommandType::Reset));
        }

        bool seek(uint64_t samplePosition) {
            return commandQueue.push(Command(CommandType::Seek, SeekData{samplePosition}));
        }

        bool allNotesOff() {
            return commandQueue.push(Command(CommandType::AllNotesOff));
        }

        // ============================================================
        // Track Management Commands
        // ============================================================

        bool addTrack(int trackId, const std::string& name, int synthType = 0, int numVoices = 8) {
            return commandQueue.push(Command(CommandType::AddTrack,
                AddTrackData{trackId, name, synthType, numVoices}));
        }

        bool addSineTrack(int trackId, const std::string& name, int numVoices = 8) {
            return addTrack(trackId, name, 0, numVoices);
        }

        bool addSquareTrack(int trackId, const std::string& name, int numVoices = 8) {
            return addTrack(trackId, name, 1, numVoices);
        }

        bool addSawtoothTrack(int trackId, const std::string& name, int numVoices = 8) {
            return addTrack(trackId, name, 2, numVoices);
        }

        bool addPWMTrack(int trackId, const std::string& name, int numVoices = 8) {
            return addTrack(trackId, name, 3, numVoices);
        }

        bool removeTrack(int trackId) {
            return commandQueue.push(Command(CommandType::RemoveTrack,
                TrackControlData{trackId, 0.0f}));
        }

        bool clearTrack(int trackId) {
            return commandQueue.push(Command(CommandType::ClearTrack,
                TrackControlData{trackId, 0.0f}));
        }

        bool setTrackVolume(int trackId, float volume) {
            return commandQueue.push(Command(CommandType::SetTrackVolume,
                TrackControlData{trackId, volume}));
        }

        bool setTrackMute(int trackId, bool muted) {
            return commandQueue.push(Command(CommandType::SetTrackMute,
                TrackControlData{trackId, muted ? 1.0f : 0.0f}));
        }

        bool setTrackSolo(int trackId, bool solo) {
            return commandQueue.push(Command(CommandType::SetTrackSolo,
                TrackControlData{trackId, solo ? 1.0f : 0.0f}));
        }

        bool setADSR(int trackId, float attack, float decay, float sustain, float release) {
            return commandQueue.push(Command(CommandType::SetADSR,
                ADSRData{trackId, attack, decay, sustain, release}));
        }

        // ============================================================
        // Note/Event Commands
        // ============================================================

        bool addNote(int trackId, uint64_t startSample, uint64_t durationSamples,
                    int midiNote, float velocity = 100.0f) {
            return commandQueue.push(Command(CommandType::AddNote,
                NoteEventData{trackId, startSample, durationSamples, midiNote, velocity}));
        }

        bool addNoteMusical(int trackId, double startBeat, double durationBeats,
                          int midiNote, float velocity, double bpm, uint64_t sampleRate) {
            return commandQueue.push(Command(CommandType::AddNote,
                NoteEventMusicalData{trackId, startBeat, durationBeats, midiNote,
                                   velocity, bpm, sampleRate}));
        }

        bool addChord(int trackId, const std::vector<int>& notes,
                     double startBeat, double durationBeats, float velocity,
                     double bpm, uint64_t sampleRate) {
            return commandQueue.push(Command(CommandType::AddChord,
                ChordData{trackId, notes, startBeat, durationBeats, velocity, bpm, sampleRate}));
        }

        [[nodiscard]]
        bool addMelody(const int trackId, const std::vector<int>& midiNotes,
                      const std::vector<double>& startBeats,
                      const std::vector<double>& durationBeats,
                      const std::vector<float>& velocities,
                      const double bpm, const uint64_t sampleRate) const {
            return commandQueue.push(Command(CommandType::AddMelody,
                MelodyData{trackId, midiNotes, startBeats, durationBeats,
                         velocities, bpm, sampleRate}));
        }

        [[nodiscard]]
        bool addArpeggio(const int trackId, const std::vector<int>& notes,
                        const double startBeat, const double noteLength, const int repetitions,
                        const float velocity, const double bpm, const uint64_t sampleRate) const {
            return commandQueue.push(Command(CommandType::AddArpeggio,
                ArpeggioData{trackId, notes, startBeat, noteLength, repetitions,
                           velocity, bpm, sampleRate}));
        }

        [[nodiscard]]
        bool rebuildTimeline() const {
            return commandQueue.push(Command(CommandType::RebuildTimeline));
        }

        // ============================================================
        // Effects
        // ============================================================

        [[nodiscard]]
        bool setTrackEffect(const SetTrackEffectData& d) const {
            return commandQueue.push(Command(CommandType::SetTrackEffect, d));
        }

        [[nodiscard]]
        bool removeTrackEffect(const int trackId, const std::string& effectType) const {
            return commandQueue.push(Command(CommandType::RemoveTrackEffect,
                RemoveTrackEffectData{trackId, effectType}));
        }

        [[nodiscard]]
        bool setEffectParam(const int trackId, const std::string& effectType,
                            const std::string& paramName, const float value) const {
            return commandQueue.push(Command(CommandType::SetEffectParam,
                SetEffectParamData{trackId, effectType, paramName, value}));
        }

        bool setAutomationLane(const AutomationLaneData& d) {
            return commandQueue.push(Command(CommandType::SetAutomationLane, d));
        }

        bool clearAutomationLane(int trackId, const std::string& paramName,
                                  double bpm, uint64_t sampleRate) {
            AutomationLaneData d;
            d.trackId    = trackId;
            d.paramName  = paramName;
            d.bpm        = bpm;
            d.sampleRate = sampleRate;
            return commandQueue.push(Command(CommandType::ClearAutomationLane, d));
        }

        // ============================================================
        // Real-time Note Triggers (for keyboard/MIDI input)
        // ============================================================

        bool noteOn(Instrument* instrument, int midiNote, float velocity) {
            return commandQueue.push(Command(CommandType::NoteOn,
                NoteData{midiNote, velocity, instrument}));
        }

        bool noteOff(Instrument* instrument, int midiNote) {
            return commandQueue.push(Command(CommandType::NoteOff,
                NoteData{midiNote, 0.0f, instrument}));
        }

        bool loadSample(int trackId, const std::string& filePath,
                        int rootNote = 69, bool oneShot = true) {
            return commandQueue.push(Command(CommandType::LoadSample,
                LoadSampleData{trackId, filePath, rootNote, oneShot}));
        }

        bool setVoiceCount(int trackId, int numVoices) {
            return commandQueue.push(Command(CommandType::SetVoiceCount,
                SetVoiceCountData{trackId, numVoices}));
        }

        bool setSynthType(int trackId, int synthType, int numVoices, double sampleRate) {
            return commandQueue.push(Command(CommandType::SetSynthType,
                SetSynthTypeData{trackId, synthType, numVoices, sampleRate}));
        }

        [[nodiscard]]
        std::expected<size_t, PluginError> createPlugin(const std::string_view pluginName, const std::string_view pluginSourceCode) const {
            return pluginManager.addPlugin(
                PluginManager::PluginType::Lua,
                pluginName,
                pluginSourceCode);
        }

        [[nodiscard]]
        bool removePlugin(const size_t pluginId) const {
            return pluginManager.removePlugin(pluginId);
        }

        [[nodiscard]]
        std::expected<void, std::string> updatePlugin(const size_t pluginId, const std::string_view newSource) const {
            return pluginManager.updatePlugin(pluginId, newSource);
        }

        [[nodiscard]]
        std::vector<PluginManager::PluginInfo> listPlugins() const {
            return pluginManager.listPlugins();
        }

        /** Assign a plugin (by id) to a track as its instrument. */
        bool assignPlugin(int trackId, size_t pluginId) {
            return commandQueue.push(Command(CommandType::AssignPlugin,
                AssignPluginData{trackId, pluginId}));
        }

        /** Change the engine's master BPM. Adjusts playhead to keep the same beat position. */
        bool setBpm(double bpm) {
            return commandQueue.push(Command(CommandType::SetBPM, SetBpmData{bpm}));
        }

    private:
        CommandQueue& commandQueue;
        PluginManager& pluginManager;
    };
}

#endif //DAWCOREENGINE_COMMANDBUILDER_H

