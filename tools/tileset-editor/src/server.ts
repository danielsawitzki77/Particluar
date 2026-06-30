import express from 'express';
import path from 'path';
import fs from 'fs';
import { exec } from 'child_process';

const app = express();
const PORT = 3000;
const BASE_URL = `http://localhost:${PORT}`;

// Project root is two levels up from tools/tileset-editor/
const PROJECT_ROOT = path.resolve(__dirname, '..', '..', '..');
const ASSETS_DIR = path.join(PROJECT_ROOT, 'assets', 'tilesets');

app.use(express.json({ limit: '10mb' }));
app.use(express.static(path.join(__dirname, '..', 'public')));

// --- Speaking URL routes ---
// Redirect root to tileset configurator
app.get('/', (req, res) => res.redirect('/tileset-configurator'));
app.get('/tileset-configurator', (req, res) => {
  res.sendFile(path.join(__dirname, '..', 'public', 'index.html'));
});
app.get('/level-editor', (req, res) => {
  res.sendFile(path.join(__dirname, '..', 'public', 'index.html'));
});

// --- API: List tilesets (recursive — any folder containing PNGs) ---
app.get('/api/tilesets', (req, res) => {
  try {
    if (!fs.existsSync(ASSETS_DIR)) {
      return res.json([]);
    }
    const results: string[] = [];
    function walkDir(dir: string, prefix: string) {
      const entries = fs.readdirSync(dir, { withFileTypes: true });
      const hasPngs = entries.some(e => !e.isDirectory() && e.name.toLowerCase().endsWith('.png'));
      if (hasPngs && prefix) {
        results.push(prefix);
      }
      for (const e of entries) {
        if (e.isDirectory()) {
          const subPath = prefix ? `${prefix}/${e.name}` : e.name;
          walkDir(path.join(dir, e.name), subPath);
        }
      }
    }
    walkDir(ASSETS_DIR, '');
    res.json(results);
  } catch (err) {
    console.error('Error listing tilesets:', err);
    res.status(500).json({ error: 'Failed to list tilesets' });
  }
});

// --- API: List all PNG files in a tileset folder ---
app.get('/api/tilesets/:name/sheets', (req, res) => {
  const name = req.params.name;
  const tilesetDir = path.join(ASSETS_DIR, name);

  if (!fs.existsSync(tilesetDir)) {
    return res.status(404).json({ error: `Tileset '${name}' not found` });
  }

  try {
    const files = fs.readdirSync(tilesetDir);
    const pngs = files.filter(f => f.toLowerCase().endsWith('.png'));
    res.json(pngs);
  } catch (err) {
    console.error(`Error listing sheets for '${name}':`, err);
    res.status(500).json({ error: 'Failed to list sheets' });
  }
});

// --- API: Serve a specific PNG sheet by filename ---
app.get('/api/tilesets/:name/sheets/:filename', (req, res) => {
  const { name, filename } = req.params;
  const filePath = path.join(ASSETS_DIR, name, filename);

  if (!fs.existsSync(filePath)) {
    return res.status(404).json({ error: `Sheet '${filename}' not found in tileset '${name}'` });
  }

  res.sendFile(filePath);
});

// --- API: Read sidecar JSON (per-sheet: <sheetname_without_ext>.json) ---
app.get('/api/tilesets/:name/json/:sheetBase', (req, res) => {
  const { name, sheetBase } = req.params;
  const jsonPath = path.join(ASSETS_DIR, name, `${sheetBase}.json`);

  if (!fs.existsSync(jsonPath)) {
    // No JSON yet — return empty tileset structure
    return res.json({ tiles: [] });
  }

  try {
    const content = fs.readFileSync(jsonPath, 'utf-8');
    const data = JSON.parse(content);
    res.json(data);
  } catch (err) {
    console.error(`Error reading JSON for '${name}/${sheetBase}':`, err);
    res.status(500).json({ error: 'Failed to parse sidecar JSON' });
  }
});

// --- API: Write sidecar JSON (per-sheet) ---
app.put('/api/tilesets/:name/json/:sheetBase', (req, res) => {
  const { name, sheetBase } = req.params;
  const tilesetDir = path.join(ASSETS_DIR, name);
  const jsonPath = path.join(tilesetDir, `${sheetBase}.json`);

  if (!fs.existsSync(tilesetDir)) {
    return res.status(404).json({ error: `Tileset '${name}' not found` });
  }

  try {
    const jsonStr = JSON.stringify(req.body, null, 2) + '\n';
    fs.writeFileSync(jsonPath, jsonStr, 'utf-8');
    res.json({ success: true });
  } catch (err) {
    console.error(`Error writing JSON for '${name}/${sheetBase}':`, err);
    res.status(500).json({ error: 'Failed to write sidecar JSON' });
  }
});

// --- Legacy single-image endpoints (backward compat) ---
app.get('/api/tilesets/:name/image', (req, res) => {
  const name = req.params.name;
  const tilesetDir = path.join(ASSETS_DIR, name);
  if (!fs.existsSync(tilesetDir)) {
    return res.status(404).json({ error: `Tileset '${name}' not found` });
  }
  const files = fs.readdirSync(tilesetDir);
  const png = files.find(f => f.toLowerCase().endsWith('.png'));
  if (png) {
    return res.sendFile(path.join(tilesetDir, png));
  }
  res.status(404).json({ error: `No PNG found in tileset '${name}'` });
});

app.get('/api/tilesets/:name/json', (req, res) => {
  const name = req.params.name;
  // Try <name>.json first, then first json found
  const jsonPath = path.join(ASSETS_DIR, name, `${name}.json`);
  if (fs.existsSync(jsonPath)) {
    try {
      const content = fs.readFileSync(jsonPath, 'utf-8');
      return res.json(JSON.parse(content));
    } catch (err) {
      return res.status(500).json({ error: 'Failed to parse sidecar JSON' });
    }
  }
  // Try first .json file in the folder
  const files = fs.readdirSync(path.join(ASSETS_DIR, name));
  const jsonFile = files.find(f => f.toLowerCase().endsWith('.json'));
  if (jsonFile) {
    try {
      const content = fs.readFileSync(path.join(ASSETS_DIR, name, jsonFile), 'utf-8');
      return res.json(JSON.parse(content));
    } catch (err) {
      return res.status(500).json({ error: 'Failed to parse sidecar JSON' });
    }
  }
  res.json({ tiles: [] });
});

app.put('/api/tilesets/:name/json', (req, res) => {
  const name = req.params.name;
  const tilesetDir = path.join(ASSETS_DIR, name);
  const jsonPath = path.join(tilesetDir, `${name}.json`);
  if (!fs.existsSync(tilesetDir)) {
    return res.status(404).json({ error: `Tileset '${name}' not found` });
  }
  try {
    const jsonStr = JSON.stringify(req.body, null, 2) + '\n';
    fs.writeFileSync(jsonPath, jsonStr, 'utf-8');
    res.json({ success: true });
  } catch (err) {
    res.status(500).json({ error: 'Failed to write sidecar JSON' });
  }
});

// --- Shutdown endpoint (called by the Close button in the UI) ---
app.post('/api/shutdown', (req, res) => {
  res.json({ message: 'Server shutting down...' });
  console.log('\nTileset Editor shutting down. Goodbye!');
  setTimeout(() => process.exit(0), 500);
});

// --- Start server ---
const server = app.listen(PORT, () => {
  console.log(`\n  Particluar Tileset Editor`);
  console.log(`  ========================`);
  console.log(`  Tileset Configurator: ${BASE_URL}/tileset-configurator`);
  console.log(`  Level Editor:         ${BASE_URL}/level-editor`);
  console.log(`  Assets directory:     ${ASSETS_DIR}\n`);

  // Auto-open browser
  const openCmd = process.platform === 'win32' ? 'start' :
                  process.platform === 'darwin' ? 'open' : 'xdg-open';
  exec(`${openCmd} ${BASE_URL}/tileset-configurator`);
});
