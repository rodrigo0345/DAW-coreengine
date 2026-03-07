import { useStore } from '../store';

/**
 * Synchronizes a single track's notes to the audio engine.
 * This is necessary because the engine doesn't track note IDs, so moving/deleting
 * requires clearing the track and re-sending all valid notes.
 */
export async function syncTrackToEngine(trackId: number) {
  if (!window.electronAPI) return;

  const { notes, bpm, sampleRate } = useStore.getState();
  const trackNotes = notes.filter((n) => n.trackId === trackId);

  // 1. Clear track in engine
  await window.electronAPI.clearTrack({ trackId });

  // 2. Re-send all notes
  // We can do this in parallel, but sequential is safer for order if engine cared (it sorts anyway)
  const promises = trackNotes.map((note) =>
    window.electronAPI!.addNote({
      trackId: note.trackId,
      startBeat: note.startBeat,
      durationBeats: note.durationBeats,
      midiNote: note.midiNote,
      velocity: note.velocity,
      bpm,
      sampleRate,
    })
  );

  await Promise.all(promises);

  // 3. Rebuild timeline
  await window.electronAPI.rebuildTimeline();
}

