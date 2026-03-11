import { useEffect, useRef, useState, useCallback } from 'react';
import { useStore } from '../store';
import { registerTrackScroller, unregisterTrackScroller, syncTrackScroll } from '../helpers/trackScroll';
import './TrackList.css';

const SYNTH_TYPES  = ['Sine', 'Square', 'Sawtooth', 'PWM', 'Sampler'];
const TRACK_COLORS = ['#f44336','#e91e63','#9c27b0','#673ab7','#3f51b5','#2196f3','#03a9f4','#00bcd4'];
const AUTOMATION_H = 72;

export default function TrackList() {
  const { tracks, selectedTrack, addTrack, updateTrack, toggleSolo, setSelectedTrack, automationLanes, setSampleFile, sampleRate } = useStore();
  const tracksRef = useRef<HTMLDivElement>(null);
  const [dragOverTrackId, setDragOverTrackId] = useState<number | null>(null);
  const [draggingOver, setDraggingOver]       = useState(false);
  // Track enter/leave nesting depth to avoid flicker
  const dragDepth = useRef(0);

  useEffect(() => {
    const el = tracksRef.current;
    if (!el) return;
    registerTrackScroller('tracklist', el);
    return () => unregisterTrackScroller('tracklist');
  }, []);

  // ── Create a brand-new sampler track from a file path ──────────────────────
  const createSamplerTrack = useCallback(async (filePath: string) => {
    const trackName = filePath.split('/').pop()?.replace(/\.[^.]+$/, '') ?? `Track ${tracks.length + 1}`;
    const newTrack = {
      id: tracks.length,
      name: trackName,
      synthType: 4,          // Sampler
      volume: 1.0,
      muted: false,
      solo: false,
      color: TRACK_COLORS[tracks.length % TRACK_COLORS.length],
      sampleFile: filePath,
      rootNote: 69,
      oneShot: true,
      voiceCount: 8,
      useMidi: false,
    };
    addTrack(newTrack);
    setSelectedTrack(newTrack.id);
    setSampleFile(newTrack.id, filePath, 69, true);

    if (window.electronAPI) {
      await window.electronAPI.addTrack({ trackId: newTrack.id, name: newTrack.name, synthType: 4, numVoices: 8 });
      window.electronAPI.loadSample?.({ trackId: newTrack.id, filePath, rootNote: 69, oneShot: true });
    }
  }, [tracks, addTrack, setSelectedTrack, setSampleFile]);

  const getFilePath = (e: React.DragEvent): string => {
    const custom = e.dataTransfer.getData('application/x-sample-path');
    if (custom) return custom;
    const uriList = e.dataTransfer.getData('text/uri-list');
    if (uriList) return uriList.replace(/^file:\/\//, '').split('\n')[0].trim();
    return e.dataTransfer.getData('text/plain').trim();
  };

  // ── Drop on an existing track (replace its sample) ────────────────────────
  const handleDropOnTrack = useCallback(async (e: React.DragEvent, trackId: number) => {
    e.preventDefault();
    e.stopPropagation();
    setDragOverTrackId(null);
    const filePath = getFilePath(e);
    if (!filePath) return;
    const track = tracks.find(t => t.id === trackId);
    if (!track) return;

    if (track.synthType !== 4) {
      updateTrack(trackId, { synthType: 4 });
      await window.electronAPI?.setSynthType({ trackId, synthType: 4, numVoices: track.voiceCount ?? 8, sampleRate });
    }
    const name = filePath.split('/').pop()?.replace(/\.[^.]+$/, '') ?? track.name;
    setSampleFile(trackId, filePath, track.rootNote ?? 69, track.oneShot ?? true);
    updateTrack(trackId, { name, sampleFile: filePath });
    window.electronAPI?.loadSample?.({ trackId, filePath, rootNote: track.rootNote ?? 69, oneShot: track.oneShot ?? true });
    setSelectedTrack(trackId);
  }, [tracks, updateTrack, setSampleFile, setSelectedTrack]);

  // ── Drop on the empty zone → create new track ─────────────────────────────
  const handleDropNew = useCallback(async (e: React.DragEvent) => {
    e.preventDefault();
    dragDepth.current = 0;
    setDraggingOver(false);
    const filePath = getFilePath(e);
    if (!filePath) return;
    await createSamplerTrack(filePath);
  }, [createSamplerTrack]);

  const handleAddTrack = async () => {
    const newTrack = {
      id: tracks.length, name: `Track ${tracks.length + 1}`,
      synthType: 0, volume: 1.0, muted: false, solo: false,
      color: TRACK_COLORS[tracks.length % TRACK_COLORS.length],
    };
    addTrack(newTrack);
    if (window.electronAPI) {
      await window.electronAPI.addTrack({ trackId: newTrack.id, name: newTrack.name, synthType: 0, numVoices: 8 });
    }
  };

  return (
    <div
      className={`track-list ${draggingOver ? 'drop-active' : ''}`}
      onDragEnter={e => {
        // Only react to sample drags
        if (!e.dataTransfer.types.includes('application/x-sample-path') &&
            !e.dataTransfer.types.includes('text/plain')) return;
        dragDepth.current++;
        setDraggingOver(true);
      }}
      onDragLeave={() => {
        dragDepth.current--;
        if (dragDepth.current <= 0) { dragDepth.current = 0; setDraggingOver(false); }
      }}
      onDragOver={e => { e.preventDefault(); e.dataTransfer.dropEffect = 'copy'; }}
      onDrop={handleDropNew}
    >
      <button className="add-track-btn" onClick={handleAddTrack}>+ Add Track</button>
      <div className="track-ruler-spacer" />

      <div
        className="tracks"
        ref={tracksRef}
        onScroll={e => syncTrackScroll('tracklist', (e.currentTarget as HTMLDivElement).scrollTop)}
      >
        {tracks.map((track) => {
          const trackLanes = automationLanes.filter(l => l.trackId === track.id);
          return (
            <div key={track.id}>
              <div
                className={`track-item ${selectedTrack === track.id ? 'selected' : ''} ${dragOverTrackId === track.id ? 'drag-over' : ''}`}
                onClick={() => setSelectedTrack(track.id)}
                onDragOver={e => { e.preventDefault(); e.stopPropagation(); e.dataTransfer.dropEffect = 'copy'; setDragOverTrackId(track.id); }}
                onDragLeave={e => { e.stopPropagation(); setDragOverTrackId(null); }}
                onDrop={e => handleDropOnTrack(e, track.id)}
              >
                <div className="track-color" style={{ background: track.color }} />
                <div className="track-info">
                  <div className="track-name">{track.name}</div>
                  <div className="track-type">{SYNTH_TYPES[track.synthType] ?? 'Sine'}</div>
                </div>
                <div className="track-controls">
                  <button className={`track-btn ${track.muted ? 'active' : ''}`} title="Mute"
                    onClick={e => { e.stopPropagation(); const n = !track.muted; updateTrack(track.id, { muted: n }); window.electronAPI?.setTrackMute({ trackId: track.id, value: n ? 1 : 0 }); }}>M</button>
                  <button className={`track-btn ${track.solo ? 'active' : ''}`} title="Solo"
                    onClick={e => { e.stopPropagation(); toggleSolo(track.id); }}>S</button>
                </div>
              </div>
              {trackLanes.map(lane => (
                <div key={lane.id} className="track-auto-spacer"
                  style={{ height: 24 + (lane.expanded ? AUTOMATION_H : 0) }}>
                  <span className="track-auto-spacer-label">
                    {lane.target.kind === 'volume' ? '~ Volume' : `~ ${lane.target.effectType} ${lane.target.paramName}`}
                  </span>
                </div>
              ))}
            </div>
          );
        })}
      </div>

      {/* Drop zone — visible when dragging, always at the bottom */}
      <div className={`track-drop-zone ${draggingOver ? 'visible' : ''}`}>
        <span>＋ Drop sample to create track</span>
      </div>

      {tracks.length === 0 && !draggingOver && (
        <div className="empty-state">
          <p>No tracks yet</p>
          <p className="hint">Click "Add Track" or drop a sample here</p>
        </div>
      )}
    </div>
  );
}
