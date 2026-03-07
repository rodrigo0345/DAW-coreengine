//
// Created by rodrigo0345 on 3/7/26.
//

#ifndef DAWCOREENGINE_SEQUENCER_H
#define DAWCOREENGINE_SEQUENCER_H

#include <vector>
#include <string>
#include "Timeline.h"

namespace coreengine {

    /**
     * Musical note representation with timing information
     */
    struct Note {
        int midiNote;
        double startBeat;
        double durationBeats;
        float velocity;

        Note(int note, double start, double duration, float vel = 100.0f)
            : midiNote(note), startBeat(start), durationBeats(duration), velocity(vel) {}
    };

    /**
     * Helper class for creating musical patterns and sequences
     */
    class Sequencer {
    public:
        /**
         * Create a chord progression on a track
         */
        static void addChord(Timeline& timeline, int trackId,
                           const std::vector<int>& notes,
                           double startBeat, double durationBeats,
                           float velocity, double bpm, uint64_t sampleRate) {
            for (int note : notes) {
                timeline.addNoteMusical(trackId, startBeat, durationBeats, note, velocity, bpm, sampleRate);
            }
        }

        /**
         * Create a melody from a sequence of notes
         */
        static void addMelody(Timeline& timeline, int trackId,
                            const std::vector<Note>& notes,
                            double bpm, uint64_t sampleRate) {
            for (const auto& note : notes) {
                timeline.addNoteMusical(trackId, note.startBeat, note.durationBeats,
                                      note.midiNote, note.velocity, bpm, sampleRate);
            }
        }

        /**
         * Create a drum pattern using MIDI note numbers
         */
        static void addDrumPattern(Timeline& timeline, int trackId,
                                 const std::vector<int>& pattern,  // MIDI notes
                                 const std::vector<double>& beats,   // Beat positions
                                 float velocity, double bpm, uint64_t sampleRate) {
            for (size_t i = 0; i < pattern.size() && i < beats.size(); ++i) {
                timeline.addNoteMusical(trackId, beats[i], 0.1, pattern[i], velocity, bpm, sampleRate);
            }
        }

        /**
         * Create an arpeggio pattern
         */
        static void addArpeggio(Timeline& timeline, int trackId,
                              const std::vector<int>& notes,
                              double startBeat, double noteLength,
                              int repetitions, float velocity,
                              double bpm, uint64_t sampleRate) {
            double currentBeat = startBeat;
            for (int rep = 0; rep < repetitions; ++rep) {
                for (int note : notes) {
                    timeline.addNoteMusical(trackId, currentBeat, noteLength,
                                          note, velocity, bpm, sampleRate);
                    currentBeat += noteLength;
                }
            }
        }

        /**
         * Create a repeating pattern
         */
        static void repeatPattern(Timeline& timeline, int trackId,
                                const std::vector<Note>& pattern,
                                int repetitions, double beatsPerPattern,
                                double bpm, uint64_t sampleRate) {
            for (int rep = 0; rep < repetitions; ++rep) {
                double offset = rep * beatsPerPattern;
                for (const auto& note : pattern) {
                    timeline.addNoteMusical(trackId,
                                          note.startBeat + offset,
                                          note.durationBeats,
                                          note.midiNote,
                                          note.velocity,
                                          bpm, sampleRate);
                }
            }
        }

        /**
         * Create a simple beat pattern (kick, snare, hihat)
         */
        static void addSimpleBeat(Timeline& timeline, int trackId,
                                int bars, double bpm, uint64_t sampleRate) {
            // Standard MIDI drum mapping
            const int KICK = 36;
            const int SNARE = 38;
            const int HIHAT = 42;

            for (int bar = 0; bar < bars; ++bar) {
                double barStart = bar * 4.0; // 4 beats per bar

                // Kick on beats 1 and 3
                timeline.addNoteMusical(trackId, barStart + 0.0, 0.1, KICK, 120.0f, bpm, sampleRate);
                timeline.addNoteMusical(trackId, barStart + 2.0, 0.1, KICK, 120.0f, bpm, sampleRate);

                // Snare on beats 2 and 4
                timeline.addNoteMusical(trackId, barStart + 1.0, 0.1, SNARE, 110.0f, bpm, sampleRate);
                timeline.addNoteMusical(trackId, barStart + 3.0, 0.1, SNARE, 110.0f, bpm, sampleRate);

                // Hihat on every beat
                for (int beat = 0; beat < 4; ++beat) {
                    timeline.addNoteMusical(trackId, barStart + beat, 0.1, HIHAT, 90.0f, bpm, sampleRate);
                }
            }
        }
    };
}

#endif //DAWCOREENGINE_SEQUENCER_H

