#include <iostream>
#include "src/CoreServiceEngine.h"
#include "src/render_loop/audio/simple_sounds/SineOscillator.h"

int main() {
    coreengine::EngineConfig config{};
    config.sampleRate = coreengine::SampleRate::CD;
    config.dspFormat = coreengine::DspFormat::FLOAT32;
    config.channels = coreengine::Channels::STEREO;

    auto synth = std::make_unique<coreengine::SineOscillator>();

    coreengine::CoreServiceEngine engine(config);
    engine.getRenderLoop().addProcessor(std::move(synth));

    engine.getRenderLoop().play();
    engine.start();

    while (true) {
        // Vulkan rendering code here
    }

    engine.stop();
    return 0;
}
