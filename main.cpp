#include <iostream>
#include <termios.h>
#include <unistd.h>
#include <thread>
#include <chrono>
#include "src/CoreServiceEngine.h"
#include "src/render_loop/audio/SynthFactory.h"
#include "src/input/KeyboardHandler.h"

// Helper to set terminal to non-canonical mode for instant key response
struct TerminalMode {
    termios oldSettings;

    TerminalMode() {
        termios newSettings;
        tcgetattr(STDIN_FILENO, &oldSettings);
        newSettings = oldSettings;
        newSettings.c_lflag &= ~(ICANON | ECHO); // Disable line buffering and echo
        tcsetattr(STDIN_FILENO, TCSANOW, &newSettings);
    }

    ~TerminalMode() {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldSettings);
    }
};

void printHelp() {
    std::cout << "\n=== DAW Core Engine - Keyboard Synth ===\n";
    std::cout << "Controls:\n";
    std::cout << "  Piano keys: Z X C V B N M , (lower row)\n";
    std::cout << "              Q W E R T Y U I (upper row)\n";
    std::cout << "  Black keys: S D G H J (between white keys)\n";
    std::cout << "              2 3 5 6 7 (upper row)\n";
    std::cout << "\n";
    std::cout << "  1: Switch to Sine Wave\n";
    std::cout << "  2: Switch to Square Wave\n";
    std::cout << "  3: Switch to Sawtooth Wave\n";
    std::cout << "  4: Switch to PWM Wave\n";
    std::cout << "\n";
    std::cout << "  +: Octave Up\n";
    std::cout << "  -: Octave Down\n";
    std::cout << "  p: Play/Pause\n";
    std::cout << "  ESC or Ctrl+C: Quit\n";
    std::cout << "========================================\n\n";
}

int main() {
    coreengine::EngineConfig config{};
    config.sampleRate = coreengine::SampleRate::CD;
    config.dspFormat = coreengine::DspFormat::FLOAT32;
    config.channels = coreengine::Channels::MONO;

    // Create multiple synths for experimentation
    auto sineSynth = coreengine::SynthFactory::createSineSynth(8);
    auto squareSynth = coreengine::SynthFactory::createSquareSynth(8);
    auto sawSynth = coreengine::SynthFactory::createSawtoothSynth(8);
    auto pwmSynth = coreengine::SynthFactory::createPWMSynth(8);

    // Keep raw pointers for command routing (ownership transferred to engine)
    auto* activeSynth = sineSynth.get();
    auto* sinePtr = sineSynth.get();
    auto* squarePtr = squareSynth.get();
    auto* sawPtr = sawSynth.get();
    auto* pwmPtr = pwmSynth.get();

    coreengine::CoreServiceEngine engine(config);

    // Add all synths to the engine
    engine.getRenderLoop().addProcessor(std::move(sineSynth));
    engine.getRenderLoop().addProcessor(std::move(squareSynth));
    engine.getRenderLoop().addProcessor(std::move(sawSynth));
    engine.getRenderLoop().addProcessor(std::move(pwmSynth));

    // Create keyboard handler
    coreengine::KeyboardHandler keyboard(engine.getRenderLoop().getCommandQueue());
    keyboard.setActiveInstrument(activeSynth);

    // Start audio engine
    engine.getRenderLoop().play();
    engine.start();

    printHelp();
    std::cout << "Current synth: SINE WAVE | Octave: 0\n";

    // Set terminal to raw mode
    TerminalMode termMode;

    bool running = true;
    char currentSynthType = '1';

    while (running) {
        char ch;
        if (read(STDIN_FILENO, &ch, 1) > 0) {
            // Handle synth switching
            if (ch >= '1' && ch <= '4') {
                // Release all notes on previous synth
                if (activeSynth) {
                    activeSynth->allNotesOff();
                }

                currentSynthType = ch;
                switch (ch) {
                    case '1':
                        activeSynth = sinePtr;
                        std::cout << "\rCurrent synth: SINE WAVE     | Octave: " << (keyboard.getOctaveOffset()/12) << "  \n";
                        break;
                    case '2':
                        activeSynth = squarePtr;
                        std::cout << "\rCurrent synth: SQUARE WAVE   | Octave: " << (keyboard.getOctaveOffset()/12) << "  \n";
                        break;
                    case '3':
                        activeSynth = sawPtr;
                        std::cout << "\rCurrent synth: SAWTOOTH WAVE | Octave: " << (keyboard.getOctaveOffset()/12) << "  \n";
                        break;
                    case '4':
                        activeSynth = pwmPtr;
                        std::cout << "\rCurrent synth: PWM WAVE      | Octave: " << (keyboard.getOctaveOffset()/12) << "  \n";
                        break;
                }
                keyboard.setActiveInstrument(activeSynth);
                continue;
            }

            // Handle octave change
            if (ch == '+' || ch == '=') {
                keyboard.changeOctave(1);
                std::cout << "\rOctave: " << (keyboard.getOctaveOffset()/12) << "  \n";
                continue;
            }
            if (ch == '-' || ch == '_') {
                keyboard.changeOctave(-1);
                std::cout << "\rOctave: " << (keyboard.getOctaveOffset()/12) << "  \n";
                continue;
            }

            // Handle play/pause
            if (ch == 'p' || ch == 'P') {
                auto& cmdQueue = engine.getRenderLoop().getCommandQueue();
                cmdQueue.push(coreengine::Command(coreengine::CommandType::Play));
                std::cout << "\rPlaying...\n";
                continue;
            }

            // Handle quit
            if (ch == 27) { // ESC key
                running = false;
                break;
            }

            // Try to trigger note
            if (keyboard.onKeyPress(ch)) {
                std::cout << "." << std::flush; // Visual feedback
            }
        }

        // Small sleep to reduce CPU usage
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Cleanup
    std::cout << "\n\nShutting down...\n";
    engine.stop();

    return 0;
}

