// Shared MIDI / music helpers used across components

export const NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'] as const;
export const BLACK_KEYS = new Set([1, 3, 6, 8, 10]); // C#, D#, F#, G#, A#

export const MIN_MIDI = 24;  // C1
export const MAX_MIDI = 96;  // C7
export const MIDI_RANGE = MAX_MIDI - MIN_MIDI + 1;

// ─── Scales ──────────────────────────────────────────────────────────────────
// Each value is an array of semitone intervals from the root (0 = root).
export const SCALES: Record<string, number[]> = {
  'None':             [],
  'Major':            [0, 2, 4, 5, 7, 9, 11],
  'Natural Minor':    [0, 2, 3, 5, 7, 8, 10],
  'Harmonic Minor':   [0, 2, 3, 5, 7, 8, 11],
  'Melodic Minor':    [0, 2, 3, 5, 7, 9, 11],
  'Dorian':           [0, 2, 3, 5, 7, 9, 10],
  'Phrygian':         [0, 1, 3, 5, 7, 8, 10],
  'Lydian':           [0, 2, 4, 6, 7, 9, 11],
  'Mixolydian':       [0, 2, 4, 5, 7, 9, 10],
  'Locrian':          [0, 1, 3, 5, 6, 8, 10],
  'Pentatonic Major': [0, 2, 4, 7, 9],
  'Pentatonic Minor': [0, 3, 5, 7, 10],
  'Blues':            [0, 3, 5, 6, 7, 10],
  'Whole Tone':       [0, 2, 4, 6, 8, 10],
  'Diminished':       [0, 2, 3, 5, 6, 8, 9, 11],
  'Chromatic':        [0,1,2,3,4,5,6,7,8,9,10,11],
};

export const SCALE_ROOTS = NOTE_NAMES as unknown as string[];

/**
 * Returns a Set of pitch-classes (0–11) that belong to the given scale + root.
 * Empty set when scale is 'None'.
 */
export function getScalePitchClasses(scale: string, root: number): Set<number> {
  const intervals = SCALES[scale];
  if (!intervals || intervals.length === 0) return new Set();
  return new Set(intervals.map(i => (root + i) % 12));
}

export function midiNoteName(midi: number): string {
  const octave = Math.floor(midi / 12) - 1;
  return `${NOTE_NAMES[midi % 12]}${octave}`;
}

export function isBlackKey(midi: number): boolean {
  return BLACK_KEYS.has(midi % 12);
}

/** Snap a beat value to the nearest grid position. 0 = no snap. */
export function snapBeat(beat: number, snap: number): number {
  if (snap <= 0) return beat;
  return Math.round(beat / snap) * snap;
}

/** Generate a short random id */
export function uid(): string {
  return Math.random().toString(36).slice(2, 11);
}

