#include <iostream>
#include <string>
#include <sstream>
#include "src/CoreServiceEngine.h"
#include "src/commands/CommandAPI.h"
#include "src/commands/CommandBuilder.h"
#include "src/commands/CommandJSONParser.h"

int main() {
    std::cout << "=== DAW Core Engine - Interactive Mode ===\n";
    std::cout << "Waiting for commands from frontend...\n\n";

    coreengine::EngineConfig config{};
    config.sampleRate = coreengine::SampleRate::STUDIO;   // 196 kHz
    config.dspFormat  = coreengine::DspFormat::FLOAT32;
    config.channels   = coreengine::Channels::STEREO;

    coreengine::CoreServiceEngine engine(config);
    coreengine::CommandBuilder cmd(engine.getRenderLoop().getCommandQueue());

    engine.start();
    std::printf("Engine started with sample rate %u Hz, format %s, channels %u\n",
        config.getSampleRateVal(),
        (config.dspFormat == coreengine::DspFormat::FLOAT32) ? "float32" : "float64",
        config.getChannelsVal());
    std::cout.flush();

    std::string line;
    coreengine::CommandAPI api(cmd);

    // used for interprocess communication
    while (std::getline(std::cin, line)) {
        try {
            auto jsonDoc = coreengine::CommandJSONParser::parse(line);
            if (!jsonDoc) {
                std::cerr << "Error: Invalid JSON command\n";
                continue;
            }
            if (const auto result = api.execute(std::move(*jsonDoc)); !result) {
                std::cerr << "Error: Command execution failed\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "Error processing command: " << e.what() << "\n";
            std::cerr.flush();
        }
    }

    std::cout << "Shutting down...\n";
    engine.stop();
    return 0;
}

