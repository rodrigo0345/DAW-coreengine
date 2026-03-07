import { useEffect, useRef } from 'react';
import { useStore } from '../store';
import { registerTrackScroller, unregisterTrackScroller, syncTrackScroll } from '../helpers/trackScroll';
import './TrackList.css';

const SYNTH_TYPES  = ['Sine', 'Square', 'Sawtooth', 'PWM'];
const TRACK_COLORS = ['#f44336','#e91e63','#9c27b0','#673ab7','#3f51b5','#2196f3','#03a9f4','#00bcd4'];
const AUTOMATION_H = 72; // must match Timeline.tsx

export default function TrackList() {
  const { tracks, selectedTrack, addTrack, updateTrack, setSelectedTrack, automationLanes } = useStore();
  const tracksRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    const el = tracksRef.current;
    if (!el) return;
    registerTrackScroller('tracklist', el);
    return () => unregisterTrackScroller('tracklist');
  }, []);

  const handleAddTrack = async () => {
    const newTrack = {
      id: tracks.length, name: `Track ${tracks.length + 1}`,
      synthType: 0, volume: 1.0, muted: false, solo: false,
      color: TRACK_COLORS[tracks.length % TRACK_COLORS.length],
    };
    addTrack(newTrack);
    if (window.electronAPI) {
      try {
        const result = await window.electronAPI.addTrack({
          trackId: newTrack.id, name: newTrack.name,
          synthType: newTrack.synthType, numVoices: 8,
        });
        if (!result.success) console.error('Failed to add track:', result.error);
      } catch (err) { console.error('Error adding track:', err); }
    }
  };

  return (
    <div className="track-list">
      <button className="add-track-btn" onClick={handleAddTrack}>+ Add Track</button>

      {/* Ruler height spacer — keeps rows aligned with Timeline ruler */}
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
                className={`track-item ${selectedTrack === track.id ? 'selected' : ''}`}
                onClick={() => setSelectedTrack(track.id)}
              >
                <div className="track-color" style={{ background: track.color }} />
                <div className="track-info">
                  <div className="track-name">{track.name}</div>
                  <div className="track-type">{SYNTH_TYPES[track.synthType]}</div>
                </div>
                <div className="track-controls">
                  <button
                    className={`track-btn ${track.muted ? 'active' : ''}`}
                    title="Mute"
                    onClick={e => { e.stopPropagation(); const n = !track.muted; updateTrack(track.id, { muted: n }); window.electronAPI?.setTrackMute({ trackId: track.id, value: n ? 1 : 0 }); }}
                  >M</button>
                  <button
                    className={`track-btn ${track.solo ? 'active' : ''}`}
                    title="Solo"
                    onClick={e => { e.stopPropagation(); const n = !track.solo; updateTrack(track.id, { solo: n }); window.electronAPI?.setTrackSolo({ trackId: track.id, value: n ? 1 : 0 }); }}
                  >S</button>
                </div>
              </div>

              {/* Automation lane labels — height must match Timeline lanes for scroll sync */}
              {trackLanes.map(lane => (
                <div
                  key={lane.id}
                  className="track-auto-spacer"
                  style={{ height: 24 + (lane.expanded ? AUTOMATION_H : 0) }}
                >
                  <span className="track-auto-spacer-label">
                    {lane.target.kind === 'volume'
                      ? '~ Volume'
                      : `~ ${lane.target.effectType} ${lane.target.paramName}`}
                  </span>
                </div>
              ))}
            </div>
          );
        })}
      </div>

      {tracks.length === 0 && (
        <div className="empty-state">
          <p>No tracks yet</p>
          <p className="hint">Click "Add Track" to get started</p>
        </div>
      )}
    </div>
  );
}
