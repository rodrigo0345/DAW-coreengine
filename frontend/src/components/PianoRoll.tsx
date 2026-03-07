import { useCallback, useEffect, useLayoutEffect, useMemo, useRef, useState } from 'react';
import { useStore } from '../store';
import {
  MIDI_RANGE, MIN_MIDI, MAX_MIDI,
  isBlackKey, midiNoteName, snapBeat, uid,
} from '../helpers/music';
import { syncTrackToEngine } from '../helpers/engine';
import './PianoRoll.css';

// ─── Layout constants ────────────────────────────────────────────────────────

const NOTE_H = 20;
const MOVE_THRESHOLD = 3; // px before a click becomes a drag
const MIN_DURATION = 0.25;

// ─── Component ───────────────────────────────────────────────────────────────

export default function PianoRoll() {
  const {
    selectedTrack, tracks, notes,
    addNote, updateNote, updateNotes, removeNote,
    selectNote, selectedNotes, clearSelection,
    bpm, sampleRate, snapValue,
    currentPosition, setCurrentPosition,
    pushUndo, pixelsPerBeat, zoomIn, zoomOut,
  } = useStore();

  // Refs
  const containerRef = useRef<HTMLDivElement>(null); // New ref for the main wrapper
  const gridRef = useRef<HTMLDivElement>(null);
  const contentRef = useRef<HTMLDivElement>(null);
  const keysRef = useRef<HTMLDivElement>(null);
  const rulerScrollRef = useRef<HTMLDivElement>(null);

  // Zoom anchor for mouse-guided zoom
  const zoomAnchor = useRef<{ beat: number; cursorX: number } | null>(null);

  // Drag state
  const drag = useRef({
    active: false,
    mode: '' as '' | 'move' | 'resize' | 'scrub' | 'erase' | 'select',
    noteId: '',
    hasMoved: false,
    originX: 0,
    originY: 0,
    // Store drag-start state for ALL selected notes
    startState: {} as Record<string, { startBeat: number; startMidi: number; durationBeats: number }>,
    erasedAny: false,
    selectionBox: null as { x: number; y: number; w: number; h: number } | null,
    shouldDeselectOthers: false, // Flag to handle "click selected note" vs "drag selected note"
  });

  // Force re-render when dragging so notes visually update
  const [, setTick] = useState(0);
  const forceRender = () => setTick((t) => t + 1);

  const track = tracks.find((t) => t.id === selectedTrack);
  const trackNotes = useMemo(
    () => notes.filter((n) => n.trackId === selectedTrack),
    [notes, selectedTrack],
  );

  const totalBeats = useMemo(() => {
    const max = Math.max(32, ...trackNotes.map((n) => n.startBeat + n.durationBeats));
    return Math.ceil(max / 4) * 4 + 4; // always pad 1 extra bar
  }, [trackNotes]);

  const playbackBeat = useMemo(() => {
    const spb = (60 / bpm) * sampleRate;
    return currentPosition / spb;
  }, [currentPosition, bpm, sampleRate]);

  // ── Sync scroll: keys ↔ grid, ruler ↔ grid ────────────────────────────────
  const handleContentScroll = useCallback(() => {
    const el = contentRef.current;
    if (!el) return;
    if (keysRef.current) keysRef.current.scrollTop = el.scrollTop;
    if (rulerScrollRef.current) rulerScrollRef.current.scrollLeft = el.scrollLeft;
  }, []);

  // ── Zoom mouse handlers ───────────────────────────────────────────────────
  const handleWheel = useCallback(
    (e: WheelEvent) => {
      if (e.ctrlKey) {
        e.preventDefault();
        e.stopPropagation();

        // Calculate zoom anchor (beat under mouse)
        const el = contentRef.current;
        if (!el) return;

        const rect = el.getBoundingClientRect();
        const cursorX = e.clientX - rect.left;
        // Use current pixelsPerBeat from store (captured in closure or ref? Store is best)
        // Note: access latest state via useStore.getState() if closure is stale,
        // but here we depend on [zoomIn, zoomOut].
        // We need pixelsPerBeat in dependency or read it from ref/store.
        const currentPx = useStore.getState().pixelsPerBeat;
        const beat = (el.scrollLeft + cursorX) / currentPx;

        zoomAnchor.current = { beat, cursorX };

        if (e.deltaY < 0) zoomIn();
        else zoomOut();
      }
    },
    [zoomIn, zoomOut]
  );

  // Apply scroll adjustment after zoom
  useLayoutEffect(() => {
    if (zoomAnchor.current && contentRef.current) {
      const { beat, cursorX } = zoomAnchor.current;
      contentRef.current.scrollLeft = beat * pixelsPerBeat - cursorX;
      zoomAnchor.current = null;
    }
  }, [pixelsPerBeat]);

  useEffect(() => {
    // Attach to the main container to capture wheel events everywhere (keys, grid, etc.)
    const el = containerRef.current;
    if (!el) return;
    el.addEventListener('wheel', handleWheel, { passive: false });
    return () => el.removeEventListener('wheel', handleWheel);
  }, [handleWheel]);

  // ── Global mouse handlers (attached only while dragging) ───────────────────
  useEffect(() => {
    const onMove = (e: MouseEvent) => {
      const d = drag.current;
      if (!d.active) return;

      // Selection box
      if (d.mode === 'select') {
        // Calculate box relative to grid content
        const currentX = e.clientX - (gridRef.current?.getBoundingClientRect().left ?? 0);
        const currentY = e.clientY - (gridRef.current?.getBoundingClientRect().top ?? 0);
        const startX = d.originX;
        const startY = d.originY;

        d.selectionBox = {
          x: Math.min(startX, currentX),
          y: Math.min(startY, currentY),
          w: Math.abs(currentX - startX),
          h: Math.abs(currentY - startY),
        };
        forceRender();
        return;
      }

      // Erase logic
      if (d.mode === 'erase') {
        const el = document.elementFromPoint(e.clientX, e.clientY);
        const noteEl = el?.closest('.pr-note');
        if (noteEl) {
          const id = noteEl.getAttribute('data-id');
          if (id) {
            // Check if note still exists in store (might have been just deleted)
            const exists = useStore.getState().notes.some((n) => n.id === id);
            if (exists) {
              removeNote(id);
              d.erasedAny = true;
            }
          }
        }
        return;
      }

      const dx = e.clientX - d.originX;
      const dy = e.clientY - d.originY;

      if (!d.hasMoved && Math.hypot(dx, dy) < MOVE_THRESHOLD) return;
      d.hasMoved = true;

      if (d.mode === 'scrub') {
        const rect = rulerScrollRef.current?.getBoundingClientRect();
        if (!rect) return;
        const scrollLeft = rulerScrollRef.current?.scrollLeft ?? 0;
        const beat = Math.max(0, (e.clientX - rect.left + scrollLeft) / pixelsPerBeat);
        const spb = (60 / bpm) * sampleRate;
        setCurrentPosition(beat * spb);
        return;
      }

      // Batch update logic
      // We iterate over everything in d.startState (which includes selected notes + the dragged note)
      const updates: { id: string; changes: Partial<typeof notes[0]> }[] = [];

      Object.entries(d.startState).forEach(([id, start]) => {
        if (d.mode === 'resize') {
          const raw = start.durationBeats + dx / pixelsPerBeat;
          const snapped = snapValue > 0 ? Math.max(snapValue, snapBeat(raw, snapValue)) : Math.max(MIN_DURATION, raw);
          updates.push({ id, changes: { durationBeats: snapped } });
        } else {
          // Move
          const rawBeat = start.startBeat + dx / pixelsPerBeat;
          const newBeat = Math.max(0, snapValue > 0 ? snapBeat(rawBeat, snapValue) : rawBeat);
          const newMidi = Math.min(MAX_MIDI, Math.max(MIN_MIDI, start.startMidi - Math.round(dy / NOTE_H)));
          updates.push({ id, changes: { startBeat: newBeat, midiNote: newMidi } });
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

      if (d.mode === 'select' && d.selectionBox) {
        // Commit selection
        const { x, y, w, h } = d.selectionBox;
        // Convert box pixels to logical range
        const startBeat = x / pixelsPerBeat;
        const endBeat = (x + w) / pixelsPerBeat;
        // Y starts at top (highest midi)
        // midi = MAX_MIDI - Math.floor(y / NOTE_H)
        // High Y in pixels = Low Midi
        // top Y = midi_top
        // bottom Y = midi_bottom
        const midiTop = MAX_MIDI - Math.floor(y / NOTE_H);
        const midiBottom = MAX_MIDI - Math.floor((y + h) / NOTE_H);

        // Find notes inside
        const newSelection: string[] = [];
        trackNotes.forEach(n => {
          const nEnd = n.startBeat + n.durationBeats;
          // Intersection:
          // Note span: [n.startBeat, nEnd]
          // Box span: [startBeat, endBeat]
          // Overlap if (n.startBeat < endBeat) && (nEnd > startBeat)
          //
          // Midi check:
          // Note midi is point. Box Y range [midiBottom, midiTop] inclusive?
          // midiBottom is smaller number (lower pitch).
          if (n.midiNote <= midiTop && n.midiNote >= midiBottom) {
             if (n.startBeat < endBeat && nEnd > startBeat) {
               newSelection.push(n.id);
             }
          }
        });

        // Replace or add selection? "Ctrl + Mouse" usually implies specialized tool.
        // Standard behavior: Replace if just drag, Add if Shift held?
        // User asked "ctrl + mouse around".
        // Let's replace selection by default for basic box select.
        // But since Ctrl is held to ACTIVATE it, maybe we should just select.
        // I'll replace previous selection with the new box selection.
        // Actually, if I want to ADD, I'd probably need Shift.
        // Let's make it replace selectedNotes.
        clearSelection();
        newSelection.forEach(id => selectNote(id, true));
      }
      else if (d.mode === 'erase') {
        if (d.erasedAny && track) {
          syncTrackToEngine(track.id);
        }
      } else if (d.mode === 'scrub') {
        window.electronAPI?.seek(useStore.getState().currentPosition).catch(console.error);
      } else if (d.hasMoved && d.noteId) {
        const note = useStore.getState().notes.find((n) => n.id === d.noteId);
        if (note) syncTrackToEngine(note.trackId);
      } else if (!d.hasMoved && d.shouldDeselectOthers) {
        // Did not move, and we clicked a note that was part of a selection.
        // This implies the user intended to select JUST this note, not drag the group.
        selectNote(d.noteId, false);
      }

      d.active = false;
      d.hasMoved = false;
      d.mode = '';
      d.erasedAny = false;
      d.selectionBox = null;
      forceRender();
    };

    window.addEventListener('mousemove', onMove);
    window.addEventListener('mouseup', onUp);
    return () => {
      window.removeEventListener('mousemove', onMove);
      window.removeEventListener('mouseup', onUp);
    };
  }, [bpm, sampleRate, snapValue, updateNotes, setCurrentPosition, removeNote, track, pixelsPerBeat, trackNotes, selectNote, clearSelection]);

  // ── Note mouse-down (move / resize) ────────────────────────────────────────
  const onNoteDown = useCallback(
    (e: React.MouseEvent, noteId: string, resize: boolean) => {
      e.stopPropagation();
      e.preventDefault();

      // Right click = Erase
      if (e.button === 2) {
        removeNote(noteId);
        drag.current = {
          active: true,
          mode: 'erase',
          noteId: '',
          hasMoved: false,
          originX: e.clientX,
          originY: e.clientY,
          startState: {},
          erasedAny: true,
          selectionBox: null,
          shouldDeselectOthers: false,
        };
        return;
      }

      const note = trackNotes.find((n) => n.id === noteId);
      if (!note) return;

      pushUndo();

      const isSelected = selectedNotes.includes(noteId);
      const isModifier = e.shiftKey || e.ctrlKey;

      // Determine what we are dragging immediately to avoid "double click" issue
      // (State updates are async, so we calculate effective selection locally)
      let draggingIds: string[] = [];
      let shouldDeselectOthers = false;

      if (isModifier) {
        // Toggle / Add behavior
        if (isSelected) {
          // Toggling off?
          selectNote(noteId, true);
          draggingIds = selectedNotes.filter(id => id !== noteId);
        } else {
          // Adding
          selectNote(noteId, true);
          draggingIds = [...selectedNotes, noteId];
        }
      } else {
        if (isSelected) {
          // Clicked an already selected note -> Drag group
          // Don't call selectNote yet, as it might deselect others.
          // We defer deselection to onUp if no move occurred.
          draggingIds = [...selectedNotes];
          shouldDeselectOthers = true;
        } else {
          // Clicked unselected -> Exclusive select and drag
          selectNote(noteId, false);
          draggingIds = [noteId];
        }
      }

      drag.current = {
        active: true,
        mode: resize ? 'resize' : 'move',
        noteId,
        hasMoved: false,
        originX: e.clientX,
        originY: e.clientY,
        // Calculate start state based on our LOCAL draggingIds, not stale store state
        startState: Object.fromEntries(
          draggingIds.map((id) => {
            const n = trackNotes.find((nt) => nt.id === id);
            return [id, n ? { startBeat: n.startBeat, startMidi: n.midiNote, durationBeats: n.durationBeats } : null];
          }).filter(Boolean) as [string, { startBeat: number; startMidi: number; durationBeats: number }][],
        ),
        erasedAny: false,
        selectionBox: null,
        shouldDeselectOthers,
      };
    },
    [trackNotes, selectNote, pushUndo, selectedNotes, removeNote],
  );

  // ── Grid click → add note (only fires if mouse didn't move) ────────────────
  const onGridMouseDown = useCallback(
    (e: React.MouseEvent) => {
      // Right click on grid -> start eraser
      if (e.button === 2) {
        drag.current = {
          active: true,
          mode: 'erase',
          noteId: '',
          hasMoved: false,
          originX: e.clientX,
          originY: e.clientY,
          startState: {},
          erasedAny: false,
        };
        return;
      }

      // Ctrl + Left Click -> Box Select
      if (e.button === 0 && e.ctrlKey) {
        const rect = gridRef.current?.getBoundingClientRect();
        if (!rect) return;

        drag.current = {
          active: true,
          mode: 'select',
          noteId: '',
          hasMoved: false,
          originX: e.clientX - rect.left,
          originY: e.clientY - rect.top,
          startState: {},
          erasedAny: false,
          selectionBox: { x: e.clientX - rect.left, y: e.clientY - rect.top, w: 0, h: 0 }
        };
        return;
      }

      if (e.button !== 0 || !track) return;

      // Store a flag: if mouseup fires without moving, we add a note
      const rect = gridRef.current?.getBoundingClientRect();
      if (!rect) return;

      // FIX: getBoundingClientRect() on the grid content already accounts for scroll.
      // Do NOT add scrollLeft/scrollTop again.
      const x = e.clientX - rect.left;
      const y = e.clientY - rect.top;

      const startClientX = e.clientX;
      const startClientY = e.clientY;

      const onUp = (upEvt: MouseEvent) => {
        window.removeEventListener('mouseup', onUp);
        const dx = upEvt.clientX - startClientX;
        const dy = upEvt.clientY - startClientY;
        if (Math.hypot(dx, dy) > MOVE_THRESHOLD) return;
        if (drag.current.active) return; // we were dragging a note

        // Calculate grid position
        const rawBeat = x / pixelsPerBeat;
        const beat = snapValue > 0 ? snapBeat(rawBeat, snapValue) : Math.floor(rawBeat);
        const midi = MAX_MIDI - Math.floor(y / NOTE_H);
        if (midi < MIN_MIDI || midi > MAX_MIDI) return;

        const newNote = {
          id: uid(),
          trackId: track.id,
          startBeat: beat,
          durationBeats: Math.max(snapValue || 1, 1),
          midiNote: midi,
          velocity: 100,
        };
        addNote(newNote);
        syncTrackToEngine(track.id);
        clearSelection();
      };

      window.addEventListener('mouseup', onUp, { once: true });
    },
    [track, addNote, snapValue, clearSelection, pixelsPerBeat],
  );

  // ── Right-click → instant delete (no confirm dialog) ───────────────────────
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

  // ── Ruler scrub ────────────────────────────────────────────────────────────
  const onRulerDown = useCallback(
    (e: React.MouseEvent) => {
      e.stopPropagation();
      const rect = rulerScrollRef.current?.getBoundingClientRect();
      if (!rect) return;
      const scrollLeft = rulerScrollRef.current?.scrollLeft ?? 0;
      const beat = Math.max(0, (e.clientX - rect.left + scrollLeft) / pixelsPerBeat);
      const spb = (60 / bpm) * sampleRate;
      setCurrentPosition(beat * spb);

      drag.current = {
        active: true,
        mode: 'scrub',
        noteId: '',
        hasMoved: false,
        originX: e.clientX,
        originY: e.clientY,
        startBeat: 0,
        startMidi: 0,
        startDuration: 0,
      };
    },
    [bpm, sampleRate, setCurrentPosition],
  );

  // ── Empty state ────────────────────────────────────────────────────────────
  if (!track) {
    return (
      <div className="pr">
        <div className="pr-empty">
          <span>Select a track to open the Piano Roll</span>
        </div>
      </div>
    );
  }

  // ── Render ─────────────────────────────────────────────────────────────────
  const gridW = totalBeats * pixelsPerBeat;
  const gridH = MIDI_RANGE * NOTE_H;

  return (
    <div className="pr" ref={containerRef} onContextMenu={(e) => e.preventDefault()}>
      {/* ── Header bar ──────────────────────────────────────────────────── */}
      <div className="pr-toolbar">
        <span className="pr-title">{track.name}</span>
        <span className="pr-badge">{trackNotes.length} notes</span>
        <div className="pr-snap">
          <label>Snap</label>
          <select
            value={useStore.getState().snapValue}
            onChange={(e) => useStore.getState().setSnapValue(Number(e.target.value))}
          >
            <option value={0}>Off</option>
            <option value={0.25}>1/16</option>
            <option value={0.5}>1/8</option>
            <option value={1}>1/4</option>
            <option value={2}>1/2</option>
            <option value={4}>Bar</option>
          </select>
        </div>
      </div>

      {/* ── Ruler ───────────────────────────────────────────────────────── */}
      <div className="pr-ruler-row">
        <div className="pr-ruler-pad" />
        <div className="pr-ruler-scroll" ref={rulerScrollRef}>
          <div
            className="pr-ruler"
            style={{ width: gridW }}
            onMouseDown={onRulerDown}
          >
            {Array.from({ length: totalBeats }, (_, i) => (
              <div
                key={i}
                className={`pr-ruler-mark ${i % 4 === 0 ? 'bar' : ''}`}
                style={{ left: i * pixelsPerBeat }}
              >
                {i % 4 === 0 && <span>{i / 4 + 1}</span>}
              </div>
            ))}
            <div className="pr-cursor-ruler" style={{ left: playbackBeat * pixelsPerBeat }} />
          </div>
        </div>
      </div>

      {/* ── Body: keys + grid ───────────────────────────────────────────── */}
      <div className="pr-body">
        {/* Piano keys */}
        <div className="pr-keys" ref={keysRef}>
          {Array.from({ length: MIDI_RANGE }, (_, i) => {
            const midi = MAX_MIDI - i;
            const black = isBlackKey(midi);
            const isC = midi % 12 === 0;
            return (
              <div
                key={midi}
                className={`pr-key ${black ? 'black' : 'white'} ${isC ? 'oct' : ''}`}
                style={{ height: NOTE_H }}
              >
                <span className="pr-key-label">
                  {(isC || midi % 12 === 5) ? midiNoteName(midi) : ''}
                </span>
              </div>
            );
          })}
        </div>

        {/* Grid + notes */}
        <div
          className="pr-content"
          ref={contentRef}
          onScroll={handleContentScroll}
        >
          <div
            className="pr-grid"
            ref={gridRef}
            style={{ width: gridW, height: gridH }}
            onMouseDown={onGridMouseDown}
          >
            {/* Selection Box */}
            {drag.current.mode === 'select' && drag.current.selectionBox && (
              <div
                className="pr-select-box"
                style={{
                  left: drag.current.selectionBox.x,
                  top: drag.current.selectionBox.y,
                  width: drag.current.selectionBox.w,
                  height: drag.current.selectionBox.h,
                }}
              />
            )}

            {/* Row stripes */}
            {Array.from({ length: MIDI_RANGE }, (_, i) => {
              const midi = MAX_MIDI - i;
              return (
                <div
                  key={midi}
                  className={`pr-row ${isBlackKey(midi) ? 'dark' : ''} ${midi % 12 === 0 ? 'oct-line' : ''}`}
                  style={{ top: i * NOTE_H, height: NOTE_H }}
                />
              );
            })}

            {/* Vertical grid lines */}
            {Array.from({ length: totalBeats }, (_, i) => (
              <div
                key={i}
                className={`pr-vline ${i % 4 === 0 ? 'bar' : ''}`}
                style={{ left: i * pixelsPerBeat }}
              />
            ))}

            {/* Playback cursor */}
            <div className="pr-cursor" style={{ left: playbackBeat * pixelsPerBeat }} />

            {/* Notes */}
            {trackNotes.map((note) => {
              const y = (MAX_MIDI - note.midiNote) * NOTE_H;
              const sel = selectedNotes.includes(note.id);
              return (
                <div
                  key={note.id}
                  data-id={note.id}
                  className={`pr-note ${sel ? 'sel' : ''}`}
                  style={{
                    left: note.startBeat * pixelsPerBeat,
                    top: y,
                    width: Math.max(8, note.durationBeats * pixelsPerBeat - 1),
                    height: NOTE_H - 1,
                    background: track.color,
                    opacity: note.velocity / 127 * 0.5 + 0.5,
                  }}
                  onMouseDown={(e) => onNoteDown(e, note.id, false)}
                >
                  <span className="pr-note-label">{midiNoteName(note.midiNote)}</span>
                  <div
                    className="pr-note-handle"
                    onMouseDown={(e) => { e.stopPropagation(); onNoteDown(e, note.id, true); }}
                  />
                </div>
              );
            })}
          </div>
        </div>
      </div>

      {/* ── Footer ──────────────────────────────────────────────────────── */}
      <div className="pr-status">
        Click to add · Drag to move / change pitch · Right edge to resize · Right-click to delete · Ctrl+A / C / V / Z
      </div>
    </div>
  );
}

