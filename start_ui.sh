#!/bin/bash

echo "=== DAW Frontend Quick Start ==="
echo ""
echo "This script will:"
echo "1. Kill any running processes"
echo "2. Rebuild the engine"
echo "3. Start the frontend"
echo ""

# Kill any existing processes
pkill -f "DAWCoreEngine" 2>/dev/null
pkill -f "electron" 2>/dev/null
pkill -f "vite" 2>/dev/null

echo "✓ Cleaned up existing processes"
echo ""

# Build engine
echo "Building C++ engine..."
cd /home/rodrigo0345/CLionProjects/DAWCoreEngine

# build for release
cmake -S . -B cmake-build-release -DCMAKE_BUILD_TYPE=Release >/dev/null 2>&1
cmake --build cmake-build-release --target DAWCoreEngine >/dev/null 2>&1

if [ $? -eq 0 ]; then
    echo "✓ Engine built successfully"
else
    echo "✗ Engine build failed"
    exit 1
fi

echo ""
echo "Starting frontend..."
echo "The UI will open in a new window"
echo "Press Ctrl+C to stop"
echo ""

cd frontend
npm run dev

