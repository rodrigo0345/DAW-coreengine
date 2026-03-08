//
// Created by rodrigo0345 on 3/7/26.
//

#ifndef DAWCOREENGINE_SAMPLEPLAYER_H
#define DAWCOREENGINE_SAMPLEPLAYER_H

#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <memory>
#include <sndfile.h>
#include "../audio/Instrument.h"
#include "../audio/ADSR.h"
#include "../audio/EffectChain.h"

namespace coreengine {

/**
 * Polyphonic sampler instrument – loads WAV/FLAC/MP3 via libsndfile.
 * Pitch-shifts via linear-interpolation sample-rate conversion.
 * Supports one-shot (drums) and sustain modes.
 */
class SamplePlayer : public Instrument {
public:
    static constexpr int DEFAULT_ROOT_NOTE = 69; // A4

    explicit SamplePlayer(int numVoices = 8, double sampleRate = 196000.0,
                          int rootNote = DEFAULT_ROOT_NOTE)
        : numVoices_(numVoices), engineSampleRate_(sampleRate)
        , rootNote_(rootNote), oneShot_(true)
    {
        voices_.resize(numVoices_);
    }

    // ── File loading ──────────────────────────────────────────────────────────
    bool loadFile(const std::string& path) {
        SF_INFO info{};
        SNDFILE* sf = sf_open(path.c_str(), SFM_READ, &info);
        if (!sf) return false;
        fileSampleRate_   = static_cast<double>(info.samplerate);
        numFileChannels_  = info.channels;
        numFrames_        = static_cast<size_t>(info.frames);
        sampleData_.resize(numFrames_ * numFileChannels_);
        sf_readf_float(sf, sampleData_.data(), static_cast<sf_count_t>(numFrames_));
        sf_close(sf);
        filePath_ = path;
        return true;
    }

    [[nodiscard]] bool isLoaded()              const { return !sampleData_.empty(); }
    [[nodiscard]] const std::string& getFilePath() const { return filePath_; }

    void setVoiceCount(int n) {
        n = std::max(1, n);
        allNotesOff();
        voices_.clear();
        voices_.resize(n);
        numVoices_ = n;
    }
    [[nodiscard]] int getVoiceCount() const { return numVoices_; }

    void setOneShot(bool v)  { oneShot_  = v; }
    void setRootNote(int m)  { rootNote_ = m; }

    void setADSRParameters(const ADSR::Parameters& p) {
        adsrParams_ = p;
        for (auto& v : voices_)
            if (v.adsr) v.adsr->setParameters(p);
    }
    [[nodiscard]] const ADSR::Parameters& getADSRParameters() const { return adsrParams_; }

    // ── Instrument interface ──────────────────────────────────────────────────
    void noteOn(int midiNote, float velocity) override {
        if (!isLoaded()) return;
        SampleVoice* v = findFreeVoice();
        if (!v) v = stealVoice();
        if (!v) return;

        const double semi = static_cast<double>(midiNote - rootNote_);
        v->speed    = std::pow(2.0, semi / 12.0) * (fileSampleRate_ / engineSampleRate_);
        v->readPos  = 0.0;
        v->midiNote = midiNote;
        v->velocity = velocity / 127.0f;
        v->active   = true;
        v->adsr     = std::make_unique<ADSR>(engineSampleRate_, adsrParams_);
        v->adsr->trigger();
    }

    void noteOff(int midiNote) override {
        if (oneShot_) return;
        for (auto& v : voices_)
            if (v.active && v.midiNote == midiNote && v.adsr)
                v.adsr->release();
    }

    void allNotesOff() override {
        for (auto& v : voices_)
            if (v.active && v.adsr) { v.adsr->release(); }
    }

    void processBlock(std::shared_ptr<AudioBuffer> buffer) override {
        if (!isLoaded() || !buffer) return;
        const size_t ns  = buffer->numSamples;
        const size_t nch = buffer->channels.size();

        for (auto& v : voices_) {
            if (!v.active || !v.adsr) continue;
            for (size_t s = 0; s < ns; ++s) {
                const float env = v.adsr->process();
                if (!v.adsr->isActive() || v.readPos >= static_cast<double>(numFrames_)) {
                    v.active = false; break;
                }
                const auto   fi   = static_cast<size_t>(v.readPos);
                const double frac = v.readPos - static_cast<double>(fi);
                float mono = 0.0f;
                for (int fc = 0; fc < numFileChannels_; ++fc) {
                    const size_t i0 = fi * numFileChannels_ + fc;
                    const size_t i1 = std::min(i0 + numFileChannels_, sampleData_.size() - 1);
                    mono += static_cast<float>(sampleData_[i0]*(1.0-frac) + sampleData_[i1]*frac);
                }
                mono /= static_cast<float>(numFileChannels_);
                const float out = mono * env * v.velocity;
                for (size_t c = 0; c < nch; ++c) buffer->channels[c][s] += out;
                v.readPos += v.speed;
            }
        }
        effectChain_.process(buffer);
    }

    void releaseResources() override { allNotesOff(); effectChain_.clear(); }
    EffectChain& getEffectChain() { return effectChain_; }

private:
    struct SampleVoice {
        bool   active   = false;
        int    midiNote = -1;
        float  velocity = 1.0f;
        double speed    = 1.0;
        double readPos  = 0.0;
        std::unique_ptr<ADSR> adsr;   // heap-allocated – ADSR has no default ctor
    };

    SampleVoice* findFreeVoice() {
        for (auto& v : voices_) if (!v.active) return &v;
        return nullptr;
    }
    SampleVoice* stealVoice() {
        SampleVoice* best = nullptr; double maxPos = -1.0;
        for (auto& v : voices_) { if (v.readPos > maxPos) { maxPos = v.readPos; best = &v; } }
        return best;
    }

    int    numVoices_;
    double engineSampleRate_;
    int    rootNote_;
    bool   oneShot_;

    std::string        filePath_;
    std::vector<float> sampleData_;
    size_t             numFrames_       = 0;
    double             fileSampleRate_  = 44100.0;
    int                numFileChannels_ = 1;

    std::vector<SampleVoice> voices_;
    ADSR::Parameters         adsrParams_;
    EffectChain              effectChain_;
};

} // namespace coreengine
#endif // DAWCOREENGINE_SAMPLEPLAYER_H

