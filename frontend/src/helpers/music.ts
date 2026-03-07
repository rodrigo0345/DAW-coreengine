// Shared MIDI / music helpers used across components

export const NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'] as const;
export const BLACK_KEYS = new Set([1, 3, 6, 8, 10]); // C#, D#, F#, G#, A#

export const MIN_MIDI = 24;  // C1
export const MAX_MIDI = 96;  // C7
export const MIDI_RANGE = MAX_MIDI - MIN_MIDI + 1;

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

