//
// Created by rodrigo0345 on 3/4/26.
//

#include "SineOscillator.h"
#include <cmath>
#include <memory>
#include "../simd_ops.h"

void coreengine::SineOscillator::generate(coreengine::AudioBuffer& buffer,
                                          const float frequency,
                                          const float amplitude,
                                          float& phase) {
    const auto sampleRate = static_cast<float>(buffer.sampleRate);
    constexpr float pi2 = 2.0f * 3.14159265358979323846f;
    const float phaseInc = (pi2 * frequency) / sampleRate;

    const size_t numSamples = buffer.numSamples;
    // channels[0] is the mono scratch owned by Voice — write once then copy
    float* ch0 = buffer.channels[0];

    // Fill ch0 with sin(phase + i*phaseInc)*amplitude using AVX2 (8 samples/iter)
    simd::sin_block(ch0, &phase, phaseInc, amplitude, numSamples);

    // Copy to remaining channels (stereo etc.) using SIMD accumulate with gain=1
    // The caller (Voice) already zeroed the scratch, so we accumulate additively.
    // For a mono oscillator writing into a mono scratch, there's nothing more to do.
    // If the buffer has >1 channel (unusual for oscillator scratch), copy.
    for (size_t c = 1; c < buffer.channels.size(); ++c)
        std::memcpy(buffer.channels[c], ch0, numSamples * sizeof(float));
}
