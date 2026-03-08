//
// Created by rodrigo0345 on 3/8/26.
//

#ifndef DAWCOREENGINE_COMMANDJSONPARSER_H
#define DAWCOREENGINE_COMMANDJSONPARSER_H
#include <expected>

#include "CommandBuilder.h"
#include "rapidjson/document.h"

namespace coreengine {
    enum class ParseError {
        InvalidJson,
        MissingType,
        UnknownCommand
    };

    class CommandJSONParser {
    public:
        explicit CommandJSONParser(CommandBuilder& builder) : cmd(builder) {}

        [[nodiscard]]
        static std::expected<rapidjson::Document, ParseError> parse(std::string_view line);

        static float getFloat(const rapidjson::Value& v, std::string_view key, float def = 0.0f);
        static int getInt(const rapidjson::Value& v, std::string_view key, int def = 0);
        static double getDouble(const rapidjson::Value& v, std::string_view key, double def = 0.0);
        static std::string_view getString(const rapidjson::Value& v, std::string_view key, std::string_view def = "");
        static bool getBool(const rapidjson::Value& v, std::string_view key, bool def = false);
    private:
        CommandBuilder& cmd;
    };
}

#endif //DAWCOREENGINE_COMMANDJSONPARSER_H