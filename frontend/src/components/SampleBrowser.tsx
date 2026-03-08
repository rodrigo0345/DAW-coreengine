import { useState, useEffect, useRef, useCallback } from 'react';
import './SampleBrowser.css';

// ─── Types ────────────────────────────────────────────────────────────────────
interface DirEntry {
  name: string;
  path: string;
  isDir: boolean;
  ext: string;
}

// ─── Waveform animation ───────────────────────────────────────────────────────
function WaveformBar() {
  return (
    <span className="sb-waveform">
      {Array.from({ length: 10 }).map((_, i) => (
        <span key={i} className="sb-waveform-bar" style={{ animationDelay: `${(i * 70) % 500}ms` }} />
      ))}
    </span>
  );
}

// ─── File row ─────────────────────────────────────────────────────────────────
interface FileRowProps {
  entry: DirEntry;
  isPlaying: boolean;
  isLoading: boolean;
  onPreview: (e: DirEntry) => void;
  onStop: () => void;
}
function FileRow({ entry, isPlaying, isLoading, onPreview, onStop }: FileRowProps) {
  const handleDragStart = (e: React.DragEvent) => {
    // Set with multiple types — Chromium may drop custom types across origins
    // but within the same Electron window all types survive
    e.dataTransfer.setData('application/x-sample-path', entry.path);
    e.dataTransfer.setData('text/plain', entry.path);
    e.dataTransfer.setData('text/uri-list', `file://${entry.path}`);
    e.dataTransfer.effectAllowed = 'copy';
    const ghost = document.createElement('div');
    ghost.textContent = `🎵 ${entry.name}`;
    ghost.style.cssText =
      'position:fixed;top:-9999px;left:-9999px;background:#1e3a5f;color:#fff;' +
      'padding:4px 10px;border-radius:4px;font-size:12px;white-space:nowrap;border:1px solid #5c9eff;';
    document.body.appendChild(ghost);
    e.dataTransfer.setDragImage(ghost, 0, 14);
    setTimeout(() => document.body.removeChild(ghost), 0);
  };

  return (
    <div
      className={`sb-file-row ${isPlaying ? 'playing' : ''} ${isLoading ? 'loading' : ''}`}
      draggable
      onDragStart={handleDragStart}
      onClick={() => { if (isLoading) return; isPlaying ? onStop() : onPreview(entry); }}
      title={`${entry.name}\n${entry.path}\nClick to preview · Drag to track`}
    >
      <span className="sb-file-icon">
        {isPlaying ? <WaveformBar /> : isLoading ? <span className="sb-loading-dot">…</span> : '🎵'}
      </span>
      <span className="sb-file-name">{entry.name}</span>
      <span className="sb-file-ext">{entry.ext.replace('.', '').toUpperCase()}</span>
      <button
        className="sb-preview-btn"
        title={isPlaying ? 'Stop' : isLoading ? 'Loading…' : 'Preview'}
        disabled={isLoading}
        onClick={ev => { ev.stopPropagation(); if (isLoading) return; isPlaying ? onStop() : onPreview(entry); }}
      >{isPlaying ? '■' : isLoading ? '…' : '▶'}</button>
    </div>
  );
}

// ─── Folder row ───────────────────────────────────────────────────────────────
interface FolderRowProps {
  entry: DirEntry;
  onNavigate: (path: string) => void;
}
function FolderRow({ entry, onNavigate }: FolderRowProps) {
  return (
    <div
      className="sb-folder-row"
      onClick={() => onNavigate(entry.path)}
      title={`Open ${entry.name}`}
    >
      <span className="sb-folder-icon">📁</span>
      <span className="sb-folder-name">{entry.name}</span>
      <span className="sb-folder-arrow">›</span>
    </div>
  );
}

// ─── Sample Browser ───────────────────────────────────────────────────────────
export default function SampleBrowser() {
  const [currentDir, setCurrentDir]         = useState('/home/rodrigo0345/Music');
  const [entries, setEntries]               = useState<DirEntry[]>([]);
  const [loading, setLoading]               = useState(false);
  const [playingPath, setPlayingPath]       = useState<string | null>(null);
  const [search, setSearch]                 = useState('');
  const [searchResults, setSearchResults]   = useState<DirEntry[] | null>(null);
  const [searching, setSearching]           = useState(false);
  const [showDirInput, setShowDirInput]     = useState(false);
  const [customDir, setCustomDir]           = useState('');
  const audioRef    = useRef<HTMLAudioElement | null>(null);
  const searchTimer = useRef<ReturnType<typeof setTimeout> | null>(null);

  // ── Init ─────────────────────────────────────────────────────────────────
  useEffect(() => {
    (async () => {
      const dir: string = await (window.electronAPI as any)?.getMusicDir?.() ?? '/home/rodrigo0345/Music';
      navigate(dir);
    })();
  }, []);

  const navigate = useCallback(async (dir: string) => {
    setLoading(true);
    setCurrentDir(dir);
    setSearch('');
    setSearchResults(null);
    const items: DirEntry[] = await (window.electronAPI as any)?.listDir(dir) ?? [];
    setEntries(items);
    setLoading(false);
  }, []);

  const [loadingPath, setLoadingPath] = useState<string | null>(null);

  // ── Preview ───────────────────────────────────────────────────────────────
  const stopPreview = useCallback(() => {
    if (audioRef.current) {
      audioRef.current.onended = null;
      audioRef.current.onerror = null;
      audioRef.current.pause();
      audioRef.current.src = '';
      audioRef.current.load(); // force release
      audioRef.current = null;
    }
    setPlayingPath(null);
    setLoadingPath(null);
  }, []);

  const previewSample = useCallback(async (entry: DirEntry) => {
    stopPreview();
    setLoadingPath(entry.path);
    const url: string | null = await (window.electronAPI as any)?.getAudioUrl(entry.path) ?? null;
    // Check we weren't cancelled while awaiting
    if (!url) { setLoadingPath(null); return; }
    const audio = new Audio();
    audio.src = url;
    audio.volume = 0.8;
    audioRef.current = audio;
    audio.onended = () => { setPlayingPath(null); setLoadingPath(null); };
    audio.onerror = (e) => { console.warn('Preview error:', e); setPlayingPath(null); setLoadingPath(null); };
    try {
      await audio.play();
      setLoadingPath(null);
      setPlayingPath(entry.path);
    } catch (err) {
      console.warn('Preview failed:', err);
      setLoadingPath(null);
      setPlayingPath(null);
    }
  }, [stopPreview]);

  useEffect(() => () => stopPreview(), []);

  // ── Search ────────────────────────────────────────────────────────────────
  const runSearch = useCallback(async (q: string) => {
    if (!q.trim()) { setSearchResults(null); return; }
    setSearching(true);
    const results: DirEntry[] = [];
    const scanDir = async (dir: string, depth: number) => {
      if (depth > 8 || results.length >= 300) return;
      const items: DirEntry[] = await (window.electronAPI as any)?.listDir(dir) ?? [];
      for (const item of items) {
        if (item.isDir) {
          await scanDir(item.path, depth + 1);
        } else if (item.name.toLowerCase().includes(q.toLowerCase())) {
          results.push(item);
        }
      }
    };
    await scanDir(currentDir, 0);
    setSearchResults(results);
    setSearching(false);
  }, [currentDir]);

  const onSearchChange = (v: string) => {
    setSearch(v);
    if (searchTimer.current) clearTimeout(searchTimer.current);
    if (!v.trim()) { setSearchResults(null); return; }
    searchTimer.current = setTimeout(() => runSearch(v), 350);
  };

  // ── Navigation ────────────────────────────────────────────────────────────
  const goUp = () => {
    const parts = currentDir.split('/').filter(Boolean);
    if (parts.length === 0) return;
    const parent = '/' + parts.slice(0, -1).join('/');
    navigate(parent || '/');
  };

  const handleSetDir = () => {
    const d = customDir.trim();
    if (!d) return;
    navigate(d);
    setShowDirInput(false);
    setCustomDir('');
  };

  const dirs  = (searchResults ?? entries).filter(e => e.isDir);
  const files = (searchResults ?? entries).filter(e => !e.isDir);
  const inSearch = searchResults !== null;

  // ─────────────────────────────────────────────────────────────────────────
  return (
    <div className="sb-root">

      {/* ── Header ─────────────────────────────────────────────────────── */}
      <div className="sb-header">
        <div className="sb-breadcrumb">
          <button className="sb-up-btn" onClick={goUp} title="Go up one folder">↑</button>
          <span className="sb-path" title={currentDir}>{currentDir.split('/').filter(Boolean).pop() || '/'}</span>
          <button className="sb-edit-btn" onClick={() => setShowDirInput(v => !v)} title="Change root folder">✎</button>
        </div>

        {showDirInput && (
          <div className="sb-dir-input-row">
            <input
              className="sb-dir-input"
              value={customDir}
              onChange={e => setCustomDir(e.target.value)}
              onKeyDown={e => e.key === 'Enter' && handleSetDir()}
              placeholder={currentDir}
              autoFocus
            />
            <button className="sb-dir-ok" onClick={handleSetDir}>Go</button>
          </div>
        )}

        <div className="sb-search-row">
          <span className="sb-search-icon">🔍</span>
          <input
            className="sb-search"
            value={search}
            onChange={e => onSearchChange(e.target.value)}
            placeholder="Search samples…"
          />
          {searching && <span className="sb-loading-dot">…</span>}
          {search && !searching && (
            <button className="sb-clear-search" onClick={() => { setSearch(''); setSearchResults(null); }}>✕</button>
          )}
        </div>
      </div>

      {/* ── Hint ───────────────────────────────────────────────────────── */}
      <div className="sb-hint">▶ preview &nbsp;·&nbsp; drag → track &nbsp;·&nbsp; click folder to open</div>

      {/* ── Body ───────────────────────────────────────────────────────── */}
      <div className="sb-body">
        {loading ? (
          <div className="sb-empty">Loading…</div>
        ) : entries.length === 0 && !inSearch ? (
          <div className="sb-empty">Nothing found in<br /><code>{currentDir}</code></div>
        ) : inSearch && searchResults!.length === 0 ? (
          <div className="sb-empty">No results for "{search}"</div>
        ) : (
          <>
            {/* Folders */}
            {!inSearch && dirs.map(e => (
              <FolderRow key={e.path} entry={e} onNavigate={navigate} />
            ))}

            {/* Audio files */}
            {files.map(e => (
              <div key={e.path} className={inSearch ? 'sb-search-result' : undefined}>
                <FileRow
                  entry={e}
                  isPlaying={playingPath === e.path}
                  isLoading={loadingPath === e.path}
                  onPreview={previewSample}
                  onStop={stopPreview}
                />
                {inSearch && (
                  <div className="sb-result-path">
                    {e.path.replace(currentDir + '/', '').split('/').slice(0, -1).join(' / ') || currentDir.split('/').pop()}
                  </div>
                )}
              </div>
            ))}
          </>
        )}
      </div>

      {/* ── Now-playing bar ────────────────────────────────────────────── */}
      {playingPath && (
        <div className="sb-now-playing">
          <WaveformBar />
          <span className="sb-now-playing-name">{playingPath.split('/').pop()}</span>
          <button className="sb-stop-btn" onClick={stopPreview} title="Stop">■</button>
        </div>
      )}
    </div>
  );
}
