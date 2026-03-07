/**
 * Shared vertical-scroll synchronisation between TrackList and Timeline.
 *
 * Both panels display the same set of tracks row-by-row.  When either
 * panel is scrolled vertically, the other should follow so the rows
 * always line up.
 *
 * Usage:
 *   registerTrackScroller('tracklist', ref)   – in TrackList
 *   registerTrackScroller('timeline',  ref)   – in Timeline
 *   unregisterTrackScroller('...')            – on unmount
 *   syncTrackScroll('timeline', 120)          – broadcast from a scroll handler
 */

type ScrollerId = 'tracklist' | 'timeline';

const scrollers = new Map<ScrollerId, HTMLElement>();
let syncing = false;   // re-entrancy guard

export function registerTrackScroller(id: ScrollerId, el: HTMLElement) {
  scrollers.set(id, el);
}

export function unregisterTrackScroller(id: ScrollerId) {
  scrollers.delete(id);
}

/**
 * Call from the onScroll handler of the element identified by `source`.
 * Propagates the scrollTop to all other registered scrollers.
 */
export function syncTrackScroll(source: ScrollerId, scrollTop: number) {
  if (syncing) return;
  syncing = true;
  scrollers.forEach((el, id) => {
    if (id !== source && Math.round(el.scrollTop) !== Math.round(scrollTop)) {
      el.scrollTop = scrollTop;
    }
  });
  syncing = false;
}

