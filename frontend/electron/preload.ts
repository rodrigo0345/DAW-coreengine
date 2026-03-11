import { contextBridge, ipcRenderer } from 'electron';

// Expose protected methods to the renderer process
contextBridge.exposeInMainWorld('electronAPI', {
  // Engine lifecycle
  startEngine: () => ipcRenderer.invoke('engine:start'),
  stopEngine: () => ipcRenderer.invoke('engine:stop'),

  // Timeline / track management
  addTrack: (data: any) => ipcRenderer.invoke('timeline:addTrack', data),
  addNote: (data: any) => ipcRenderer.invoke('timeline:addNote', data),
  rebuildTimeline: () => ipcRenderer.invoke('timeline:rebuild'),
  clearTrack: (data: any) => ipcRenderer.invoke('timeline:clearTrack', data),

  // Track controls
  setTrackVolume: (data: any) => ipcRenderer.invoke('track:setVolume', data),
  setTrackMute: (data: any) => ipcRenderer.invoke('track:setMute', data),
  setTrackSolo: (data: any) => ipcRenderer.invoke('track:setSolo', data),

  // ADSR envelope
  setADSR: (data: any) => ipcRenderer.invoke('track:setADSR', data),

  // Effects
  setTrackEffect:    (data: any) => ipcRenderer.invoke('track:setEffect', data),
  removeTrackEffect: (data: any) => ipcRenderer.invoke('track:removeEffect', data),
  setEffectParam:    (data: any) => ipcRenderer.invoke('track:setEffectParam', data),

  // Automation
  setAutomationLane:   (data: any) => ipcRenderer.invoke('track:setAutomationLane', data),
  clearAutomationLane: (data: any) => ipcRenderer.invoke('track:clearAutomationLane', data),

  // Sampler / instrument management
  loadSample:       (data: any) => ipcRenderer.invoke('track:loadSample', data),
  setVoiceCount:    (data: any) => ipcRenderer.invoke('track:setVoiceCount', data),
  setSynthType:     (data: any) => ipcRenderer.invoke('track:setSynthType', data),
  openAudioFile:    ()           => ipcRenderer.invoke('dialog:openAudioFile'),

  // Playback
  play: () => ipcRenderer.invoke('playback:play'),
  stop: () => ipcRenderer.invoke('playback:stop'),
  pause: () => ipcRenderer.invoke('playback:pause'),
  seek: (samplePosition: number) => ipcRenderer.invoke('playback:seek', samplePosition),
  setBpm: (bpm: number) => ipcRenderer.invoke('playback:setBpm', bpm),

  // Generic command (for future use)
  sendCommand: (cmd: string) => ipcRenderer.invoke('send-command', cmd),

  // Sample Browser
  getMusicDir: () => ipcRenderer.invoke('browser:getMusicDir'),
  listDir: (dirPath: string) => ipcRenderer.invoke('browser:listDir', dirPath),
  getAudioUrl: (filePath: string) => ipcRenderer.invoke('browser:getAudioUrl', filePath),

  // Lua Plugins
  createPlugin: (data: { pluginName: string; pluginSourceCode: string }) =>
    ipcRenderer.invoke('plugin:create', data),
  removePlugin: (data: { pluginId: number }) =>
    ipcRenderer.invoke('plugin:remove', data),
  updatePlugin: (data: { pluginId: number; pluginSourceCode: string }) =>
    ipcRenderer.invoke('plugin:update', data),
  listPlugins: () =>
    ipcRenderer.invoke('plugin:list'),
  assignPlugin: (data: { trackId: number; pluginId: number }) =>
    ipcRenderer.invoke('plugin:assign', data),

  // Engine event listeners
  onEngineOutput: (cb: (data: string) => void) => {
    ipcRenderer.on('engine-output', (_e, data) => cb(data));
  },
  onEngineError: (cb: (data: string) => void) => {
    ipcRenderer.on('engine-error', (_e, data) => cb(data));
  },
  onEngineClosed: (cb: (code: number) => void) => {
    ipcRenderer.on('engine-closed', (_e, code) => cb(code));
  },
  // Typed push events from the engine (PluginCreated, PluginUpdated, PluginRemoved, PluginList)
  onEngineEvent: (eventType: string, cb: (data: unknown) => void) => {
    ipcRenderer.on(`engine:${eventType}`, (_e, data) => cb(data));
  },
});
