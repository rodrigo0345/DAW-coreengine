import { useEffect, useState } from 'react';
import { useStore } from '../store';
import { syncTrackToEngine } from '../helpers/engine';
import './TransportBar.css';

const fmtTime = (samples: number, sr = 44100) => {
  const s = samples / sr;
  const m = Math.floor(s / 60);
  const sec = Math.floor(s % 60);
  const ms = Math.floor((s % 1) * 100);
  return `${m}:${sec.toString().padStart(2, '0')}.${ms.toString().padStart(2, '0')}`;
};

export default function TransportBar() {
  const { isPlaying, currentPosition, bpm, sampleRate, setBpm, setIsPlaying, setCurrentPosition, tracks } = useStore();
  const [hasEngine, setHasEngine] = useState(false);

  useEffect(() => { setHasEngine(!!window.electronAPI); }, []);

  const play = async () => {
    const res = await window.electronAPI?.play();
    if (res?.success) setIsPlaying(true);
  };
  const stop = async () => {
    const res = await window.electronAPI?.stop();
    if (res?.success) { setIsPlaying(false); setCurrentPosition(0); }
  };
  const pause = async () => {
    const res = await window.electronAPI?.pause();
    if (res?.success) setIsPlaying(false);
  };
  const updateBpm = async (newBpm: number) => {
    const val = Math.max(40, Math.min(300, newBpm));
    setBpm(val);

    // Sync all tracks to engine with new BPM
    for (const t of useStore.getState().tracks) {
      await syncTrackToEngine(t.id);
    }
  };

  return (
    <div className="transport-bar">
      <div className="transport-controls">
        <button className="transport-btn" onClick={stop} disabled={!hasEngine} title="Stop">⏹</button>
        {!isPlaying
          ? <button className="transport-btn play" onClick={play} disabled={!hasEngine} title="Play (Space)">▶</button>
          : <button className="transport-btn pause" onClick={pause} disabled={!hasEngine} title="Pause (Space)">⏸</button>
        }
        <button className="transport-btn" disabled title="Record">⏺</button>
      </div>

      <div className="transport-info">
        <div className="transport-field">
          <span className="transport-label">Pos</span>
          <span className="transport-value">{fmtTime(currentPosition, sampleRate)}</span>
        </div>
        <div className="transport-field">
          <span className="transport-label">BPM</span>
          <input
            type="number"
            className="bpm-input"
            value={bpm}
            onChange={(e) => updateBpm(Number(e.target.value))}
            onBlur={(e) => { if (!e.target.value || Number(e.target.value) < 40) updateBpm(120); }}
            min={40} max={300} step={1}
          />
          <div className="bpm-btns">
            <button onClick={() => updateBpm(bpm - 1)}>−</button>
            <button onClick={() => updateBpm(bpm + 1)}>+</button>
          </div>
        </div>
        {!hasEngine && <span className="transport-warn">⚠ No engine</span>}
      </div>
    </div>
  );
}
