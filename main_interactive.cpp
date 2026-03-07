#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <sstream>
#include "src/CoreServiceEngine.h"
#include "src/commands/CommandBuilder.h"

// Simple JSON parser (minimal, just for our use case)
class SimpleJSON {
public:
    static std::string extractData(const std::string& json) {
        size_t dataPos = json.find("\"data\"");
        if (dataPos == std::string::npos) return json;

        size_t colonPos = json.find(":", dataPos);
        size_t braceStart = json.find("{", colonPos);
        if (braceStart == std::string::npos) return json;

        // Find matching closing brace
        int braceCount = 1;
        size_t pos = braceStart + 1;
        while (pos < json.length() && braceCount > 0) {
            if (json[pos] == '{') braceCount++;
            else if (json[pos] == '}') braceCount--;
            pos++;
        }

        return json.substr(braceStart, pos - braceStart);
    }

    static std::string getString(const std::string& json, const std::string& key) {
        size_t keyPos = json.find("\"" + key + "\"");
        if (keyPos == std::string::npos) return "";

        size_t colonPos = json.find(":", keyPos);
        size_t startQuote = json.find("\"", colonPos);
        size_t endQuote = json.find("\"", startQuote + 1);

        if (startQuote != std::string::npos && endQuote != std::string::npos) {
            return json.substr(startQuote + 1, endQuote - startQuote - 1);
        }
        return "";
    }

    static int getInt(const std::string& json, const std::string& key) {
        size_t keyPos = json.find("\"" + key + "\"");
        if (keyPos == std::string::npos) return 0;

        size_t colonPos = json.find(":", keyPos);
        size_t numStart = json.find_first_of("-0123456789", colonPos);
        if (numStart == std::string::npos) return 0;
        size_t numEnd = json.find_first_not_of("-0123456789", numStart);

        std::string numStr = json.substr(numStart, numEnd - numStart);
        try {
            return std::stoi(numStr);
        } catch (...) {
            return 0;
        }
    }

    static double getDouble(const std::string& json, const std::string& key) {
        size_t keyPos = json.find("\"" + key + "\"");
        if (keyPos == std::string::npos) return 0.0;

        size_t colonPos = json.find(":", keyPos);
        size_t numStart = json.find_first_of("-0123456789.", colonPos);
        if (numStart == std::string::npos) return 0.0;
        size_t numEnd = json.find_first_not_of("-0123456789.", numStart);

        std::string numStr = json.substr(numStart, numEnd - numStart);
        try {
            return std::stod(numStr);
        } catch (...) {
            return 0.0;
        }
    }

    static float getFloat(const std::string& json, const std::string& key) {
        return static_cast<float>(getDouble(json, key));
    }
};

void processCommand(const std::string& line, coreengine::CommandBuilder& cmd) {
    if (line.empty()) return;

    std::string type = SimpleJSON::getString(line, "type");

    if (type.empty()) {
        std::cerr << "Error: No command type found\n";
        return;
    }

    std::cout << "Processing command: " << type << std::endl;

    // Extract data object if it exists
    std::string dataJson = SimpleJSON::extractData(line);

    // Playback commands
    if (type == "Play") {
        cmd.play();
        std::cout << "OK: Playback started\n";
    }
    else if (type == "Stop") {
        cmd.stop();
        std::cout << "OK: Playback stopped\n";
    }
    else if (type == "Pause") {
        cmd.pause();
        std::cout << "OK: Playback paused\n";
    }
    else if (type == "Reset") {
        cmd.reset();
        std::cout << "OK: Reset to beginning\n";
    }
    else if (type == "Seek") {
        uint64_t samplePosition = SimpleJSON::getInt(dataJson, "samplePosition");
        cmd.seek(samplePosition);
        std::cout << "OK: Seeked to sample " << samplePosition << "\n";
    }

    // Track management commands
    else if (type == "AddTrack") {
        int trackId = SimpleJSON::getInt(dataJson, "trackId");
        std::string name = SimpleJSON::getString(dataJson, "name");
        int synthType = SimpleJSON::getInt(dataJson, "synthType");
        int numVoices = SimpleJSON::getInt(dataJson, "numVoices");

        if (numVoices == 0) numVoices = 8; // Default

        cmd.addTrack(trackId, name, synthType, numVoices);
        std::cout << "OK: Added track " << trackId << " (" << name << ")\n";
    }
    else if (type == "SetTrackVolume") {
        int trackId = SimpleJSON::getInt(dataJson, "trackId");
        float volume = SimpleJSON::getFloat(dataJson, "value");
        cmd.setTrackVolume(trackId, volume);
        std::cout << "OK: Set track " << trackId << " volume to " << volume << "\n";
    }
    else if (type == "SetTrackMute") {
        int trackId = SimpleJSON::getInt(dataJson, "trackId");
        bool muted = SimpleJSON::getFloat(dataJson, "value") > 0.5f;
        cmd.setTrackMute(trackId, muted);
        std::cout << "OK: Track " << trackId << " mute: " << (muted ? "ON" : "OFF") << "\n";
    }
    else if (type == "SetTrackSolo") {
        int trackId = SimpleJSON::getInt(dataJson, "trackId");
        bool solo = SimpleJSON::getFloat(dataJson, "value") > 0.5f;
        cmd.setTrackSolo(trackId, solo);
        std::cout << "OK: Track " << trackId << " solo: " << (solo ? "ON" : "OFF") << "\n";
    }
    else if (type == "ClearTrack") {
        int trackId = SimpleJSON::getInt(dataJson, "trackId");
        cmd.clearTrack(trackId);
        std::cout << "OK: Cleared track " << trackId << "\n";
    }

    // Note commands
    else if (type == "AddNote") {
        int trackId = SimpleJSON::getInt(dataJson, "trackId");
        double startBeat = SimpleJSON::getDouble(dataJson, "startBeat");
        double durationBeats = SimpleJSON::getDouble(dataJson, "durationBeats");
        int midiNote = SimpleJSON::getInt(dataJson, "midiNote");
        float velocity = SimpleJSON::getFloat(dataJson, "velocity");
        double bpm = SimpleJSON::getDouble(dataJson, "bpm");
        uint64_t sampleRate = SimpleJSON::getInt(dataJson, "sampleRate");

        cmd.addNoteMusical(trackId, startBeat, durationBeats, midiNote, velocity, bpm, sampleRate);
        std::cout << "OK: Added note to track " << trackId << "\n";
    }

    // ADSR
    else if (type == "SetADSR") {
        int trackId = SimpleJSON::getInt(dataJson, "trackId");
        float attack = SimpleJSON::getFloat(dataJson, "attack");
        float decay = SimpleJSON::getFloat(dataJson, "decay");
        float sustain = SimpleJSON::getFloat(dataJson, "sustain");
        float release = SimpleJSON::getFloat(dataJson, "release");
        cmd.setADSR(trackId, attack, decay, sustain, release);
        std::cout << "OK: Set ADSR on track " << trackId
                  << " A=" << attack << " D=" << decay
                  << " S=" << sustain << " R=" << release << "\n";
    }

    // Rebuild timeline
    else if (type == "RebuildTimeline") {
        cmd.rebuildTimeline();
        std::cout << "OK: Timeline rebuilt\n";
    }

    else {
        std::cerr << "Error: Unknown command type: " << type << "\n";
    }

    std::cout.flush();
}

int main() {
    std::cout << "=== DAW Core Engine - Interactive Mode ===\n";
    std::cout << "Waiting for commands from frontend...\n\n";

    // Setup audio engine
    coreengine::EngineConfig config{};
    config.sampleRate = coreengine::SampleRate::CD;
    config.dspFormat = coreengine::DspFormat::FLOAT32;
    config.channels = coreengine::Channels::MONO;

    coreengine::CoreServiceEngine engine(config);
    coreengine::CommandBuilder cmd(engine.getRenderLoop().getCommandQueue());

    // Start audio engine
    engine.start();
    std::cout << "Audio engine started\n";
    std::cout << "Ready to receive commands (JSON format)\n\n";
    std::cout.flush();

    // Command input loop
    std::string line;
    while (std::getline(std::cin, line)) {
        try {
            processCommand(line, cmd);
        } catch (const std::exception& e) {
            std::cerr << "Error processing command: " << e.what() << "\n";
            std::cerr.flush();
        }
    }

    // Cleanup
    std::cout << "Shutting down...\n";
    engine.stop();

    return 0;
}

