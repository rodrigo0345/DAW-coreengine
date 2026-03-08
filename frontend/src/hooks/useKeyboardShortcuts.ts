import { useEffect } from 'react';
import { useStore } from '../store';
import { syncTrackToEngine, syncPatternToEngine } from '../helpers/engine';
import { MIN_MIDI, MAX_MIDI } from '../helpers/music';

/**
 * Global keyboard shortcuts – FL Studio inspired.
 * Mount once in App.tsx.
 *
 *   Space              Toggle play/pause
 *   Delete/Backspace   Delete selected notes (pattern) or clips
 *   Ctrl+A             Select all notes in active pattern
 *   Ctrl+D             Deselect all
 *   Ctrl+C             Copy selected notes
 *   Ctrl+V             Paste
 *   Ctrl+B             Duplicate selected notes
 *   Ctrl+Z             Undo   |  Ctrl+Shift+Z / Ctrl+Y  Redo
 *   Ctrl+Up            Transpose selected +1 octave (group-clamped)
 *   Ctrl+Down          Transpose selected -1 octave (group-clamped)
 */
export function useKeyboardShortcuts() {
  const {
    notes,
    selectedTrack,
    selectedNotes,
    removeNotes,
    updateNotes,
    updateNotesInPattern,
    selectAllNotesForTrack,
    clearSelection,
    copySelectedNotes,
    pasteNotes,
    duplicateSelectedNotes,
    undo,
    redo,
    isPlaying,
    setIsPlaying,
    setCurrentPosition,
  } = useStore();

  useEffect(() => {
    const handler = (e: KeyboardEvent) => {
      const tag = (e.target as HTMLElement).tagName;
      const inInput = tag === 'INPUT' || tag === 'TEXTAREA' || tag === 'SELECT';

      // ── Space – ALWAYS play/stop regardless of focus ──────────────────────
      if (e.key === ' ' || e.code === 'Space') {
        e.preventDefault();
        if (isPlaying) {
          window.electronAPI?.pause();
          setIsPlaying(false);
        } else {
          window.electronAPI?.play();
          setIsPlaying(true);
        }
        return;
      }

      // All shortcuts below are blocked when typing in an input
      if (inInput) return;

      const ctrl = e.ctrlKey || e.metaKey;

      // ── Delete / Backspace ──────────────────────────────────────────────
      if (e.key === 'Delete' || e.key === 'Backspace') {
        const { selectedNotes: sn, selectedClips: sc } = useStore.getState();

        if (sn.length > 0) {
          e.preventDefault();
          // Try pattern-note deletion first (Piano Roll is open)
          const { activePatternId, patterns, removeNoteFromPattern } = useStore.getState();
          const activePat = patterns.find(p => p.id === activePatternId);
          if (activePat) {
            const patIds  = new Set(activePat.notes.map(n => n.id));
            const targets = sn.filter(id => patIds.has(id));
            if (targets.length) {
              targets.forEach(id => removeNoteFromPattern(activePatternId, id));
              syncPatternToEngine(activePatternId);
              return;
            }
          }
          // Fallback: legacy direct notes
          const trackIds = new Set(
            useStore.getState().notes.filter(n => sn.includes(n.id)).map(n => n.trackId)
          );
          removeNotes([...sn]);
          trackIds.forEach(tid => syncTrackToEngine(tid));
        } else if (sc.length > 0) {
          e.preventDefault();
          const { clips, deleteSelectedClips } = useStore.getState();
          const tids = new Set(clips.filter(c => sc.includes(c.id)).map(c => c.trackId));
          deleteSelectedClips();
          tids.forEach(tid => syncTrackToEngine(tid));
        }
        return;
      }


      if (!ctrl) return;

      // ── Ctrl+Up / Ctrl+Down — octave transpose (group-clamped) ──────────
      if (e.key === 'ArrowUp' || e.key === 'ArrowDown') {
        e.preventDefault();
        const delta = e.key === 'ArrowUp' ? 12 : -12;
        const { selectedNotes: sn, activePatternId, patterns } = useStore.getState();
        if (!sn.length) return;

        // Case A: selected notes live in the active pattern
        const activePat = patterns.find(p => p.id === activePatternId);
        if (activePat) {
          const patIds = new Set(activePat.notes.map(n => n.id));
          const patSel = sn.filter(id => patIds.has(id));
          if (patSel.length) {
            const midiVals = patSel.map(
              id => activePat.notes.find(n => n.id === id)?.midiNote ?? 60
            );
            // Clamp so the whole group stays inside [MIN_MIDI, MAX_MIDI]
            const clamped = delta > 0
              ? Math.min(delta, MAX_MIDI - Math.max(...midiVals))
              : Math.max(delta, MIN_MIDI - Math.min(...midiVals));
            if (clamped === 0) return;

            useStore.getState().pushUndo();
            updateNotesInPattern(
              activePatternId,
              patSel.map(id => ({
                id,
                changes: {
                  midiNote: (activePat.notes.find(n => n.id === id)?.midiNote ?? 60) + clamped,
                },
              })),
            );
            syncPatternToEngine(activePatternId);
            return;
          }
        }

        // Case B: legacy direct notes
        const legacyNotes = useStore.getState().notes.filter(n => sn.includes(n.id));
        if (!legacyNotes.length) return;
        const midiVals = legacyNotes.map(n => n.midiNote);
        const clamped  = delta > 0
          ? Math.min(delta, MAX_MIDI - Math.max(...midiVals))
          : Math.max(delta, MIN_MIDI - Math.min(...midiVals));
        if (clamped === 0) return;

        const tids = new Set<number>();
        updateNotes(legacyNotes.map(n => {
          tids.add(n.trackId);
          return { id: n.id, changes: { midiNote: n.midiNote + clamped } };
        }));
        tids.forEach(tid => syncTrackToEngine(tid));
        return;
      }

      // ── Other Ctrl shortcuts ────────────────────────────────────────────
      switch (e.key.toLowerCase()) {
        case 'a': {
          e.preventDefault();
          const { activePatternId, patterns, selectedNotes, selectedClips } = useStore.getState();
          // If Piano Roll has notes or notes are already selected → select all pattern notes
          const pat = patterns.find(p => p.id === activePatternId);
          if (pat && (selectedNotes.length > 0 || (selectedClips.length === 0 && pat.notes.length > 0))) {
            useStore.setState({ selectedNotes: pat.notes.map(n => n.id) });
          } else if (selectedTrack != null && selectedNotes.length > 0) {
            selectAllNotesForTrack(selectedTrack);
          } else {
            // Timeline: select all clips
            useStore.getState().selectAllClips();
          }
          break;
        }
        case 'd':
          e.preventDefault();
          clearSelection();
          useStore.getState().clearClipSelection();
          break;
        case 'c': {
          e.preventDefault();
          const { selectedNotes: sn, selectedClips: sc } = useStore.getState();
          if (sn.length > 0) {
            // Piano Roll context — copy notes
            copySelectedNotes();
          } else if (sc.length > 0) {
            // Timeline context — copy clips
            useStore.getState().copySelectedClips();
          }
          break;
        }
        case 'v': {
          e.preventDefault();
          const { noteClipboard, clipClipboard, selectedNotes: sn } = useStore.getState();
          // Prefer note paste when Piano Roll is open (has selected notes OR note clipboard has content
          // AND the active pattern exists). Clip paste only when explicitly in timeline context.
          const pat = useStore.getState().patterns.find(
            p => p.id === useStore.getState().activePatternId
          );
          if (noteClipboard.length > 0 && (sn.length > 0 || pat)) {
            pasteNotes();
            syncPatternToEngine(useStore.getState().activePatternId);
          } else if (clipClipboard.length > 0) {
            useStore.getState().pasteClips();
            window.electronAPI?.rebuildTimeline?.();
          }
          break;
        }
        case 'b': {
          e.preventDefault();
          const { selectedNotes: sn, selectedClips: sc } = useStore.getState();
          if (sn.length > 0) {
            duplicateSelectedNotes();
            syncPatternToEngine(useStore.getState().activePatternId);
          } else if (sc.length > 0) {
            useStore.getState().duplicateSelectedClips();
            window.electronAPI?.rebuildTimeline?.();
          }
          break;
        }
        case 'z':
          e.preventDefault();
          if (e.shiftKey) redo(); else undo();
          window.electronAPI?.rebuildTimeline?.();
          break;
        case 'y':
          e.preventDefault();
          redo();
          window.electronAPI?.rebuildTimeline?.();
          break;
      }
    };

    window.addEventListener('keydown', handler);
    return () => window.removeEventListener('keydown', handler);
  }, [
    selectedTrack, selectedNotes, removeNotes, selectAllNotesForTrack,
    clearSelection, copySelectedNotes, pasteNotes, duplicateSelectedNotes,
    updateNotesInPattern, updateNotes,
    undo, redo, isPlaying, setIsPlaying, setCurrentPosition, notes,
  ]);
}
