//
// Created by rodrigo0345 on 3/7/26.
//

#ifndef DAWCOREENGINE_TRACK_H
#define DAWCOREENGINE_TRACK_H

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
            : trackId(id), trackName(std::move(name)), instrument(std::move(inst)) {}

        int getTrackId() const { return trackId; }
        const std::string& getName() const { return trackName; }
        Instrument* getInstrument() { return instrument.get(); }

        /** Replace the instrument (e.g. when synth type changes). Keeps notes. */
        void replaceInstrument(std::unique_ptr<Instrument> newInst) {
            if (instrument) instrument->allNotesOff();
            instrument = std::move(newInst);
            // Re-point existing events at the new instrument
            for (auto& ev : events) {
                ev.instrument = instrument.get();
            }
        }

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

        // Process audio for this track into the output buffer
        void processBlock(std::shared_ptr<AudioBuffer> buffer) {
            if (!instrument || isMuted) return;

            const auto numSamples  = buffer->numSamples;
            const auto numChannels = buffer->channels.size();

            // Ensure scratch buffers are large enough
            if (scratchChannels_.size() != numChannels) {
                scratchChannels_.resize(numChannels);
            }
            for (auto& ch : scratchChannels_) {
                if (ch.size() < numSamples)
                    ch.resize(numSamples, 0.0f);
                std::fill_n(ch.data(), numSamples, 0.0f);
            }

            // Build a temporary AudioBuffer pointing at our scratch memory
            auto tmpBuf = std::make_shared<AudioBuffer>();
            tmpBuf->sampleRate = buffer->sampleRate;
            tmpBuf->numSamples = numSamples;
            tmpBuf->channels.resize(numChannels);
            for (size_t c = 0; c < numChannels; ++c)
                tmpBuf->channels[c] = scratchChannels_[c].data();

            // Let the instrument render into the scratch buffer
            instrument->processBlock(tmpBuf);

            // ── Run effect chain on the scratch buffer ─────────────────────
            effectChain_.process(tmpBuf);

            // Mix scratch into the real output, applying track volume
            for (size_t c = 0; c < numChannels; ++c) {
                for (size_t s = 0; s < numSamples; ++s) {
                    buffer->channels[c][s] += scratchChannels_[c][s] * volume;
                }
            }
        }

    private:
        int trackId;
        std::string trackName;
        std::unique_ptr<Instrument> instrument;
        std::vector<TimelineEvent> events;
        EffectChain effectChain_;

        bool  isMuted = false;
        bool  isSolo  = false;
        float volume  = 1.0f;

        // Per-track scratch buffers for safe mixing
        std::vector<std::vector<float>> scratchChannels_;
    };
}

#endif //DAWCOREENGINE_TRACK_H

