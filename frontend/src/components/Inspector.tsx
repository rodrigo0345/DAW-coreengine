import { useState, useCallback } from 'react';
import { useStore } from '../store';
import './Inspector.css';

const SYNTH_TYPES = ['Sine', 'Square', 'Sawtooth', 'PWM'];

// ─── ADSR state per track (local until sent to engine) ──────────────────────
interface ADSRState {
  attack: number;   // seconds
  decay: number;    // seconds
  sustain: number;  // 0–1
  release: number;  // seconds
}

const DEFAULT_ADSR: ADSRState = { attack: 0.005, decay: 0.05, sustain: 0.7, release: 0.05 };

export default function Inspector() {
  const { tracks, selectedTrack, updateTrack, bpm, sampleRate } = useStore();
  const track = tracks.find((t) => t.id === selectedTrack);

  // Per-track ADSR (keyed by trackId)
  const [adsrMap, setAdsrMap] = useState<Record<number, ADSRState>>({});

  const adsr = track ? (adsrMap[track.id] ?? DEFAULT_ADSR) : DEFAULT_ADSR;

  // ── Send ADSR to engine (debounced on mouseUp) ────────────────────────────
  const sendADSR = useCallback(
    (params: ADSRState) => {
      if (!track) return;
      window.electronAPI?.setADSR({
        trackId: track.id,
        attack: params.attack,
        decay: params.decay,
        sustain: params.sustain,
        release: params.release,
      });
    },
    [track],
  );

  const updateADSR = (key: keyof ADSRState, value: number) => {
    if (!track) return;
    const next = { ...adsr, [key]: value };
    setAdsrMap((m) => ({ ...m, [track.id]: next }));
  };

  // ── Synth type change → re-create track in engine ─────────────────────────
  const handleSynthTypeChange = async (newSynthType: number) => {
    if (!track) return;
    updateTrack(track.id, { synthType: newSynthType });

    if (window.electronAPI) {
      await window.electronAPI.addTrack({
        trackId: track.id,
        name: track.name,
        synthType: newSynthType,
        numVoices: 8,
      });
      // Re-apply ADSR to the fresh instrument
      sendADSR(adsr);
      await window.electronAPI.rebuildTimeline();
    }
  };

  // ── Volume change → send to engine ─────────────────────────────────────────
  const handleVolumeChange = (v: number) => {
    if (!track) return;
    updateTrack(track.id, { volume: v });
    window.electronAPI?.setTrackVolume({ trackId: track.id, value: v });
  };

  // ── Mute / Solo → send to engine ──────────────────────────────────────────
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

  // ── Empty ──────────────────────────────────────────────────────────────────
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

  return (
    <div className="inspector">
      {/* ── Track settings ──────────────────────────────────────────────── */}
      <div className="inspector-section">
        <h4>Track</h4>

        <div className="ctrl">
          <label>Name</label>
          <input
            type="text"
            value={track.name}
            onChange={(e) => updateTrack(track.id, { name: e.target.value })}
          />
        </div>

        <div className="ctrl">
          <label>Synth</label>
          <select
            value={track.synthType}
            onChange={(e) => handleSynthTypeChange(Number(e.target.value))}
          >
            {SYNTH_TYPES.map((t, i) => (
              <option key={i} value={i}>{t}</option>
            ))}
          </select>
        </div>

        <div className="ctrl">
          <label>Volume</label>
          <input
            type="range" min={0} max={1} step={0.01}
            value={track.volume}
            onChange={(e) => handleVolumeChange(Number(e.target.value))}
          />
          <span className="ctrl-val">{Math.round(track.volume * 100)}%</span>
        </div>

        <div className="ctrl-row">
          <button className={`toggle-btn ${track.muted ? 'on' : ''}`} onClick={handleMuteToggle}>
            M
          </button>
          <button className={`toggle-btn ${track.solo ? 'on' : ''}`} onClick={handleSoloToggle}>
            S
          </button>
        </div>
      </div>

      {/* ── ADSR Envelope ──────────────────────────────────────────────── */}
      <div className="inspector-section">
        <h4>ADSR Envelope</h4>

        <div className="ctrl">
          <label>Attack</label>
          <input
            type="range" min={0} max={2} step={0.001}
            value={adsr.attack}
            onChange={(e) => updateADSR('attack', Number(e.target.value))}
            onMouseUp={() => sendADSR({ ...adsr })}
          />
          <span className="ctrl-val">{(adsr.attack * 1000).toFixed(0)}ms</span>
        </div>

        <div className="ctrl">
          <label>Decay</label>
          <input
            type="range" min={0} max={2} step={0.001}
            value={adsr.decay}
            onChange={(e) => updateADSR('decay', Number(e.target.value))}
            onMouseUp={() => sendADSR({ ...adsr })}
          />
          <span className="ctrl-val">{(adsr.decay * 1000).toFixed(0)}ms</span>
        </div>

        <div className="ctrl">
          <label>Sustain</label>
          <input
            type="range" min={0} max={1} step={0.01}
            value={adsr.sustain}
            onChange={(e) => updateADSR('sustain', Number(e.target.value))}
            onMouseUp={() => sendADSR({ ...adsr })}
          />
          <span className="ctrl-val">{Math.round(adsr.sustain * 100)}%</span>
        </div>

        <div className="ctrl">
          <label>Release</label>
          <input
            type="range" min={0} max={5} step={0.001}
            value={adsr.release}
            onChange={(e) => updateADSR('release', Number(e.target.value))}
            onMouseUp={() => sendADSR({ ...adsr })}
          />
          <span className="ctrl-val">{(adsr.release * 1000).toFixed(0)}ms</span>
        </div>
      </div>

      {/* ── Modulation (prepared for future) ────────────────────────────── */}
      <div className="inspector-section">
        <h4>Modulation</h4>
        <div className="ctrl">
          <label>Pitch (semi)</label>
          <input type="range" min={-24} max={24} step={1} defaultValue={0} disabled />
          <span className="ctrl-val dim">0</span>
        </div>
        <div className="ctrl">
          <label>Pan</label>
          <input type="range" min={-1} max={1} step={0.01} defaultValue={0} disabled />
          <span className="ctrl-val dim">C</span>
        </div>
        <div className="ctrl">
          <label>Stereo</label>
          <select disabled><option>Mono</option><option>Stereo</option></select>
          <span className="ctrl-val dim">—</span>
        </div>
        <p className="hint">Coming soon</p>
      </div>

      {/* ── Effects (prepared for future) ────────────────────────────────── */}
      <div className="inspector-section">
        <h4>Effects</h4>
        <p className="hint">Reverb, Delay, Distortion — coming soon</p>
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
