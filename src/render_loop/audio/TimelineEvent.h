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

        // NOTE: instrument pointer intentionally removed from note events.
        // It was a dangling-pointer time-bomb: sortedEvents holds a flat copy
        // of all events; when replaceInstrument() destroys the old Instrument
        // on the audio thread (between processCommands and processEventsForBlock
        // inside the same processNextBlock call), the cached raw pointer in
        // sortedEvents became invalid. We now resolve the live instrument at
        // trigger time via trackId.
        int trackId  = -1;   // used by ALL events to look up the live instrument
        int midiNote = -1;
        float velocity = 0.0f;

        // Constructor for note events (trackId mandatory, no instrument ptr)
        TimelineEvent(uint64_t pos, EventType t, int track, int note, float vel)
            : samplePosition(pos), type(t), trackId(track), midiNote(note), velocity(vel) {}

        // Constructor for non-note events
        explicit TimelineEvent(uint64_t pos, EventType t, int track = -1)
            : samplePosition(pos), type(t), trackId(track) {}

        // For sorting events by time
        bool operator<(const TimelineEvent& other) const {
            return samplePosition < other.samplePosition;
        }
    };
}

#endif //DAWCOREENGINE_TIMELINEEVENT_H

