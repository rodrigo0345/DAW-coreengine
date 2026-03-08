//
// Created by rodrigo0345 on 3/4/26.
//

#ifndef DAWCOREENGINE_AUDIOBLOCK_H
#define DAWCOREENGINE_AUDIOBLOCK_H

#include "AudioBuffer.h"


namespace coreengine {
    class AudioBlock {
    public:
        virtual void processBlock(AudioBuffer& buffer) = 0;
        virtual void releaseResources() = 0;
        virtual ~AudioBlock() = default;
    };
} // coreengine

#endif //DAWCOREENGINE_AUDIOBLOCK_H