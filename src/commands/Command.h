//
// Created by rodrigo0345 on 3/7/26.
//

#ifndef DAWCOREENGINE_COMMAND_H
#define DAWCOREENGINE_COMMAND_H

#include <variant>
#include <cstdint>
#include <string>
#include <vector>
#include "../render_loop/audio/Instrument.h"

namespace coreengine {

    enum class CommandType {
        // Real-time commands
        NoteOn,
        NoteOff,
        AllNotesOff,
        Play,
        Stop,
        Pause,
        Reset,
        Seek,

        // Timeline editing commands
        AddTrack,
        RemoveTrack,
        AddNote,
        RemoveNote,
        AddChord,
        AddMelody,
        AddArpeggio,
        ClearTrack,
        SetTrackVolume,
        SetTrackMute,
        SetTrackSolo,
        SetADSR,
        RebuildTimeline,

        // Effects
        SetTrackEffect,    // add or replace a named effect on a track
        RemoveTrackEffect, // remove effect by name
        SetEffectParam,    // set a float param on a named effect

        // Automation
        SetAutomationLane,
        ClearAutomationLane,

        // Sampler / instrument management
        LoadSample,     // load WAV/FLAC file into a SamplePlayer track
        SetVoiceCount,  // change number of voices on a track's instrument
        SetSynthType,   // replace synth type (sine/square/saw/pwm/sampler)

        // Legacy
        AddInstrument,
        SetTimestamp,
    };
    struct NoteData { int midiNote; float velocity; Instrument *synth; };
    struct InstrumentData { int pluginId; int trackId; };
    struct TimestampData { uint64_t samples; };
    struct ParamData { int pluginId; int paramId; float value; };

    // Timeline command data structures
    struct AddTrackData {
        int trackId;
        std::string trackName;
        int synthType; // 0=Sine, 1=Square, 2=Sawtooth, 3=PWM
        int numVoices;
    };

    struct TrackControlData {
        int trackId;
        float value; // Used for volume, mute (0/1), solo (0/1)
    };

    struct NoteEventData {
        int trackId;
        uint64_t startSample;
        uint64_t durationSamples;
        int midiNote;
        float velocity;
    };

    struct NoteEventMusicalData {
        int trackId;
        double startBeat;
        double durationBeats;
        int midiNote;
        float velocity;
        double bpm;
        uint64_t sampleRate;
    };

    struct ChordData {
        int trackId;
        std::vector<int> notes;
        double startBeat;
        double durationBeats;
        float velocity;
        double bpm;
        uint64_t sampleRate;
    };

    struct MelodyData {
        int trackId;
        std::vector<int> midiNotes;
        std::vector<double> startBeats;
        std::vector<double> durationBeats;
        std::vector<float> velocities;
        double bpm;
        uint64_t sampleRate;
    };

    struct ArpeggioData {
        int trackId;
        std::vector<int> notes;
        double startBeat;
        double noteLength;
        int repetitions;
        float velocity;
        double bpm;
        uint64_t sampleRate;
    };

    struct SeekData {
        uint64_t samplePosition;
    };

    struct ADSRData {
        int trackId;
        float attack;
        float decay;
        float sustain;
        float release;
    };

    // effect type strings: "Reverb" | "Delay" | "Distortion"
    struct SetTrackEffectData {
        int         trackId;
        std::string effectType; // "Reverb", "Delay", "Distortion"
        bool        enabled;
        float       mix;        // 0–1
        // reverb
        float roomSize = 0.5f;
        float damping  = 0.5f;
        // delay
        float delayMs  = 300.f;
        float feedback = 0.4f;
        float delayDamping = 0.3f;
        // distortion
        float drive = 2.0f;
    };

    struct RemoveTrackEffectData {
        int         trackId;
        std::string effectType;
    };

    // Generic float param setter: paramName in {"mix","roomSize","damping","delayMs","feedback","drive"}
    struct SetEffectParamData {
        int         trackId;
        std::string effectType;
        std::string paramName;
        float       value;
    };

    struct AutomationPointData {
        double beat;
        float  value;
    };

    struct AutomationLaneData {
        int         trackId;
        std::string paramName;
        std::vector<AutomationPointData> points;
        double bpm;
        uint64_t sampleRate;
    };

    struct LoadSampleData {
        int         trackId;
        std::string filePath;   // absolute path to WAV/FLAC/MP3
        int         rootNote;   // MIDI root note (default 69 = A4)
        bool        oneShot;    // true = drum/one-shot, false = sustain
    };

    struct SetVoiceCountData {
        int trackId;
        int numVoices;
    };

    struct SetSynthTypeData {
        int trackId;
        int synthType;   // 0=Sine 1=Square 2=Saw 3=PWM 4=Sampler
        int numVoices;
        double sampleRate;
    };

    using CommandData = std::variant<
        std::monostate,
        NoteData,
        InstrumentData,
        TimestampData,
        ParamData,
        AddTrackData,
        TrackControlData,
        NoteEventData,
        NoteEventMusicalData,
        ChordData,
        MelodyData,
        ArpeggioData,
        SeekData,
        ADSRData,
        SetTrackEffectData,
        RemoveTrackEffectData,
        SetEffectParamData,
        AutomationLaneData,
        LoadSampleData,
        SetVoiceCountData,
        SetSynthTypeData
    >;

    struct Command {
        CommandType type;
        CommandData data;

        // Default constructor
        Command() : type(CommandType::Play), data(std::monostate{}) {}

        // Constructor for simple commands (Play, Stop, AllNotesOff)
        explicit Command(CommandType t) : type(t), data(std::monostate{}) {}

        // Constructor for data commands
        Command(CommandType t, const CommandData& d) : type(t), data(d) {}
    };
}

#endif //DAWCOREENGINE_COMMAND_H