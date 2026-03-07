//
// Created by rodrigo0345 on 3/7/26.
//

#ifndef DAWCOREENGINE_TRACK_H
#define DAWCOREENGINE_TRACK_H

#include <memory>
#include <string>
#include <vector>
#include "Instrument.h"
#include "TimelineEvent.h"

namespace coreengine {

    /**
     * Represents a track in the DAW timeline.
     * Each track has an instrument and a list of events.
     */
    class Track {
    public:
        Track(int id, std::string name, std::unique_ptr<Instrument> inst)
            : trackId(id), trackName(std::move(name)), instrument(std::move(inst)) {}

        int getTrackId() const { return trackId; }
        const std::string& getName() const { return trackName; }
        Instrument* getInstrument() { return instrument.get(); }

        void setMuted(bool muted) { isMuted = muted; }
        bool getMuted() const { return isMuted; }

        void setSolo(bool solo) { isSolo = solo; }
        bool getSolo() const { return isSolo; }

        void setVolume(float vol) { volume = vol; }
        float getVolume() const { return volume; }

        // Add a note to this track
        void addNote(uint64_t startSample, uint64_t durationSamples, int midiNote, float velocity) {
            events.emplace_back(startSample, EventType::NoteOn, instrument.get(), midiNote, velocity);
            events.emplace_back(startSample + durationSamples, EventType::NoteOff, instrument.get(), midiNote, 0.0f);
        }

        // Get all events for this track
        const std::vector<TimelineEvent>& getEvents() const { return events; }

        // Clear all events
        void clearEvents() { events.clear(); }

        // Process audio for this track
        void processBlock(std::shared_ptr<AudioBuffer> buffer) {
            if (!isMuted && instrument) {
                instrument->processBlock(buffer);

                // Apply track volume
                if (volume != 1.0f) {
                    for (auto& channel : buffer->channels) {
                        for (size_t i = 0; i < buffer->numSamples; ++i) {
                            channel[i] *= volume;
                        }
                    }
                }
            }
        }

    private:
        int trackId;
        std::string trackName;
        std::unique_ptr<Instrument> instrument;
        std::vector<TimelineEvent> events;

        bool isMuted = false;
        bool isSolo = false;
        float volume = 1.0f;
    };
}

#endif //DAWCOREENGINE_TRACK_H

