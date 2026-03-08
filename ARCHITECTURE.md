# Architecture Class Diagram

## Inheritance Hierarchy

```
┌─────────────────┐
│   AudioBlock    │  (Abstract Interface)
│   (interface)   │  - processBlock()
└────────┬────────┘  - releaseResources()
         │
         ├────────────────────────┬──────────────────┐
         │                        │                  │
┌────────▼────────┐      ┌────────▼────────┐   ┌───▼─────┐
│   Instrument    │      │      Voice      │   │  Other  │
│   (abstract)    │      │  (composition)  │   │ Blocks  │
└────────┬────────┘      └────────┬────────┘   └─────────┘
         │                        │
         │                Contains│
┌────────▼────────┐      ┌────────▼────────┐
│   SimpleSynth   │      │   Oscillator    │  (Abstract)
│ (polyphonic)    │      │   (abstract)    │
└─────────────────┘      └────────┬────────┘
                                  │
                    ┌─────────────┼─────────────┐
                    │             │             │
           ┌────────▼────┐ ┌──────▼──────┐ ┌───▼────────┐
           │    Sine     │ │   Square    │ │  Sawtooth  │
           │ Oscillator  │ │ Oscillator  │ │ Oscillator │
           └─────────────┘ └─────────────┘ └────────────┘
```

## Component Relationships

### SimpleSynth
```
SimpleSynth
  │
  ├─ voices: vector<unique_ptr<Voice>>
  │    │
  │    └─ Each Voice contains:
  │         ├─ oscillator: unique_ptr<Oscillator>
  │         ├─ frequency: float
  │         ├─ amplitude: float
  │         ├─ phase: float
  │         ├─ isActive: bool
  │         └─ midiNote: int
  │
  └─ oscillatorCreator: function<unique_ptr<Oscillator>()>
```

## Data Flow

### Note On Event
```
User/MIDI Input
      │
      ▼
SimpleSynth::noteOn(midiNote, velocity)
      │
      ├─ Convert MIDI to frequency
      ├─ Convert velocity to amplitude
      │
      ▼
Find idle Voice
      │
      ▼
Voice::start(freq, amp)
      │
      ├─ Set frequency, amplitude
      ├─ Reset phase
      ├─ Set isActive = true
      └─ Call oscillator->reset()
```

### Audio Processing
```
RenderLoop::processNextBlock()
      │
      ▼
For each AudioBlock (Instrument):
      │
      ▼
SimpleSynth::processBlock(buffer)
      │
      ▼
For each Voice:
      │
      ▼
Voice::processBlock(buffer)
      │
      ▼
Oscillator::generate(buffer, freq, amp, phase)
      │
      ├─ Calculate samples
      └─ Write to buffer channels
```

## Design Patterns Used

### 1. Strategy Pattern
- **Oscillator** acts as the strategy interface
- Different oscillator implementations (Sine, Square, Sawtooth) are concrete strategies
- **Voice** is the context that uses the strategy

### 2. Factory Pattern
- **VoiceFactory** creates Voice instances with specific Oscillator types
- Allows runtime configuration of voice behavior

### 3. Template Method Pattern
- **AudioBlock** defines the processing interface
- Subclasses implement specific processing behavior

### 4. Composition over Inheritance
- **Voice** contains an **Oscillator** rather than inheriting from it
- Allows runtime swapping of oscillator types
- More flexible than inheritance hierarchy

## Key Benefits

1. **Separation of Concerns**
   - Oscillators focus on waveform generation
   - Voices handle note state and lifecycle
   - Instruments handle MIDI events and polyphony

2. **Open/Closed Principle**
   - Open for extension (new oscillators, instruments)
   - Closed for modification (existing code unchanged)

3. **Dependency Inversion**
   - High-level modules (SimpleSynth) depend on abstractions (Oscillator)
   - Not on concrete implementations (SineOscillator)

4. **Single Responsibility**
   - Each class has one clear purpose
   - Easy to test and maintain

## Extension Points

### Adding New Oscillators
```cpp
class NewOscillator : public Oscillator {
    void generate(...) override { /* ... */ }
};
```

### Adding New Instruments
```cpp
class NewInstrument : public Instrument {
    void noteOn(...) override { /* ... */ }
    void noteOff(...) override { /* ... */ }
    void processBlock(...) override { /* ... */ }
};
```

### Adding New Effects
```cpp
class Effect : public AudioBlock {
    void processBlock(...) override {
        // Process buffer (reverb, delay, etc.)
    }
};
```

## Frontend ↔ Backend Communication

The UI and the C++ engine are two completely separate processes. All communication flows
through a single **newline-delimited JSON** channel over the engine's **stdin / stdout**.

```
┌─────────────────────────────────────────────────────────────────┐
│  Electron Renderer (React + Zustand)                            │
│                                                                 │
│  window.electronAPI.addTrack(...)   ← preload.ts bridge        │
└────────────────────┬────────────────────────────────────────────┘
                     │  contextBridge / ipcRenderer.invoke()
                     ▼
┌─────────────────────────────────────────────────────────────────┐
│  Electron Main Process  (electron/main.ts)                      │
│                                                                 │
│  ipcMain.handle('timeline:addTrack', ...)                       │
│    → sendToEngine({ type: 'AddTrack', data: { ... } })         │
└────────────────────┬────────────────────────────────────────────┘
                     │  engineProcess.stdin.write(json + '\n')
                     ▼
┌─────────────────────────────────────────────────────────────────┐
│  C++ Engine  (main_interactive.cpp)                             │
│                                                                 │
│  while (getline(cin, line))                                     │
│    doc = CommandJSONParser::parse(line)                         │
│    commandQueue.push( CommandAPI::execute(doc) )                │
└────────────────────┬────────────────────────────────────────────┘
                     │  CommandQueue (lock-free push from main,
                     │               pop from audio thread)
                     ▼
┌─────────────────────────────────────────────────────────────────┐
│  RenderLoop  (audio thread)                                     │
│                                                                 │
│  processNextBlock()                                             │
│    → processCommands()   ← drains the queue each block         │
│    → timeline.processEventsForBlock()                           │
│    → track->processBlock()  for each audible track             │
│    → masterLimiter                                              │
└─────────────────────────────────────────────────────────────────┘
```

### Message format

Every message sent **to** the engine is a single JSON line:

```json
{ "type": "CommandName", "data": { ...payload... } }
```

The engine replies on **stdout** with plain-text log lines (displayed in the UI console).
There is no request/response pairing — commands are fire-and-forget; the audio thread
applies them on the next block boundary.

### Full command reference

| `type` | Direction | Payload fields |
|---|---|---|
| `Play` | UI → Engine | — |
| `Stop` | UI → Engine | — |
| `Pause` | UI → Engine | — |
| `Seek` | UI → Engine | `samplePosition: number` |
| `AddTrack` | UI → Engine | `trackId, name, synthType, numVoices` |
| `ClearTrack` | UI → Engine | `trackId` |
| `AddNote` | UI → Engine | `trackId, startBeat, durationBeats, midiNote, velocity, bpm, sampleRate` |
| `RebuildTimeline` | UI → Engine | — |
| `SetTrackVolume` | UI → Engine | `trackId, value` (0–1) |
| `SetTrackMute` | UI → Engine | `trackId, value` (0 or 1) |
| `SetTrackSolo` | UI → Engine | `trackId, value` (0 or 1) |
| `SetADSR` | UI → Engine | `trackId, attack, decay, sustain, release` (seconds) |
| `SetSynthType` | UI → Engine | `trackId, synthType, numVoices, sampleRate` |
| `SetVoiceCount` | UI → Engine | `trackId, numVoices` |
| `LoadSample` | UI → Engine | `trackId, filePath, rootNote, oneShot` |
| `SetTrackEffect` | UI → Engine | `trackId, effectType, enabled, mix, [roomSize, damping, delayMs, feedback, drive …]` |
| `RemoveTrackEffect` | UI → Engine | `trackId, effectType` |
| `SetEffectParam` | UI → Engine | `trackId, effectType, paramName, value` |
| `SetAutomationLane` | UI → Engine | `trackId, paramName, points:[{beat,value}], bpm, sampleRate` |
| `ClearAutomationLane` | UI → Engine | `trackId, paramName` |
| `SetTimestamp` | UI → Engine | `samples` |

### Thread safety

The command queue is the **only** shared data structure between the main thread (JSON reader)
and the audio thread (render loop).  All other engine state is owned exclusively by the
audio thread and mutated only inside `processCommands()` at the top of each block, so no
locks are needed on the hot path.

```
Main thread          Audio thread
──────────           ────────────
parse JSON           processNextBlock()
push Command    ───▶  processCommands()   (pop + apply)
                      processEventsForBlock()
                      track->processBlock() × N
                      masterLimiter
                      pa_simple_write()
```
