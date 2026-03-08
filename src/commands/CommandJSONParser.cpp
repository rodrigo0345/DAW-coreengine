//
// Created by rodrigo0345 on 3/8/26.
//

#include "CommandJSONParser.h"
#include <iostream>

#include "../configs/EngineConfig.h"

namespace coreengine {

std::expected<rapidjson::Document, ParseError> coreengine::CommandJSONParser::parse(const std::string_view line) {
    if (line.empty()) return std::unexpected(ParseError::InvalidJson);
    rapidjson::Document doc;

    // Use Parse (not ParseInsitu) so RapidJSON copies strings into its own
    // allocator. ParseInsitu stores raw pointers into the input buffer — if
    // that buffer is a local std::string it is destroyed on return, leaving
    // the Document with dangling pointers (heap/stack-use-after-free).
    if (doc.Parse(line.data(), line.size()).HasParseError()) {
        return std::unexpected(ParseError::InvalidJson);
    }

    if (!doc.HasMember("type") || !doc["type"].IsString()) {
        return std::unexpected(ParseError::MissingType);
    }
    return doc;
}

float coreengine::CommandJSONParser::getFloat(const rapidjson::Value& v, const std::string_view key, float def) {
    const auto it = v.FindMember(key.data());
    return (it != v.MemberEnd() && it->value.IsNumber()) ? it->value.GetFloat() : def;
}

int coreengine::CommandJSONParser::getInt(const rapidjson::Value& v, std::string_view key, int def) {
    const auto it = v.FindMember(key.data());
    return (it != v.MemberEnd() && it->value.IsInt()) ? it->value.GetInt() : def;
}

double coreengine::CommandJSONParser::getDouble(const rapidjson::Value& v, const std::string_view key, const double def) {
    const auto it = v.FindMember(key.data());
    return (it != v.MemberEnd() && it->value.IsNumber()) ? it->value.GetDouble() : def;
}

std::string_view coreengine::CommandJSONParser::getString(const rapidjson::Value& v, const std::string_view key, const std::string_view def) {
    const auto it = v.FindMember(key.data());
    return (it != v.MemberEnd() && it->value.IsString()) ? it->value.GetString() : def;
}

bool coreengine::CommandJSONParser::getBool(const rapidjson::Value& v, const std::string_view key, const bool def) {
    const auto it = v.FindMember(key.data());
    if (it == v.MemberEnd()) return def;
    if (it->value.IsBool()) return it->value.GetBool();
    return it->value.GetFloat() > 0.5f;
}

}
