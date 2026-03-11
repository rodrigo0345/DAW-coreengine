import { create } from 'zustand';
// store v2 – full arrangement clipboard + proper undo

// ─── Lua Plugin ───────────────────────────────────────────────────────────────
export interface LuaPlugin {
  /** Engine-assigned id (index into pool). -1 = not yet persisted in engine */
  id: number;
  name: string;
  sourceCode: string;
  ready: boolean;
}

// ─── Data types ──────────────────────────────────────────────────────────────
export interface Track {
  id: number;
  name: string;
  synthType: number;  // 0=Sine 1=Square 2=Saw 3=PWM 4=Sampler
  volume: number;
  muted: boolean;
  solo: boolean;
  color: string;
  // Sampler fields
  sampleFile?: string;   // absolute path to loaded audio file
  rootNote?: number;     // MIDI root note (default 69 = A4)
  oneShot?: boolean;     // true = drum/one-shot
  // Polyphony
  voiceCount?: number;   // number of voices (default 8)
  // Display mode
  useMidi?: boolean;     // false (default) = show as audio clip, true = show MIDI notes
  // Lua plugin assigned as instrument (-1 = none)
  luaPluginId?: number;
}
export interface Note {
  id: string;
  trackId: number;
  startBeat: number;
  durationBeats: number;
  midiNote: number;
  velocity: number;
}
export interface Pattern {
  id: string;
  name: string;
  duration: number;
  notes: Note[];
}
export interface Clip {
  id: string;
  trackId: number;
  patternId: string;
  startBeat: number;
  duration: number;
}

// ─── Automation ───────────────────────────────────────────────────────────────
export type AutomationTarget =
  | { kind: 'volume' }
  | { kind: 'effect'; effectType: string; paramName: string };

export interface AutomationPoint {
  id: string;
  beat: number;   // position in beats
  value: number;  // 0–1 normalised
}

export interface AutomationLane {
  id: string;
  trackId: number;
  target: AutomationTarget;
  points: AutomationPoint[];
  expanded: boolean;
}
// ─── Snapshot covers both notes AND arrangement ───────────────────────────────
interface Snapshot {
  notes: Note[];
  selectedNotes: string[];
  clips: Clip[];
  patterns: Pattern[];
  selectedClips: string[];
  automationLanes: AutomationLane[];
}
const MAX_UNDO = 50;
interface AppState {
  isPlaying: boolean;
  currentPosition: number;
  bpm: number;
  sampleRate: number;
  tracks: Track[];
  notes: Note[];
  patterns: Pattern[];
  clips: Clip[];
  activePatternId: string;
  noteClipboard: Note[];
  clipClipboard: Clip[];
  selectedTrack: number | null;
  selectedNotes: string[];
  selectedClips: string[];
  snapValue: number;
  setSnapValue: (v: number) => void;
  undoStack: Snapshot[];
  redoStack: Snapshot[];
  pushUndo: () => void;
  undo: () => void;
  redo: () => void;
  pixelsPerBeat: number;
  setPixelsPerBeat: (px: number) => void;
  zoomIn: () => void;
  zoomOut: () => void;
  setIsPlaying: (v: boolean) => void;
  setCurrentPosition: (v: number) => void;
  setSampleRate: (v: number) => void;
  addTrack: (t: Track) => void;
  removeTrack: (id: number) => void;
  updateTrack: (id: number, u: Partial<Track>) => void;
  setSelectedTrack: (id: number | null) => void;
  addNote: (note: Note) => void;
  removeNote: (id: string) => void;
  removeNotes: (ids: string[]) => void;
  updateNote: (id: string, u: Partial<Note>) => void;
  updateNotes: (updates: { id: string; changes: Partial<Note> }[]) => void;
  selectNote: (id: string, append?: boolean) => void;
  selectAllNotesForTrack: (trackId: number) => void;
  clearSelection: () => void;
  copySelectedNotes: () => void;
  pasteNotes: () => void;
  duplicateSelectedNotes: () => void;
  createPattern: () => string;
  clonePattern: (patternId: string) => string;
  makeClipUnique: (clipId: string) => void;
  setActivePattern: (id: string) => void;
  renamePattern: (patternId: string, name: string) => void;
  addNoteToPattern: (patternId: string, note: Note) => void;
  removeNoteFromPattern: (patternId: string, noteId: string) => void;
  updateNotesInPattern: (patternId: string, updates: { id: string; changes: Partial<Note> }[]) => void;
  addClip: (clip: Clip) => void;
  removeClip: (id: string) => void;
  updateClip: (id: string, u: Partial<Clip>) => void;
  selectClip: (id: string, append?: boolean) => void;
  selectAllClips: () => void;
  clearClipSelection: () => void;
  deleteSelectedClips: () => void;
  copySelectedClips: () => void;
  pasteClips: (atBeat?: number) => void;
  duplicateSelectedClips: () => void;
  // ── Automation ──────────────────────────────────────────────────────────
  automationLanes: AutomationLane[];
  addAutomationLane: (trackId: number, target: AutomationTarget) => string;
  removeAutomationLane: (laneId: string) => void;
  toggleAutomationLane: (laneId: string) => void;
  addAutomationPoint: (laneId: string, point: AutomationPoint) => void;
  updateAutomationPoint: (laneId: string, pointId: string, beat: number, value: number) => void;
  removeAutomationPoint: (laneId: string, pointId: string) => void;
  // ── Sampler / instrument ─────────────────────────────────────────────────
  setSampleFile: (trackId: number, filePath: string, rootNote: number, oneShot: boolean) => void;
  setTrackVoiceCount: (trackId: number, numVoices: number) => void;
  setSynthTypeOnTrack: (trackId: number, synthType: number) => void;
  // ── Solo (exclusive) ─────────────────────────────────────────────────────
  toggleSolo: (trackId: number) => void;
  // ── Lua Plugins ──────────────────────────────────────────────────────────
  luaPlugins: LuaPlugin[];
  setLuaPlugins: (plugins: LuaPlugin[]) => void;
  addLuaPlugin: (plugin: LuaPlugin) => void;
  updateLuaPlugin: (id: number, changes: Partial<LuaPlugin>) => void;
  removeLuaPlugin: (id: number) => void;
}
export const uid = () => Math.random().toString(36).slice(2, 11);
function snap(s: AppState): Snapshot {
  return {
    notes: s.notes.map(n => ({ ...n })),
    selectedNotes: [...s.selectedNotes],
    clips: s.clips.map(c => ({ ...c })),
    patterns: s.patterns.map(p => ({ ...p, notes: p.notes.map(n => ({ ...n })) })),
    selectedClips: [...s.selectedClips],
    automationLanes: s.automationLanes.map(l => ({ ...l, points: l.points.map(p => ({ ...p })) })),
  };
}
export const useStore = create<AppState>((set, get) => ({
  isPlaying: false,
  currentPosition: 0,
  bpm: 120,
  sampleRate: 44100,   // updated at runtime from engine's EngineReady event
  tracks: [],
  notes: [],
  patterns: [{ id: 'pat-1', name: 'Pattern 1', duration: 16, notes: [] }],
  clips: [],
  activePatternId: 'pat-1',
  noteClipboard: [],
  clipClipboard: [],
  automationLanes: [],
  luaPlugins: [],
  selectedTrack: null,
  selectedNotes: [],
  selectedClips: [],
  snapValue: 0.25,
  undoStack: [],
  redoStack: [],
  pixelsPerBeat: 80,
  setSnapValue: (v) => set({ snapValue: v }),
  setPixelsPerBeat: (px) => set({ pixelsPerBeat: px }),
  zoomIn:  () => set(s => ({ pixelsPerBeat: s.pixelsPerBeat * 1.15 })),
  zoomOut: () => set(s => ({ pixelsPerBeat: Math.max(20, s.pixelsPerBeat / 1.15) })),
  pushUndo: () => {
    const cur = snap(get() as AppState);
    set(s => ({ undoStack: [...s.undoStack.slice(-(MAX_UNDO - 1)), cur], redoStack: [] }));
  },
  undo: () => {
    const { undoStack } = get();
    if (!undoStack.length) return;
    const prev = undoStack[undoStack.length - 1];
    const cur  = snap(get() as AppState);
    set(s => ({
      ...prev,
      undoStack: s.undoStack.slice(0, -1),
      redoStack: [...s.redoStack, cur],
    }));
  },
  redo: () => {
    const { redoStack } = get();
    if (!redoStack.length) return;
    const next = redoStack[redoStack.length - 1];
    const cur  = snap(get() as AppState);
    set(s => ({
      ...next,
      redoStack: s.redoStack.slice(0, -1),
      undoStack: [...s.undoStack, cur],
    }));
  },
  setIsPlaying: (v) => set({ isPlaying: v }),
  setCurrentPosition: (v) => set({ currentPosition: v }),
  setBpm: (v) => set({ bpm: v }),
  setSampleRate: (v) => set({ sampleRate: v }),
  addTrack: (t) => set(s => ({ tracks: [...s.tracks, t] })),
  removeTrack: (id) => set(s => ({
    tracks: s.tracks.filter(t => t.id !== id),
    notes:  s.notes.filter(n => n.trackId !== id),
    clips:  s.clips.filter(c => c.trackId !== id),
  })),
  updateTrack: (id, u) => set(s => ({ tracks: s.tracks.map(t => t.id === id ? { ...t, ...u } : t) })),
  toggleSolo: (trackId) => {
    const { tracks } = get();
    const track = tracks.find(t => t.id === trackId);
    if (!track) return;
    const newSolo = !track.solo;
    // Exclusive solo: clear all others first, then set this one
    set(s => ({
      tracks: s.tracks.map(t => ({ ...t, solo: t.id === trackId ? newSolo : false })),
    }));
    // Tell the engine about every track's new solo state
    tracks.forEach(t => {
      const val = t.id === trackId ? (newSolo ? 1 : 0) : 0;
      window.electronAPI?.setTrackSolo({ trackId: t.id, value: val });
    });
  },
  setSelectedTrack: (id) => set({ selectedTrack: id }),
  addNote: (note) => { get().pushUndo(); set(s => ({ notes: [...s.notes, note] })); },
  removeNote: (id) => {
    get().pushUndo();
    set(s => ({
      notes: s.notes.filter(n => n.id !== id),
      selectedNotes: s.selectedNotes.filter(sid => sid !== id),
    }));
  },
  removeNotes: (ids) => {
    if (!ids.length) return;
    get().pushUndo();
    const s_ = new Set(ids);
    set(s => ({
      notes: s.notes.filter(n => !s_.has(n.id)),
      selectedNotes: s.selectedNotes.filter(sid => !s_.has(sid)),
    }));
  },
  updateNote: (id, u) => set(s => ({ notes: s.notes.map(n => n.id === id ? { ...n, ...u } : n) })),
  updateNotes: (updates) => {
    get().pushUndo();
    set(s => ({
      notes: s.notes.map(n => {
        const u = updates.find(x => x.id === n.id);
        return u ? { ...n, ...u.changes } : n;
      }),
    }));
  },
  selectNote: (id, append = false) => set(s => ({
    selectedNotes: append
      ? s.selectedNotes.includes(id) ? s.selectedNotes.filter(sid => sid !== id) : [...s.selectedNotes, id]
      : [id],
  })),
  selectAllNotesForTrack: (trackId) => set(s => ({
    selectedNotes: s.notes.filter(n => n.trackId === trackId).map(n => n.id),
  })),
  clearSelection: () => set({ selectedNotes: [] }),
  copySelectedNotes: () => {
    const { notes, selectedNotes, activePatternId, patterns } = get();
    const ids = new Set(selectedNotes);
    if (!ids.size) return;

    // Prefer pattern notes (Piano Roll context)
    const activePat = patterns.find(p => p.id === activePatternId);
    if (activePat) {
      const patSel = activePat.notes.filter(n => ids.has(n.id));
      if (patSel.length) {
        set({ noteClipboard: patSel.map(n => ({ ...n })) });
        return;
      }
    }
    // Fallback: legacy flat notes
    set({ noteClipboard: notes.filter(n => ids.has(n.id)).map(n => ({ ...n })) });
  },
  pasteNotes: () => {
    const { noteClipboard, activePatternId, patterns } = get();
    if (!noteClipboard.length) return;
    get().pushUndo();

    const activePat = patterns.find(p => p.id === activePatternId);
    if (activePat) {
      // Paste into active pattern, offset after the last note in the pattern
      const minBeat = Math.min(...noteClipboard.map(n => n.startBeat));
      const patMax  = activePat.notes.length
        ? Math.max(...activePat.notes.map(n => n.startBeat + n.durationBeats))
        : 0;
      const offset  = patMax - minBeat;
      const pasted  = noteClipboard.map(n => ({ ...n, id: uid(), startBeat: n.startBeat + offset }));
      set(s => ({
        patterns: s.patterns.map(p =>
          p.id === activePatternId ? { ...p, notes: [...p.notes, ...pasted] } : p
        ),
        selectedNotes: pasted.map(n => n.id),
      }));
      return;
    }
    // Fallback: legacy flat notes
    const { notes } = get();
    const minBeat = Math.min(...noteClipboard.map(n => n.startBeat));
    const maxEnd  = Math.max(...notes.map(n => n.startBeat + n.durationBeats), 0);
    const offset  = maxEnd - minBeat;
    const pasted  = noteClipboard.map(n => ({ ...n, id: uid(), startBeat: n.startBeat + offset }));
    set(s => ({ notes: [...s.notes, ...pasted], selectedNotes: pasted.map(n => n.id) }));
  },
  duplicateSelectedNotes: () => {
    const { notes, selectedNotes, activePatternId, patterns } = get();
    if (!selectedNotes.length) return;
    get().pushUndo();
    const ids = new Set(selectedNotes);

    // Prefer pattern notes
    const activePat = patterns.find(p => p.id === activePatternId);
    if (activePat) {
      const sel = activePat.notes.filter(n => ids.has(n.id));
      if (sel.length) {
        const max = Math.max(...sel.map(n => n.startBeat + n.durationBeats));
        const min = Math.min(...sel.map(n => n.startBeat));
        const dup = sel.map(n => ({ ...n, id: uid(), startBeat: n.startBeat + (max - min) }));
        set(s => ({
          patterns: s.patterns.map(p =>
            p.id === activePatternId ? { ...p, notes: [...p.notes, ...dup] } : p
          ),
          selectedNotes: dup.map(n => n.id),
        }));
        return;
      }
    }
    // Fallback: legacy flat notes
    const sel  = notes.filter(n => ids.has(n.id));
    const max  = Math.max(...sel.map(n => n.startBeat + n.durationBeats));
    const min  = Math.min(...sel.map(n => n.startBeat));
    const dup  = sel.map(n => ({ ...n, id: uid(), startBeat: n.startBeat + (max - min) }));
    set(s => ({ notes: [...s.notes, ...dup], selectedNotes: dup.map(n => n.id) }));
  },
  createPattern: () => {
    const id = `pat-${uid()}`;
    set(s => ({
      patterns: [...s.patterns, { id, name: `Pattern ${s.patterns.length + 1}`, duration: 16, notes: [] }],
      activePatternId: id,
    }));
    return id;
  },
  clonePattern: (sourceId) => {
    const src = get().patterns.find(p => p.id === sourceId);
    if (!src) return '';
    const newId = `pat-${uid()}`;
    set(s => ({
      patterns: [...s.patterns, { ...src, id: newId, name: `${src.name} (Clone)`, notes: src.notes.map(n => ({ ...n, id: uid() })) }],
    }));
    return newId;
  },
  makeClipUnique: (clipId) => {
    get().pushUndo();
    const { clips, patterns } = get();
    const clip = clips.find(c => c.id === clipId);
    if (!clip) return;
    const src = patterns.find(p => p.id === clip.patternId);
    if (!src) return;
    const newId = `pat-${uid()}`;
    set(s => ({
      patterns: [...s.patterns, { ...src, id: newId, name: `${src.name} (Unique)`, notes: src.notes.map(n => ({ ...n, id: uid() })) }],
      clips: s.clips.map(c => c.id === clipId ? { ...c, patternId: newId } : c),
      activePatternId: newId,
    }));
  },
  setActivePattern: (id) => set({ activePatternId: id }),
  renamePattern: (patternId, name) => set(s => ({
    patterns: s.patterns.map(p => p.id === patternId ? { ...p, name } : p),
  })),
  addNoteToPattern: (patternId, note) => {
    get().pushUndo();
    set(s => ({ patterns: s.patterns.map(p => p.id === patternId ? { ...p, notes: [...p.notes, note] } : p) }));
  },
  removeNoteFromPattern: (patternId, noteId) => {
    get().pushUndo();
    set(s => ({ patterns: s.patterns.map(p => p.id === patternId ? { ...p, notes: p.notes.filter(n => n.id !== noteId) } : p) }));
  },
  updateNotesInPattern: (patternId, updates) => {
    get().pushUndo();
    set(s => ({
      patterns: s.patterns.map(p => {
        if (p.id !== patternId) return p;
        return { ...p, notes: p.notes.map(n => { const u = updates.find(x => x.id === n.id); return u ? { ...n, ...u.changes } : n; }) };
      }),
    }));
  },
  addClip: (clip) => set(s => ({ clips: [...s.clips, clip] })),
  removeClip: (id) => set(s => ({
    clips: s.clips.filter(c => c.id !== id),
    selectedClips: s.selectedClips.filter(cid => cid !== id),
  })),
  updateClip: (id, u) => set(s => ({ clips: s.clips.map(c => c.id === id ? { ...c, ...u } : c) })),
  selectClip: (id, append = false) => set(s => ({
    selectedClips: append
      ? s.selectedClips.includes(id) ? s.selectedClips.filter(cid => cid !== id) : [...s.selectedClips, id]
      : [id],
  })),
  selectAllClips: () => set(s => ({ selectedClips: s.clips.map(c => c.id) })),
  clearClipSelection: () => set({ selectedClips: [] }),
  deleteSelectedClips: () => {
    get().pushUndo();
    const ids = new Set(get().selectedClips);
    set(s => ({ clips: s.clips.filter(c => !ids.has(c.id)), selectedClips: [] }));
  },
  copySelectedClips: () => {
    const { clips, selectedClips } = get();
    const ids = new Set(selectedClips);
    set({ clipClipboard: clips.filter(c => ids.has(c.id)).map(c => ({ ...c })) });
  },
  pasteClips: (atBeat) => {
    const { clipClipboard } = get();
    if (!clipClipboard.length) return;
    get().pushUndo();
    const minBeat = Math.min(...clipClipboard.map(c => c.startBeat));
    const offset  = atBeat !== undefined
      ? atBeat - minBeat
      : Math.max(...clipClipboard.map(c => c.startBeat + c.duration)) - minBeat;
    const pasted  = clipClipboard.map(c => ({ ...c, id: uid(), startBeat: c.startBeat + offset }));
    set(s => ({ clips: [...s.clips, ...pasted], selectedClips: pasted.map(c => c.id) }));
  },
  duplicateSelectedClips: () => {
    const { clips, selectedClips } = get();
    if (!selectedClips.length) return;
    get().pushUndo();
    const ids  = new Set(selectedClips);
    const sel  = clips.filter(c => ids.has(c.id));
    const max  = Math.max(...sel.map(c => c.startBeat + c.duration));
    const min  = Math.min(...sel.map(c => c.startBeat));
    const dup  = sel.map(c => ({ ...c, id: uid(), startBeat: c.startBeat + (max - min) }));
    set(s => ({ clips: [...s.clips, ...dup], selectedClips: dup.map(c => c.id) }));
  },
  // ── Automation ─────────────────────────────────────────────────────────────
  addAutomationLane: (trackId, target) => {
    const id = `auto-${uid()}`;
    set(s => ({
      automationLanes: [...s.automationLanes, { id, trackId, target, points: [], expanded: true }],
    }));
    return id;
  },
  removeAutomationLane: (laneId) =>
    set(s => ({ automationLanes: s.automationLanes.filter(l => l.id !== laneId) })),
  toggleAutomationLane: (laneId) =>
    set(s => ({
      automationLanes: s.automationLanes.map(l =>
        l.id === laneId ? { ...l, expanded: !l.expanded } : l
      ),
    })),
  addAutomationPoint: (laneId, point) => {
    get().pushUndo();
    set(s => ({
      automationLanes: s.automationLanes.map(l =>
        l.id !== laneId ? l : {
          ...l,
          points: [...l.points, point].sort((a, b) => a.beat - b.beat),
        }
      ),
    }));
  },
  updateAutomationPoint: (laneId, pointId, beat, value) => {
    set(s => ({
      automationLanes: s.automationLanes.map(l =>
        l.id !== laneId ? l : {
          ...l,
          points: l.points
            .map(p => p.id === pointId ? { ...p, beat, value } : p)
            .sort((a, b) => a.beat - b.beat),
        }
      ),
    }));
  },
  removeAutomationPoint: (laneId, pointId) => {
    get().pushUndo();
    set(s => ({
      automationLanes: s.automationLanes.map(l =>
        l.id !== laneId ? l : { ...l, points: l.points.filter(p => p.id !== pointId) }
      ),
    }));
  },

  // ── Sampler / instrument ───────────────────────────────────────────────────
  setSampleFile: (trackId, filePath, rootNote, oneShot) =>
    set(s => ({
      tracks: s.tracks.map(t =>
        t.id === trackId ? { ...t, synthType: 4, sampleFile: filePath, rootNote, oneShot } : t
      ),
    })),

  setTrackVoiceCount: (trackId, numVoices) =>
    set(s => ({
      tracks: s.tracks.map(t =>
        t.id === trackId ? { ...t, voiceCount: Math.max(1, numVoices) } : t
      ),
    })),

  setSynthTypeOnTrack: (trackId, synthType) =>
    set(s => ({
      tracks: s.tracks.map(t =>
        t.id === trackId
          ? { ...t, synthType, sampleFile: synthType !== 4 ? undefined : t.sampleFile }
          : t
      ),
    })),

  // ── Lua Plugins ────────────────────────────────────────────────────────────
  setLuaPlugins: (plugins) => set({ luaPlugins: plugins }),
  addLuaPlugin: (plugin) => set(s => ({ luaPlugins: [...s.luaPlugins, plugin] })),
  updateLuaPlugin: (id, changes) =>
    set(s => ({
      luaPlugins: s.luaPlugins.map(p => p.id === id ? { ...p, ...changes } : p),
    })),
  removeLuaPlugin: (id) =>
    set(s => ({ luaPlugins: s.luaPlugins.filter(p => p.id !== id) })),
}));
