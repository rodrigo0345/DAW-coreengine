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

