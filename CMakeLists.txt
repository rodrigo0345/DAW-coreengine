cmake_minimum_required(VERSION 3.31.9)
project(DAWCoreEngine)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Dependencies ────────────────────────────────────────────────────────
find_package(PkgConfig REQUIRED)

# Map dependencies directly to CMake targets
pkg_check_modules(PULSE REQUIRED libpulse libpulse-simple IMPORTED_TARGET)
pkg_check_modules(SNDFILE REQUIRED sndfile IMPORTED_TARGET)
pkg_check_modules(LUAJIT REQUIRED luajit IMPORTED_TARGET)

# ── FetchContent (Header-only) ──────────────────────────────────────────
include(FetchContent)
set(RAPIDJSON_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(RAPIDJSON_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(RAPIDJSON_BUILD_THIRDPARTY_GTEST OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
        rapidjson
        GIT_REPOSITORY https://github.com/Tencent/rapidjson.git
        GIT_TAG master
)

FetchContent_Declare(
        sol2
        GIT_REPOSITORY https://github.com/ThePhD/sol2.git
        GIT_TAG main
)

FetchContent_MakeAvailable(rapidjson sol2)

# ── Compiler Hardening ──────────────────────────────────────────────────
if(MSVC)
    add_compile_options(/W4 /WX /permissive- /Zc:__cplusplus /arch:AVX2)
else()
    add_compile_options(
            -Wall -Wextra -Wpedantic
            -Werror
            -Wshadow
            -Wnon-virtual-dtor
            -Wold-style-cast
            -Wcast-align
            -Wunused
            -Woverloaded-virtual
            -Wconversion
            -Wnull-dereference
            -Wdouble-promotion
            -Wformat=2
    )
    # ── SIMD: require AVX2 + FMA on this machine ──────────────────────────
    # These flags are per-target so they don't leak into FetchContent deps.
    # Applied below to DAWCoreEngine / DAWCoreEngineDemo targets individually.
endif()

# ── SIMD compile options (applied to engine targets only) ───────────────
if(NOT MSVC)
    set(SIMD_FLAGS -mavx2 -mfma -msse4.2)
    if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        list(APPEND SIMD_FLAGS -O3 -ffast-math -funroll-loops)
    endif()
endif()

# ── Sanitizers ──────────────────────────────────────────────────────────
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(NOT MSVC)
        add_compile_options(-fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address -fsanitize=undefined)
        message(STATUS "Build: Debug with ASan and UBSan enabled")
    endif()
endif()

# ── Shared Engine Source Files ──────────────────────────────────────────
set(ENGINE_SOURCES
        src/CoreServiceEngine.cpp
        src/CoreServiceEngine.h
        src/configs/EngineConfig.cpp
        src/configs/EngineConfig.h
        src/render_loop/RenderLoop.cpp
        src/render_loop/RenderLoop.h
        src/render_loop/audio/AudioBlock.h
        src/render_loop/audio/simple_sounds/SineOscillator.cpp
        src/render_loop/audio/simple_sounds/SineOscillator.h
        src/render_loop/audio/AudioBuffer.h
        src/render_loop/audio/simple_sounds/SimpleSynth.cpp
        src/render_loop/audio/simple_sounds/SimpleSynth.h
        src/render_loop/audio/Voice.h
        src/render_loop/audio/utils.h
        src/commands/Command.cpp
        src/commands/Command.h
        src/commands/CommandQueue.cpp
        src/commands/CommandQueue.h
)

# ── Target: DAWCoreEngine (Interactive JSON Mode) ───────────────────────
add_executable(DAWCoreEngine main_interactive.cpp ${ENGINE_SOURCES}
        src/commands/CommandJSONParser.cpp
        src/commands/CommandJSONParser.h
        src/commands/CommandAPI.cpp
        src/commands/CommandAPI.h
        src/plugins/PluginManager.cpp
        src/plugins/PluginManager.h
        src/plugins/Plugin.h
        src/plugins/LuaPlugin.cpp
        src/plugins/LuaPlugin.h)

target_include_directories(DAWCoreEngine PRIVATE
        ${rapidjson_SOURCE_DIR}/include
)

# ── SIMD flags (AVX2 + FMA) ─────────────────────────────────────────────
if(DEFINED SIMD_FLAGS)
    target_compile_options(DAWCoreEngine PRIVATE ${SIMD_FLAGS})
endif()

# Enforce Sol2 safety constraints and LuaJIT compatibility
target_compile_definitions(DAWCoreEngine PRIVATE
        SOL_ALL_SAFETIES_ON=1
        SOL_LUAJIT=1
        SOL_USING_CXX_LUA=0
)

# Link using modern imported targets
target_link_libraries(DAWCoreEngine PRIVATE
        PkgConfig::PULSE
        PkgConfig::SNDFILE
        PkgConfig::LUAJIT
        sol2
)

# ── Target: DAWCoreEngineDemo (Static Playback Mode) ────────────────────
add_executable(DAWCoreEngineDemo main.cpp ${ENGINE_SOURCES})

target_include_directories(DAWCoreEngineDemo PRIVATE
        ${rapidjson_SOURCE_DIR}/include
)

if(DEFINED SIMD_FLAGS)
    target_compile_options(DAWCoreEngineDemo PRIVATE ${SIMD_FLAGS})
endif()

target_link_libraries(DAWCoreEngineDemo PRIVATE
        PkgConfig::PULSE
        PkgConfig::SNDFILE
)