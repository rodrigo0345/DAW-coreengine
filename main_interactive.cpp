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

    static bool getBool(const std::string& json, const std::string& key) {
        size_t keyPos = json.find("\"" + key + "\"");
        if (keyPos == std::string::npos) return false;
        size_t colonPos = json.find(":", keyPos);
        if (colonPos == std::string::npos) return false;
        // Skip whitespace after colon
        size_t valPos = json.find_first_not_of(" \t\r\n", colonPos + 1);
        if (valPos == std::string::npos) return false;
        // Check for boolean literal
        if (json.substr(valPos, 4) == "true")  return true;
        if (json.substr(valPos, 5) == "false") return false;
        // Fallback: treat as numeric (1 = true)
        return getFloat(json, key) > 0.5f;
    }

    // Parse an array of {beat, value} objects from a JSON string.
    // Expects: "points":[{"beat":0,"value":0.5},...]
    static std::vector<std::pair<double,float>> getPointsArray(
        const std::string& json, const std::string& key)
    {
        std::vector<std::pair<double,float>> result;
        size_t kp = json.find("\"" + key + "\"");
        if (kp == std::string::npos) return result;
        size_t arrStart = json.find('[', kp);
        if (arrStart == std::string::npos) return result;
        size_t arrEnd = json.find(']', arrStart);
        if (arrEnd == std::string::npos) return result;
        std::string arr = json.substr(arrStart + 1, arrEnd - arrStart - 1);
        // Split by '}'
        size_t pos = 0;
        while (pos < arr.size()) {
            size_t ob = arr.find('{', pos);
            size_t cb = arr.find('}', ob);
            if (ob == std::string::npos || cb == std::string::npos) break;
            std::string obj = arr.substr(ob, cb - ob + 1);
            double beat  = getDouble(obj, "beat");
            float  value = getFloat (obj, "value");
            result.emplace_back(beat, value);
            pos = cb + 1;
        }
        return result;
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

    // ── Effects ────────────────────────────────────────────────────────────
    else if (type == "SetTrackEffect") {
        coreengine::SetTrackEffectData d;
        d.trackId      = SimpleJSON::getInt(dataJson, "trackId");
        d.effectType   = SimpleJSON::getString(dataJson, "effectType");
        d.enabled      = SimpleJSON::getBool(dataJson, "enabled");

        // Use -1 as sentinel for "not provided", then apply defaults
        auto getOr = [&](const std::string& key, float def) -> float {
            size_t kp = dataJson.find("\"" + key + "\"");
            if (kp == std::string::npos) return def;
            return SimpleJSON::getFloat(dataJson, key);
        };

        d.mix          = getOr("mix",          0.3f);
        d.roomSize     = getOr("roomSize",      0.5f);
        d.damping      = getOr("damping",       0.5f);
        d.delayMs      = getOr("delayMs",       300.f);
        d.feedback     = getOr("feedback",      0.4f);
        d.delayDamping = getOr("delayDamping",  0.3f);
        d.drive        = getOr("drive",         2.0f);

        cmd.setTrackEffect(d);
        std::cout << "OK: Set effect " << d.effectType
                  << " on track " << d.trackId
                  << " enabled=" << d.enabled
                  << " mix=" << d.mix << "\n";
    }
    else if (type == "RemoveTrackEffect") {
        int trackId = SimpleJSON::getInt(dataJson, "trackId");
        std::string effectType = SimpleJSON::getString(dataJson, "effectType");
        cmd.removeTrackEffect(trackId, effectType);
        std::cout << "OK: Removed effect " << effectType << " from track " << trackId << "\n";
    }
    else if (type == "SetEffectParam") {
        int trackId = SimpleJSON::getInt(dataJson, "trackId");
        std::string effectType = SimpleJSON::getString(dataJson, "effectType");
        std::string paramName  = SimpleJSON::getString(dataJson, "paramName");
        float value            = SimpleJSON::getFloat(dataJson, "value");
        cmd.setEffectParam(trackId, effectType, paramName, value);
        std::cout << "OK: Set " << effectType << "." << paramName << "=" << value
                  << " on track " << trackId << "\n";
    }
    else if (type == "SetAutomationLane") {
        coreengine::AutomationLaneData d;
        d.trackId    = SimpleJSON::getInt(dataJson, "trackId");
        d.paramName  = SimpleJSON::getString(dataJson, "paramName");
        d.bpm        = SimpleJSON::getDouble(dataJson, "bpm");
        d.sampleRate = static_cast<uint64_t>(SimpleJSON::getInt(dataJson, "sampleRate"));
        if (d.bpm <= 0) d.bpm = 120.0;
        if (d.sampleRate == 0) d.sampleRate = 44100;
        auto pts = SimpleJSON::getPointsArray(dataJson, "points");
        for (auto& [beat, val] : pts) {
            d.points.push_back({ beat, val });
        }
        cmd.setAutomationLane(d);
        std::cout << "OK: SetAutomationLane track=" << d.trackId
                  << " param=" << d.paramName
                  << " points=" << d.points.size() << "\n";
    }
    else if (type == "ClearAutomationLane") {
        int         trackId   = SimpleJSON::getInt(dataJson, "trackId");
        std::string paramName = SimpleJSON::getString(dataJson, "paramName");
        double      bpm       = SimpleJSON::getDouble(dataJson, "bpm");
        uint64_t    sr        = static_cast<uint64_t>(SimpleJSON::getInt(dataJson, "sampleRate"));
        if (bpm <= 0) bpm = 120.0;
        if (sr   == 0) sr  = 196000;
        cmd.clearAutomationLane(trackId, paramName, bpm, sr);
        std::cout << "OK: ClearAutomationLane track=" << trackId
                  << " param=" << paramName << "\n";
    }
    else if (type == "LoadSample") {
        int         trackId  = SimpleJSON::getInt(dataJson, "trackId");
        std::string filePath = SimpleJSON::getString(dataJson, "filePath");
        int         rootNote = SimpleJSON::getInt(dataJson, "rootNote");
        bool        oneShot  = SimpleJSON::getBool(dataJson, "oneShot");
        if (rootNote == 0) rootNote = 69; // A4 default
        cmd.loadSample(trackId, filePath, rootNote, oneShot);
        std::cout << "OK: LoadSample track=" << trackId
                  << " file=" << filePath
                  << " root=" << rootNote
                  << " oneShot=" << oneShot << "\n";
    }
    else if (type == "SetVoiceCount") {
        int trackId   = SimpleJSON::getInt(dataJson, "trackId");
        int numVoices = SimpleJSON::getInt(dataJson, "numVoices");
        if (numVoices < 1) numVoices = 1;
        cmd.setVoiceCount(trackId, numVoices);
        std::cout << "OK: SetVoiceCount track=" << trackId << " voices=" << numVoices << "\n";
    }
    else if (type == "SetSynthType") {
        int    trackId   = SimpleJSON::getInt(dataJson, "trackId");
        int    synthType = SimpleJSON::getInt(dataJson, "synthType");
        int    numVoices = SimpleJSON::getInt(dataJson, "numVoices");
        double sr        = SimpleJSON::getDouble(dataJson, "sampleRate");
        if (numVoices < 1) numVoices = 8;
        if (sr <= 0)       sr = 196000.0;
        cmd.setSynthType(trackId, synthType, numVoices, sr);
        std::cout << "OK: SetSynthType track=" << trackId
                  << " type=" << synthType
                  << " voices=" << numVoices << "\n";
    }

    else {
        std::cerr << "Error: Unknown command type: " << type << "\n";
    }

    std::cout.flush();
}

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
    while (std::getline(std::cin, line)) {
        try {
            processCommand(line, cmd);
        } catch (const std::exception& e) {
            std::cerr << "Error processing command: " << e.what() << "\n";
            std::cerr.flush();
        }
    }

    std::cout << "Shutting down...\n";
    engine.stop();

    return 0;
}

