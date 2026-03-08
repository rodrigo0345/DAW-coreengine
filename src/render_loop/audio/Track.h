//
// Created by rodrigo0345 on 3/7/26.
//

#ifndef DAWCOREENGINE_TRACK_H
#define DAWCOREENGINE_TRACK_H

#include <array>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include "Instrument.h"
#include "TimelineEvent.h"
#include "EffectChain.h"

namespace coreengine {

    /**
     * Represents a track in the DAW timeline.
     * Each track has an instrument and a list of events.
     */
    class Track {
    public:
        Track(int id, std::string name, std::unique_ptr<Instrument> inst)
            : trackId(id), trackName(std::move(name)), instrument(std::move(inst))
        {
            // Pre-wire scratch AudioBuffer — channels point into fixed storage, never re-allocated
            scratchBuf_.sampleRate = 44100;
            scratchBuf_.numSamples = MAX_BLOCK_SIZE;
            scratchBuf_.channels.resize(MAX_CHANNELS);
            for (size_t i = 0; i < MAX_CHANNELS; ++i)
                scratchBuf_.channels[i] = scratchStorage_[i].data();
        }

        int getTrackId() const { return trackId; }
        const std::string& getName() const { return trackName; }
        Instrument* getInstrument() { return instrument.get(); }

        /** Replace the instrument (e.g. when synth type changes). Keeps notes. */
        void replaceInstrument(std::unique_ptr<Instrument> newInst) {
            if (instrument) instrument->allNotesOff();
            instrument = std::move(newInst);
            // No need to re-point events: TimelineEvent no longer stores a raw
            // instrument pointer. triggerEvent resolves the live instrument via
            // trackId at call time, so sortedEvents in Timeline is always safe.
        }

        void setMuted(bool muted) { isMuted = muted; }
        bool getMuted() const { return isMuted; }

        void setSolo(bool solo) { isSolo = solo; }
        bool getSolo() const { return isSolo; }

        void setVolume(float vol) { volume = vol; }
        float getVolume() const { return volume; }

        // Add a note to this track
        void addNote(uint64_t startSample, uint64_t durationSamples, int midiNote, float velocity) {
            events.emplace_back(startSample,                   EventType::NoteOn,  trackId, midiNote, velocity);
            events.emplace_back(startSample + durationSamples, EventType::NoteOff, trackId, midiNote, 0.0f);
        }

        // Get all events for this track
        const std::vector<TimelineEvent>& getEvents() const { return events; }

        // Clear all events
        void clearEvents() { events.clear(); }

        // ── Effect chain API ───────────────────────────────────────────────
        void addEffect(std::unique_ptr<AudioEffect> effect) {
            effectChain_.addEffect(std::move(effect));
        }

        void removeEffect(const std::string& name) {
            effectChain_.removeEffectByName(name);
        }

        AudioEffect* getEffect(const std::string& name) {
            return effectChain_.getEffectByName(name);
        }

        EffectChain& getEffectChain() { return effectChain_; }

        // ── Zero-alloc processBlock ────────────────────────────────────────────
        void processBlock(AudioBuffer& output) {
            if (!instrument || isMuted) return;

            const size_t ns  = output.numSamples;
            const size_t nch = std::min(output.channels.size(), MAX_CHANNELS);

            // Update scratch size/rate (no allocation — storage is fixed)
            scratchBuf_.sampleRate = output.sampleRate;
            scratchBuf_.numSamples = ns;
            // Resize channel pointer vector only if channel count changed (rare)
            if (scratchBuf_.channels.size() != nch) {
                scratchBuf_.channels.resize(nch);
                for (size_t i = 0; i < nch; ++i)
                    scratchBuf_.channels[i] = scratchStorage_[i].data();
            }

            for (size_t i = 0; i < nch; ++i)
                std::fill_n(scratchStorage_[i].data(), ns, 0.f);

            instrument->processBlock(scratchBuf_);
            effectChain_.process(scratchBuf_);

            for (size_t c = 0; c < nch; ++c)
                for (size_t s = 0; s < ns; ++s)
                    output.channels[c][s] += scratchStorage_[c][s] * volume;
        }

    private:
        int   trackId;
        std::string trackName;
        std::unique_ptr<Instrument> instrument;
        std::vector<TimelineEvent>  events;
        EffectChain effectChain_;

        bool  isMuted = false;
        bool  isSolo  = false;
        float volume  = 1.0f;

        // Fixed scratch — no heap per block
        std::array<std::array<float, MAX_BLOCK_SIZE>, MAX_CHANNELS> scratchStorage_{};
        AudioBuffer scratchBuf_;   // channels[] wired to scratchStorage_ in ctor
    };
}

#endif //DAWCOREENGINE_TRACK_H

