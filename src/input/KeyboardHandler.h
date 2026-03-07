//
// Created by rodrigo0345 on 3/7/26.
//

#ifndef DAWCOREENGINE_KEYBOARDHANDLER_H
#define DAWCOREENGINE_KEYBOARDHANDLER_H

#include <unordered_map>
#include <functional>
#include "../commands/CommandQueue.h"
#include "../render_loop/audio/Instrument.h"

namespace coreengine {

    /**
     * Maps keyboard keys to MIDI notes for playing instruments.
     * Uses a piano-like layout on the keyboard.
     */
    class KeyboardHandler {
    public:
        explicit KeyboardHandler(CommandQueue& queue) : commandQueue(queue) {}

        // Set the current instrument to send notes to
        void setActiveInstrument(Instrument* inst) {
            activeInstrument = inst;
        }

        // Handle key press - returns true if key was mapped to a note
        bool onKeyPress(char key) {
            auto it = keyToMidi.find(key);
            if (it == keyToMidi.end() || !activeInstrument) {
                return false;
            }

            int midiNote = it->second + octaveOffset;

            // Don't retrigger if already playing
            if (activeNotes.find(key) != activeNotes.end()) {
                return true;
            }

            activeNotes[key] = midiNote;

            // Send NoteOn command
            Command cmd(CommandType::NoteOn, NoteData{
                .midiNote = midiNote,
                .velocity = currentVelocity,
                .synth = activeInstrument
            });

            return commandQueue.push(cmd);
        }

        // Handle key release
        bool onKeyRelease(char key) {
            auto it = activeNotes.find(key);
            if (it == activeNotes.end() || !activeInstrument) {
                return false;
            }

            int midiNote = it->second;
            activeNotes.erase(it);

            // Send NoteOff command
            Command cmd(CommandType::NoteOff, NoteData{
                .midiNote = midiNote,
                .velocity = 0,
                .synth = activeInstrument
            });

            return commandQueue.push(cmd);
        }

        // Change octave (up/down)
        void changeOctave(int delta) {
            octaveOffset += delta * 12;
            // Clamp to valid MIDI range
            if (octaveOffset < -48) octaveOffset = -48;
            if (octaveOffset > 48) octaveOffset = 48;
        }

        void setVelocity(float velocity) {
            currentVelocity = velocity;
        }

        int getOctaveOffset() const { return octaveOffset; }

    private:
        CommandQueue& commandQueue;
        Instrument* activeInstrument = nullptr;
        std::unordered_map<char, int> activeNotes;
        int octaveOffset = 0; // Middle C is 60
        float currentVelocity = 100.0f;

        // Piano-like keyboard layout (two rows for white and black keys)
        // Lower row: ZSXDCVGBHNJM (white keys starting from C)
        // Upper row: Q2W3ER5T6Y7U (black keys)
        const std::unordered_map<char, int> keyToMidi = {
            // Lower row - white keys (C major scale starting at C4 = 60)
            {'z', 60},  // C
            {'s', 61},  // C#
            {'x', 62},  // D
            {'d', 63},  // D#
            {'c', 64},  // E
            {'v', 65},  // F
            {'g', 66},  // F#
            {'b', 67},  // G
            {'h', 68},  // G#
            {'n', 69},  // A
            {'j', 70},  // A#
            {'m', 71},  // B
            {',', 72},  // C (next octave)

            // Upper row - more notes
            {'q', 72},  // C (higher octave)
            {'2', 73},  // C#
            {'w', 74},  // D
            {'3', 75},  // D#
            {'e', 76},  // E
            {'r', 77},  // F
            {'5', 78},  // F#
            {'t', 79},  // G
            {'6', 80},  // G#
            {'y', 81},  // A
            {'7', 82},  // A#
            {'u', 83},  // B
            {'i', 84},  // C
        };
    };
}

#endif //DAWCOREENGINE_KEYBOARDHANDLER_H

