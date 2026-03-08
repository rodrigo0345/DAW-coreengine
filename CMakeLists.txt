cmake_minimum_required(VERSION 3.31.9)
project(DAWCoreEngine)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Dependencies ────────────────────────────────────────────────────────

# Find PulseAudio (required for core audio engine)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PULSE REQUIRED libpulse libpulse-simple)

# Find libsndfile (required for loading audio samples)
pkg_check_modules(SNDFILE REQUIRED sndfile)

# ── RapidJSON (Header-only) ─────────────────────────────────────────────
include(FetchContent)
set(RAPIDJSON_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(RAPIDJSON_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(RAPIDJSON_BUILD_THIRDPARTY_GTEST OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
        rapidjson
        GIT_REPOSITORY https://github.com/Tencent/rapidjson.git
        GIT_TAG master
)
FetchContent_MakeAvailable(rapidjson)

# ── Compiler Hardening (Warnings as Errors & UB Prevention) ─────────────
if(MSVC)
    add_compile_options(/W4 /WX /permissive- /Zc:__cplusplus)
else()
    add_compile_options(
            -Wall -Wextra -Wpedantic    # Enable all meaningful warnings
            -Werror                     # Treat warnings as errors
            -Wshadow                    # Local variable shadows another
            -Wnon-virtual-dtor          # Virtual classes must have virtual destructors
            -Wold-style-cast            # Use static_cast/reinterpret_cast
            -Wcast-align                # Casting results in alignment issues
            -Wunused                    # Catch unused code/variables
            -Woverloaded-virtual        # Warn if hiding virtual functions
            -Wconversion                # Implicit type conversions with data loss
            -Wnull-dereference          # Static analysis for null pointers
            -Wdouble-promotion          # Warn if float promotes to double (Slow in DSP)
            -Wformat=2                  # Formatting string security/correctness
    )
endif()

# ── Sanitizers for Debugging ────────────────────────────────────────────
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(NOT MSVC)
        # AddressSanitizer and UndefinedBehaviorSanitizer
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
        src/commands/CommandAPI.h)

target_include_directories(DAWCoreEngine PRIVATE
        ${PULSE_INCLUDE_DIRS}
        ${SNDFILE_INCLUDE_DIRS}
        ${rapidjson_SOURCE_DIR}/include
)

target_link_libraries(DAWCoreEngine PRIVATE
        ${PULSE_LIBRARIES}
        ${SNDFILE_LIBRARIES}
)

# ── Target: DAWCoreEngineDemo (Static Playback Mode) ────────────────────
add_executable(DAWCoreEngineDemo main.cpp ${ENGINE_SOURCES})

target_include_directories(DAWCoreEngineDemo PRIVATE
        ${PULSE_INCLUDE_DIRS}
        ${SNDFILE_INCLUDE_DIRS}
        ${rapidjson_SOURCE_DIR}/include
)

target_link_libraries(DAWCoreEngineDemo PRIVATE
        ${PULSE_LIBRARIES}
        ${SNDFILE_LIBRARIES}
)

# ── Best Practice: Clang-Tidy (Optional but Recommended) ────────────────
# If you have clang-tidy installed, this will run it during every build
# find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
# if(CLANG_TIDY_EXE)
#     set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE};-checks=*,-llvmlibc-*")
# endif()