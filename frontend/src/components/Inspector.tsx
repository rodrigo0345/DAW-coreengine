import { useState, useCallback } from 'react';
import { useStore } from '../store';
import './Inspector.css';
const SYNTH_TYPES = ['Sine', 'Square', 'Sawtooth', 'PWM'];

interface ADSRState {
  attack: number;
  decay: number;
  sustain: number;
  release: number;
}
const DEFAULT_ADSR: ADSRState = { attack: 0.005, decay: 0.05, sustain: 0.7, release: 0.05 };

interface ReverbParams { mix: number; roomSize: number; damping: number }
interface DelayParams  { mix: number; delayMs: number; feedback: number; damping: number }
interface DistParams   { mix: number; drive: number }

type EffectType = 'Reverb' | 'Delay' | 'Distortion';

interface EffectState {
  enabled: boolean;
  reverb:  ReverbParams;
  delay:   DelayParams;
  distortion: DistParams;
}

const DEFAULT_EFFECTS: EffectState = {
  enabled: false,
  reverb:  { mix: 0.3, roomSize: 0.5, damping: 0.5 },
  delay:   { mix: 0.3, delayMs: 300, feedback: 0.4, damping: 0.3 },
  distortion: { mix: 1.0, drive: 2.0 },
};


// ─── Knob component ───────────────────────────────────────────────────────────
interface KnobProps {
  label: string; value: number; min: number; max: number; step?: number;
  format?: (v: number) => string;
  onChange: (v: number) => void;
  onCommit?: (v: number) => void;
}
function Knob({ label, value, min, max, step = 0.01, format, onChange, onCommit }: KnobProps) {
  const pct = (value - min) / (max - min);
  const angle = -140 + pct * 280;   // -140° to +140°
  const r = 22, cx = 28, cy = 28;
  // Arc path
  const startAngle = (-140 - 90) * (Math.PI / 180);
  const endAngle   = (angle - 90) * (Math.PI / 180);
  const x1 = cx + r * Math.cos(startAngle);
  const y1 = cy + r * Math.sin(startAngle);
  const x2 = cx + r * Math.cos(endAngle);
  const y2 = cy + r * Math.sin(endAngle);
  const largeArc = pct > 0.5 ? 1 : 0;

  return (
    <div className="knob-wrap">
      <svg width={56} height={56} className="knob-svg"
        onMouseDown={e => {
          const startY = e.clientY;
          const startV = value;
          const range  = max - min;
          const onMove = (mv: MouseEvent) => {
            const dy = startY - mv.clientY; // drag up = increase
            const newVal = Math.min(max, Math.max(min,
              startV + (dy / 120) * range
            ));
            const snapped = step > 0
              ? Math.round(newVal / step) * step
              : newVal;
            onChange(parseFloat(snapped.toFixed(6)));
          };
          const onUp = (uv: MouseEvent) => {
            window.removeEventListener('mousemove', onMove);
            window.removeEventListener('mouseup', onUp);
            const dy = startY - uv.clientY;
            const newVal = Math.min(max, Math.max(min,
              startV + (dy / 120) * range
            ));
            const snapped = step > 0
              ? Math.round(newVal / step) * step
              : newVal;
            onCommit?.(parseFloat(snapped.toFixed(6)));
          };
          window.addEventListener('mousemove', onMove);
          window.addEventListener('mouseup', onUp);
        }}
      >
        {/* Track ring */}
        <circle cx={cx} cy={cy} r={r} fill="none" stroke="#2a2a2a" strokeWidth={5} />
        {/* Value arc */}
        {pct > 0 && (
          <path
            d={`M ${x1} ${y1} A ${r} ${r} 0 ${largeArc} 1 ${x2} ${y2}`}
            fill="none" stroke="var(--accent, #4fa3e0)" strokeWidth={5} strokeLinecap="round"
          />
        )}
        {/* Pointer dot */}
        <circle cx={x2} cy={y2} r={3} fill="#fff" />
      </svg>
      <div className="knob-val">{format ? format(value) : value.toFixed(2)}</div>
      <div className="knob-label">{label}</div>
    </div>
  );
}

// ─── Single Effect Panel ──────────────────────────────────────────────────────
interface EffectPanelProps {
  type: EffectType;
  trackId: number;
  state: EffectState;
  onToggle: (type: EffectType, enabled: boolean) => void;
  onParamChange: (type: EffectType, paramName: string, value: number) => void;
  onParamCommit: (type: EffectType, paramName: string, value: number) => void;
}
function EffectPanel({ type, state, onToggle, onParamChange, onParamCommit }: EffectPanelProps) {
  const active = state.enabled;

  return (
    <div className={`fx-panel ${active ? 'active' : ''}`}>
      <div className="fx-header">
        <span className="fx-name">{type}</span>
        <label className="fx-toggle">
          <input type="checkbox" checked={active}
            onChange={e => onToggle(type, e.target.checked)} />
          <span className="fx-toggle-track" />
        </label>
      </div>
      {active && (
        <div className="fx-knobs">
          {type === 'Reverb' && (
            <>
              <Knob label="Mix"  value={state.reverb.mix}  min={0} max={1}
                format={v => `${Math.round(v*100)}%`}
                onChange={v => onParamChange('Reverb','mix',v)}
                onCommit={v => onParamCommit('Reverb','mix',v)} />
              <Knob label="Size" value={state.reverb.roomSize} min={0} max={1}
                format={v => v.toFixed(2)}
                onChange={v => onParamChange('Reverb','roomSize',v)}
                onCommit={v => onParamCommit('Reverb','roomSize',v)} />
              <Knob label="Damp" value={state.reverb.damping} min={0} max={1}
                format={v => v.toFixed(2)}
                onChange={v => onParamChange('Reverb','damping',v)}
                onCommit={v => onParamCommit('Reverb','damping',v)} />
            </>
          )}
          {type === 'Delay' && (
            <>
              <Knob label="Mix"  value={state.delay.mix} min={0} max={1}
                format={v => `${Math.round(v*100)}%`}
                onChange={v => onParamChange('Delay','mix',v)}
                onCommit={v => onParamCommit('Delay','mix',v)} />
              <Knob label="Time" value={state.delay.delayMs} min={10} max={2000} step={1}
                format={v => `${Math.round(v)}ms`}
                onChange={v => onParamChange('Delay','delayMs',v)}
                onCommit={v => onParamCommit('Delay','delayMs',v)} />
              <Knob label="Fdbk" value={state.delay.feedback} min={0} max={0.95}
                format={v => `${Math.round(v*100)}%`}
                onChange={v => onParamChange('Delay','feedback',v)}
                onCommit={v => onParamCommit('Delay','feedback',v)} />
              <Knob label="Damp" value={state.delay.damping} min={0} max={1}
                format={v => v.toFixed(2)}
                onChange={v => onParamChange('Delay','damping',v)}
                onCommit={v => onParamCommit('Delay','damping',v)} />
            </>
          )}
          {type === 'Distortion' && (
            <>
              <Knob label="Mix"   value={state.distortion.mix}   min={0} max={1}
                format={v => `${Math.round(v*100)}%`}
                onChange={v => onParamChange('Distortion','mix',v)}
                onCommit={v => onParamCommit('Distortion','mix',v)} />
              <Knob label="Drive" value={state.distortion.drive} min={0.1} max={10} step={0.1}
                format={v => v.toFixed(1)}
                onChange={v => onParamChange('Distortion','drive',v)}
                onCommit={v => onParamCommit('Distortion','drive',v)} />
            </>
          )}
        </div>
      )}
    </div>
  );
}

// ─── Inspector ────────────────────────────────────────────────────────────────
export default function Inspector() {
  const { tracks, selectedTrack, updateTrack, bpm, sampleRate } = useStore();
  const track = tracks.find(t => t.id === selectedTrack);

  const [adsrMap,    setAdsrMap]    = useState<Record<number, ADSRState>>({});
  const [effectsMap, setEffectsMap] = useState<Record<number, Record<EffectType, EffectState>>>({});

  const adsr    = track ? (adsrMap[track.id]    ?? DEFAULT_ADSR) : DEFAULT_ADSR;
  const effects = track
    ? (effectsMap[track.id] ?? {} as Record<EffectType, EffectState>)
    : {} as Record<EffectType, EffectState>;

  const getEffectState = (type: EffectType): EffectState =>
    effects[type] ?? { ...DEFAULT_EFFECTS };

  // ── Send ADSR ─────────────────────────────────────────────────────────────
  const sendADSR = useCallback((params: ADSRState) => {
    if (!track) return;
    window.electronAPI?.setADSR({
      trackId: track.id, ...params,
    });
  }, [track]);

  const updateADSR = (key: keyof ADSRState, value: number) => {
    if (!track) return;
    const next = { ...adsr, [key]: value };
    setAdsrMap(m => ({ ...m, [track.id]: next }));
  };

  // ── Effect toggle ─────────────────────────────────────────────────────────
  const handleEffectToggle = useCallback((type: EffectType, enabled: boolean) => {
    if (!track) return;

    setEffectsMap(m => {
      const existing: EffectState = m[track.id]?.[type] ?? { ...DEFAULT_EFFECTS };
      const next: EffectState = { ...existing, enabled };
      const updated = {
        ...m,
        [track.id]: { ...(m[track.id] ?? {}), [type]: next },
      };

      // Send to engine immediately using the resolved next state
      if (enabled) {
        const p = next;
        window.electronAPI?.setTrackEffect({
          trackId:    track.id,
          effectType: type,
          enabled:    true,
          ...(type === 'Reverb'     && { mix: p.reverb.mix,      roomSize: p.reverb.roomSize, damping: p.reverb.damping }),
          ...(type === 'Delay'      && { mix: p.delay.mix,       delayMs: p.delay.delayMs, feedback: p.delay.feedback, delayDamping: p.delay.damping }),
          ...(type === 'Distortion' && { mix: p.distortion.mix,  drive: p.distortion.drive }),
        });
      } else {
        window.electronAPI?.removeTrackEffect({ trackId: track.id, effectType: type });
      }

      return updated;
    });
  }, [track]);

  // ── Effect param change (live preview — updates state only) ──────────────
  const handleParamChange = useCallback((type: EffectType, paramName: string, value: number) => {
    if (!track) return;
    setEffectsMap(m => {
      const existing: EffectState = m[track.id]?.[type] ?? { ...DEFAULT_EFFECTS };
      let next: EffectState;
      if (type === 'Reverb')      next = { ...existing, reverb:      { ...existing.reverb,      [paramName]: value } };
      else if (type === 'Delay')  next = { ...existing, delay:       { ...existing.delay,       [paramName]: value } };
      else                        next = { ...existing, distortion:  { ...existing.distortion,  [paramName]: value } };
      return { ...m, [track.id]: { ...(m[track.id] ?? {}), [type]: next } };
    });
  }, [track]);

  // ── Effect param commit (send full effect state to engine on mouse-up) ────
  const handleParamCommit = useCallback((type: EffectType, paramName: string, value: number) => {
    if (!track) return;
    // Re-read fresh state via a functional setter read-trick, then send
    setEffectsMap(m => {
      const existing: EffectState = m[track.id]?.[type] ?? { ...DEFAULT_EFFECTS };
      // Apply the committed value
      let updated: EffectState;
      if (type === 'Reverb')      updated = { ...existing, reverb:      { ...existing.reverb,      [paramName]: value } };
      else if (type === 'Delay')  updated = { ...existing, delay:       { ...existing.delay,       [paramName]: value } };
      else                        updated = { ...existing, distortion:  { ...existing.distortion,  [paramName]: value } };

      if (updated.enabled) {
        // Resend full effect config so engine is always in sync
        const p = updated;
        window.electronAPI?.setTrackEffect({
          trackId:    track.id,
          effectType: type,
          enabled:    true,
          ...(type === 'Reverb'     && { mix: p.reverb.mix,      roomSize: p.reverb.roomSize,  damping: p.reverb.damping }),
          ...(type === 'Delay'      && { mix: p.delay.mix,       delayMs:  p.delay.delayMs,    feedback: p.delay.feedback, delayDamping: p.delay.damping }),
          ...(type === 'Distortion' && { mix: p.distortion.mix,  drive:    p.distortion.drive }),
        });
      }

      return { ...m, [track.id]: { ...(m[track.id] ?? {}), [type]: updated } };
    });
  }, [track]);

  // ── Synth type ────────────────────────────────────────────────────────────
  const handleSynthTypeChange = async (newSynthType: number) => {
    if (!track) return;
    updateTrack(track.id, { synthType: newSynthType });
    if (window.electronAPI) {
      await window.electronAPI.addTrack({ trackId: track.id, name: track.name, synthType: newSynthType, numVoices: 8 });
      sendADSR(adsr);
      // Re-apply active effects using current state (not closure)
      const currentEffects = effectsMap[track.id] ?? {} as Record<EffectType, EffectState>;
      (['Reverb', 'Delay', 'Distortion'] as EffectType[]).forEach(type => {
        const s = currentEffects[type];
        if (s?.enabled) {
          window.electronAPI?.setTrackEffect({
            trackId: track.id, effectType: type, enabled: true,
            ...(type === 'Reverb'     && { mix: s.reverb.mix,      roomSize: s.reverb.roomSize, damping: s.reverb.damping }),
            ...(type === 'Delay'      && { mix: s.delay.mix,       delayMs: s.delay.delayMs, feedback: s.delay.feedback, delayDamping: s.delay.damping }),
            ...(type === 'Distortion' && { mix: s.distortion.mix,  drive: s.distortion.drive }),
          });
        }
      });
      await window.electronAPI.rebuildTimeline();
    }
  };

  const handleVolumeChange = (v: number) => {
    if (!track) return;
    updateTrack(track.id, { volume: v });
    window.electronAPI?.setTrackVolume({ trackId: track.id, value: v });
  };
  const handleMuteToggle = () => {
    if (!track) return;
    const next = !track.muted;
    updateTrack(track.id, { muted: next });
    window.electronAPI?.setTrackMute({ trackId: track.id, value: next ? 1 : 0 });
  };
  const handleSoloToggle = () => {
    if (!track) return;
    const next = !track.solo;
    updateTrack(track.id, { solo: next });
    window.electronAPI?.setTrackSolo({ trackId: track.id, value: next ? 1 : 0 });
  };

  if (!track) {
    return (
      <div className="inspector">
        <div className="inspector-empty">
          <p>No track selected</p>
          <p className="hint">Select a track to edit</p>
        </div>
      </div>
    );
  }

  const EFFECTS: EffectType[] = ['Reverb', 'Delay', 'Distortion'];

  return (
    <div className="inspector">
      {/* ── Track ─────────────────────────────────────────────────────────── */}
      <div className="inspector-section">
        <h4>Track</h4>
        <div className="ctrl">
          <label>Name</label>
          <input type="text" value={track.name}
            onChange={e => updateTrack(track.id, { name: e.target.value })} />
        </div>
        <div className="ctrl">
          <label>Synth</label>
          <select value={track.synthType}
            onChange={e => handleSynthTypeChange(Number(e.target.value))}>
            {SYNTH_TYPES.map((t, i) => <option key={i} value={i}>{t}</option>)}
          </select>
        </div>
        <div className="ctrl">
          <label>Volume</label>
          <input type="range" min={0} max={1} step={0.01} value={track.volume}
            onChange={e => handleVolumeChange(Number(e.target.value))} />
          <span className="ctrl-val">{Math.round(track.volume * 100)}%</span>
        </div>
        <div className="ctrl-row">
          <button className={`toggle-btn ${track.muted ? 'on' : ''}`} onClick={handleMuteToggle}>M</button>
          <button className={`toggle-btn ${track.solo  ? 'on' : ''}`} onClick={handleSoloToggle}>S</button>
        </div>
      </div>

      {/* ── ADSR ──────────────────────────────────────────────────────────── */}
      <div className="inspector-section">
        <h4>ADSR Envelope</h4>
        {(['attack','decay','sustain','release'] as (keyof ADSRState)[]).map(key => {
          const cfg = {
            attack:  { min: 0,    max: 2,   step: 0.001, fmt: (v:number)=>`${(v*1000).toFixed(0)}ms` },
            decay:   { min: 0,    max: 2,   step: 0.001, fmt: (v:number)=>`${(v*1000).toFixed(0)}ms` },
            sustain: { min: 0,    max: 1,   step: 0.01,  fmt: (v:number)=>`${Math.round(v*100)}%` },
            release: { min: 0,    max: 5,   step: 0.001, fmt: (v:number)=>`${(v*1000).toFixed(0)}ms` },
          }[key];
          return (
            <div className="ctrl" key={key}>
              <label>{key.charAt(0).toUpperCase() + key.slice(1)}</label>
              <input type="range" min={cfg.min} max={cfg.max} step={cfg.step}
                value={adsr[key]}
                onChange={e => updateADSR(key, Number(e.target.value))}
                onMouseUp={() => sendADSR({ ...adsr })}
              />
              <span className="ctrl-val">{cfg.fmt(adsr[key])}</span>
            </div>
          );
        })}
      </div>

      {/* ── Effects ───────────────────────────────────────────────────────── */}
      <div className="inspector-section">
        <h4>Effects</h4>
        <div className="fx-rack">
          {EFFECTS.map(type => (
            <EffectPanel
              key={type}
              type={type}
              trackId={track.id}
              state={getEffectState(type)}
              onToggle={handleEffectToggle}
              onParamChange={handleParamChange}
              onParamCommit={handleParamCommit}
            />
          ))}
        </div>
      </div>

      {/* ── Info ──────────────────────────────────────────────────────────── */}
      <div className="inspector-section">
        <h4>Info</h4>
        <div className="info-grid">
          <div className="info-item">
            <span className="info-label">BPM</span>
            <span className="info-val">{bpm}</span>
          </div>
          <div className="info-item">
            <span className="info-label">Sample Rate</span>
            <span className="info-val">{sampleRate} Hz</span>
          </div>
        </div>
      </div>
    </div>
  );
}
