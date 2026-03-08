import { useState, useCallback } from 'react';
import { useStore } from '../store';
import './Inspector.css';
const SYNTH_TYPES = ['Sine', 'Square', 'Sawtooth', 'PWM', 'Sampler'];
interface ADSRState { attack: number; decay: number; sustain: number; release: number; }
const DEFAULT_ADSR: ADSRState = { attack: 0.005, decay: 0.05, sustain: 0.7, release: 0.05 };
interface ReverbParams  { mix: number; roomSize: number; damping: number }
interface DelayParams   { mix: number; delayMs: number; feedback: number; damping: number }
interface DistParams    { mix: number; drive: number }
type EffectType = 'Reverb' | 'Delay' | 'Distortion';
interface EffectState {
  enabled: boolean;
  reverb:  ReverbParams;
  delay:   DelayParams;
  distortion: DistParams;
}
const DEFAULT_EFFECTS: EffectState = {
  enabled: false,
  reverb:      { mix: 0.3, roomSize: 0.5, damping: 0.5 },
  delay:       { mix: 0.3, delayMs: 300, feedback: 0.4, damping: 0.3 },
  distortion:  { mix: 1.0, drive: 2.0 },
};
// ─── Knob ─────────────────────────────────────────────────────────────────────
interface KnobProps {
  label: string; value: number; min: number; max: number; step?: number;
  format?: (v: number) => string;
  onChange: (v: number) => void;
  onCommit?: (v: number) => void;
}
function Knob({ label, value, min, max, step = 0.01, format, onChange, onCommit }: KnobProps) {
  const pct = (value - min) / (max - min);
  const angle = -140 + pct * 280;
  const r = 22, cx = 28, cy = 28;
  const startAngle = (-140 - 90) * (Math.PI / 180);
  const endAngle   = (angle  - 90) * (Math.PI / 180);
  const x1 = cx + r * Math.cos(startAngle);
  const y1 = cy + r * Math.sin(startAngle);
  const x2 = cx + r * Math.cos(endAngle);
  const y2 = cy + r * Math.sin(endAngle);
  const largeArc = pct > 0.5 ? 1 : 0;
  return (
    <div className="knob-wrap">
      <svg width={56} height={56} className="knob-svg"
        onMouseDown={e => {
          e.preventDefault();
          const startY = e.clientY, startV = value, range = max - min;
          const snap  = (v: number) => step > 0 ? parseFloat((Math.round(v / step) * step).toFixed(6)) : v;
          const clamp = (v: number) => Math.min(max, Math.max(min, v));
          const onMove = (mv: MouseEvent) => onChange(snap(clamp(startV + ((startY - mv.clientY) / 120) * range)));
          const onUp   = (uv: MouseEvent) => {
            window.removeEventListener('mousemove', onMove);
            window.removeEventListener('mouseup', onUp);
            onCommit?.(snap(clamp(startV + ((startY - uv.clientY) / 120) * range)));
          };
          window.addEventListener('mousemove', onMove);
          window.addEventListener('mouseup', onUp);
        }}
      >
        <circle cx={cx} cy={cy} r={r} fill="none" stroke="#2a2a2a" strokeWidth={5} />
        {pct > 0 && (
          <path d={`M ${x1} ${y1} A ${r} ${r} 0 ${largeArc} 1 ${x2} ${y2}`}
            fill="none" stroke="var(--accent,#4fa3e0)" strokeWidth={5} strokeLinecap="round" />
        )}
        <circle cx={x2} cy={y2} r={3} fill="#fff" />
      </svg>
      <div className="knob-val">{format ? format(value) : value.toFixed(2)}</div>
      <div className="knob-label">{label}</div>
    </div>
  );
}
// ─── Effect Panel ─────────────────────────────────────────────────────────────
interface EffectPanelProps {
  type: EffectType; trackId: number; state: EffectState;
  onToggle:      (type: EffectType, enabled: boolean)                 => void;
  onParamChange: (type: EffectType, paramName: string, value: number) => void;
  onParamCommit: (type: EffectType, paramName: string, value: number) => void;
}
function EffectPanel({ type, state, onToggle, onParamChange, onParamCommit }: EffectPanelProps) {
  return (
    <div className={`fx-panel ${state.enabled ? 'active' : ''}`}>
      <div className="fx-header">
        <span className="fx-name">{type}</span>
        <label className="fx-toggle">
          <input type="checkbox" checked={state.enabled}
            onChange={e => onToggle(type, e.target.checked)} />
          <span className="fx-toggle-track" />
        </label>
      </div>
      {state.enabled && (
        <div className="fx-knobs">
          {type === 'Reverb' && (<>
            <Knob label="Mix"  value={state.reverb.mix}      min={0} max={1}    format={v=>`${Math.round(v*100)}%`} onChange={v=>onParamChange('Reverb','mix',v)}      onCommit={v=>onParamCommit('Reverb','mix',v)} />
            <Knob label="Size" value={state.reverb.roomSize} min={0} max={1}    format={v=>v.toFixed(2)}           onChange={v=>onParamChange('Reverb','roomSize',v)} onCommit={v=>onParamCommit('Reverb','roomSize',v)} />
            <Knob label="Damp" value={state.reverb.damping}  min={0} max={1}    format={v=>v.toFixed(2)}           onChange={v=>onParamChange('Reverb','damping',v)}  onCommit={v=>onParamCommit('Reverb','damping',v)} />
          </>)}
          {type === 'Delay' && (<>
            <Knob label="Mix"  value={state.delay.mix}      min={0}  max={1}    format={v=>`${Math.round(v*100)}%`} onChange={v=>onParamChange('Delay','mix',v)}      onCommit={v=>onParamCommit('Delay','mix',v)} />
            <Knob label="Time" value={state.delay.delayMs}  min={10} max={2000} step={1} format={v=>`${Math.round(v)}ms`} onChange={v=>onParamChange('Delay','delayMs',v)} onCommit={v=>onParamCommit('Delay','delayMs',v)} />
            <Knob label="Fdbk" value={state.delay.feedback} min={0}  max={0.95} format={v=>`${Math.round(v*100)}%`} onChange={v=>onParamChange('Delay','feedback',v)} onCommit={v=>onParamCommit('Delay','feedback',v)} />
            <Knob label="Damp" value={state.delay.damping}  min={0}  max={1}    format={v=>v.toFixed(2)}           onChange={v=>onParamChange('Delay','damping',v)}  onCommit={v=>onParamCommit('Delay','damping',v)} />
          </>)}
          {type === 'Distortion' && (<>
            <Knob label="Mix"   value={state.distortion.mix}   min={0}   max={1}  format={v=>`${Math.round(v*100)}%`} onChange={v=>onParamChange('Distortion','mix',v)}   onCommit={v=>onParamCommit('Distortion','mix',v)} />
            <Knob label="Drive" value={state.distortion.drive} min={0.1} max={10} step={0.1} format={v=>v.toFixed(1)} onChange={v=>onParamChange('Distortion','drive',v)} onCommit={v=>onParamCommit('Distortion','drive',v)} />
          </>)}
        </div>
      )}
    </div>
  );
}
// ─── Helper ───────────────────────────────────────────────────────────────────
function midiNoteName(midi: number): string {
  const names = ['C','C#','D','D#','E','F','F#','G','G#','A','A#','B'];
  return `${names[midi % 12]}${Math.floor(midi / 12) - 1}`;
}
// ─── Inspector ────────────────────────────────────────────────────────────────
export default function Inspector() {
  const {
    tracks, selectedTrack, updateTrack, toggleSolo, bpm, sampleRate,
    setSampleFile, setTrackVoiceCount, setSynthTypeOnTrack,
  } = useStore();
  const track = tracks.find(t => t.id === selectedTrack);
  const [adsrMap,    setAdsrMap]    = useState<Record<number, ADSRState>>({});
  const [effectsMap, setEffectsMap] = useState<Record<number, Record<EffectType, EffectState>>>({});
  const adsr  = track ? (adsrMap[track.id]    ?? DEFAULT_ADSR) : DEFAULT_ADSR;
  const fxMap = track ? (effectsMap[track.id] ?? {})           : {};
  const getFx = (t: EffectType): EffectState => (fxMap as Record<string, EffectState>)[t] ?? { ...DEFAULT_EFFECTS };
  // ── ADSR ──────────────────────────────────────────────────────────────────
  const sendADSR = useCallback((params: ADSRState) => {
    if (!track) return;
    window.electronAPI?.setADSR({ trackId: track.id, ...params });
  }, [track]);
  const updateADSR = (key: keyof ADSRState, value: number) => {
    if (!track) return;
    setAdsrMap(m => ({ ...m, [track.id]: { ...adsr, [key]: value } }));
  };
  // ── Effects ───────────────────────────────────────────────────────────────
  const patchFx = useCallback((type: EffectType, patch: Partial<EffectState>) => {
    if (!track) return;
    setEffectsMap(m => ({
      ...m,
      [track.id]: { ...(m[track.id] ?? {}), [type]: { ...getFx(type), ...patch } },
    }));
  // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [track, fxMap]);
  const sendFx = useCallback((type: EffectType, s: EffectState) => {
    if (!track) return;
    if (!s.enabled) {
      window.electronAPI?.removeTrackEffect({ trackId: track.id, effectType: type });
      return;
    }
    window.electronAPI?.setTrackEffect({
      trackId: track.id, effectType: type, enabled: true,
      ...(type === 'Reverb'     && { mix: s.reverb.mix,      roomSize: s.reverb.roomSize, damping: s.reverb.damping }),
      ...(type === 'Delay'      && { mix: s.delay.mix,       delayMs: s.delay.delayMs,    feedback: s.delay.feedback, delayDamping: s.delay.damping }),
      ...(type === 'Distortion' && { mix: s.distortion.mix,  drive: s.distortion.drive }),
    });
  }, [track]);
  const handleEffectToggle = useCallback((type: EffectType, enabled: boolean) => {
    const next = { ...getFx(type), enabled };
    patchFx(type, { enabled });
    sendFx(type, next);
  // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [patchFx, sendFx, fxMap]);
  const handleParamChange = useCallback((type: EffectType, paramName: string, value: number) => {
    const s = getFx(type);
    if      (type === 'Reverb')     patchFx(type, { reverb:     { ...s.reverb,     [paramName]: value } });
    else if (type === 'Delay')      patchFx(type, { delay:      { ...s.delay,      [paramName]: value } });
    else                            patchFx(type, { distortion: { ...s.distortion, [paramName]: value } });
  // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [patchFx, fxMap]);
  const handleParamCommit = useCallback((type: EffectType, paramName: string, value: number) => {
    const s = getFx(type);
    let u: EffectState;
    if      (type === 'Reverb')     u = { ...s, reverb:     { ...s.reverb,     [paramName]: value } };
    else if (type === 'Delay')      u = { ...s, delay:      { ...s.delay,      [paramName]: value } };
    else                            u = { ...s, distortion: { ...s.distortion, [paramName]: value } };
    patchFx(type, u);
    if (u.enabled) sendFx(type, u);
  // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [patchFx, sendFx, fxMap]);
  // ── Synth type ────────────────────────────────────────────────────────────
  const handleSynthTypeChange = async (newType: number) => {
    if (!track) return;
    setSynthTypeOnTrack(track.id, newType);
    updateTrack(track.id, { synthType: newType });
    if (!window.electronAPI) return;
    await window.electronAPI.setSynthType({
      trackId: track.id, synthType: newType,
      numVoices: track.voiceCount ?? 8, sampleRate,
    });
    sendADSR(adsr);
    (['Reverb','Delay','Distortion'] as EffectType[]).forEach(t => { const s = getFx(t); if (s.enabled) sendFx(t, s); });
    await window.electronAPI.rebuildTimeline();
  };
  // ── Voice count ───────────────────────────────────────────────────────────
  const handleVoiceCount = (n: number) => {
    if (!track) return;
    const v = Math.max(1, Math.min(64, n));
    setTrackVoiceCount(track.id, v);
    window.electronAPI?.setVoiceCount?.({ trackId: track.id, numVoices: v });
  };
  // ── Sampler ───────────────────────────────────────────────────────────────
  const handleLoadSample = async () => {
    if (!track) return;
    const filePath = await window.electronAPI?.openAudioFile?.();
    if (!filePath) return;
    const rootNote = track.rootNote ?? 69;
    const oneShot  = track.oneShot  ?? true;
    setSampleFile(track.id, filePath, rootNote, oneShot);
    window.electronAPI?.loadSample?.({ trackId: track.id, filePath, rootNote, oneShot });
  };
  const handleRootNote = (n: number) => {
    if (!track?.sampleFile) return;
    updateTrack(track.id, { rootNote: n });
    window.electronAPI?.loadSample?.({ trackId: track.id, filePath: track.sampleFile, rootNote: n, oneShot: track.oneShot ?? true });
  };
  const handleOneShot = () => {
    if (!track?.sampleFile) return;
    const next = !(track.oneShot ?? true);
    updateTrack(track.id, { oneShot: next });
    window.electronAPI?.loadSample?.({ trackId: track.id, filePath: track.sampleFile, rootNote: track.rootNote ?? 69, oneShot: next });
  };
  // ── Volume / Mute / Solo ──────────────────────────────────────────────────
  const handleVolume = (v: number) => { if (!track) return; updateTrack(track.id, { volume: v }); window.electronAPI?.setTrackVolume({ trackId: track.id, value: v }); };
  const handleMute   = () => { if (!track) return; const n = !track.muted; updateTrack(track.id, { muted: n }); window.electronAPI?.setTrackMute({ trackId: track.id, value: n ? 1 : 0 }); };
  const handleSolo   = () => { if (!track) return; toggleSolo(track.id); };
  // ── Render ────────────────────────────────────────────────────────────────
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
  const isSamplerOneShot = track.synthType === 4 && (track.oneShot ?? true);
  return (
    <div className="inspector">
      {/* ── Track ─────────────────────────────────────────────────────────── */}
      <div className="inspector-section">
        <h4>Track</h4>
        <div className="ctrl">
          <label>Name</label>
          <input type="text" value={track.name} onChange={e => updateTrack(track.id, { name: e.target.value })} />
        </div>
        <div className="ctrl">
          <label>Synth</label>
          <select value={track.synthType} onChange={e => handleSynthTypeChange(Number(e.target.value))}>
            {SYNTH_TYPES.map((t, i) => <option key={i} value={i}>{t}</option>)}
          </select>
        </div>
        <div className="ctrl">
          <label>Voices</label>
          <input type="number" min={1} max={64} step={1}
            value={track.voiceCount ?? 8}
            onChange={e => handleVoiceCount(Number(e.target.value))}
            className="ctrl-number" />
          <span className="ctrl-val ctrl-hint">polyphony</span>
        </div>
        <div className="ctrl">
          <label>Volume</label>
          <input type="range" min={0} max={1} step={0.01} value={track.volume}
            onChange={e => handleVolume(Number(e.target.value))} />
          <span className="ctrl-val">{Math.round(track.volume * 100)}%</span>
        </div>
        <div className="ctrl-row">
          <button className={`toggle-btn ${track.muted ? 'on' : ''}`} onClick={handleMute}>M</button>
          <button className={`toggle-btn ${track.solo  ? 'on' : ''}`} onClick={handleSolo}>S</button>
        </div>
      </div>
      {/* ── Sampler ───────────────────────────────────────────────────────── */}
      {track.synthType === 4 && (
        <div className="inspector-section">
          <h4>Sampler</h4>
          <div className="ctrl ctrl-col">
            <label>Sample File</label>
            <div className="sample-row">
              <button className="sample-btn" onClick={handleLoadSample}>
                📂 {track.sampleFile ? 'Change…' : 'Load File…'}
              </button>
              {track.sampleFile && (
                <span className="sample-name" title={track.sampleFile}>
                  {track.sampleFile.split('/').pop()}
                </span>
              )}
            </div>
          </div>
          <div className="ctrl">
            <label>Root Note</label>
            <input type="number" min={0} max={127} step={1}
              value={track.rootNote ?? 69}
              onChange={e => handleRootNote(Number(e.target.value))}
              className="ctrl-number" />
            <span className="ctrl-val ctrl-hint">{midiNoteName(track.rootNote ?? 69)}</span>
          </div>
          <div className="ctrl ctrl-row">
            <label>One-Shot</label>
            <button className={`toggle-btn ${(track.oneShot ?? true) ? 'on' : ''}`}
              onClick={handleOneShot}
              title="Drum mode: play full sample on noteOn, ignore noteOff">
              {(track.oneShot ?? true) ? 'ON' : 'OFF'}
            </button>
            <span className="ctrl-hint" style={{ marginLeft: 6 }}>
              {(track.oneShot ?? true) ? 'Drum / one-shot' : 'Sustain (ADSR)'}
            </span>
          </div>
          <div className="ctrl ctrl-row">
            <label>Clip view</label>
            <button
              className={`toggle-btn ${(track.useMidi ?? false) ? 'on' : ''}`}
              onClick={() => updateTrack(track.id, { useMidi: !(track.useMidi ?? false) })}
              title="Toggle between audio waveform view and MIDI note view in the timeline"
            >
              {(track.useMidi ?? false) ? 'MIDI' : 'AUDIO'}
            </button>
            <span className="ctrl-hint" style={{ marginLeft: 6 }}>
              {(track.useMidi ?? false) ? 'Showing MIDI notes' : 'Showing audio clip'}
            </span>
          </div>
        </div>
      )}
      {/* ── ADSR ──────────────────────────────────────────────────────────── */}
      {!isSamplerOneShot && (
        <div className="inspector-section">
          <h4>ADSR Envelope</h4>
          {(['attack','decay','sustain','release'] as (keyof ADSRState)[]).map(key => {
            const cfg = {
              attack:  { min: 0, max: 2, step: 0.001, fmt: (v: number) => `${(v*1000).toFixed(0)}ms` },
              decay:   { min: 0, max: 2, step: 0.001, fmt: (v: number) => `${(v*1000).toFixed(0)}ms` },
              sustain: { min: 0, max: 1, step: 0.01,  fmt: (v: number) => `${Math.round(v*100)}%`    },
              release: { min: 0, max: 5, step: 0.001, fmt: (v: number) => `${(v*1000).toFixed(0)}ms` },
            }[key];
            return (
              <div className="ctrl" key={key}>
                <label>{key.charAt(0).toUpperCase() + key.slice(1)}</label>
                <input type="range" min={cfg.min} max={cfg.max} step={cfg.step}
                  value={adsr[key]}
                  onChange={e => updateADSR(key, Number(e.target.value))}
                  onMouseUp={() => sendADSR({ ...adsr })} />
                <span className="ctrl-val">{cfg.fmt(adsr[key])}</span>
              </div>
            );
          })}
        </div>
      )}
      {/* ── Effects ───────────────────────────────────────────────────────── */}
      <div className="inspector-section">
        <h4>Effects</h4>
        <div className="fx-rack">
          {EFFECTS.map(type => (
            <EffectPanel key={type} type={type} trackId={track.id}
              state={getFx(type)}
              onToggle={handleEffectToggle}
              onParamChange={handleParamChange}
              onParamCommit={handleParamCommit} />
          ))}
        </div>
      </div>
      {/* ── Info ──────────────────────────────────────────────────────────── */}
      <div className="inspector-section">
        <h4>Info</h4>
        <div className="info-grid">
          <div className="info-item"><span className="info-label">BPM</span><span className="info-val">{bpm}</span></div>
          <div className="info-item"><span className="info-label">Sample Rate</span><span className="info-val">{sampleRate.toLocaleString()} Hz</span></div>
          <div className="info-item"><span className="info-label">Voices</span><span className="info-val">{track.voiceCount ?? 8}</span></div>
        </div>
      </div>
    </div>
  );
}
