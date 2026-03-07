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
        float attack;   // seconds
        float decay;     // seconds
        float sustain;   // 0.0–1.0
        float release;   // seconds
    };

    using CommandData = std::variant<
        std::monostate,       // Empty/None
        NoteData,             // NoteOn, NoteOff
        InstrumentData,       // AddInstrument
        TimestampData,        // SetTimestamp
        ParamData,            // Volume, Pan, VST Params
        AddTrackData,         // AddTrack
        TrackControlData,     // SetTrackVolume, SetTrackMute, SetTrackSolo, RemoveTrack
        NoteEventData,        // AddNote (sample-based)
        NoteEventMusicalData, // AddNote (beat-based)
        ChordData,            // AddChord
        MelodyData,           // AddMelody
        ArpeggioData,         // AddArpeggio
        SeekData,             // Seek
        ADSRData              // SetADSR
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