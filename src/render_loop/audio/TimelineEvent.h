//
// Created by rodrigo0345 on 3/7/26.
//

#ifndef DAWCOREENGINE_TIMELINEEVENT_H
#define DAWCOREENGINE_TIMELINEEVENT_H

#include <cstdint>
#include <memory>
#include <functional>
#include "Instrument.h"

namespace coreengine {

    enum class EventType {
        NoteOn,
        NoteOff,
        InstrumentAdd,
        InstrumentRemove,
        Tempo,
        TimeSignature,
    };

    /**
     * Represents a scheduled event on the timeline.
     * Events are triggered at specific sample positions.
     */
    struct TimelineEvent {
        uint64_t samplePosition;  // When to trigger this event
        EventType type;

        // Event-specific data
        Instrument* instrument = nullptr;  // Target instrument
        int midiNote = -1;
        float velocity = 0.0f;
        int trackId = -1;

        // Constructor for note events
        TimelineEvent(uint64_t pos, EventType t, Instrument* inst, int note, float vel)
            : samplePosition(pos), type(t), instrument(inst), midiNote(note), velocity(vel) {}

        // Constructor for other events
        TimelineEvent(uint64_t pos, EventType t, int track = -1)
            : samplePosition(pos), type(t), trackId(track) {}

        // For sorting events by time
        bool operator<(const TimelineEvent& other) const {
            return samplePosition < other.samplePosition;
        }
    };
}

#endif //DAWCOREENGINE_TIMELINEEVENT_H

