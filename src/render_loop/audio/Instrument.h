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

        /**
         * Returns true if the instrument has active voices that may produce audio.
         * Used by Track to prevent silence-caching while notes are held.
         * Default: false (native synths rely on ADSR isActive instead).
         */
        [[nodiscard]] virtual bool hasActiveVoices() const { return false; }
    };
}

#endif //DAWCOREENGINE_INSTRUMENT_H

