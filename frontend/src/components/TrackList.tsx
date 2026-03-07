import { useStore } from '../store';
import './TrackList.css';

const SYNTH_TYPES = ['Sine', 'Square', 'Sawtooth', 'PWM'];
const TRACK_COLORS = ['#f44336', '#e91e63', '#9c27b0', '#673ab7', '#3f51b5', '#2196f3', '#03a9f4', '#00bcd4'];

export default function TrackList() {
  const { tracks, selectedTrack, addTrack, updateTrack, setSelectedTrack } = useStore();

  const handleAddTrack = async () => {
    const newTrack = {
      id: tracks.length,
      name: `Track ${tracks.length + 1}`,
      synthType: 0,
      volume: 1.0,
      muted: false,
      solo: false,
      color: TRACK_COLORS[tracks.length % TRACK_COLORS.length]
    };

    // Add to local state first
    addTrack(newTrack);

    // Send to engine
    if (window.electronAPI) {
      try {
        const result = await window.electronAPI.addTrack({
          trackId: newTrack.id,
          name: newTrack.name,
          synthType: newTrack.synthType,
          numVoices: 8
        });

        if (!result.success) {
          console.error('Failed to add track to engine:', result.error);
          alert('Failed to add track to engine. Is the engine running?');
        } else {
          console.log('Track added successfully:', newTrack.name);
        }
      } catch (error) {
        console.error('Error adding track:', error);
        alert('Error communicating with engine');
      }
    } else {
      console.warn('Running in browser mode - track not sent to engine');
    }
  };

  return (
    <div className="track-list">
      <button className="add-track-btn" onClick={handleAddTrack}>
        + Add Track
      </button>

      <div className="tracks">
        {tracks.map((track) => (
          <div
            key={track.id}
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
                onClick={(e) => {
                  e.stopPropagation();
                  const next = !track.muted;
                  updateTrack(track.id, { muted: next });
                  window.electronAPI?.setTrackMute({ trackId: track.id, value: next ? 1 : 0 });
                }}
                title="Mute"
              >
                M
              </button>
              <button
                className={`track-btn ${track.solo ? 'active' : ''}`}
                onClick={(e) => {
                  e.stopPropagation();
                  const next = !track.solo;
                  updateTrack(track.id, { solo: next });
                  window.electronAPI?.setTrackSolo({ trackId: track.id, value: next ? 1 : 0 });
                }}
                title="Solo"
              >
                S
              </button>
            </div>
          </div>
        ))}
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

