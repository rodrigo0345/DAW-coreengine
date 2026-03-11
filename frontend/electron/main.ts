import { app, BrowserWindow, ipcMain, dialog } from 'electron';
import { spawn, ChildProcess } from 'child_process';
import * as path from 'path';
import * as fs from 'fs';
import * as os from 'os';


let mainWindow: BrowserWindow | null = null;
let engineProcess: ChildProcess | null = null;

// Pending resolvers for request/response style engine messages
const pendingEngineResponses = new Map<string, (data: unknown) => void>();

const ENGINE_PATH = path.join(__dirname, '../../cmake-build-debug/DAWCoreEngine');

// ─── Helpers ─────────────────────────────────────────────────────────────────

/** Send a JSON command line to the engine via stdin */
function sendToEngine(obj: Record<string, unknown>): { success: boolean; error?: string } {
  if (engineProcess?.stdin) {
    engineProcess.stdin.write(JSON.stringify(obj) + '\n');
    return { success: true };
  }
  return { success: false, error: 'Engine not running' };
}

/** Send a command and wait for a matching response type from the engine stdout */
function sendToEngineAndWait(obj: Record<string, unknown>, responseType: string, timeoutMs = 5000): Promise<unknown> {
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => {
      pendingEngineResponses.delete(responseType);
      reject(new Error(`Timeout waiting for ${responseType}`));
    }, timeoutMs);

    pendingEngineResponses.set(responseType, (data) => {
      clearTimeout(timer);
      resolve(data);
    });

    const result = sendToEngine(obj);
    if (!result.success) {
      clearTimeout(timer);
      pendingEngineResponses.delete(responseType);
      reject(new Error(result.error));
    }
  });
}

// ─── Window ──────────────────────────────────────────────────────────────────

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1400,
    height: 900,
    webPreferences: {
      nodeIntegration: false,
      contextIsolation: true,
      preload: path.join(__dirname, 'preload.js'),
    },
    backgroundColor: '#1a1a1a',
    frame: true,
    title: 'DAW Core Engine',
  });

  const isDev = !app.isPackaged;
  if (isDev) {
    mainWindow.loadURL('http://localhost:5173');
    mainWindow.webContents.openDevTools();
  } else {
    mainWindow.loadFile(path.join(__dirname, '../dist-react/index.html'));
  }

  mainWindow.on('closed', () => { mainWindow = null; });
}

// ─── Engine process ──────────────────────────────────────────────────────────

function startEngine() {
  console.log('Starting engine at:', ENGINE_PATH);
  engineProcess = spawn(ENGINE_PATH, [], { stdio: ['pipe', 'pipe', 'pipe'] });

  // Line-buffered stdout parser
  let stdoutBuf = '';
  engineProcess.stdout?.on('data', (data: Buffer) => {
    stdoutBuf += data.toString();
    const lines = stdoutBuf.split('\n');
    stdoutBuf = lines.pop() ?? '';          // keep incomplete last line
    for (const line of lines) {
      const trimmed = line.trim();
      if (!trimmed) continue;
      console.log('Engine:', trimmed);
      // Forward raw text to renderer
      mainWindow?.webContents.send('engine-output', trimmed);
      // Try to parse as JSON and dispatch typed events
      try {
        const msg = JSON.parse(trimmed) as Record<string, unknown>;
        const type = msg.type as string | undefined;
        if (type) {
          // Resolve pending request/response waiter
          const resolver = pendingEngineResponses.get(type);
          if (resolver) {
            pendingEngineResponses.delete(type);
            resolver(msg);
          }
          // Also push to renderer as a named event
          mainWindow?.webContents.send(`engine:${type}`, msg);
        }
      } catch { /* non-JSON line – already forwarded as raw text */ }
    }
  });
  engineProcess.stderr?.on('data', (data) => {
    console.error('Engine ERR:', data.toString().trim());
    mainWindow?.webContents.send('engine-error', data.toString());
  });
  engineProcess.on('close', (code) => {
    console.log('Engine exited with code', code);
    mainWindow?.webContents.send('engine-closed', code);
    engineProcess = null;
  });
}

// ─── IPC: Engine lifecycle ───────────────────────────────────────────────────

ipcMain.handle('engine:start', async () => {
  if (engineProcess) return { success: false, error: 'Already running' };
  startEngine();
  return { success: true };
});

ipcMain.handle('engine:stop', async () => {
  if (!engineProcess) return { success: false, error: 'Not running' };
  engineProcess.kill();
  engineProcess = null;
  return { success: true };
});

// ─── IPC: Playback ──────────────────────────────────────────────────────────

ipcMain.handle('playback:play',  async () => sendToEngine({ type: 'Play' }));
ipcMain.handle('playback:stop',  async () => sendToEngine({ type: 'Stop' }));
ipcMain.handle('playback:pause', async () => sendToEngine({ type: 'Pause' }));

ipcMain.handle('playback:seek', async (_e, samplePosition: number) =>
  sendToEngine({ type: 'Seek', data: { samplePosition: Math.floor(samplePosition) } })
);

ipcMain.handle('playback:setBpm', async (_e, bpm: number) =>
  sendToEngine({ type: 'SetBPM', data: { bpm } })
);

// ─── IPC: Track management ──────────────────────────────────────────────────

ipcMain.handle('timeline:addTrack', async (_e, data: {
  trackId: number; name: string; synthType: number; numVoices: number;
}) => sendToEngine({ type: 'AddTrack', data }));

ipcMain.handle('timeline:addNote', async (_e, data: {
  trackId: number; startBeat: number; durationBeats: number;
  midiNote: number; velocity: number; bpm: number; sampleRate: number;
}) => sendToEngine({ type: 'AddNote', data }));

ipcMain.handle('timeline:rebuild', async () => sendToEngine({ type: 'RebuildTimeline' }));

ipcMain.handle('timeline:clearTrack', async (_e, data: { trackId: number }) =>
  sendToEngine({ type: 'ClearTrack', data })
);

// ─── IPC: Track controls (volume, mute, solo) ──────────────────────────────

ipcMain.handle('track:setVolume', async (_e, data: { trackId: number; value: number }) =>
  sendToEngine({ type: 'SetTrackVolume', data })
);

ipcMain.handle('track:setMute', async (_e, data: { trackId: number; value: number }) =>
  sendToEngine({ type: 'SetTrackMute', data })
);

ipcMain.handle('track:setSolo', async (_e, data: { trackId: number; value: number }) =>
  sendToEngine({ type: 'SetTrackSolo', data })
);

// ─── IPC: ADSR ──────────────────────────────────────────────────────────────

ipcMain.handle('track:setADSR', async (_e, data: {
  trackId: number; attack: number; decay: number; sustain: number; release: number;
}) => sendToEngine({ type: 'SetADSR', data }));

// ─── IPC: Effects ────────────────────────────────────────────────────────────

ipcMain.handle('track:setEffect', async (_e, data: {
  trackId: number; effectType: string; enabled: boolean; mix: number;
  roomSize?: number; damping?: number;
  delayMs?: number; feedback?: number; delayDamping?: number;
  drive?: number;
}) => sendToEngine({ type: 'SetTrackEffect', data }));

ipcMain.handle('track:removeEffect', async (_e, data: {
  trackId: number; effectType: string;
}) => sendToEngine({ type: 'RemoveTrackEffect', data }));

ipcMain.handle('track:setEffectParam', async (_e, data: {
  trackId: number; effectType: string; paramName: string; value: number;
}) => sendToEngine({ type: 'SetEffectParam', data }));

// ─── IPC: Automation ─────────────────────────────────────────────────────────

ipcMain.handle('track:setAutomationLane', async (_e, data: {
  trackId: number; paramName: string;
  points: { beat: number; value: number }[];
  bpm: number; sampleRate: number;
}) => sendToEngine({ type: 'SetAutomationLane', data }));

ipcMain.handle('track:clearAutomationLane', async (_e, data: {
  trackId: number; paramName: string; bpm: number; sampleRate: number;
}) => sendToEngine({ type: 'ClearAutomationLane', data }));

// ─── IPC: Sampler / instruments ──────────────────────────────────────────────

ipcMain.handle('track:loadSample', async (_e, data: {
  trackId: number; filePath: string; rootNote: number; oneShot: boolean;
}) => sendToEngine({ type: 'LoadSample', data }));

ipcMain.handle('track:setVoiceCount', async (_e, data: {
  trackId: number; numVoices: number;
}) => sendToEngine({ type: 'SetVoiceCount', data }));

ipcMain.handle('track:setSynthType', async (_e, data: {
  trackId: number; synthType: number; numVoices: number; sampleRate: number;
}) => sendToEngine({ type: 'SetSynthType', data }));

// Open a native file picker and return the chosen path (or null)
ipcMain.handle('dialog:openAudioFile', async () => {
  const result = await dialog.showOpenDialog({
    title: 'Select Audio Sample',
    filters: [
      { name: 'Audio Files', extensions: ['wav', 'flac', 'aiff', 'ogg'] },
      { name: 'All Files', extensions: ['*'] },
    ],
    properties: ['openFile'],
  });
  if (result.canceled || result.filePaths.length === 0) return null;
  return result.filePaths[0];
});

// ─── IPC: Plugins (Lua) ──────────────────────────────────────────────────────

ipcMain.handle('plugin:create', async (_e, data: { pluginName: string; pluginSourceCode: string }) => {
  try {
    const result = await sendToEngineAndWait(
      { type: 'CreatePlugin', data },
      'PluginCreated',
    );
    return result;
  } catch (err) {
    return { success: false, error: String(err) };
  }
});

ipcMain.handle('plugin:remove', async (_e, data: { pluginId: number }) => {
  try {
    const result = await sendToEngineAndWait(
      { type: 'RemovePlugin', data },
      'PluginRemoved',
    );
    return result;
  } catch (err) {
    return { success: false, error: String(err) };
  }
});

ipcMain.handle('plugin:update', async (_e, data: { pluginId: number; pluginSourceCode: string }) => {
  try {
    const result = await sendToEngineAndWait(
      { type: 'UpdatePlugin', data },
      'PluginUpdated',
    );
    return result;
  } catch (err) {
    return { success: false, error: String(err) };
  }
});

ipcMain.handle('plugin:list', async () => {
  try {
    const result = await sendToEngineAndWait(
      { type: 'GetPlugins' },
      'PluginList',
    );
    return result;
  } catch (err) {
    return { plugins: [], error: String(err) };
  }
});

ipcMain.handle('plugin:assign', async (_e, data: { trackId: number; pluginId: number }) => {
  try {
    const result = await sendToEngineAndWait(
      { type: 'AssignPlugin', data },
      'PluginAssigned',
    );
    return result;
  } catch (err) {
    return { success: false, error: String(err) };
  }
});

// ─── IPC: Generic command (for future extensibility) ─────────────────────────

ipcMain.handle('send-command', async (_e, command: string) => {
  if (engineProcess?.stdin) {
    engineProcess.stdin.write(command + '\n');
    return { success: true };
  }
  return { success: false, error: 'Engine not running' };
});

// ─── IPC: Sample Browser ─────────────────────────────────────────────────────

const AUDIO_EXTENSIONS = new Set(['.wav', '.flac', '.aiff', '.aif', '.ogg', '.mp3']);

interface DirEntry {
  name: string;
  path: string;
  isDir: boolean;
  ext: string;
}

ipcMain.handle('browser:getMusicDir', async () => {
  // Try /home/rodrigo0345/Music first, fall back to ~/Music
  const preferred = '/home/rodrigo0345/Music';
  if (fs.existsSync(preferred)) return preferred;
  return path.join(os.homedir(), 'Music');
});

ipcMain.handle('browser:listDir', async (_e, dirPath: string): Promise<DirEntry[]> => {
  try {
    const entries = fs.readdirSync(dirPath, { withFileTypes: true });
    return entries
      .filter(e => {
        if (e.isDirectory()) return !e.name.startsWith('.');
        const ext = path.extname(e.name).toLowerCase();
        return AUDIO_EXTENSIONS.has(ext);
      })
      .map(e => ({
        name: e.name,
        path: path.join(dirPath, e.name),
        isDir: e.isDirectory(),
        ext: path.extname(e.name).toLowerCase(),
      }))
      .sort((a, b) => {
        if (a.isDir !== b.isDir) return a.isDir ? -1 : 1;
        return a.name.localeCompare(b.name);
      });
  } catch (err) {
    console.error('browser:listDir error', err);
    return [];
  }
});

ipcMain.handle('browser:getAudioUrl', async (_e, filePath: string): Promise<string | null> => {
  try {
    const ext = path.extname(filePath).toLowerCase();
    const mime: Record<string, string> = {
      '.wav':  'audio/wav',
      '.mp3':  'audio/mpeg',
      '.ogg':  'audio/ogg',
      '.flac': 'audio/flac',
      '.aiff': 'audio/aiff',
      '.aif':  'audio/aiff',
    };
    const contentType = mime[ext] ?? 'audio/octet-stream';
    const data = fs.readFileSync(filePath);
    const b64 = data.toString('base64');
    return `data:${contentType};base64,${b64}`;
  } catch (err) {
    console.error('browser:getAudioUrl error:', err);
    return null;
  }
});

// ─── App lifecycle ───────────────────────────────────────────────────────────

app.whenReady().then(async () => {

  if (!app.isPackaged) {
    console.log('Dev mode: waiting for Vite…');
    await new Promise((r) => setTimeout(r, 2000));
  }
  createWindow();
  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) createWindow();
  });
});

app.on('window-all-closed', () => {
  engineProcess?.kill();
  if (process.platform !== 'darwin') app.quit();
});

app.on('before-quit', () => {
  engineProcess?.kill();
});
