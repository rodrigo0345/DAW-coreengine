//
// Created by rodrigo0345 on 3/7/26.
//

#ifndef DAWCOREENGINE_INSTRUMENT_H
#define DAWCOREENGINE_INSTRUMENT_H

#include "AudioBlock.h"

namespace coreengine {
    /**
     * Abstract base class for musical instruments.
     * Provides a common interface for MIDI note handling.
     */
    class Instrument : public AudioBlock {
    public:
        virtual ~Instrument() = default;

        /**
         * Trigger a note on event
         * @param midiNote MIDI note number (0-127)
         * @param velocity Note velocity (0.0-127.0)
         */
        virtual void noteOn(int midiNote, float velocity) = 0;

        /**
         * Trigger a note off event
         * @param midiNote MIDI note number to stop
         */
        virtual void noteOff(int midiNote) = 0;

        /**
         * Stop all currently playing notes
         */
        virtual void allNotesOff() = 0;
    };
}

#endif //DAWCOREENGINE_INSTRUMENT_H

