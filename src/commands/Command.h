//
// Created by rodrigo0345 on 3/7/26.
//

#ifndef DAWCOREENGINE_COMMAND_H
#define DAWCOREENGINE_COMMAND_H

#include <variant>
#include <cstdint>
#include "../render_loop/audio/Instrument.h"

namespace coreengine {

    enum class CommandType {
        NoteOn,
        NoteOff,
        AllNotesOff,
        AddInstrument, // consider instrument to be either an oscillator or anything created with a plugin vst2
        Play,
        Stop,
        SetTimestamp,
    };
    struct NoteData { int midiNote; float velocity; Instrument *synth; };
    struct InstrumentData { int pluginId; int trackId; };
    struct TimestampData { uint64_t samples; };
    struct ParamData { int pluginId; int paramId; float value; };

    using CommandData = std::variant<
        std::monostate,   // Empty/None
        NoteData,         // NoteOn, NoteOff
        InstrumentData,   // AddInstrument
        TimestampData,    // SetTimestamp
        ParamData         // Volume, Pan, VST Params
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