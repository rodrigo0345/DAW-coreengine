//
// Created by rodrigo0345 on 3/4/26.
//

#ifndef DAWCOREENGINE_SINEOSCILLATOR_H
#define DAWCOREENGINE_SINEOSCILLATOR_H

#include <memory>

#include "../AudioBuffer.h"
#include "../AudioBlock.h"


namespace coreengine {

class SineOscillator: public coreengine::AudioBlock {
public:
    void processBlock(std::shared_ptr<coreengine::AudioBuffer> buffer) override;
    void releaseResources() override;
private:
    float frequency = 440.0f; // A4 note - this could be a great inside into notes
    float phase = 0.0f;
    float amplitude = 0.5f; // 50% volume
};

}

#endif //DAWCOREENGINE_SINEOSCILLATOR_H