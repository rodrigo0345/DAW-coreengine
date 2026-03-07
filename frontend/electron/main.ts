import { app, BrowserWindow, ipcMain } from 'electron';
import { spawn, ChildProcess } from 'child_process';
import * as path from 'path';

let mainWindow: BrowserWindow | null = null;
let engineProcess: ChildProcess | null = null;

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

  engineProcess.stdout?.on('data', (data) => {
    console.log('Engine:', data.toString().trim());
    mainWindow?.webContents.send('engine-output', data.toString());
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

// ─── IPC: Generic command (for future extensibility) ─────────────────────────

ipcMain.handle('send-command', async (_e, command: string) => {
  if (engineProcess?.stdin) {
    engineProcess.stdin.write(command + '\n');
    return { success: true };
  }
  return { success: false, error: 'Engine not running' };
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
