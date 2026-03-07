import { create } from 'zustand';

// ─── Data types ──────────────────────────────────────────────────────────────

export interface Track {
  id: number;
  name: string;
  synthType: number;
  volume: number;
  muted: boolean;
  solo: boolean;
  color: string;
}

export interface Note {
  id: string;
  trackId: number;
  startBeat: number;
  durationBeats: number;
  midiNote: number;
  velocity: number;
}

// ─── Undo / redo snapshot (notes only – tracks rarely change) ────────────────

interface Snapshot {
  notes: Note[];
  selectedNotes: string[];
}

const MAX_UNDO = 50;

// ─── Store interface ─────────────────────────────────────────────────────────

interface AppState {
  // Playback
  isPlaying: boolean;
  currentPosition: number;
  bpm: number;
  sampleRate: number;

  // Data
  tracks: Track[];
  notes: Note[];
  selectedTrack: number | null;
  selectedNotes: string[];

  // Snap (beats)
  snapValue: number; // 0 = off, 0.25 = 1/16, 0.5 = 1/8, 1 = 1/4
  setSnapValue: (v: number) => void;

  // Clipboard (internal, for copy-paste)
  clipboard: Note[];

  // Undo / redo
  undoStack: Snapshot[];
  redoStack: Snapshot[];
  pushUndo: () => void;
  undo: () => void;
  redo: () => void;

  // Playback actions
  setIsPlaying: (playing: boolean) => void;
  setCurrentPosition: (position: number) => void;
  setBpm: (bpm: number) => void;

  // Track actions
  addTrack: (track: Track) => void;
  removeTrack: (id: number) => void;
  updateTrack: (id: number, updates: Partial<Track>) => void;
  setSelectedTrack: (id: number | null) => void;

  // Note actions
  addNote: (note: Note) => void;
  removeNote: (id: string) => void;
  removeNotes: (ids: string[]) => void;
  updateNote: (id: string, updates: Partial<Note>) => void;
  selectNote: (id: string, append?: boolean) => void;
  selectAllNotesForTrack: (trackId: number) => void;
  clearSelection: () => void;

  // Zoom (pixels per beat)
  pixelsPerBeat: number;
  setPixelsPerBeat: (px: number) => void;
  zoomIn: () => void;
  zoomOut: () => void;

  // Batch notes update
  updateNotes: (updates: { id: string; changes: Partial<Note> }[]) => void;

  // Clipboard
  copySelected: () => void;
  pasteClipboard: () => void;
  duplicateSelected: () => void;
}

// ─── Helpers ─────────────────────────────────────────────────────────────────

const uid = () => Math.random().toString(36).slice(2, 11);

// ─── Store ───────────────────────────────────────────────────────────────────

export const useStore = create<AppState>((set, get) => ({
  // Initial state
  isPlaying: false,
  currentPosition: 0,
  bpm: 120,
  sampleRate: 44100,
  tracks: [],
  notes: [],
  selectedTrack: null,
  selectedNotes: [],
  snapValue: 0.25,
  clipboard: [],
  undoStack: [],
  redoStack: [],
  pixelsPerBeat: 80,

  // ── Snap ─────────────────────────────────────────────────────────────────
  setSnapValue: (v) => set({ snapValue: v }),

  // ── Undo / redo ──────────────────────────────────────────────────────────
  pushUndo: () => {
    const { notes, selectedNotes, undoStack } = get();
    const snapshot: Snapshot = {
      notes: notes.map((n) => ({ ...n })),
      selectedNotes: [...selectedNotes],
    };
    set({
      undoStack: [...undoStack.slice(-(MAX_UNDO - 1)), snapshot],
      redoStack: [],
    });
  },

  undo: () => {
    const { undoStack, notes, selectedNotes } = get();
    if (undoStack.length === 0) return;
    const prev = undoStack[undoStack.length - 1];
    const current: Snapshot = {
      notes: notes.map((n) => ({ ...n })),
      selectedNotes: [...selectedNotes],
    };
    set((s) => ({
      notes: prev.notes,
      selectedNotes: prev.selectedNotes,
      undoStack: s.undoStack.slice(0, -1),
      redoStack: [...s.redoStack, current],
    }));
  },

  redo: () => {
    const { redoStack, notes, selectedNotes } = get();
    if (redoStack.length === 0) return;
    const next = redoStack[redoStack.length - 1];
    const current: Snapshot = {
      notes: notes.map((n) => ({ ...n })),
      selectedNotes: [...selectedNotes],
    };
    set((s) => ({
      notes: next.notes,
      selectedNotes: next.selectedNotes,
      redoStack: s.redoStack.slice(0, -1),
      undoStack: [...s.undoStack, current],
    }));
  },

  // ── Playback ─────────────────────────────────────────────────────────────
  setIsPlaying: (playing) => set({ isPlaying: playing }),
  setCurrentPosition: (position) => set({ currentPosition: position }),
  setBpm: (bpm) => set({ bpm }),

  // ── Tracks ───────────────────────────────────────────────────────────────
  addTrack: (track) =>
    set((s) => ({ tracks: [...s.tracks, track] })),

  removeTrack: (id) =>
    set((s) => ({
      tracks: s.tracks.filter((t) => t.id !== id),
      notes: s.notes.filter((n) => n.trackId !== id),
    })),

  updateTrack: (id, updates) =>
    set((s) => ({
      tracks: s.tracks.map((t) => (t.id === id ? { ...t, ...updates } : t)),
    })),

  setSelectedTrack: (id) => set({ selectedTrack: id }),

  // ── Notes ────────────────────────────────────────────────────────────────
  addNote: (note) => {
    get().pushUndo();
    set((s) => ({ notes: [...s.notes, note] }));
  },

  removeNote: (id) => {
    get().pushUndo();
    set((s) => ({
      notes: s.notes.filter((n) => n.id !== id),
      selectedNotes: s.selectedNotes.filter((sid) => sid !== id),
    }));
  },

  removeNotes: (ids) => {
    if (ids.length === 0) return;
    get().pushUndo();
    const idSet = new Set(ids);
    set((s) => ({
      notes: s.notes.filter((n) => !idSet.has(n.id)),
      selectedNotes: s.selectedNotes.filter((sid) => !idSet.has(sid)),
    }));
  },

  updateNote: (id, updates) =>
    set((s) => ({
      notes: s.notes.map((n) => (n.id === id ? { ...n, ...updates } : n)),
    })),

  selectNote: (id, append = false) =>
    set((s) => ({
      selectedNotes: append
        ? s.selectedNotes.includes(id)
          ? s.selectedNotes.filter((sid) => sid !== id)
          : [...s.selectedNotes, id]
        : [id],
    })),

  selectAllNotesForTrack: (trackId) =>
    set((s) => ({
      selectedNotes: s.notes
        .filter((n) => n.trackId === trackId)
        .map((n) => n.id),
    })),

  clearSelection: () => set({ selectedNotes: [] }),

  // ── Zoom ─────────────────────────────────────────────────────────────────
  setPixelsPerBeat: (px) => set({ pixelsPerBeat: px }),
  zoomIn: () => {
    const { pixelsPerBeat } = get();
    set({ pixelsPerBeat: pixelsPerBeat * 1.1 });
  },
  zoomOut: () => {
    const { pixelsPerBeat } = get();
    set({ pixelsPerBeat: pixelsPerBeat / 1.1 });
  },

  // ── Batch notes update ────────────────────────────────────────────────────
  updateNotes: (updates) => {
    get().pushUndo();
    set((s) => ({
      notes: s.notes.map((n) => {
        const update = updates.find((u) => u.id === n.id);
        return update ? { ...n, ...update.changes } : n;
      }),
    }));
  },

  // ── Clipboard ────────────────────────────────────────────────────────────
  copySelected: () => {
    const { notes, selectedNotes } = get();
    const idSet = new Set(selectedNotes);
    set({ clipboard: notes.filter((n) => idSet.has(n.id)).map((n) => ({ ...n })) });
  },

  pasteClipboard: () => {
    const { clipboard, notes } = get();
    if (clipboard.length === 0) return;
    get().pushUndo();
    const minBeat = Math.min(...clipboard.map((n) => n.startBeat));
    const maxEnd = Math.max(...notes.map((n) => n.startBeat + n.durationBeats), 0);
    const offset = maxEnd - minBeat;
    const pasted = clipboard.map((n) => ({
      ...n,
      id: uid(),
      startBeat: n.startBeat + offset,
    }));
    set((s) => ({
      notes: [...s.notes, ...pasted],
      selectedNotes: pasted.map((n) => n.id),
    }));
  },

  duplicateSelected: () => {
    const { notes, selectedNotes } = get();
    if (selectedNotes.length === 0) return;
    get().pushUndo();
    const idSet = new Set(selectedNotes);
    const selected = notes.filter((n) => idSet.has(n.id));
    const maxEnd = Math.max(...selected.map((n) => n.startBeat + n.durationBeats));
    const minStart = Math.min(...selected.map((n) => n.startBeat));
    const span = maxEnd - minStart;
    const duped = selected.map((n) => ({
      ...n,
      id: uid(),
      startBeat: n.startBeat + span,
    }));
    set((s) => ({
      notes: [...s.notes, ...duped],
      selectedNotes: duped.map((n) => n.id),
    }));
  },
}));

