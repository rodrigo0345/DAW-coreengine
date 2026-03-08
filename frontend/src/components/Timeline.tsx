import React, { useCallback, useEffect, useLayoutEffect, useMemo, useRef, useState } from 'react';
import { useStore, uid, AutomationLane, AutomationTarget } from '../store';
import { snapBeat } from '../helpers/music';
import { syncTrackToEngine, syncAutomationLaneToEngine, clearAutomationLaneFromEngine } from '../helpers/engine';
import { registerTrackScroller, unregisterTrackScroller, syncTrackScroll } from '../helpers/trackScroll';
import './Timeline.css';

// ─── Constants ────────────────────────────────────────────────────────────────
const TRACK_H          = 56;
const AUTOMATION_H     = 72;   // height of one automation lane
const MOVE_THR         = 4;
const MIN_DUR          = 1;

// Human-readable label for an automation target
function autoLabel(target: AutomationTarget): string {
  if (target.kind === 'volume') return 'Volume';
  return `${target.effectType} – ${target.paramName}`;
}
// Encode target to paramName string sent to engine
function encodeParam(target: AutomationTarget): string {
  if (target.kind === 'volume') return 'volume';
  return `${target.effectType}.${target.paramName}`;
}

// ─── Automation lane curve editor ────────────────────────────────────────────
interface AutomationLaneViewProps {
  lane: AutomationLane;
  pixelsPerBeat: number;
  totalBeats: number;
}
function AutomationLaneView({ lane, pixelsPerBeat, totalBeats }: AutomationLaneViewProps) {
  const {
    addAutomationPoint, updateAutomationPoint, removeAutomationPoint,
    toggleAutomationLane, removeAutomationLane,
  } = useStore();
  const svgRef = useRef<SVGSVGElement>(null);
  const dragging = useRef<{ pointId: string } | null>(null);
  const W = totalBeats * pixelsPerBeat;
  const H = AUTOMATION_H;
  const paramName = encodeParam(lane.target);

  // Convert beat/value → SVG coords
  const toX = (beat: number) => beat * pixelsPerBeat;
  const toY = (value: number) => H - value * (H - 8) - 4;  // 4px padding top/bottom
  const fromXY = (x: number, y: number) => ({
    beat:  Math.max(0, x / pixelsPerBeat),
    value: Math.max(0, Math.min(1, (H - y - 4) / (H - 8))),
  });

  // Build SVG path from sorted points
  const pathD = useMemo(() => {
    const pts = lane.points;
    if (!pts.length) return '';
    let d = `M ${toX(pts[0].beat)} ${toY(pts[0].value)}`;
    for (let i = 1; i < pts.length; i++) {
      d += ` L ${toX(pts[i].beat)} ${toY(pts[i].value)}`;
    }
    return d;
  }, [lane.points, pixelsPerBeat]);

  const onSvgMouseDown = (e: React.MouseEvent<SVGSVGElement>) => {
    if (e.button !== 0) return;
    const rect = svgRef.current!.getBoundingClientRect();
    const x = e.clientX - rect.left;
    const y = e.clientY - rect.top;

    // Check if clicking near an existing point
    const hit = lane.points.find(p => {
      const px = toX(p.beat);
      const py = toY(p.value);
      return Math.hypot(px - x, py - y) < 8;
    });

    if (hit) {
      // Right-click → delete
      if (e.button === 0 && e.altKey) {
        removeAutomationPoint(lane.id, hit.id);
        clearAutomationLaneFromEngine(lane.trackId, paramName);  // clear then re-send
        const newPts = lane.points.filter(p => p.id !== hit.id).map(p => ({ beat: p.beat, value: p.value }));
        syncAutomationLaneToEngine(lane.trackId, paramName, newPts);
        return;
      }
      dragging.current = { pointId: hit.id };
      return;
    }
    // Add new point
    const { beat, value } = fromXY(x, y);
    const newId = uid();
    addAutomationPoint(lane.id, { id: newId, beat, value });
    const newPts = [...lane.points.map(p => ({ beat: p.beat, value: p.value })), { beat, value }];
    syncAutomationLaneToEngine(lane.trackId, paramName, newPts);
    dragging.current = { pointId: newId };
  };

  const onSvgMouseMove = (e: React.MouseEvent<SVGSVGElement>) => {
    if (!dragging.current) return;
    const rect = svgRef.current!.getBoundingClientRect();
    const x = e.clientX - rect.left;
    const y = e.clientY - rect.top;
    const { beat, value } = fromXY(x, y);
    updateAutomationPoint(lane.id, dragging.current.pointId, beat, value);
  };

  const onSvgMouseUp = () => {
    if (!dragging.current) return;
    dragging.current = null;
    // Send final state to engine
    syncAutomationLaneToEngine(
      lane.trackId, paramName,
      lane.points.map(p => ({ beat: p.beat, value: p.value })),
    );
  };

  const onContextMenu = (e: React.MouseEvent) => {
    e.preventDefault();
    const rect = svgRef.current!.getBoundingClientRect();
    const x = e.clientX - rect.left;
    const y = e.clientY - rect.top;
    const hit = lane.points.find(p =>
      Math.hypot(toX(p.beat) - x, toY(p.value) - y) < 8
    );
    if (hit) {
      removeAutomationPoint(lane.id, hit.id);
      const newPts = lane.points.filter(p => p.id !== hit.id).map(p => ({ beat: p.beat, value: p.value }));
      syncAutomationLaneToEngine(lane.trackId, paramName, newPts);
    }
  };

  return (
    <div className="tl-auto-lane">
      {/* Header */}
      <div className="tl-auto-header">
        <button className="tl-auto-collapse" onClick={() => toggleAutomationLane(lane.id)}>
          {lane.expanded ? '▾' : '▸'}
        </button>
        <span className="tl-auto-label">{autoLabel(lane.target)}</span>
        <span className="tl-auto-hint">click=add · right-click=delete</span>
        <button className="tl-auto-remove" onClick={() => {
          clearAutomationLaneFromEngine(lane.trackId, paramName);
          removeAutomationLane(lane.id);
        }}>✕</button>
      </div>

      {/* Curve editor */}
      {lane.expanded && (
        <div className="tl-auto-canvas-wrap">
          <svg
            ref={svgRef}
            className="tl-auto-svg"
            width={W}
            height={H}
            onMouseDown={onSvgMouseDown}
            onMouseMove={onSvgMouseMove}
            onMouseUp={onSvgMouseUp}
            onMouseLeave={onSvgMouseUp}
            onContextMenu={onContextMenu}
          >
            {/* Grid lines */}
            {[0.25, 0.5, 0.75].map(v => (
              <line key={v} x1={0} x2={W} y1={toY(v)} y2={toY(v)}
                stroke="rgba(255,255,255,0.05)" strokeWidth={1} />
            ))}
            {/* Curve */}
            {pathD && <path d={pathD} fill="none" stroke="var(--accent)" strokeWidth={2} />}
            {/* Fill under curve */}
            {lane.points.length > 0 && (
              <path
                d={`${pathD} L ${toX(lane.points[lane.points.length - 1].beat)} ${H} L ${toX(lane.points[0].beat)} ${H} Z`}
                fill="rgba(79,163,224,0.12)"
              />
            )}
            {/* Control points */}
            {lane.points.map(p => (
              <circle
                key={p.id}
                cx={toX(p.beat)}
                cy={toY(p.value)}
                r={5}
                fill="var(--accent)"
                stroke="#fff"
                strokeWidth={1.5}
                style={{ cursor: 'grab' }}
              />
            ))}
          </svg>
        </div>
      )}
    </div>
  );
}

// ─── Add-automation-lane popup ────────────────────────────────────────────────
const AUTOMATION_TARGETS: { label: string; target: AutomationTarget }[] = [
  { label: 'Volume',            target: { kind: 'volume' } },
  { label: 'Reverb – Mix',      target: { kind: 'effect', effectType: 'Reverb',      paramName: 'mix' } },
  { label: 'Reverb – Size',     target: { kind: 'effect', effectType: 'Reverb',      paramName: 'roomSize' } },
  { label: 'Reverb – Damping',  target: { kind: 'effect', effectType: 'Reverb',      paramName: 'damping' } },
  { label: 'Delay – Mix',       target: { kind: 'effect', effectType: 'Delay',       paramName: 'mix' } },
  { label: 'Delay – Time',      target: { kind: 'effect', effectType: 'Delay',       paramName: 'delayMs' } },
  { label: 'Delay – Feedback',  target: { kind: 'effect', effectType: 'Delay',       paramName: 'feedback' } },
  { label: 'Distortion – Mix',  target: { kind: 'effect', effectType: 'Distortion',  paramName: 'mix' } },
  { label: 'Distortion – Drive',target: { kind: 'effect', effectType: 'Distortion',  paramName: 'drive' } },
];

// ─── Component ────────────────────────────────────────────────────────────────
type DragMode = '' | 'move' | 'resize' | 'scrub' | 'box-select';
interface DragStart { startBeat: number; duration: number; trackId: number }
interface BoxSel    { x: number; y: number; w: number; h: number }

// ─── Component ────────────────────────────────────────────────────────────────
export default function Timeline() {
  const store = useStore();
  const {
    tracks, clips, patterns,
    selectedTrack, selectedClips,
    addClip, updateClip,
    selectClip, clearClipSelection, selectAllClips,
    copySelectedClips, pasteClips, duplicateSelectedClips, deleteSelectedClips,
    makeClipUnique, renamePattern,
    activePatternId, setActivePattern, setSelectedTrack,
    bpm, sampleRate, snapValue,
    isPlaying, currentPosition, setCurrentPosition,
    pushUndo,
    pixelsPerBeat, zoomIn, zoomOut,
    automationLanes, addAutomationLane,
  } = store;

  const canvasRef    = useRef<HTMLDivElement>(null);
  const scrollRef    = useRef<HTMLDivElement>(null);
  const rulerScrollRef = useRef<HTMLDivElement>(null);

  // ── Drag state (mutable ref – avoids stale closures) ─────────────────────
  const drag = useRef({
    active:      false,
    mode:        '' as DragMode,
    clipId:      '',
    hasMoved:    false,
    originClientX: 0,
    originClientY: 0,
    originBeat:  0,     // beat under cursor at drag start (for scrub)
    startState:  {} as Record<string, DragStart>,
    box:         null as BoxSel | null,
  });

  const [, setTick] = useState(0);
  const forceRender = useCallback(() => setTick(t => t + 1), []);

  // ── Context menu ────────────────────────────────────────────────────────────
  const [ctxMenu, setCtxMenu] = useState<{ x: number; y: number; clipId: string } | null>(null);
  const [renameTarget, setRenameTarget] = useState<{ patternId: string; value: string } | null>(null);

  // ── Zoom anchor (mouse-guided zoom) ────────────────────────────────────────
  const zoomAnchor = useRef<{ beat: number; cursorX: number } | null>(null);

  useLayoutEffect(() => {
    if (zoomAnchor.current && scrollRef.current) {
      const { beat, cursorX } = zoomAnchor.current;
      scrollRef.current.scrollLeft = beat * pixelsPerBeat - cursorX;
      zoomAnchor.current = null;
    }
  }, [pixelsPerBeat]);

  // ── Derived ─────────────────────────────────────────────────────────────────
  const totalBeats = useMemo(() => {
    const maxEnd = Math.max(128, ...clips.map(c => c.startBeat + c.duration));
    return Math.ceil(maxEnd / 4) * 4 + 32;
  }, [clips]);

  const playbackBeat = useMemo(() => {
    const spb = (60 / bpm) * sampleRate;
    return currentPosition / spb;
  }, [currentPosition, bpm, sampleRate]);

  // ── Helpers ──────────────────────────────────────────────────────────────────
  const beatAtClientX = useCallback((clientX: number) => {
    const rect = canvasRef.current?.getBoundingClientRect();
    const sl   = scrollRef.current?.scrollLeft ?? 0;
    if (!rect) return 0;
    return Math.max(0, (clientX - rect.left + sl) / pixelsPerBeat);
  }, [pixelsPerBeat]);

  const trackAtClientY = useCallback((clientY: number) => {
    const rect = canvasRef.current?.getBoundingClientRect();
    const sl   = scrollRef.current?.scrollTop ?? 0;
    if (!rect) return -1;
    const relY = clientY - rect.top + sl;
    // Find which track this y falls into using dynamic tops
    const tops = useStore.getState().tracks.map((t, idx) => ({
      trackId: t.id,
      idx,
      top: Object.values(useStore.getState().tracks).reduce((acc, _, i) => {
        if (i >= idx) return acc;
        const tid = useStore.getState().tracks[i].id;
        const lanes = useStore.getState().automationLanes.filter(l => l.trackId === tid);
        return acc + TRACK_H + lanes.reduce((s, l) => s + 24 + (l.expanded ? AUTOMATION_H : 0), 0);
      }, 0),
    }));
    for (const { idx, top } of tops) {
      if (relY >= top && relY < top + TRACK_H) return idx;
    }
    return -1;
  }, []);

  const clipsOverlap = useCallback(
    (id: string, trackId: number, start: number, dur: number): boolean => {
      return useStore.getState().clips.some(c =>
        c.id !== id &&
        c.trackId === trackId &&
        start < c.startBeat + c.duration &&
        start + dur > c.startBeat,
      );
    }, [],
  );

  // ── Playback timer with looping ─────────────────────────────────────────────
  useEffect(() => {
    if (!isPlaying) return;

    const id = setInterval(() => {
      const state = useStore.getState();
      const spb   = (60 / state.bpm) * state.sampleRate;   // samples per beat

      // ── Determine loop end ─────────────────────────────────────────────────
      const soloTrack = state.tracks.find(t => t.solo) ?? null;

      let loopEndSamples: number;

      if (soloTrack) {
        // Solo: loop over the active pattern's notes only
        const pat = state.patterns.find(p => p.id === state.activePatternId);
        const lastBeat = pat && pat.notes.length > 0
          ? Math.max(...pat.notes.map(n => n.startBeat + n.durationBeats))
          : (pat?.duration ?? 16);
        loopEndSamples = Math.max(lastBeat, 1) * spb;
      } else {
        // Normal: loop over all clips in the timeline
        const lastBeat = state.clips.length > 0
          ? Math.max(...state.clips.map(c => c.startBeat + c.duration))
          : 0;
        // If nothing in the timeline yet, don't loop (let it run free)
        loopEndSamples = lastBeat > 0 ? lastBeat * spb : Infinity;
      }

      const next = state.currentPosition + state.sampleRate / 60;

      if (next >= loopEndSamples && loopEndSamples !== Infinity) {
        // Wrap around: seek engine and reset position
        setCurrentPosition(0);
        window.electronAPI?.seek(0).catch(() => {});
      } else {
        setCurrentPosition(next);
      }
    }, 16);

    return () => clearInterval(id);
  }, [isPlaying, sampleRate, setCurrentPosition]);

  // ── Wheel: zoom + scroll ─────────────────────────────────────────────────────
  useEffect(() => {
    const el = canvasRef.current;
    if (!el) return;
    const onWheel = (e: WheelEvent) => {
      if (!e.ctrlKey) return;
      e.preventDefault();
      const rect = el.getBoundingClientRect();
      const cursorX = e.clientX - rect.left;
      const beat = (( scrollRef.current?.scrollLeft ?? 0) + cursorX) / pixelsPerBeat;
      zoomAnchor.current = { beat, cursorX };
      if (e.deltaY < 0) zoomIn(); else zoomOut();
    };
    el.addEventListener('wheel', onWheel, { passive: false });
    return () => el.removeEventListener('wheel', onWheel);
  }, [pixelsPerBeat, zoomIn, zoomOut]);

  // ── Register for cross-component vertical scroll sync ────────────────────
  useEffect(() => {
    const el = scrollRef.current;
    if (!el) return;
    registerTrackScroller('timeline', el);
    return () => unregisterTrackScroller('timeline');
  }, []);

  // ── Ruler scroll sync + vertical track sync ───────────────────────────────
  const syncRulerScroll = useCallback(() => {
    const el = scrollRef.current;
    if (!el) return;
    if (rulerScrollRef.current)
      rulerScrollRef.current.scrollLeft = el.scrollLeft;
    syncTrackScroll('timeline', el.scrollTop);
  }, []);

  // ── Global mouse move/up ─────────────────────────────────────────────────────
  useEffect(() => {
    const onMove = (e: MouseEvent) => {
      const d = drag.current;
      if (!d.active) return;

      // ── Box select ────────────────────────────────────────────────────────
      if (d.mode === 'box-select') {
        const rect = canvasRef.current?.getBoundingClientRect();
        const sl   = scrollRef.current?.scrollLeft ?? 0;
        if (!rect) return;
        const ox = d.originClientX - rect.left + sl;
        const oy = d.originClientY - rect.top;
        const cx = e.clientX     - rect.left + sl;
        const cy = e.clientY     - rect.top;
        d.box = { x: Math.min(ox, cx), y: Math.min(oy, cy), w: Math.abs(cx - ox), h: Math.abs(cy - oy) };
        d.hasMoved = true;
        forceRender();
        return;
      }

      const dx = e.clientX - d.originClientX;
      if (!d.hasMoved && Math.abs(dx) < MOVE_THR) return;
      d.hasMoved = true;

      // ── Scrub ─────────────────────────────────────────────────────────────
      if (d.mode === 'scrub') {
        const beat = beatAtClientX(e.clientX);
        const spb  = (60 / bpm) * sampleRate;
        setCurrentPosition(beat * spb);
        forceRender();
        return;
      }

      // ── Move / Resize ─────────────────────────────────────────────────────
      const dBeat = dx / pixelsPerBeat;
      Object.entries(d.startState).forEach(([id, start]) => {
        let newStart = start.startBeat;
        let newDur   = start.duration;

        if (d.mode === 'resize') {
          const raw = start.duration + dBeat;
          newDur = snapValue > 0 ? Math.max(MIN_DUR, snapBeat(raw, snapValue)) : Math.max(MIN_DUR, raw);
        } else {
          const raw = start.startBeat + dBeat;
          newStart = Math.max(0, snapValue > 0 ? snapBeat(raw, snapValue) : raw);
        }

        if (!clipsOverlap(id, start.trackId, newStart, newDur)) {
          updateClip(id, d.mode === 'resize' ? { duration: newDur } : { startBeat: newStart });
        }
      });
      forceRender();
    };

    const onUp = (e: MouseEvent) => {
      const d = drag.current;
      if (!d.active) return;

      if (d.mode === 'box-select' && d.box) {
        // Commit box selection
        const boxL = d.box.x;
        const boxR = d.box.x + d.box.w;
        const boxT = d.box.y;
        const boxB = d.box.y + d.box.h;
        const newSel: string[] = [];
        useStore.getState().clips.forEach(c => {
          const tIdx = tracks.findIndex(t => t.id === c.trackId);
          if (tIdx < 0) return;
          const cL = c.startBeat * pixelsPerBeat;
          const cR = cL + c.duration * pixelsPerBeat;
          const cT = tIdx * TRACK_H;
          const cB = cT + TRACK_H;
          if (cL < boxR && cR > boxL && cT < boxB && cB > boxT) newSel.push(c.id);
        });
        if (!e.shiftKey) clearClipSelection();
        newSel.forEach(id => selectClip(id, true));
        d.box = null;
      } else if (d.mode === 'scrub') {
        window.electronAPI?.seek(useStore.getState().currentPosition).catch(() => {});
      } else if (d.hasMoved) {
        // Sync engine for moved/resized clips
        const affected = new Set<number>();
        Object.values(d.startState).forEach(s => affected.add(s.trackId));
        useStore.getState().clips.forEach(c => {
          if (d.startState[c.id]) affected.add(c.trackId);
        });
        affected.forEach(tid => syncTrackToEngine(tid));
      } else if (!d.hasMoved) {
        // Plain click on a clip without drag = select only that clip
        if (d.clipId && !e.shiftKey && !e.ctrlKey) {
          clearClipSelection();
          selectClip(d.clipId);
        }
      }

      d.active   = false;
      d.hasMoved = false;
      d.mode     = '';
      d.box      = null;
      forceRender();
    };

    window.addEventListener('mousemove', onMove);
    window.addEventListener('mouseup',   onUp);
    return () => {
      window.removeEventListener('mousemove', onMove);
      window.removeEventListener('mouseup',   onUp);
    };
  }, [
    bpm, sampleRate, snapValue, pixelsPerBeat,
    updateClip, setCurrentPosition, beatAtClientX,
    selectClip, clearClipSelection, clipsOverlap, tracks, forceRender,
  ]);

  // ── Ruler mousedown ──────────────────────────────────────────────────────────
  const onRulerDown = useCallback((e: React.MouseEvent) => {
    e.stopPropagation();
    e.preventDefault();
    const beat = beatAtClientX(e.clientX);
    const spb  = (60 / bpm) * sampleRate;
    setCurrentPosition(beat * spb);
    drag.current = { ...drag.current, active: true, mode: 'scrub', clipId: '', hasMoved: false,
      originClientX: e.clientX, originClientY: e.clientY, startState: {}, box: null };
  }, [bpm, sampleRate, setCurrentPosition, beatAtClientX]);

  // ── Clip mousedown ──────────────────────────────────────────────────────────
  const onClipDown = useCallback((e: React.MouseEvent, clipId: string, resize: boolean) => {
    e.stopPropagation();
    e.preventDefault();
    if (e.button !== 0) return;

    const clip = useStore.getState().clips.find(c => c.id === clipId);
    if (!clip) return;

    pushUndo();

    // Selection logic: ctrl/shift = toggle, otherwise select this + drag group
    const alreadySel = useStore.getState().selectedClips.includes(clipId);
    if (e.ctrlKey || e.shiftKey) {
      selectClip(clipId, true);
    } else if (!alreadySel) {
      clearClipSelection();
      selectClip(clipId);
    }
    // Use the post-update selection
    const effectiveSel = e.ctrlKey || e.shiftKey
      ? [...new Set([...useStore.getState().selectedClips, clipId])]
      : alreadySel
        ? useStore.getState().selectedClips
        : [clipId];

    const startState: Record<string, DragStart> = {};
    effectiveSel.forEach(id => {
      const c = useStore.getState().clips.find(x => x.id === id);
      if (c) startState[id] = { startBeat: c.startBeat, duration: c.duration, trackId: c.trackId };
    });

    drag.current = {
      active: true,
      mode:   resize ? 'resize' : 'move',
      clipId,
      hasMoved: false,
      originClientX: e.clientX,
      originClientY: e.clientY,
      originBeat: beatAtClientX(e.clientX),
      startState,
      box: null,
    };
  }, [selectClip, clearClipSelection, pushUndo, beatAtClientX]);

  // ── Canvas mousedown (empty space = add clip OR box-select) ────────────────
  const onCanvasDown = useCallback((e: React.MouseEvent) => {
    if (e.button !== 0) return;
    e.preventDefault(); // prevent text selection on drag
    setCtxMenu(null);

    const beat     = beatAtClientX(e.clientX);
    const tIdx     = trackAtClientY(e.clientY);
    if (tIdx < 0 || tIdx >= tracks.length) return;
    const t = tracks[tIdx];

    // Middle of a track lane – check if we're over an existing clip
    // (clip events stop propagation so this fires only on empty space)

    if (e.ctrlKey) {
      // Box-select start
      drag.current = { ...drag.current, active: true, mode: 'box-select', clipId: '',
        hasMoved: false, originClientX: e.clientX, originClientY: e.clientY,
        originBeat: beat, startState: {}, box: { x: 0, y: 0, w: 0, h: 0 } };
      return;
    }

    // Plain click → place new clip from active pattern
    const startX = e.clientX;
    const onUp = (upEvt: MouseEvent) => {
      window.removeEventListener('mouseup', onUp);
      if (Math.abs(upEvt.clientX - startX) > MOVE_THR) return;
      if (drag.current.active) return;

      const snappedBeat = snapValue > 0 ? snapBeat(beat, snapValue) : Math.floor(beat);
      const patId = useStore.getState().activePatternId || useStore.getState().patterns[0]?.id;
      if (!patId) return;

      const newClip = { id: uid(), trackId: t.id, patternId: patId, startBeat: snappedBeat, duration: 16 };
      if (clipsOverlap(newClip.id, t.id, snappedBeat, 16)) return; // don't place on top of existing

      setSelectedTrack(t.id);
      addClip(newClip);
      clearClipSelection();
      selectClip(newClip.id);
      syncTrackToEngine(t.id);
    };
    window.addEventListener('mouseup', onUp, { once: true });
  }, [totalBeats, tracks, snapValue, beatAtClientX, trackAtClientY, addClip, selectClip, clearClipSelection, setSelectedTrack, clipsOverlap]);

  // ── Clip right-click context menu ────────────────────────────────────────────
  const onClipContext = useCallback((e: React.MouseEvent, clipId: string) => {
    e.preventDefault();
    e.stopPropagation();
    if (!useStore.getState().selectedClips.includes(clipId)) {
      clearClipSelection();
      selectClip(clipId);
    }
    setCtxMenu({ x: e.clientX, y: e.clientY, clipId });
  }, [selectClip, clearClipSelection]);

  // ── Double-click: open pattern in piano roll ─────────────────────────────────
  const onClipDblClick = useCallback((e: React.MouseEvent, clipId: string) => {
    e.stopPropagation();
    const clip = clips.find(c => c.id === clipId);
    if (!clip) return;
    setActivePattern(clip.patternId);
    setSelectedTrack(clip.trackId);
  }, [clips, setActivePattern, setSelectedTrack]);

  // ── Context menu actions ─────────────────────────────────────────────────────
  const handleCtxAction = useCallback((action: string) => {
    if (!ctxMenu) return;
    const { clipId } = ctxMenu;
    const clip = useStore.getState().clips.find(c => c.id === clipId);
    setCtxMenu(null);

    switch (action) {
      case 'open':
        if (clip) { setActivePattern(clip.patternId); setSelectedTrack(clip.trackId); }
        break;
      case 'make-unique':
        makeClipUnique(clipId);
        if (clip) syncTrackToEngine(clip.trackId);
        break;
      case 'rename': {
        if (!clip) break;
        const pat = useStore.getState().patterns.find(p => p.id === clip.patternId);
        if (pat) setRenameTarget({ patternId: pat.id, value: pat.name });
        break;
      }
      case 'duplicate':
        duplicateSelectedClips();
        useStore.getState().selectedClips.forEach(id => {
          const c = useStore.getState().clips.find(x => x.id === id);
          if (c) syncTrackToEngine(c.trackId);
        });
        break;
      case 'delete':
        if (clip) {
          deleteSelectedClips();
          syncTrackToEngine(clip.trackId);
        }
        break;
    }
  }, [ctxMenu, setActivePattern, setSelectedTrack, makeClipUnique, duplicateSelectedClips, deleteSelectedClips]);

  // Close ctx menu on outside click
  useEffect(() => {
    const close = (e: MouseEvent) => {
      if (!(e.target as Element).closest('.tl-ctx-menu')) setCtxMenu(null);
    };
    window.addEventListener('mousedown', close);
    return () => window.removeEventListener('mousedown', close);
  }, []);

  // ── Keyboard shortcuts (when timeline is focused) ────────────────────────────
  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      const tag = (e.target as HTMLElement).tagName;
      if (tag === 'INPUT' || tag === 'TEXTAREA' || tag === 'SELECT') return;
      const ctrl = e.ctrlKey || e.metaKey;

      if ((e.key === 'Delete' || e.key === 'Backspace') && useStore.getState().selectedClips.length) {
        e.preventDefault();
        const ids = new Set(useStore.getState().selectedClips);
        const tids = new Set(useStore.getState().clips.filter(c => ids.has(c.id)).map(c => c.trackId));
        deleteSelectedClips();
        tids.forEach(tid => syncTrackToEngine(tid));
        return;
      }
      if (!ctrl) return;
      switch (e.key.toLowerCase()) {
        case 'a': e.preventDefault(); selectAllClips(); break;
        case 'c': e.preventDefault(); copySelectedClips(); break;
        case 'v': e.preventDefault();
          pasteClips();
          useStore.getState().selectedClips.forEach(id => {
            const c = useStore.getState().clips.find(x => x.id === id);
            if (c) syncTrackToEngine(c.trackId);
          });
          break;
        case 'b': e.preventDefault();
          duplicateSelectedClips();
          useStore.getState().selectedClips.forEach(id => {
            const c = useStore.getState().clips.find(x => x.id === id);
            if (c) syncTrackToEngine(c.trackId);
          });
          break;
        case 'd': e.preventDefault(); clearClipSelection(); break;
      }
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [deleteSelectedClips, selectAllClips, copySelectedClips, pasteClips,
      duplicateSelectedClips, clearClipSelection]);

  // ── Mini-note preview renderer ──────────────────────────────────────────────
  const renderPreview = useCallback((patternId: string, clipDuration: number) => {
    const pat = patterns.find(p => p.id === patternId);
    if (!pat || !pat.notes.length) return null;
    const allMidi = pat.notes.map(n => n.midiNote);
    const midiMin = Math.min(...allMidi);
    const midiMax = Math.max(...allMidi);
    const midiRange = Math.max(midiMax - midiMin, 1);

    return pat.notes.map(n => {
      if (n.startBeat >= clipDuration) return null;
      const left    = (n.startBeat / clipDuration) * 100;
      const width   = Math.max(0.5, (Math.min(n.durationBeats, clipDuration - n.startBeat) / clipDuration) * 100);
      // Map midi to vertical position (top = high pitch)
      const topPct  = 100 - ((n.midiNote - midiMin) / midiRange) * 80 - 10; // 10-90% range
      return (
        <div
          key={n.id}
          className="tl-clip-note"
          style={{ left: `${left}%`, width: `${width}%`, top: `${topPct}%` }}
        />
      );
    });
  }, [patterns]);

  // ── Render ───────────────────────────────────────────────────────────────────
  // Track right-click context menu (automation + track actions)
  const [trackCtxMenu, setTrackCtxMenu] = useState<{
    trackId: number; x: number; y: number; showAuto: boolean;
  } | null>(null);

  // Close track ctx menu on outside click
  useEffect(() => {
    const close = (e: MouseEvent) => {
      if (!(e.target as Element).closest('.tl-track-ctx-menu')) setTrackCtxMenu(null);
    };
    window.addEventListener('mousedown', close);
    return () => window.removeEventListener('mousedown', close);
  }, []);

  const gridW = totalBeats * pixelsPerBeat;

  // Height: each track lane + each expanded automation lane below it
  const gridH = useMemo(() => {
    let h = 0;
    tracks.forEach(t => {
      h += TRACK_H;
      automationLanes
        .filter(l => l.trackId === t.id)
        .forEach(l => { h += 24 + (l.expanded ? AUTOMATION_H : 0); }); // 24 = header height
    });
    return Math.max(h, 200);
  }, [tracks, automationLanes]);

  // Compute each track's top offset (accounts for automation lanes above it)
  const trackTops = useMemo(() => {
    const tops: Record<number, number> = {};
    let y = 0;
    tracks.forEach(t => {
      tops[t.id] = y;
      y += TRACK_H;
      automationLanes
        .filter(l => l.trackId === t.id)
        .forEach(l => { y += 24 + (l.expanded ? AUTOMATION_H : 0); });
    });
    return tops;
  }, [tracks, automationLanes]);

  const d = drag.current;

  return (
    <div className="tl" onContextMenu={e => e.preventDefault()}>

      {/* ── Ruler ─────────────────────────────────────────────────────────── */}
      <div className="tl-ruler-row">
        <div className="tl-ruler-scroll" ref={rulerScrollRef} style={{ overflow: 'hidden' }}>
          <div className="tl-ruler" style={{ width: gridW }} onMouseDown={onRulerDown}>
            {Array.from({ length: Math.ceil(totalBeats / 4) }, (_, bar) => (
              <div key={bar} className="tl-bar-mark" style={{ left: bar * 4 * pixelsPerBeat }}>
                {bar + 1}
                {/* Beat ticks */}
                {[1, 2, 3].map(b => (
                  <div key={b} className="tl-beat-tick" style={{ left: b * pixelsPerBeat }} />
                ))}
              </div>
            ))}
            <div className="tl-cursor-ruler" style={{ left: playbackBeat * pixelsPerBeat }} />
          </div>
        </div>
      </div>

      {/* ── Canvas ────────────────────────────────────────────────────────── */}
      <div className="tl-scroll" ref={scrollRef} onScroll={syncRulerScroll}>
        <div
          ref={canvasRef}
          className="tl-canvas"
          style={{ width: gridW, height: gridH }}
          draggable={false}
          onDragStart={e => e.preventDefault()}
          onMouseDown={onCanvasDown}
        >
          {/* Track lane backgrounds + automation lanes */}
          {tracks.map((track, idx) => {
            const top = trackTops[track.id] ?? idx * TRACK_H;
            const trackLanes = automationLanes.filter(l => l.trackId === track.id);
            return (
              <React.Fragment key={track.id}>
                {/* Track lane — right-click opens automation/track menu */}
                <div
                  className={`tl-lane ${selectedTrack === track.id ? 'sel' : ''}`}
                  style={{ top, height: TRACK_H, '--lane-color': track.color } as React.CSSProperties}
                  onContextMenu={e => {
                    e.preventDefault();
                    e.stopPropagation();
                    setTrackCtxMenu({ trackId: track.id, x: e.clientX, y: e.clientY, showAuto: false });
                  }}
                />
                {/* Automation lanes */}
                {trackLanes.map(lane => (
                  <div
                    key={lane.id}
                    className="tl-auto-lane-row"
                    style={{
                      position: 'absolute',
                      left: 0,
                      width: '100%',
                      top: top + TRACK_H + trackLanes.slice(0, trackLanes.indexOf(lane)).reduce(
                        (s, l) => s + 24 + (l.expanded ? AUTOMATION_H : 0), 0
                      ),
                    }}
                  >
                    <AutomationLaneView
                      lane={lane}
                      pixelsPerBeat={pixelsPerBeat}
                      totalBeats={totalBeats}
                    />
                  </div>
                ))}
              </React.Fragment>
            );
          })}

          {/* Bar / beat grid lines */}
          {Array.from({ length: totalBeats }, (_, i) => (
            <div key={i} className={`tl-vline ${i % 4 === 0 ? 'bar' : ''}`}
              style={{ left: i * pixelsPerBeat }} />
          ))}

          {/* Playback cursor */}
          <div className={`tl-cursor ${isPlaying ? 'playing' : ''}`} style={{ left: playbackBeat * pixelsPerBeat }}>
            <div className="tl-cursor-head" />
            <div className="tl-cursor-line" />
          </div>

          {/* Clips */}
          {clips.map(clip => {
            const tIdx = tracks.findIndex(t => t.id === clip.trackId);
            if (tIdx < 0) return null;
            const track    = tracks[tIdx];
            const sel      = selectedClips.includes(clip.id);
            const isActive = clip.patternId === activePatternId;
            const pat      = patterns.find(p => p.id === clip.patternId);
            const w        = Math.max(8, clip.duration * pixelsPerBeat - 2);
            const top      = (trackTops[track.id] ?? tIdx * TRACK_H) + 2;

            // Audio clip: sampler track that hasn't opted into MIDI view
            const isAudioClip = track.synthType === 4 && track.sampleFile && !track.useMidi;

            return (
              <div
                key={clip.id}
                className={`tl-clip ${sel ? 'sel' : ''} ${isActive ? 'active-pat' : ''} ${isAudioClip ? 'audio-clip' : ''}`}
                style={{
                  left:   clip.startBeat * pixelsPerBeat,
                  top,
                  width:  w,
                  height: TRACK_H - 4,
                  '--clip-color': track.color,
                } as React.CSSProperties}
                onMouseDown={e => onClipDown(e, clip.id, false)}
                onDoubleClick={e => onClipDblClick(e, clip.id)}
                onContextMenu={e => onClipContext(e, clip.id)}
              >
                {isAudioClip ? (
                  /* ── Audio clip (FL-studio style) ── */
                  <>
                    <div className="tl-clip-audio-label">
                      <span className="tl-clip-audio-icon">▶</span>
                      {track.sampleFile!.split('/').pop()?.replace(/\.[^.]+$/, '')}
                    </div>
                    <div className="tl-clip-waveform" aria-hidden>
                      {Array.from({ length: Math.max(4, Math.floor(w / 6)) }).map((_, i) => {
                        const h = 30 + Math.sin(i * 1.7) * 18 + Math.sin(i * 0.9 + 1) * 12;
                        return <span key={i} className="tl-wf-bar" style={{ height: `${Math.abs(h)}%` }} />;
                      })}
                    </div>
                  </>
                ) : (
                  /* ── MIDI clip ── */
                  <>
                    <div className="tl-clip-label">{pat?.name ?? '?'}</div>
                    <div className="tl-clip-preview">
                      {renderPreview(clip.patternId, clip.duration)}
                    </div>
                  </>
                )}

                {/* Resize handle */}
                <div
                  className="tl-clip-resize"
                  onMouseDown={e => { e.stopPropagation(); onClipDown(e, clip.id, true); }}
                />
              </div>
            );
          })}

          {/* Box-selection overlay */}
          {d.mode === 'box-select' && d.box && (
            <div
              className="tl-box-sel"
              style={{
                left:   d.box.x - (scrollRef.current?.scrollLeft ?? 0),
                top:    d.box.y,
                width:  d.box.w,
                height: d.box.h,
              }}
            />
          )}
        </div>
      </div>

      {/* ── Track right-click menu (automation + track actions) ───────── */}
      {trackCtxMenu && (() => {
        const { trackId, x, y, showAuto } = trackCtxMenu;
        const trackLanes = automationLanes.filter(l => l.trackId === trackId);
        return (
          <div className="tl-track-ctx-menu" style={{ left: x, top: y }}>
            <div className="tl-ctx-title">Track {trackId + 1}</div>

            {/* ── Existing automation lanes ── */}
            {trackLanes.length > 0 && (
              <>
                <div className="tl-ctx-section">Automation</div>
                {trackLanes.map(lane => (
                  <button
                    key={lane.id}
                    className="tl-ctx-item tl-ctx-item--lane"
                    onClick={() => {
                      useStore.getState().toggleAutomationLane(lane.id);
                      setTrackCtxMenu(null);
                    }}
                  >
                    <span className="tl-ctx-check">{lane.expanded ? '▾' : '▸'}</span>
                    {autoLabel(lane.target)}
                    <span
                      className="tl-ctx-remove"
                      title="Remove lane"
                      onClick={e => {
                        e.stopPropagation();
                        clearAutomationLaneFromEngine(trackId, encodeParam(lane.target));
                        useStore.getState().removeAutomationLane(lane.id);
                        setTrackCtxMenu(null);
                      }}
                    >✕</span>
                  </button>
                ))}
                <div className="tl-ctx-sep" />
              </>
            )}

            {/* ── Add automation submenu ── */}
            <button
              className="tl-ctx-item tl-ctx-item--sub"
              onMouseEnter={() => setTrackCtxMenu(m => m ? { ...m, showAuto: true } : m)}
            >
              + Add Automation ▸
            </button>

            {showAuto && (
              <div className="tl-track-ctx-menu tl-ctx-submenu" style={{ left: 190, top: 0 }}>
                {AUTOMATION_TARGETS.map(({ label, target }) => {
                  const already = trackLanes.some(l => encodeParam(l.target) === encodeParam(target));
                  return (
                    <button
                      key={label}
                      className={`tl-ctx-item ${already ? 'tl-ctx-item--active' : ''}`}
                      onClick={() => {
                        if (!already) addAutomationLane(trackId, target);
                        setTrackCtxMenu(null);
                      }}
                    >
                      {already && <span className="tl-ctx-check">✓</span>}
                      {label}
                    </button>
                  );
                })}
              </div>
            )}
          </div>
        );
      })()}

      {/* ── Context menu ────────────────────────────────────────────────── */}
      {ctxMenu && (
        <div className="tl-ctx-menu" style={{ left: ctxMenu.x, top: ctxMenu.y }}>
          <button onClick={() => handleCtxAction('open')}>✏ Edit in Piano Roll</button>
          <button onClick={() => handleCtxAction('rename')}>✏ Rename Pattern…</button>
          <div className="tl-ctx-sep" />
          <button onClick={() => handleCtxAction('make-unique')}>⎇ Make Unique</button>
          <button onClick={() => handleCtxAction('duplicate')}>⎘ Duplicate</button>
          <div className="tl-ctx-sep" />
          <button className="danger" onClick={() => handleCtxAction('delete')}>🗑 Delete</button>
        </div>
      )}

      {/* ── Rename modal ────────────────────────────────────────────────── */}
      {renameTarget && (
        <div className="tl-rename-overlay" onClick={() => setRenameTarget(null)}>
          <div className="tl-rename-modal" onClick={e => e.stopPropagation()}>
            <label>Pattern name</label>
            <input
              autoFocus
              value={renameTarget.value}
              onChange={e => setRenameTarget(r => r ? { ...r, value: e.target.value } : r)}
              onKeyDown={e => {
                if (e.key === 'Enter') {
                  renamePattern(renameTarget.patternId, renameTarget.value);
                  setRenameTarget(null);
                } else if (e.key === 'Escape') {
                  setRenameTarget(null);
                }
              }}
            />
            <div className="tl-rename-btns">
              <button onClick={() => setRenameTarget(null)}>Cancel</button>
              <button className="primary" onClick={() => {
                renamePattern(renameTarget.patternId, renameTarget.value);
                setRenameTarget(null);
              }}>OK</button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
