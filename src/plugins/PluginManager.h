//
// Created by rodrigo0345 on 3/9/26.
//

#ifndef DAWCOREENGINE_PLUGINMANAGER_H
#define DAWCOREENGINE_PLUGINMANAGER_H
#include <algorithm>
#include <array>
#include <atomic>
#include <expected>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "Plugin.h"
#include "LuaPlugin.h" // For factory creation

namespace coreengine {
    enum class PluginError {
        LimitReached,
        MemoryAllocationFailure,
        CompilationFailed,
    };

    class PluginManager {
    public:
        constexpr static size_t MAX_PLUGINS = 512;
        enum class PluginType { Lua, Native }; // Only Lua supported for now

        std::expected<size_t, PluginError> addPlugin(
            const PluginType type,
            std::string_view name,
            std::string_view source)
        {
            size_t index = pluginCount.fetch_add(1uz, std::memory_order_relaxed);

            if (index >= MAX_PLUGINS) {
                pluginCount.fetch_sub(1uz, std::memory_order_relaxed);
                return std::unexpected(PluginError::LimitReached);
            }

            // 2. Factory creation based on type
            std::unique_ptr<Plugin> newPlugin;
            if (type == PluginType::Lua) {
                newPlugin = std::make_unique<LuaPlugin>(static_cast<int>(index), name, source);
            }

            if (!newPlugin) return std::unexpected(PluginError::MemoryAllocationFailure);

            if (const auto compileResult = newPlugin->compileAndOptimize(); !compileResult) {
                pluginCount.fetch_sub(1uz, std::memory_order_relaxed);
                return std::unexpected(PluginError::CompilationFailed);
            }

            pluginPool[index] = std::move(newPlugin);
            return index;
        }

        bool removePlugin(const size_t id) {
            if (static_cast<size_t>(id) >= pluginCount.load(std::memory_order_acquire)) {
                return false;
            }
            pluginPool[id].reset();
            return true;
        }

        /** Returns a pointer to the plugin or nullptr if not found / removed. */
        [[nodiscard]] Plugin* getPlugin(const size_t id) const {
            if (id >= pluginCount.load(std::memory_order_acquire)) return nullptr;
            return pluginPool[id].get();
        }

        /** Struct used when listing plugins back to the client. */
        struct PluginInfo {
            size_t      id;
            std::string name;
            std::string sourceCode;
            bool        ready;
        };

        [[nodiscard]] std::vector<PluginInfo> listPlugins() const {
            std::vector<PluginInfo> result;
            const size_t count = pluginCount.load(std::memory_order_acquire);
            for (size_t i = 0; i < count; ++i) {
                if (pluginPool[i]) {
                    result.push_back({ i, pluginPool[i]->name, pluginPool[i]->sourceCode,
                                       pluginPool[i]->isReady.load(std::memory_order_acquire) });
                }
            }
            return result;
        }

        /** Hot-reload a Lua plugin's source. Returns false if the id is invalid or compilation fails. */
        std::expected<void, std::string> updatePlugin(const size_t id, const std::string_view newSource) {
            if (id >= pluginCount.load(std::memory_order_acquire) || !pluginPool[id])
                return std::unexpected("Plugin not found");
            auto* luaPlug = dynamic_cast<LuaPlugin*>(pluginPool[id].get());
            if (!luaPlug) return std::unexpected("Plugin is not a Lua plugin");
            return luaPlug->updateSource(newSource);
        }

        void processAll(AudioBuffer& buffer) {
            const size_t count = pluginCount.load(std::memory_order_acquire);
            for (size_t i = 0uz; i < count; ++i) {
                if (!pluginPool[i]) continue;
                if (!pluginPool[i]->isReady.load(std::memory_order_acquire)) continue;
                // Only run effect plugins here — instrument plugins live on a Track
                // and are driven by the timeline's noteOn/noteOff machinery.
                auto* lua = dynamic_cast<LuaPlugin*>(pluginPool[i].get());
                if (lua && lua->isInstrument()) continue;
                pluginPool[i]->processBlock(buffer);
            }
        }

        /**
         * Transfer ownership of the plugin out of the pool so it can be assigned
         * to a Track as its instrument.  The slot is left nullptr (id is gone).
         * Returns nullptr if the id is invalid or the plugin is not a LuaPlugin.
         */
        std::unique_ptr<Plugin> takePlugin(const size_t id) {
            if (id >= pluginCount.load(std::memory_order_acquire) || !pluginPool[id])
                return nullptr;
            return std::move(pluginPool[id]);
        }

    private:
        std::atomic<size_t> pluginCount{0};
        std::array<std::unique_ptr<Plugin>, MAX_PLUGINS> pluginPool;
    };
}


#endif //DAWCOREENGINE_PLUGINMANAGER_H

