//
// Created by rodrigo0345 on 3/8/26.
//

#include "CommandAPI.h"
#include <iostream>

#include "CommandJSONParser.h"
#include "../configs/EngineConfig.h"

class CommandJSONParser;

void coreengine::CommandAPI::setupRoutes() {
    // ── Transport ──────────────────────────────────────────────────────────
    routes["Play"]  = [](const auto&, auto& c) { c.play(); };
    routes["Stop"]  = [](const auto&, auto& c) { c.stop(); };
    routes["Pause"] = [](const auto&, auto& c) { c.pause(); };
    routes["Reset"] = [](const auto&, auto& c) { c.reset(); };

    routes["Seek"] = [](const auto& d, auto& c) {
        c.seek(static_cast<uint64_t>(CommandJSONParser::getDouble(d, "samplePosition")));
    };

    // ── Track Management ───────────────────────────────────────────────────
    routes["AddTrack"] = [](const auto& d, auto& c) {
        const int trackId = CommandJSONParser::getInt(d, "trackId");
        const std::string trackName{ CommandJSONParser::getString(d, "name") };
        const int synthType = CommandJSONParser::getInt(d, "synthType");
        const int numVoices = CommandJSONParser::getInt(d, "numVoices", 8);
        c.addTrack(trackId, trackName, synthType, numVoices);
    };

    routes["SetTrackVolume"] = [](const auto& d, auto& c) {
        c.setTrackVolume(CommandJSONParser::getInt(d, "trackId"),
                         CommandJSONParser::getFloat(d, "value"));
    };

    routes["SetTrackMute"] = [](const auto& d, auto& c) {
        c.setTrackMute(CommandJSONParser::getInt(d, "trackId"),
                       CommandJSONParser::getFloat(d, "value") > 0.5f);
    };

    routes["SetTrackSolo"] = [](const auto& d, auto& c) {
        c.setTrackSolo(CommandJSONParser::getInt(d, "trackId"),
                       CommandJSONParser::getFloat(d, "value") > 0.5f);
    };

    routes["ClearTrack"] = [](const auto& d, auto& c) {
        c.clearTrack(CommandJSONParser::getInt(d, "trackId"));
    };

    routes["SetVoiceCount"] = [](const auto& d, auto& c) {
        c.setVoiceCount(CommandJSONParser::getInt(d, "trackId"),
                        std::max(1, CommandJSONParser::getInt(d, "numVoices")));
    };

    routes["SetSynthType"] = [](const auto& d, auto& c) {
        c.setSynthType(CommandJSONParser::getInt(d, "trackId"),
                       CommandJSONParser::getInt(d, "synthType"),
                       CommandJSONParser::getInt(d, "numVoices", 8),
                       CommandJSONParser::getDouble(d, "sampleRate", 48000.0));
    };

    // ── Note & Timeline ────────────────────────────────────────────────────
    routes["AddNote"] = [](const auto& d, auto& c) {
        c.addNoteMusical(
            CommandJSONParser::getInt(d, "trackId"),
            CommandJSONParser::getDouble(d, "startBeat"),
            CommandJSONParser::getDouble(d, "durationBeats"),
            CommandJSONParser::getInt(d, "midiNote"),
            CommandJSONParser::getFloat(d, "velocity"),
            CommandJSONParser::getDouble(d, "bpm", 120.0),
            static_cast<uint64_t>(CommandJSONParser::getInt(d, "sampleRate", static_cast<int>(SampleRate::HIGHRES)))
        );
    };

    routes["RebuildTimeline"] = [](const auto&, auto& c) {
        if (!c.rebuildTimeline()) {
            std::cerr << "Critical: Failed to rebuild engine timeline\n";
        }
    };

    // ── ADSR & Automation ──────────────────────────────────────────────────
    routes["SetADSR"] = [](const auto& d, auto& c) {
        c.setADSR(CommandJSONParser::getInt(d, "trackId"),
                  CommandJSONParser::getFloat(d, "attack"),
                  CommandJSONParser::getFloat(d, "decay"),
                  CommandJSONParser::getFloat(d, "sustain"),
                  CommandJSONParser::getFloat(d, "release"));
    };

    routes["SetAutomationLane"] = [](const auto& d, auto& c) {
        AutomationLaneData lane;
        lane.trackId    = CommandJSONParser::getInt(d, "trackId");
        lane.paramName  = CommandJSONParser::getString(d, "paramName");
        lane.bpm        = CommandJSONParser::getDouble(d, "bpm", 120.0);
        lane.sampleRate = static_cast<uint64_t>(CommandJSONParser::getInt(d, "sampleRate", 44100));

        if (d.HasMember("points") && d["points"].IsArray()) {
            for (const auto& pt : d["points"].GetArray()) {
                lane.points.push_back({
                    CommandJSONParser::getDouble(pt, "beat"),
                    CommandJSONParser::getFloat(pt, "value")
                });
            }
        }
        c.setAutomationLane(lane);
    };

    routes["ClearAutomationLane"] = [](const auto& d, auto& c) {
        c.clearAutomationLane(
            CommandJSONParser::getInt(d, "trackId"),
            std::string{ CommandJSONParser::getString(d, "paramName") },
            CommandJSONParser::getDouble(d, "bpm", 120.0),
            static_cast<uint64_t>(CommandJSONParser::getInt(d, "sampleRate", 48000))
        );
    };

    // ── Effects & Sampling ─────────────────────────────────────────────────
    routes["LoadSample"] = [](const auto& d, auto& c) {
        c.loadSample(
            CommandJSONParser::getInt(d, "trackId"),
            std::string{ CommandJSONParser::getString(d, "filePath") },
            CommandJSONParser::getInt(d, "rootNote", 69),
            CommandJSONParser::getBool(d, "oneShot")
        );
    };

    routes["SetTrackEffect"] = [](const auto& d, auto& c) {
        SetTrackEffectData data{
            .trackId      = CommandJSONParser::getInt(d, "trackId"),
            .effectType   = std::string{ CommandJSONParser::getString(d, "effectType") },
            .enabled      = CommandJSONParser::getBool(d, "enabled"),
            .mix          = CommandJSONParser::getFloat(d, "mix", 0.3f),
            .roomSize     = CommandJSONParser::getFloat(d, "roomSize", 0.5f),
            .damping      = CommandJSONParser::getFloat(d, "damping", 0.5f),
            .delayMs      = CommandJSONParser::getFloat(d, "delayMs", 300.f),
            .feedback     = CommandJSONParser::getFloat(d, "feedback", 0.4f),
            .delayDamping = CommandJSONParser::getFloat(d, "delayDamping", 0.3f),
            .drive        = CommandJSONParser::getFloat(d, "drive", 2.0f)
        };
        const auto result = c.setTrackEffect(data);
        if (!result) {
            std::cerr << "Error: Failed to set effect " << data.effectType
                      << " on track " << data.trackId << "\n";
        }
    };

    routes["RemoveTrackEffect"] = [](const auto& d, auto& c) {
        const auto result = c.removeTrackEffect(CommandJSONParser::getInt(d, "trackId"),
                            std::string{ CommandJSONParser::getString(d, "effectType") });
        if (!result) {
            std::cerr << "Error: Failed to remove effect "
                      << CommandJSONParser::getString(d, "effectType")
                      << " from track " << CommandJSONParser::getInt(d, "trackId") << "\n";
        }
    };

    routes["SetEffectParam"] = [](const auto& d, auto& c) {
        const auto result = c.setEffectParam(
            CommandJSONParser::getInt(d, "trackId"),
            std::string{ CommandJSONParser::getString(d, "effectType") },
            std::string{ CommandJSONParser::getString(d, "paramName") },
            CommandJSONParser::getFloat(d, "value")
        );
        if (!result) {
            std::cerr << "Error: Failed to set effect param "
                      << CommandJSONParser::getString(d, "paramName")
                      << " on effect " << CommandJSONParser::getString(d, "effectType")
                      << " for track " << CommandJSONParser::getInt(d, "trackId") << "\n";
        }
    };
}