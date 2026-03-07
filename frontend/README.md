# DAW Core Engine - Frontend

Modern web-based UI for the DAW Core Engine built with Electron, React, and TypeScript.

## Features

- 🎹 **Track Management**: Add, remove, and configure multiple synth tracks
- 🎵 **Timeline View**: Visual representation of your music composition
- ⚡ **Real-time Control**: Play, stop, and adjust BPM on the fly
- 🎚️ **Inspector Panel**: Edit track parameters and add notes
- 🔊 **Multiple Synths**: Sine, Square, Sawtooth, and PWM waveforms

## Tech Stack

- **Electron**: Cross-platform desktop app framework
- **React 18**: Modern UI library with hooks
- **TypeScript**: Type-safe development
- **Zustand**: Lightweight state management
- **Vite**: Fast build tooling

## Getting Started

### Prerequisites

- Node.js 18+ and npm
- Your C++ engine compiled at `../cmake-build-debug/DAWCoreEngine`

### Installation

```bash
cd frontend
npm install
```

### Development

Run the dev server (auto-reloads on changes):

```bash
npm run dev
```

This will:
1. Start Vite dev server on http://localhost:5173
2. Launch Electron app
3. Enable hot reload for React components

### Building

```bash
npm run build
```

## Project Structure

```
frontend/
├── electron/           # Electron main process
│   ├── main.ts        # Main process entry point
│   └── preload.ts     # Preload script (IPC bridge)
├── src/
│   ├── components/    # React components
│   │   ├── Timeline.tsx
│   │   ├── TrackList.tsx
│   │   ├── TransportBar.tsx
│   │   └── Inspector.tsx
│   ├── App.tsx        # Main app component
│   ├── store.ts       # Zustand state management
│   └── main.tsx       # React entry point
├── package.json
└── vite.config.ts
```

## Communication with C++ Engine

The frontend communicates with your C++ engine via:

1. **Child Process**: Electron spawns the engine as a subprocess
2. **IPC (Inter-Process Communication)**: Commands are sent via stdin/stdout
3. **Command Protocol**: JSON-formatted commands matching your CommandBuilder API

### Example Commands

```typescript
// Add a track
window.electronAPI.addTrack({
  trackId: 0,
  name: "Bass",
  synthType: 2, // Sawtooth
  numVoices: 8
});

// Add a note
window.electronAPI.addNote({
  trackId: 0,
  startBeat: 0.0,
  durationBeats: 1.0,
  midiNote: 60,
  velocity: 100,
  bpm: 120,
  sampleRate: 44100
});

// Playback control
window.electronAPI.play();
window.electronAPI.stop();
```

## Next Steps

### ✅ Engine Integration COMPLETE!

Your C++ engine is now ready to accept commands from the UI! The interactive mode has been implemented in `main_interactive.cpp`.

### Testing the Integration

1. **Build the engine** (if not already built):
   ```bash
   cd ..
   cmake --build cmake-build-debug
   ```

2. **Test the engine manually**:
   ```bash
   ./test_interactive.sh
   ```
   This will add a track, some notes, and play audio!

3. **Run the frontend**:
   ```bash
   cd frontend
   npm install  # First time only
   npm run dev
   ```

The UI will now:
- Start the C++ engine automatically
- Send commands when you add tracks
- Control playback with transport buttons
- Show engine output in the console

### How It Works

```
UI Action                  JSON Command                Engine Response
─────────                  ────────────                ───────────────
Click "Add Track"    →    {"type":"AddTrack",...}  →  Track created
Click "Play"         →    {"type":"Play"}          →  Audio starts
Add Note            →    {"type":"AddNote",...}    →  Note scheduled
Adjust Volume       →    {"type":"SetTrackVolume"} →  Volume changed
```

### Troubleshooting

**No sound?**
- Make sure you've added a track and some notes
- Click "Add Note (C4)" in the Inspector panel
- Click "Play" in the transport bar

**Engine not responding?**
- Check the console output at the bottom of the UI
- The engine prints "OK:" messages for each command
- Make sure the engine status shows "Engine Running"

### Future Enhancements

- [ ] Drag-and-drop note editing on timeline
- [ ] Piano roll editor
- [ ] MIDI keyboard input support
- [ ] Track waveform visualization
- [ ] Undo/redo functionality
- [ ] Save/load projects
- [ ] Export to audio file
- [ ] VST plugin support UI
- [ ] Mixer with effects

## Troubleshooting

**Engine not starting?**
- Check that `cmake-build-debug/DAWCoreEngine` exists and is executable
- Look at Electron console for error messages

**UI not connecting?**
- Make sure the engine is running (check status indicator)
- Open DevTools (Ctrl+Shift+I) to see any errors

**Hot reload not working?**
- Restart the dev server with `npm run dev`

## License

Same as DAW Core Engine

