//
// Created by rodrigo0345 on 3/8/26.
//

#ifndef DAWCOREENGINE_COMMANDAPI_H
#define DAWCOREENGINE_COMMANDAPI_H
#include <expected>
#include <functional>
#include <string>
#include <unordered_map>

#include "CommandBuilder.h"
#include "rapidjson/document.h"

namespace coreengine {
    enum class APIError { UnknownCommand, MalformedData, TypeMismatch };

    using CommandAPIHandler = std::function<void(const rapidjson::Value&, CommandBuilder&)>;

    class CommandAPI {
    public:
        explicit CommandAPI(CommandBuilder& builder) : cmd(builder) {
            setupRoutes();
        }

        // Takes ownership of doc so it is guaranteed to be freed when execute() returns.
        std::expected<void, APIError> execute(rapidjson::Document&& doc) {
            if (!doc.HasMember("type") || !doc["type"].IsString()) {
                return std::unexpected(APIError::MalformedData);
            }
            const std::string type{ doc["type"].GetString() };

            // If the JSON has a "data" sub-object use it, otherwise treat the
            // whole document as the data payload.
            const rapidjson::Value& data = doc.HasMember("data") ? doc["data"] : doc;

            const auto it = routes.find(type);
            if (it == routes.end()) return std::unexpected(APIError::UnknownCommand);

            it->second(data, cmd);
            // doc goes out of scope here and is destroyed, releasing all memory.
            return {};
        }

    private:
        CommandBuilder& cmd;
        // std::string keys — never dangling, safe to look up with std::string.
        std::unordered_map<std::string, CommandAPIHandler> routes;

        void setupRoutes();
    };
}

#endif //DAWCOREENGINE_COMMANDAPI_H
