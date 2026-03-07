import { useStore } from '../store';

/**
 * Synchronizes a single track's notes to the audio engine.
 * This is necessary because the engine doesn't track note IDs, so moving/deleting
 * requires clearing the track and re-sending all valid notes.
 */
export async function syncTrackToEngine(trackId: number) {
  if (!window.electronAPI) return;

  const { notes, clips, patterns, bpm, sampleRate } = useStore.getState();

  // 1. Legacy/Direct notes
  const directNotes = notes.filter((n) => n.trackId === trackId);

  // 2. Pattern notes via Clips
  const trackClips = clips.filter((c) => c.trackId === trackId);
  const patternNotes = trackClips.flatMap((clip) => {
    const pattern = patterns.find((p) => p.id === clip.patternId);
    if (!pattern || !pattern.notes.length) return [];

    const notesForClip = [];
    const loopLen = pattern.duration || 16;
    // Iterate loops
    for (let loopStart = 0; loopStart < clip.duration; loopStart += loopLen) {
      for (const pNote of pattern.notes) {
        // Check if note starts within the remaining clip duration
        if (loopStart + pNote.startBeat < clip.duration) {
          notesForClip.push({
            trackId,
            startBeat: clip.startBeat + loopStart + pNote.startBeat,
            durationBeats: pNote.durationBeats, // Logic to truncate if it exceeds clip? Let's keep simple.
            midiNote: pNote.midiNote,
            velocity: pNote.velocity
          });
        }
      }
    }
    return notesForClip;
  });

  const allNotes = [...directNotes, ...patternNotes];

  // 1. Clear track in engine
  await window.electronAPI.clearTrack({ trackId });

  // 2. Re-send all notes
  // We can do this in parallel, but sequential is safer for order if engine cared (it sorts anyway)
  const promises = allNotes.map((note) =>
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

/**
 * Synchronizes all tracks that use a specific pattern.
 * Call this when a pattern is edited.
 */
export async function syncPatternToEngine(patternId: string) {
  const { clips } = useStore.getState();
  const affectedTrackIds = new Set<number>();

  clips.forEach(c => {
    if (c.patternId === patternId) {
      affectedTrackIds.add(c.trackId);
    }
  });

  const promises = Array.from(affectedTrackIds).map(tid => syncTrackToEngine(tid));
  await Promise.all(promises);
}

/**
 * Sends a full automation lane (all points) to the engine.
 * paramName format: "volume" | "Reverb.mix" | "Delay.delayMs" | "Distortion.drive" …
 */
export function syncAutomationLaneToEngine(
  trackId: number,
  paramName: string,
  points: { beat: number; value: number }[],
) {
  if (!window.electronAPI) return;
  const { bpm, sampleRate } = useStore.getState();
  window.electronAPI.setAutomationLane({ trackId, paramName, points, bpm, sampleRate });
}

export function clearAutomationLaneFromEngine(trackId: number, paramName: string) {
  if (!window.electronAPI) return;
  const { bpm, sampleRate } = useStore.getState();
  window.electronAPI.clearAutomationLane({ trackId, paramName, bpm, sampleRate });
}

