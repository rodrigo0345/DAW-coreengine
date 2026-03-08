cmake_minimum_required(VERSION 3.31.9)
project(DAWCoreEngine)

set(CMAKE_CXX_STANDARD 23)

# PulseAudio packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(PULSE REQUIRED libpulse libpulse-simple)

# libsndfile for WAV/FLAC/MP3 sample loading
pkg_check_modules(SNDFILE REQUIRED sndfile)

# Vulkan + GLFW for native GUI
find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

# ── Shared engine source files ──────────────────────────────────────────
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

# ── Dear ImGui sources ──────────────────────────────────────────────────
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui)
set(IMGUI_SOURCES
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/imgui_impl_glfw.cpp
        ${IMGUI_DIR}/imgui_impl_vulkan.cpp
)

# ── Interactive (stdin/stdout JSON) target (legacy) ─────────────────────
add_executable(DAWCoreEngine main_interactive.cpp ${ENGINE_SOURCES})
target_include_directories(DAWCoreEngine PRIVATE ${PULSE_INCLUDE_DIRS} ${SNDFILE_INCLUDE_DIRS})
target_link_libraries(DAWCoreEngine PRIVATE ${PULSE_LIBRARIES} ${SNDFILE_LIBRARIES})

# ── Demo target ─────────────────────────────────────────────────────────
add_executable(DAWCoreEngineDemo main.cpp ${ENGINE_SOURCES})
target_include_directories(DAWCoreEngineDemo PRIVATE ${PULSE_INCLUDE_DIRS} ${SNDFILE_INCLUDE_DIRS})
target_link_libraries(DAWCoreEngineDemo PRIVATE ${PULSE_LIBRARIES} ${SNDFILE_LIBRARIES})

# ── Native GUI (Vulkan + ImGui) target ──────────────────────────────────
add_executable(DAWCoreEngineGUI main_gui.cpp ${ENGINE_SOURCES} ${IMGUI_SOURCES})
target_include_directories(DAWCoreEngineGUI PRIVATE
        ${PULSE_INCLUDE_DIRS}
        ${SNDFILE_INCLUDE_DIRS}
        ${IMGUI_DIR}
        ${Vulkan_INCLUDE_DIRS}
)
target_link_libraries(DAWCoreEngineGUI PRIVATE
        ${PULSE_LIBRARIES}
        ${SNDFILE_LIBRARIES}
        Vulkan::Vulkan
        glfw
)

