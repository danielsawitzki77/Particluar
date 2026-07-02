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

// --- API: Save blocker bitmap (base64 PNG) ---
app.put('/api/tilesets/:name/blocker-bitmap/:sheetBase', (req, res) => {
  const { name, sheetBase } = req.params;
  const tilesetDir = path.join(ASSETS_DIR, name);

  if (!fs.existsSync(tilesetDir)) {
    return res.status(404).json({ error: `Tileset '${name}' not found` });
  }

  const { data } = req.body;
  if (!data || typeof data !== 'string') {
    return res.status(400).json({ error: 'Missing or invalid "data" field (expected base64 PNG)' });
  }

  try {
    const buffer = Buffer.from(data, 'base64');
    const outputPath = path.join(tilesetDir, `${sheetBase}_blockers.png`);
    fs.writeFileSync(outputPath, buffer);
    console.log(`Blocker bitmap saved: ${outputPath}`);
    res.json({ success: true, path: `${sheetBase}_blockers.png` });
  } catch (err) {
    console.error(`Error writing blocker bitmap for '${name}/${sheetBase}':`, err);
    res.status(500).json({ error: 'Failed to write blocker bitmap' });
  }
});

// --- API: Check if file exists ---
app.get('/api/tilesets/:name/exists/:filename', (req, res) => {
  const { name, filename } = req.params;
  const filePath = path.join(ASSETS_DIR, name, filename);
  res.json({ exists: fs.existsSync(filePath) });
});

// --- API: Open in system file explorer ---
app.post('/api/tilesets/:name/open-explorer', (req, res) => {
  const name = req.params.name;
  const tilesetDir = path.join(ASSETS_DIR, name);

  if (!fs.existsSync(tilesetDir)) {
    return res.status(404).json({ error: `Tileset '${name}' not found` });
  }

  const { file } = req.body || {};
  let target = tilesetDir;
  if (file) {
    const filePath = path.join(tilesetDir, file);
    if (fs.existsSync(filePath)) {
      target = filePath;
    }
  }

  if (process.platform === 'win32') {
    exec(`explorer /select,"${target}"`);
  } else if (process.platform === 'darwin') {
    exec(`open -R "${target}"`);
  } else {
    exec(`xdg-open "${path.dirname(target)}"`);
  }

  res.json({ success: true });
});

// --- API: List TMX files in a tileset folder ---
app.get('/api/tilesets/:name/tmx-files', (req, res) => {
  const name = req.params.name;
  const tilesetDir = path.join(ASSETS_DIR, name);
  if (!fs.existsSync(tilesetDir)) {
    return res.status(404).json({ error: `Tileset '${name}' not found` });
  }
  const files = fs.readdirSync(tilesetDir);
  const tmxFiles = files.filter(f => f.toLowerCase().endsWith('.tmx'));
  res.json(tmxFiles);
});

// --- API: Serve raw TMX file content ---
app.get('/api/tilesets/:name/tmx-raw/:filename', (req, res) => {
  const { name, filename } = req.params;
  const filePath = path.join(ASSETS_DIR, name, filename);
  if (!fs.existsSync(filePath)) {
    return res.status(404).json({ error: `TMX file not found: ${filename}` });
  }
  res.type('application/xml').sendFile(filePath);
});

// --- API: Parse a TMX file and extract tileset partition info ---
app.get('/api/tilesets/:name/parse-tmx/:filename', (req, res) => {
  const { name, filename } = req.params;
  const tmxPath = path.join(ASSETS_DIR, name, filename);
  if (!fs.existsSync(tmxPath)) {
    return res.status(404).json({ error: `TMX file not found: ${filename}` });
  }
  try {
    const xml = fs.readFileSync(tmxPath, 'utf-8');
    // Simple XML parsing for tileset elements (no dependency needed)
    const tilesets: Array<{
      name: string;
      tilewidth: number;
      tileheight: number;
      tilecount: number;
      columns: number;
      image: string;
      imagewidth: number;
      imageheight: number;
      firstgid: number;
      animations: Array<{
        tileid: number;
        frames: Array<{ tileid: number; duration_ms: number; source_rect: { x: number; y: number; w: number; h: number } }>;
      }>;
    }> = [];

    // Match all <tileset ...> elements
    const tilesetRegex = /<tileset\s+([^>]+)>/g;
    const imageRegex = /<image\s+([^>]+)\/>/g;
    let match;
    let allTilesetBlocks = xml.split(/<tileset\s+/);
    allTilesetBlocks.shift(); // remove content before first tileset

    for (const block of allTilesetBlocks) {
      const attrStr = block.substring(0, block.indexOf('>'));
      const getAttr = (name: string) => {
        const m = attrStr.match(new RegExp(`${name}="([^"]+)"`));
        return m ? m[1] : '';
      };

      const tsName = getAttr('name');
      const tilewidth = parseInt(getAttr('tilewidth')) || 16;
      const tileheight = parseInt(getAttr('tileheight')) || 16;
      const tilecount = parseInt(getAttr('tilecount')) || 0;
      const columns = parseInt(getAttr('columns')) || 1;
      const firstgid = parseInt(getAttr('firstgid')) || 1;

      // Find <image> within this tileset block
      const imgMatch = block.match(/<image\s+([^>]+)\/>/);
      let imgSource = '', imgW = 0, imgH = 0;
      if (imgMatch) {
        const imgAttrs = imgMatch[1];
        const getSrc = (n: string) => {
          const m2 = imgAttrs.match(new RegExp(`${n}="([^"]+)"`));
          return m2 ? m2[1] : '';
        };
        imgSource = getSrc('source');
        imgW = parseInt(getSrc('width')) || 0;
        imgH = parseInt(getSrc('height')) || 0;
      }

      // Extract <animation> blocks from <tile> elements
      const animations: Array<{
        tileid: number;
        frames: Array<{ tileid: number; duration_ms: number; source_rect: { x: number; y: number; w: number; h: number } }>;
      }> = [];

      // Find all <tile id="N"> ... </tile> blocks that contain <animation>
      const tileBlockRegex = /<tile\s+id="(\d+)"[^>]*>([\s\S]*?)<\/tile>/g;
      let tileMatch;
      while ((tileMatch = tileBlockRegex.exec(block)) !== null) {
        const tileId = parseInt(tileMatch[1]);
        const tileContent = tileMatch[2];

        // Check if this tile has an <animation> block
        const animMatch = tileContent.match(/<animation>([\s\S]*?)<\/animation>/);
        if (animMatch) {
          const animContent = animMatch[1];
          const frames: Array<{ tileid: number; duration_ms: number; source_rect: { x: number; y: number; w: number; h: number } }> = [];

          // Extract <frame tileid="N" duration="M"/> entries
          const frameRegex = /<frame\s+tileid="(\d+)"\s+duration="(\d+)"\s*\/?>/g;
          let frameMatch;
          while ((frameMatch = frameRegex.exec(animContent)) !== null) {
            const frameTileId = parseInt(frameMatch[1]);
            const duration = parseInt(frameMatch[2]);
            // Convert tileid to source_rect coordinates
            const col = frameTileId % columns;
            const row = Math.floor(frameTileId / columns);
            frames.push({
              tileid: frameTileId,
              duration_ms: duration,
              source_rect: {
                x: col * tilewidth,
                y: row * tileheight,
                w: tilewidth,
                h: tileheight
              }
            });
          }

          if (frames.length > 0) {
            animations.push({ tileid: tileId, frames });
          }
        }
      }

      tilesets.push({
        name: tsName,
        tilewidth,
        tileheight,
        tilecount,
        columns,
        image: imgSource,
        imagewidth: imgW,
        imageheight: imgH,
        firstgid,
        animations
      });
    }

    res.json({ tilesets });
  } catch (err) {
    console.error(`Error parsing TMX '${filename}':`, err);
    res.status(500).json({ error: 'Failed to parse TMX file' });
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
