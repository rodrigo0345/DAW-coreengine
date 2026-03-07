#!/bin/bash

# Test script for the interactive DAW engine

echo "=== Testing DAW Core Engine Interactive Mode ==="
echo ""

ENGINE="./cmake-build-debug/DAWCoreEngine"

if [ ! -f "$ENGINE" ]; then
    echo "Error: Engine not found at $ENGINE"
    echo "Please build the project first with: cmake --build cmake-build-debug"
    exit 1
fi

echo "1. Adding a sine wave track..."
echo '{"type":"AddTrack","data":{"trackId":0,"name":"Sine Bass","synthType":0,"numVoices":8}}'

echo ""
echo "2. Adding some notes..."
echo '{"type":"AddNote","data":{"trackId":0,"startBeat":0.0,"durationBeats":1.0,"midiNote":60,"velocity":100,"bpm":120,"sampleRate":44100}}'
echo '{"type":"AddNote","data":{"trackId":0,"startBeat":1.0,"durationBeats":1.0,"midiNote":64,"velocity":100,"bpm":120,"sampleRate":44100}}'
echo '{"type":"AddNote","data":{"trackId":0,"startBeat":2.0,"durationBeats":1.0,"midiNote":67,"velocity":100,"bpm":120,"sampleRate":44100}}'

echo ""
echo "3. Rebuilding timeline..."
echo '{"type":"RebuildTimeline"}'

echo ""
echo "4. Starting playback..."
echo '{"type":"Play"}'

echo ""
echo "Sending commands to engine (will play for 5 seconds)..."
echo ""

(
    echo '{"type":"AddTrack","data":{"trackId":0,"name":"Sine Bass","synthType":0,"numVoices":8}}'
    echo '{"type":"AddNote","data":{"trackId":0,"startBeat":0.0,"durationBeats":1.0,"midiNote":60,"velocity":100,"bpm":120,"sampleRate":44100}}'
    echo '{"type":"AddNote","data":{"trackId":0,"startBeat":1.0,"durationBeats":1.0,"midiNote":64,"velocity":100,"bpm":120,"sampleRate":44100}}'
    echo '{"type":"AddNote","data":{"trackId":0,"startBeat":2.0,"durationBeats":1.0,"midiNote":67,"velocity":100,"bpm":120,"sampleRate":44100}}'
    echo '{"type":"AddNote","data":{"trackId":0,"startBeat":3.0,"durationBeats":1.0,"midiNote":72,"velocity":100,"bpm":120,"sampleRate":44100}}'
    echo '{"type":"RebuildTimeline"}'
    echo '{"type":"Play"}'
    sleep 5
) | $ENGINE

echo ""
echo "Test complete!"

