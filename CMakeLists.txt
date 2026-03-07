cmake_minimum_required(VERSION 4.1)
project(DAWCoreEngine)

set(CMAKE_CXX_STANDARD 23)

# PulseAudio packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(PULSE REQUIRED libpulse libpulse-simple)

add_executable(DAWCoreEngine main.cpp
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

target_include_directories(DAWCoreEngine PRIVATE ${PULSE_INCLUDE_DIRS})
target_link_libraries(DAWCoreEngine PRIVATE ${PULSE_LIBRARIES})