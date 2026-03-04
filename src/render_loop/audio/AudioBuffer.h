//
// Created by rodrigo0345 on 3/4/26.
//

#ifndef DAWCOREENGINE_AUDIOBUFFER_H
#define DAWCOREENGINE_AUDIOBUFFER_H

#include <vector>

namespace coreengine {
        struct AudioBuffer {
            typedef float* ptr_float32;
            // this is a simple interleaved buffer, where channels are stored sequentially for each sample
            std::vector<ptr_float32> channels;
            const uint64_t sampleRate;
            const size_t numSamples;
        };
}

#endif //DAWCOREENGINE_AUDIOBUFFER_H