import { useEffect } from 'react';
import { useStore } from '../store';
import { syncTrackToEngine } from '../helpers/engine';
import { MIN_MIDI, MAX_MIDI } from '../helpers/music';

/**
 * Global keyboard shortcuts – FL Studio inspired.
 * Mount once in App.tsx.
 *
 *   Ctrl+A   Select all notes in current track
 *   Ctrl+D   Deselect all
 *   Ctrl+C   Copy selected notes
 *   Ctrl+V   Paste clipboard
 *   Ctrl+B   Duplicate selected notes
 *   Ctrl+Z   Undo
 *   Ctrl+Shift+Z / Ctrl+Y   Redo
 *   Ctrl+Up  Transpose selected +1 Octave (+12 semitones)
 *   Ctrl+Down Transpose selected -1 Octave (-12 semitones)
 *   Delete / Backspace   Delete selected notes
 *   Space    Toggle play/pause
 */
export function useKeyboardShortcuts() {
  const {
    notes,
    selectedTrack,
    selectedNotes,
    removeNotes,
    updateNotes,
    selectAllNotesForTrack,
    clearSelection,
    copySelected,
    pasteClipboard,
    duplicateSelected,
    undo,
    redo,
    isPlaying,
    setIsPlaying,
    setCurrentPosition,
  } = useStore();

  useEffect(() => {
    const handler = (e: KeyboardEvent) => {
      // Ignore shortcuts when focus is inside an input / textarea
      const tag = (e.target as HTMLElement).tagName;
      if (tag === 'INPUT' || tag === 'TEXTAREA' || tag === 'SELECT') return;

      const ctrl = e.ctrlKey || e.metaKey;

      // ── Delete / Backspace ──────────────────────────────────────────────
      if (e.key === 'Delete' || e.key === 'Backspace') {
        if (selectedNotes.length > 0) {
          e.preventDefault();
          // Find affected tracks before removing to sync them
          const trackIds = new Set(
            notes.filter(n => selectedNotes.includes(n.id)).map(n => n.trackId)
          );

          removeNotes([...selectedNotes]);

          trackIds.forEach(tid => syncTrackToEngine(tid));
        }
        return;
      }

      // ── Space – toggle play / pause ─────────────────────────────────────
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

      if (!ctrl) return;

      // ── Transpose Octave (Ctrl + Up/Down) ───────────────────────────────
      if (e.key === 'ArrowUp' || e.key === 'ArrowDown') {
        if (selectedNotes.length === 0) return;
        e.preventDefault();

        const delta = e.key === 'ArrowUp' ? 12 : -12;
        const updates: { id: string; changes: { midiNote: number } }[] = [];
        const affectedTrackIds = new Set<number>();

        selectedNotes.forEach(id => {
          const note = notes.find(n => n.id === id);
          if (!note) return;

          const newMidi = note.midiNote + delta;
          if (newMidi >= MIN_MIDI && newMidi <= MAX_MIDI) {
            updates.push({ id, changes: { midiNote: newMidi } });
            affectedTrackIds.add(note.trackId);
          }
        });

        if (updates.length > 0) {
          updateNotes(updates);
          affectedTrackIds.forEach(tid => syncTrackToEngine(tid));
        }
        return;
      }

      switch (e.key.toLowerCase()) {
        case 'a':
          e.preventDefault();
          if (selectedTrack != null) selectAllNotesForTrack(selectedTrack);
          break;

        case 'd':
          e.preventDefault();
          clearSelection();
          break;

        case 'c':
          e.preventDefault();
          copySelected();
          break;

        case 'v':
          e.preventDefault();
          pasteClipboard();
          window.electronAPI?.rebuildTimeline();
          break;

        case 'b':
          e.preventDefault();
          duplicateSelected();
          window.electronAPI?.rebuildTimeline();
          break;

        case 'z':
          e.preventDefault();
          if (e.shiftKey) {
            redo();
          } else {
            undo();
          }
          window.electronAPI?.rebuildTimeline();
          break;

        case 'y':
          e.preventDefault();
          redo();
          window.electronAPI?.rebuildTimeline();
          break;
      }
    };

    window.addEventListener('keydown', handler);
    return () => window.removeEventListener('keydown', handler);
  }, [
    selectedTrack,
    selectedNotes,
    removeNotes,
    selectAllNotesForTrack,
    clearSelection,
    copySelected,
    pasteClipboard,
    duplicateSelected,
    undo,
    redo,
    isPlaying,
    setIsPlaying,
    setCurrentPosition,
    notes,
    updateNotes
  ]);
}
