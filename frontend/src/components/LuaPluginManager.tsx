import { useState, useEffect, useCallback } from 'react';
import { useStore, LuaPlugin } from '../store';
import './LuaPluginManager.css';

const DEFAULT_LUA = `-- Effect plugin: processBlock receives the mixed audio and can modify it.
-- Use buffer:getChannel(n) for direct zero-copy array access (fast path).
function processBlock(buffer)
  local n = buffer.numSamples
  local l = buffer:getChannel(0)
  local r = buffer:getChannel(1)
  for i = 0, n - 1 do
    l[i] = l[i] * 0.8   -- simple gain reduction, left
    r[i] = r[i] * 0.8   -- simple gain reduction, right
  end
end
`;

const INSTRUMENT_LUA = `-- Instrument plugin: define noteOn/noteOff to generate audio from scratch.
-- A simple sine-wave synth example.
local voices = {}
local TWO_PI = 2 * math.pi

function noteOn(midiNote, velocity)
  local freq = 440.0 * 2^((midiNote - 69) / 12.0)
  voices[midiNote] = { phase = 0.0, freq = freq, vel = velocity / 127.0 }
end

function noteOff(midiNote)
  voices[midiNote] = nil
end

function allNotesOff()
  voices = {}
end

function processBlock(buffer)
  local sr = buffer.sampleRate
  local n  = buffer.numSamples
  local l  = buffer:getChannel(0)
  local r  = buffer:getChannel(1)
  for i = 0, n - 1 do
    local out = 0.0
    for _, v in pairs(voices) do
      out = out + math.sin(v.phase) * v.vel * 0.3
      v.phase = v.phase + TWO_PI * v.freq / sr
    end
    l[i] = out
    r[i] = out
  end
end
`;

export default function LuaPluginManager() {
  const {
    luaPlugins, setLuaPlugins, addLuaPlugin, updateLuaPlugin, removeLuaPlugin,
    selectedTrack, tracks, updateTrack, sampleRate,
  } = useStore();

  const track = tracks.find(t => t.id === selectedTrack) ?? null;

  // ── Editor state ───────────────────────────────────────────────────────────
  const [editingId, setEditingId]   = useState<number | null>(null);
  const [editorCode, setEditorCode] = useState('');
  const [newName, setNewName]       = useState('');
  const [isCreating, setIsCreating] = useState(false);
  const [newCode, setNewCode]       = useState(DEFAULT_LUA);
  const [status, setStatus]         = useState<{ ok: boolean; msg: string } | null>(null);
  const [loading, setLoading]       = useState(false);
  const [template, setTemplate]     = useState<'effect' | 'instrument'>('effect');

  // ── Fetch plugin list on mount ─────────────────────────────────────────────
  const refreshList = useCallback(async () => {
    if (!window.electronAPI) return;
    const result = await window.electronAPI.listPlugins();
    if (result?.plugins) {
      setLuaPlugins(result.plugins);
    }
  }, [setLuaPlugins]);

  useEffect(() => { refreshList(); }, [refreshList]);

  // Keep new-code in sync with template selection
  useEffect(() => {
    if (isCreating) setNewCode(template === 'instrument' ? INSTRUMENT_LUA : DEFAULT_LUA);
  }, [template, isCreating]);

  // ── Create ─────────────────────────────────────────────────────────────────
  const handleCreate = async () => {
    if (!window.electronAPI || !newName.trim()) return;
    setLoading(true);
    setStatus(null);
    const result = await window.electronAPI.createPlugin({
      pluginName: newName.trim(),
      pluginSourceCode: newCode,
    });
    setLoading(false);
    if (result?.success) {
      addLuaPlugin({ id: result.id, name: newName.trim(), sourceCode: newCode, ready: true });
      setStatus({ ok: true, msg: `Plugin "${newName.trim()}" created (id=${result.id})` });
      setIsCreating(false);
      setNewName('');
      setNewCode(DEFAULT_LUA);
    } else {
      setStatus({ ok: false, msg: result?.error ?? 'Failed to create plugin' });
    }
  };

  // ── Save / update ──────────────────────────────────────────────────────────
  const handleSave = async (plugin: LuaPlugin) => {
    if (!window.electronAPI) return;
    setLoading(true);
    setStatus(null);
    const result = await window.electronAPI.updatePlugin({
      pluginId: plugin.id,
      pluginSourceCode: editorCode,
    });
    setLoading(false);
    if (result?.success) {
      updateLuaPlugin(plugin.id, { sourceCode: editorCode });
      setStatus({ ok: true, msg: `Plugin "${plugin.name}" updated` });
      setEditingId(null);
    } else {
      setStatus({ ok: false, msg: result?.error ?? 'Compilation failed' });
    }
  };

  // ── Remove ─────────────────────────────────────────────────────────────────
  const handleRemove = async (id: number) => {
    if (!window.electronAPI) return;
    const result = await window.electronAPI.removePlugin({ pluginId: id });
    if (result?.success) {
      // If this plugin was assigned to any track, clear it
      tracks.forEach(t => {
        if (t.luaPluginId === id) updateTrack(t.id, { luaPluginId: undefined, synthType: 0 });
      });
      removeLuaPlugin(id);
      if (editingId === id) setEditingId(null);
      setStatus({ ok: true, msg: `Plugin removed` });
    }
  };

  // ── Assign plugin as instrument on the selected track ─────────────────────
  const handleAssign = async (plugin: LuaPlugin) => {
    if (!track || !window.electronAPI) return;
    setStatus(null);
    const result = await window.electronAPI.assignPlugin({
      trackId: track.id,
      pluginId: plugin.id,
    });
    if (result?.success) {
      // synthType 5 = Lua Plugin instrument
      updateTrack(track.id, { luaPluginId: plugin.id, synthType: 5 });
      setStatus({ ok: true, msg: `"${plugin.name}" assigned to "${track.name}"` });
    } else {
      setStatus({ ok: false, msg: result?.error ?? 'Assign failed' });
    }
  };

  // ── Unassign — restore sine synth ─────────────────────────────────────────
  const handleUnassign = async () => {
    if (!track || !window.electronAPI) return;
    await window.electronAPI.setSynthType?.({
      trackId: track.id, synthType: 0,
      numVoices: track.voiceCount ?? 8, sampleRate,
    });
    updateTrack(track.id, { luaPluginId: undefined, synthType: 0 });
    setStatus({ ok: true, msg: 'Reverted to Sine synth' });
  };

  // ── Open editor ────────────────────────────────────────────────────────────
  const openEditor = (plugin: LuaPlugin) => {
    setEditingId(plugin.id);
    setEditorCode(plugin.sourceCode);
    setIsCreating(false);
    setStatus(null);
  };

  const assignedPlugin = track?.luaPluginId != null
    ? luaPlugins.find(p => p.id === track.luaPluginId)
    : null;

  return (
    <div className="lua-pm">
      <div className="lua-pm-header">
        <span className="lua-pm-title">Lua Plugins</span>
        <div className="lua-pm-actions">
          <button className="lua-btn lua-btn-refresh" onClick={refreshList} title="Refresh plugin list">↻</button>
          <button className="lua-btn lua-btn-new"
            onClick={() => { setIsCreating(true); setEditingId(null); setStatus(null); }}
            title="New Lua plugin"
          >＋</button>
        </div>
      </div>

      {/* ── Assigned plugin banner ──────────────────────────────────────── */}
      {track && assignedPlugin && (
        <div className="lua-assigned-banner">
          <span>🎹 <strong>{assignedPlugin.name}</strong> is the instrument for <em>{track.name}</em></span>
          <button className="lua-btn lua-btn-del" onClick={handleUnassign} title="Remove – revert to Sine">✕</button>
        </div>
      )}

      {/* ── Plugin list ──────────────────────────────────────────────────── */}
      {luaPlugins.length === 0 && !isCreating && (
        <div className="lua-pm-empty">No plugins yet. Click ＋ to add one.</div>
      )}

      {luaPlugins.map(p => {
        const isAssignedHere = track?.luaPluginId === p.id;
        return (
          <div key={p.id} className={`lua-plugin-row ${editingId === p.id ? 'editing' : ''} ${isAssignedHere ? 'assigned' : ''}`}>
            <span className={`lua-ready-dot ${p.ready ? 'ready' : 'error'}`} title={p.ready ? 'Ready' : 'Error'} />
            <span className="lua-plugin-name">{p.name}</span>
            <span className="lua-plugin-id">#{p.id}</span>
            <div className="lua-plugin-btns">
              {track && !isAssignedHere && (
                <button className="lua-btn lua-btn-assign" onClick={() => handleAssign(p)} title={`Assign to "${track.name}" as instrument`}>
                  🎹
                </button>
              )}
              <button className="lua-btn" onClick={() => openEditor(p)} title="Edit source">✏️</button>
              <button className="lua-btn lua-btn-del" onClick={() => handleRemove(p.id)} title="Remove">🗑</button>
            </div>
          </div>
        );
      })}

      {/* ── Status bar ───────────────────────────────────────────────────── */}
      {status && (
        <div className={`lua-status ${status.ok ? 'ok' : 'err'}`}>{status.msg}</div>
      )}

      {/* ── New plugin form ───────────────────────────────────────────────── */}
      {isCreating && (
        <div className="lua-editor-panel">
          <div className="lua-editor-toolbar">
            <input
              className="lua-name-input"
              placeholder="Plugin name…"
              value={newName}
              onChange={e => setNewName(e.target.value)}
            />
            <select
              className="lua-template-select"
              value={template}
              onChange={e => setTemplate(e.target.value as 'effect' | 'instrument')}
              title="Start from a template"
            >
              <option value="effect">Effect template</option>
              <option value="instrument">Instrument template</option>
            </select>
            <button className="lua-btn lua-btn-save" onClick={handleCreate} disabled={loading || !newName.trim()}>
              {loading ? '…' : 'Create'}
            </button>
            <button className="lua-btn" onClick={() => setIsCreating(false)}>Cancel</button>
          </div>
          <textarea
            className="lua-code-editor"
            value={newCode}
            onChange={e => setNewCode(e.target.value)}
            spellCheck={false}
            rows={16}
          />
        </div>
      )}

      {/* ── Edit existing plugin ─────────────────────────────────────────── */}
      {editingId !== null && (() => {
        const plugin = luaPlugins.find(p => p.id === editingId);
        if (!plugin) return null;
        return (
          <div className="lua-editor-panel">
            <div className="lua-editor-toolbar">
              <span className="lua-editing-label">Editing: <strong>{plugin.name}</strong></span>
              <button className="lua-btn lua-btn-save" onClick={() => handleSave(plugin)} disabled={loading}>
                {loading ? '…' : 'Save & Reload'}
              </button>
              <button className="lua-btn" onClick={() => setEditingId(null)}>Cancel</button>
            </div>
            <textarea
              className="lua-code-editor"
              value={editorCode}
              onChange={e => setEditorCode(e.target.value)}
              spellCheck={false}
              rows={16}
            />
          </div>
        );
      })()}
    </div>
  );
}

