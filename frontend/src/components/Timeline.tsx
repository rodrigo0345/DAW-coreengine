import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import { useStore } from '../store';
import { midiNoteName, snapBeat, uid } from '../helpers/music';
import { syncTrackToEngine } from '../helpers/engine';
import './Timeline.css';

// ─── Layout constants ────────────────────────────────────────────────────────

const TRACK_H = 56;
const MIN_NOTE_W = 16;
const MOVE_THRESHOLD = 3;

// ─── Component ───────────────────────────────────────────────────────────────

export default function Timeline() {
  const {
    tracks, notes, selectedTrack,
    addNote, updateNotes, removeNote,
    selectNote, selectedNotes, clearSelection,
    bpm, sampleRate, snapValue,
    isPlaying, currentPosition, setCurrentPosition,
    pushUndo,
    pixelsPerBeat, zoomIn, zoomOut,
  } = useStore();

  const canvasRef = useRef<HTMLDivElement>(null);
  const scrollRef = useRef<HTMLDivElement>(null);

  const drag = useRef({
    active: false,
    mode: '' as '' | 'move' | 'resize' | 'scrub',
    noteId: '',
    hasMoved: false,
    originX: 0,
    // Store drag-start state for ALL selected notes
    startState: {} as Record<string, { startBeat: number; durationBeats: number }>,
  });
  const [, setTick] = useState(0);
  const forceRender = () => setTick((t) => t + 1);

  const totalBeats = useMemo(() => {
    const max = Math.max(32, ...notes.map((n) => n.startBeat + n.durationBeats));
    return Math.ceil(max / 4) * 4 + 4;
  }, [notes]);

  const playbackBeat = useMemo(() => {
    const spb = (60 / bpm) * sampleRate;
    return currentPosition / spb;
  }, [currentPosition, bpm, sampleRate]);

  // ── Playback position advancement ──────────────────────────────────────────
  useEffect(() => {
    if (!isPlaying) return;
    const interval = setInterval(() => {
      const newPos = useStore.getState().currentPosition + (sampleRate / 60);
      setCurrentPosition(newPos);
    }, 16);
    return () => clearInterval(interval);
  }, [isPlaying, sampleRate, setCurrentPosition]);

  // ── Zoom mouse handlers ───────────────────────────────────────────────────
  const handleWheel = useCallback(
    (e: WheelEvent) => {
      if (e.ctrlKey) {
        e.preventDefault();
        e.stopPropagation();
        if (e.deltaY < 0) zoomIn();
        else zoomOut();
      }
    },
    [zoomIn, zoomOut]
  );

  useEffect(() => {
    const el = canvasRef.current;
    if (!el) return;
    el.addEventListener('wheel', handleWheel, { passive: false });
    return () => el.removeEventListener('wheel', handleWheel);
  }, [handleWheel]);

  // ── Global mouse handlers ──────────────────────────────────────────────────
  useEffect(() => {
    const onMove = (e: MouseEvent) => {
      const d = drag.current;
      if (!d.active) return;
      const dx = e.clientX - d.originX;
      if (!d.hasMoved && Math.abs(dx) < MOVE_THRESHOLD) return;
      d.hasMoved = true;

      if (d.mode === 'scrub') {
        const rect = canvasRef.current?.getBoundingClientRect();
        const scrollLeft = scrollRef.current?.scrollLeft ?? 0;
        if (!rect) return;
        const beat = Math.max(0, (e.clientX - rect.left + scrollLeft) / pixelsPerBeat);
        const spb = (60 / bpm) * sampleRate;
        setCurrentPosition(beat * spb);
        return;
      }

      // Batch update logic
      const updates: { id: string; changes: Partial<typeof notes[0]> }[] = [];

      Object.entries(d.startState).forEach(([id, start]) => {
        if (d.mode === 'resize') {
          const raw = start.durationBeats + dx / pixelsPerBeat;
          const val = snapValue > 0 ? Math.max(snapValue, snapBeat(raw, snapValue)) : Math.max(0.25, raw);
          updates.push({ id, changes: { durationBeats: val } });
        } else {
          // Move
          const raw = start.startBeat + dx / pixelsPerBeat;
          const val = Math.max(0, snapValue > 0 ? snapBeat(raw, snapValue) : raw);
          updates.push({ id, changes: { startBeat: val } });
        }
      });

      if (updates.length > 0) {
        updateNotes(updates);
      }
      forceRender();
    };

    const onUp = () => {
      const d = drag.current;
      if (!d.active) return;
      if (d.mode === 'scrub') {
        window.electronAPI?.seek(useStore.getState().currentPosition).catch(console.error);
      } else if (d.hasMoved && d.noteId) {
        // Sync affected tracks. Ideally we optimize, but syncing affected note tracks is safe.
        // We iterate notes that changed? No, just sync current note's track?
        // Batch editing might affect multiple tracks if we allowed multi-track selection (which we do).
        // Let's find unique trackIds involved in the batch edit.
        const affectedTrackIds = new Set(
          Object.keys(d.startState).map((nid) => useStore.getState().notes.find((n) => n.id === nid)?.trackId).filter(Boolean)
        );
        affectedTrackIds.forEach((tid) => syncTrackToEngine(tid as number));
      }
      d.active = false;
      d.hasMoved = false;
      d.mode = '';
      forceRender();
    };

    window.addEventListener('mousemove', onMove);
    window.addEventListener('mouseup', onUp);
    return () => {
      window.removeEventListener('mousemove', onMove);
      window.removeEventListener('mouseup', onUp);
    };
  }, [bpm, sampleRate, snapValue, updateNotes, setCurrentPosition, pixelsPerBeat]);

  // ── Ruler scrub ────────────────────────────────────────────────────────────
  const onRulerDown = useCallback(
    (e: React.MouseEvent) => {
      e.stopPropagation();
      const rect = canvasRef.current?.getBoundingClientRect();
      const scrollLeft = scrollRef.current?.scrollLeft ?? 0;
      if (!rect) return;
      const beat = Math.max(0, (e.clientX - rect.left + scrollLeft) / pixelsPerBeat);
      const spb = (60 / bpm) * sampleRate;
      setCurrentPosition(beat * spb);
      drag.current = { active: true, mode: 'scrub', noteId: '', hasMoved: false, originX: e.clientX, startState: {} };
    },
    [bpm, sampleRate, setCurrentPosition, pixelsPerBeat],
  );

  // ── Note drag ──────────────────────────────────────────────────────────────
  const onNoteDown = useCallback(
    (e: React.MouseEvent, noteId: string, resize: boolean) => {
      e.stopPropagation();
      e.preventDefault();
      const note = notes.find((n) => n.id === noteId);
      if (!note) return;
      pushUndo();
      selectNote(noteId, e.shiftKey || e.ctrlKey);

      drag.current = {
        active: true,
        mode: resize ? 'resize' : 'move',
        noteId,
        hasMoved: false,
        originX: e.clientX,
        startState: Object.fromEntries(
          selectedNotes.includes(noteId) || e.shiftKey || e.ctrlKey // if we just clicked it or it was already selected
            ? (e.shiftKey || e.ctrlKey || selectedNotes.includes(noteId) ?
                // If it's a multiselection, use the updated selection list
                // Problem: selectNote is async in Zustand? No, it's synchronous but we might refer to stale closure if we don't be careful.
                // Actually `selectedNotes` here comes from useStore(), which is current render cycle.
                // We should assume `selectedNotes` contains `noteId` because we just called `selectNote`.
                // Wait, React state updates are batched. `selectNote` calls `set`, which triggers re-render.
                // So inside this handler, `selectedNotes` is OLD.
                // Use `useStore.getState().selectedNotes`.
                [...new Set([...useStore.getState().selectedNotes, noteId])]
              : [noteId])
              .map((id) => {
                const n = notes.find((nt) => nt.id === id);
                return [id, n ? { startBeat: n.startBeat, durationBeats: n.durationBeats } : null];
              }).filter(Boolean) as [string, { startBeat: number; durationBeats: number }][]
            : [[noteId, { startBeat: note.startBeat, durationBeats: note.durationBeats }]] // fallback
        ),
      };
    },
    [notes, selectNote, pushUndo],
  );

  // ── Grid click → add note ──────────────────────────────────────────────────
  const onCanvasMouseDown = useCallback(
    (e: React.MouseEvent) => {
      if (e.button !== 0 || !selectedTrack) return;
      const rect = canvasRef.current?.getBoundingClientRect();
      if (!rect) return;

      const x = e.clientX - rect.left; // No scrollLeft
      const y = e.clientY - rect.top;  // No scrollTop
      const startX = e.clientX;

      const onUp = (upEvt: MouseEvent) => {
        window.removeEventListener('mouseup', onUp);
        if (Math.abs(upEvt.clientX - startX) > MOVE_THRESHOLD) return;
        if (drag.current.active) return;

        const rawBeat = x / pixelsPerBeat;
        const beat = snapValue > 0 ? snapBeat(rawBeat, snapValue) : Math.floor(rawBeat);
        const trackIndex = Math.floor(y / TRACK_H);
        if (trackIndex < 0 || trackIndex >= tracks.length) return;
        const t = tracks[trackIndex];

        const newNote = {
          id: uid(),
          trackId: t.id,
          startBeat: beat,
          durationBeats: Math.max(snapValue || 1, 1),
          midiNote: 60,
          velocity: 100,
        };
        addNote(newNote);
        syncTrackToEngine(t.id);
        clearSelection();
      };
      window.addEventListener('mouseup', onUp, { once: true });
    },
    [selectedTrack, tracks, addNote, snapValue, clearSelection, pixelsPerBeat],
  );

  // ── Right-click delete ─────────────────────────────────────────────────────
  const onNoteContext = useCallback(
    (e: React.MouseEvent, noteId: string) => {
      e.preventDefault();
      e.stopPropagation();
      const n = notes.find((nt) => nt.id === noteId);
      removeNote(noteId);
      if (n) syncTrackToEngine(n.trackId);
    },
    [removeNote, notes],
  );

  // ── Render ─────────────────────────────────────────────────────────────────
  const gridW = totalBeats * pixelsPerBeat;
  const gridH = Math.max(tracks.length * TRACK_H, 180);

  return (
    <div className="tl">
      {/* Ruler */}
      <div className="tl-ruler-row">
        <div
          className="tl-ruler"
          style={{ width: gridW }}
          onMouseDown={onRulerDown}
        >
          {Array.from({ length: totalBeats }, (_, i) => (
            <div
              key={i}
              className={`tl-mark ${i % 4 === 0 ? 'bar' : ''}`}
              style={{ left: i * pixelsPerBeat }}
            >
              {i % 4 === 0 && <span>{i / 4 + 1}</span>}
            </div>
          ))}
          <div className="tl-cursor-ruler" style={{ left: playbackBeat * pixelsPerBeat }} />
        </div>
      </div>

      {/* Scrollable content */}
      <div className="tl-scroll" ref={scrollRef}>
        <div
          ref={canvasRef}
          className="tl-canvas"
          style={{ width: gridW, height: gridH }}
          onMouseDown={onCanvasMouseDown}
          onContextMenu={(e) => e.preventDefault()}
        >
          {/* Grid lines */}
          {Array.from({ length: totalBeats }, (_, i) => (
            <div
              key={i}
              className={`tl-vline ${i % 4 === 0 ? 'bar' : ''}`}
              style={{ left: i * pixelsPerBeat }}
            />
          ))}

          {/* Playback cursor */}
          <div className={`tl-cursor ${isPlaying ? 'active' : ''}`} style={{ left: playbackBeat * pixelsPerBeat }}>
            <div className="tl-cursor-head" />
            <div className="tl-cursor-line" />
          </div>

          {/* Tracks */}
          {tracks.map((track, idx) => (
            <div
              key={track.id}
              className={`tl-lane ${selectedTrack === track.id ? 'sel' : ''}`}
              style={{ top: idx * TRACK_H, height: TRACK_H, borderLeftColor: track.color }}
            >
              <span className="tl-lane-label">{track.name}</span>
            </div>
          ))}

          {/* Notes */}
          {notes.map((note) => {
            const tIdx = tracks.findIndex((t) => t.id === note.trackId);
            if (tIdx === -1) return null;
            const t = tracks[tIdx];
            const sel = selectedNotes.includes(note.id);
            return (
              <div
                key={note.id}
                className={`tl-note ${sel ? 'sel' : ''}`}
                style={{
                  left: note.startBeat * pixelsPerBeat,
                  top: tIdx * TRACK_H + 6,
                  width: Math.max(MIN_NOTE_W, note.durationBeats * pixelsPerBeat - 2),
                  height: TRACK_H - 12,
                  background: t.color,
                }}
                onMouseDown={(e) => onNoteDown(e, note.id, false)}
                onContextMenu={(e) => onNoteContext(e, note.id)}
              >
                <div
                  className="tl-note-handle"
                  onMouseDown={(e) => { e.stopPropagation(); onNoteDown(e, note.id, true); }}
                />
                <span className="tl-note-label">{midiNoteName(note.midiNote)}</span>
              </div>
            );
          })}
        </div>
      </div>
    </div>
  );
}

