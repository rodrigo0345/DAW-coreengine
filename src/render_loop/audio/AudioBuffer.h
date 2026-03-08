//
// Created by rodrigo0345 on 3/4/26.
//

#ifndef DAWCOREENGINE_AUDIOBUFFER_H
#define DAWCOREENGINE_AUDIOBUFFER_H

#include <vector>
#include <array>
#include <cstdint>
#include <cstddef>

namespace coreengine {

// Maximum supported channels and block size — compile-time constants used
// everywhere to avoid per-block heap allocation.
inline constexpr size_t MAX_CHANNELS   = 2;
inline constexpr size_t MAX_BLOCK_SIZE = 1024;  // samples per block

// ── Owning buffer (lives in RenderLoop, allocated once at startup) ────────────
struct AudioBuffer {
    using ptr_float32 = float*;
    std::vector<ptr_float32> channels;  // pointers into owning storage below
    uint64_t sampleRate  = 44100;
    size_t   numSamples  = 512;

    // Owning backing store — resized once at construction, never again.
    std::array<std::array<float, MAX_BLOCK_SIZE>, MAX_CHANNELS> storage{};

    // Call once after construction to wire channels → storage.
    void initStorage(size_t numChannels, size_t blockSize, uint64_t sr) {
        sampleRate = sr;
        numSamples = blockSize;
        channels.resize(numChannels);
        for (size_t i = 0; i < numChannels; ++i)
            channels[i] = storage[i].data();
    }
};

// ── Non-owning view used for scratch buffers (zero heap) ─────────────────────
// Constructed on the stack, points into a caller-owned block of memory.
struct AudioBufferView {
    float* channels[MAX_CHANNELS]{};
    size_t   numChannels = 0;
    size_t   numSamples  = 0;
    uint64_t sampleRate  = 44100;

    // Wrap a slice of a fixed scratch array.
    void init(float scratch[][MAX_BLOCK_SIZE], size_t nCh, size_t nSamples, uint64_t sr) {
        numChannels = nCh;
        numSamples  = nSamples;
        sampleRate  = sr;
        for (size_t i = 0; i < nCh; ++i) channels[i] = scratch[i];
    }
};

} // namespace coreengine
#endif // DAWCOREENGINE_AUDIOBUFFER_H

