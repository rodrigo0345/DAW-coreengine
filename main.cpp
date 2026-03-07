#include <iostream>
#include <thread>
#include <chrono>
#include "src/CoreServiceEngine.h"
#include "src/commands/CommandBuilder.h"

int main() {
    std::cout << "=== DAW Timeline Demo (Command-Based) ===\n\n";

    coreengine::EngineConfig config{};
    config.sampleRate = coreengine::SampleRate::CD;
    config.dspFormat = coreengine::DspFormat::FLOAT32;
    config.channels = coreengine::Channels::MONO;

    coreengine::CoreServiceEngine engine(config);
    coreengine::CommandBuilder cmd(engine.getRenderLoop().getCommandQueue());

    constexpr double BPM = 120.0;
    constexpr uint64_t SAMPLE_RATE = 44100;

    std::cout << "Creating bass track...\n";
    cmd.addSawtoothTrack(0, "Bass", 1);

    // Add bass notes using commands
    const std::vector<int> bassNotes = {36, 36, 43, 41, 38, 38, 36};
    const std::vector<double> bassStarts = {0.0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0};
    const std::vector<double> bassDurations = {0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 1.0};
    const std::vector<float> bassVelocities = {100.0f, 80.0f, 100.0f, 90.0f, 100.0f, 80.0f, 100.0f};

    // Repeat the bass pattern 4 times
    for (int rep = 0; rep < 4; ++rep) {
        double offset = rep * 4.0;
        for (size_t i = 0; i < bassNotes.size(); ++i) {
            cmd.addNoteMusical(0, bassStarts[i] + offset, bassDurations[i],
                             bassNotes[i], bassVelocities[i], BPM, SAMPLE_RATE);
        }
    }

    // ============================================================
    // Track 2: Chord progression (Square wave) - Using Commands
    // ============================================================
    std::cout << "Creating chord track...\n";
    cmd.addSquareTrack(1, "Chords", 8);

    // C major chord
    cmd.addChord(1, {60, 64, 67}, 0.0, 4.0, 80.0f, BPM, SAMPLE_RATE);

    // F major chord
    cmd.addChord(1, {65, 69, 72}, 4.0, 4.0, 80.0f, BPM, SAMPLE_RATE);

    // G major chord
    cmd.addChord(1, {67, 71, 74}, 8.0, 4.0, 80.0f, BPM, SAMPLE_RATE);

    // C major chord (resolution)
    cmd.addChord(1, {60, 64, 67}, 12.0, 4.0, 80.0f, BPM, SAMPLE_RATE);

    // ============================================================
    // Track 3: Lead melody (Sine wave) - Using Commands
    // ============================================================
    std::cout << "Creating lead track...\n";
    cmd.addSineTrack(2, "Lead", 4);

    cmd.addMelody(2,
        {72, 74, 76, 77, 79, 77, 76},        // MIDI notes
        {0.0, 0.5, 1.0, 1.5, 2.0, 3.0, 3.5}, // Start beats
        {0.5, 0.5, 0.5, 0.5, 1.0, 0.5, 0.5}, // Durations
        {100.0f, 100.0f, 100.0f, 100.0f, 100.0f, 100.0f, 100.0f}, // Velocities
        BPM, SAMPLE_RATE);

    // ============================================================
    // Track 4: Arpeggio (PWM) - Using Commands
    // ============================================================
    std::cout << "Creating arpeggio track...\n";
    cmd.addPWMTrack(3, "Arpeggio", 4);

    cmd.addArpeggio(3, {60, 64, 67, 72}, 8.0, 0.25, 8, 90.0f, BPM, SAMPLE_RATE);

    // Rebuild the timeline after adding all events
    cmd.rebuildTimeline();

    // Start playback using commands
    std::cout << "Starting playback...\n";
    cmd.play();
    engine.start();

    std::cout << "Playing timeline...\n";
    std::cout << "Total duration: " << (engine.getRenderLoop().getTimeline().getTotalDuration() / (double)SAMPLE_RATE) << " seconds\n";
    std::cout << "Press Ctrl+C to stop\n\n";

    std::cout << "Controls:\n";
    std::cout << "  This demo runs automatically, but you could add:\n";
    std::cout << "  - Mute/solo tracks with cmd.setTrackMute(trackId, true)\n";
    std::cout << "  - Change volume with cmd.setTrackVolume(trackId, 0.5f)\n";
    std::cout << "  - Seek with cmd.seek(samplePosition)\n";
    std::cout << "  - Stop with cmd.stop()\n\n";

    // Show playback progress
    auto startTime = std::chrono::steady_clock::now();

    while (true) {
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count() / 1000.0;

        uint64_t currentPos = engine.getRenderLoop().getCurrentPosition();
        double currentSeconds = currentPos / (double)SAMPLE_RATE;

        std::cout << "\rPlayback: " << currentSeconds << "s / "
                  << (engine.getRenderLoop().getTimeline().getTotalDuration() / (double)SAMPLE_RATE) << "s    " << std::flush;

        // Loop back to beginning when done
        if (currentPos >= engine.getRenderLoop().getTimeline().getTotalDuration()) {
            cmd.reset();
            startTime = std::chrono::steady_clock::now();
            std::cout << "\n[Looping...]\n";
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    cmd.stop();
    engine.stop();
    return 0;
}

