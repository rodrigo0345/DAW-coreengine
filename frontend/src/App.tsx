import { useState, useEffect, useRef, useCallback } from 'react';
import './App.css';
import TransportBar from './components/TransportBar';
import TrackList from './components/TrackList';
import Timeline from './components/Timeline';
import PianoRoll from './components/PianoRoll';
import Inspector from './components/Inspector';
import { useStore } from './store';
import { useKeyboardShortcuts } from './hooks/useKeyboardShortcuts';

// ─── Electron API type definitions ───────────────────────────────────────────
declare global {
  interface Window {
    electronAPI?: {
      // Engine lifecycle
      startEngine: () => Promise<any>;
      stopEngine: () => Promise<any>;
      // Track / timeline
      addTrack: (data: any) => Promise<any>;
      addNote: (data: any) => Promise<any>;
      rebuildTimeline: () => Promise<any>;
      clearTrack: (data: any) => Promise<any>;
      // Track controls
      setTrackVolume: (data: any) => Promise<any>;
      setTrackMute: (data: any) => Promise<any>;
      setTrackSolo: (data: any) => Promise<any>;
      // ADSR
      setADSR: (data: any) => Promise<any>;
      // Effects
      setTrackEffect:    (data: any) => Promise<any>;
      removeTrackEffect: (data: any) => Promise<any>;
      setEffectParam:    (data: any) => Promise<any>;
      // Automation
      setAutomationLane:   (data: any) => Promise<any>;
      clearAutomationLane: (data: any) => Promise<any>;
      // Playback
      play: () => Promise<any>;
      stop: () => Promise<any>;
      pause: () => Promise<any>;
      seek: (samplePosition: number) => Promise<any>;
      // Generic
      sendCommand: (cmd: string) => Promise<any>;
      // Events
      onEngineOutput: (callback: (data: string) => void) => void;
      onEngineError: (callback: (data: string) => void) => void;
      onEngineClosed: (callback: (code: number) => void) => void;
    };
  }
}

// ─── Resizable split constants ───────────────────────────────────────────────
const MIN_SPLIT = 120;          // px – minimum panel height
const DEFAULT_SPLIT_PCT = 0.4;  // 40% timeline, 60% piano roll

function App() {
  const [engineRunning, setEngineRunning] = useState(false);
  const [engineOutput, setEngineOutput] = useState<string[]>([]);
  const [showConsole, setShowConsole] = useState(false);
  const { setIsPlaying } = useStore();

  // ── Global keyboard shortcuts (FL Studio-style) ────────────────────────────
  useKeyboardShortcuts();

  // ── Resizable split between Timeline and PianoRoll ─────────────────────────
  const splitContainerRef = useRef<HTMLDivElement>(null);
  const [splitPx, setSplitPx] = useState<number | null>(null); // null → use %

  const onSplitDown = useCallback((e: React.MouseEvent) => {
    e.preventDefault();
    const startY = e.clientY;
    const startSplit = splitPx ?? (splitContainerRef.current
      ? splitContainerRef.current.clientHeight * DEFAULT_SPLIT_PCT
      : 300);

    const onMove = (ev: MouseEvent) => {
      const containerH = splitContainerRef.current?.clientHeight ?? 600;
      const raw = startSplit + (ev.clientY - startY);
      setSplitPx(Math.min(containerH - MIN_SPLIT, Math.max(MIN_SPLIT, raw)));
    };
    const onUp = () => {
      window.removeEventListener('mousemove', onMove);
      window.removeEventListener('mouseup', onUp);
    };
    window.addEventListener('mousemove', onMove);
    window.addEventListener('mouseup', onUp);
  }, [splitPx]);

  // ── Engine lifecycle ───────────────────────────────────────────────────────
  useEffect(() => {
    if (!window.electronAPI) return;

    window.electronAPI.onEngineOutput((data) => {
      setEngineOutput((prev) => [...prev.slice(-100), data].filter(Boolean));
      if (!engineRunning) setEngineRunning(true);
    });
    window.electronAPI.onEngineError((data) => {
      console.error('Engine error:', data);
      setEngineOutput((prev) => [...prev.slice(-100), `ERROR: ${data}`]);
    });
    window.electronAPI.onEngineClosed((code) => {
      setEngineRunning(false);
      setIsPlaying(false);
      setEngineOutput((prev) => [...prev, `Engine stopped (code ${code})`]);
    });

    handleStartEngine();
  }, []);

  const handleStartEngine = async () => {
    if (!window.electronAPI) return;
    try {
      const res = await window.electronAPI.startEngine();
      if (res.success) setEngineRunning(true);
      else console.error('Start failed:', res.error);
    } catch (err) {
      console.error('Start error:', err);
    }
  };

  const handleStopEngine = async () => {
    if (!window.electronAPI) return;
    const res = await window.electronAPI.stopEngine();
    if (res.success) { setEngineRunning(false); setIsPlaying(false); }
  };

  // ── Split height calculation ───────────────────────────────────────────────
  const topStyle = splitPx != null
    ? { height: splitPx, flex: 'none' as const }
    : { flex: `${DEFAULT_SPLIT_PCT}` };
  const botStyle = splitPx != null
    ? { flex: '1' }
    : { flex: `${1 - DEFAULT_SPLIT_PCT}` };

  // ── Render ─────────────────────────────────────────────────────────────────
  return (
    <div className="app">
      {/* ── Header ────────────────────────────────────────────────────────── */}
      <header className="app-header">
        <div className="header-left">
          <h1>🎵 DAW Core</h1>
          <span className="version">v1.0</span>
        </div>
        <div className="engine-status">
          <span className={`status-dot ${engineRunning ? 'on' : ''}`} />
          <span className="status-label">{engineRunning ? 'Running' : 'Stopped'}</span>
          {!engineRunning
            ? <button onClick={handleStartEngine} className="btn-sm btn-primary">Start</button>
            : <button onClick={handleStopEngine} className="btn-sm btn-danger">Stop</button>
          }
        </div>
      </header>

      {/* ── Body ──────────────────────────────────────────────────────────── */}
      <div className="app-body">
        <aside className="sidebar-left">
          <div className="sidebar-hdr"><h3>Tracks</h3></div>
          <TrackList />
        </aside>

        <main className="main-area">
          <TransportBar />

          {/* Split: Timeline (top) / PianoRoll (bottom) */}
          <div className="split-container" ref={splitContainerRef}>
            <div className="split-top" style={topStyle}>
              <Timeline />
            </div>
            <div className="split-handle" onMouseDown={onSplitDown} />
            <div className="split-bot" style={botStyle}>
              <PianoRoll />
            </div>
          </div>
        </main>

        <aside className="sidebar-right">
          <div className="sidebar-hdr"><h3>Inspector</h3></div>
          <Inspector />
        </aside>
      </div>

      {/* ── Console ───────────────────────────────────────────────────────── */}
      {engineOutput.length > 0 && (
        <div className={`console ${showConsole ? 'open' : ''}`}>
          <div className="console-bar">
            <span>Console</span>
            <div className="console-btns">
              <button onClick={() => setEngineOutput([])} className="btn-sm">Clear</button>
              <button onClick={() => setShowConsole(!showConsole)} className="btn-sm">
                {showConsole ? '▾' : '▸'}
              </button>
            </div>
          </div>
          {showConsole && (
            <div className="console-body">
              {engineOutput.map((line, i) => (
                <div key={i} className={line.startsWith('ERROR') ? 'err' : line.startsWith('OK') ? 'ok' : ''}>
                  {line}
                </div>
              ))}
            </div>
          )}
        </div>
      )}
    </div>
  );
}

export default App;

