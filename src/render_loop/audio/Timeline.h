//
// Created by rodrigo0345 on 3/7/26.
//

#ifndef DAWCOREENGINE_TIMELINE_H
#define DAWCOREENGINE_TIMELINE_H

#include <vector>
#include <memory>
#include <algorithm>
#include <iostream>
#include <map>
#include "Track.h"
#include "TimelineEvent.h"
#include "sol/sol.hpp"

namespace coreengine {

    /**
     * The Timeline/Sequencer manages tracks and events over time.
     * It schedules and triggers events at the correct sample positions.
     */
    class Timeline {
    public:
        Timeline() = default;

        // Track management — auto-assign ID
        int addTrack(std::string name, std::unique_ptr<Instrument> instrument) {
            int trackId = nextTrackId++;
            tracks.emplace_back(std::make_unique<Track>(trackId, std::move(name), std::move(instrument)));
            rebuildEventQueue();
            return trackId;
        }

        // Track management — caller-specified ID (used by the frontend)
        void addTrackWithId(int trackId, std::string name, std::unique_ptr<Instrument> instrument) {
            // If a track with this ID already exists, replace its instrument
            for (auto& t : tracks) {
                if (t->getTrackId() == trackId) {
                    t->replaceInstrument(std::move(instrument));
                    rebuildEventQueue();
                    return;
                }
            }
            tracks.emplace_back(std::make_unique<Track>(trackId, std::move(name), std::move(instrument)));
            if (trackId >= nextTrackId) nextTrackId = trackId + 1;
            rebuildEventQueue();
        }

        Track* getTrack(int trackId) {
            for (auto& track : tracks) {
                if (track->getTrackId() == trackId) {
                    return track.get();
                }
            }
            return nullptr;
        }

        std::vector<std::unique_ptr<Track>>& getAllTracks() {
            return tracks;
        }

        // Add a note to a specific track
        void addNote(int trackId, uint64_t startSample, uint64_t durationSamples, int midiNote, float velocity) {
            Track* track = getTrack(trackId);
            if (track) {
                track->addNote(startSample, durationSamples, midiNote, velocity);
                rebuildEventQueue();
            }
        }

        // Add a note using musical time (bars, beats)
        void addNoteMusical(const int trackId, const double startBeat,
            const double durationBeats, const int midiNote, const float velocity,
            const double bpm, const uint64_t sampleRate) {
            const double samplesPerBeat = (60.0 * static_cast<double>(sampleRate)) / bpm;
            const auto startSample = static_cast<uint64_t>(startBeat * samplesPerBeat);
            const auto durationSamples = static_cast<uint64_t>(durationBeats * samplesPerBeat);
            addNote(trackId, startSample, durationSamples, midiNote, velocity);
        }

        // Returns true if at least one track has solo enabled
        [[nodiscard]] bool anySoloActive() const {
            for (const auto& t : tracks)
                if (t->getSolo()) return true;
            return false;
        }

        // Returns true if this track should produce audio given current solo/mute state
        [[nodiscard]] bool trackIsAudible(const Track& t) const {
            if (t.getMuted()) return false;
            if (anySoloActive() && !t.getSolo()) return false;
            return true;
        }

        // Process events for the current block
        void processEventsForBlock(uint64_t currentSamplePos, uint64_t blockSize) {
            const uint64_t blockEnd  = currentSamplePos + blockSize;
            const bool     soloMode  = anySoloActive();

            while (eventQueueIndex < sortedEvents.size()) {
                const auto& event = sortedEvents[eventQueueIndex];
                if (event.samplePosition >= blockEnd) break;

                if (event.samplePosition >= currentSamplePos) {
                    triggerEvent(event, soloMode);
                }

                eventQueueIndex++;
            }
        }

        // Reset playback to beginning
        void reset() {
            eventQueueIndex = 0;
            for (auto& track : tracks) {
                if (track->getInstrument()) {
                    track->getInstrument()->allNotesOff();
                }
            }
        }

        // Seek to a specific position
        void seekTo(uint64_t samplePosition) {
            // Turn off all notes first
            for (auto& track : tracks) {
                if (track->getInstrument()) {
                    track->getInstrument()->allNotesOff();
                }
            }

            // Find the correct position in the event queue
            eventQueueIndex = 0;
            while (eventQueueIndex < sortedEvents.size() &&
                   sortedEvents[eventQueueIndex].samplePosition < samplePosition) {
                eventQueueIndex++;
            }
        }

        // Get total duration in samples
        uint64_t getTotalDuration() const {
            uint64_t maxDuration = 0;
            for (const auto& event : sortedEvents) {
                if (event.samplePosition > maxDuration) {
                    maxDuration = event.samplePosition;
                }
            }
            return maxDuration;
        }

        // Clear all tracks
        void clear() {
            tracks.clear();
            sortedEvents.clear();
            eventQueueIndex = 0;
            nextTrackId = 0;
        }

        // Rebuild the sorted event queue from all tracks
        void rebuildEventQueue() {
            sortedEvents.clear();

            for (auto& track : tracks) {
                const auto& trackEvents = track->getEvents();
                sortedEvents.insert(sortedEvents.end(), trackEvents.begin(), trackEvents.end());
            }

            // Sort all events by time
            std::sort(sortedEvents.begin(), sortedEvents.end());

            // Reset playback position
            eventQueueIndex = 0;
        }

    private:
        std::vector<std::unique_ptr<Track>> tracks;
        std::vector<TimelineEvent> sortedEvents;  // All events from all tracks, sorted by time
        size_t eventQueueIndex = 0;  // Current position in event queue
        int nextTrackId = 0;

        void triggerEvent(const TimelineEvent& event, const bool soloMode) { // TODO: add solo mode
            std::cout << "Solo mode: " << (soloMode? "ON" : "OFF");
            Track* track = getTrack(event.trackId);
            if (!track) return;

            // Enforce mute / solo: silence NoteOn for non-audible tracks.
            // Always let NoteOff through so voices don't hang.
            if (event.type == EventType::NoteOn && !trackIsAudible(*track)) return;

            Instrument* inst = track->getInstrument();
            if (!inst) return;

            switch (event.type) {
                case EventType::NoteOn:
                    inst->noteOn(event.midiNote, event.velocity);
                    track->markDirty("timeline:noteOn");
                    break;
                case EventType::NoteOff:
                    inst->noteOff(event.midiNote);
                    track->markDirty("timeline:noteOff");
                    break;
                default:
                    break;
            }
        }
    };
}

#endif //DAWCOREENGINE_TIMELINE_H

