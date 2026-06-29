import express from 'express';
import path from 'path';
import fs from 'fs';

const app = express();
const PORT = 3000;

// Project root is two levels up from tools/web-editor/
const PROJECT_ROOT = path.resolve(__dirname, '..', '..', '..');
const ASSETS_DIR = path.join(PROJECT_ROOT, 'assets', 'tilesets');

app.use(express.json());
app.use(express.static(path.join(__dirname, '..', 'public')));

// GET /api/tilesets — list tileset subfolders in assets/tilesets/
app.get('/api/tilesets', (req, res) => {
  try {
    if (!fs.existsSync(ASSETS_DIR)) {
      return res.json([]);
    }
    const entries = fs.readdirSync(ASSETS_DIR, { withFileTypes: true });
    const tilesets = entries
      .filter(e => e.isDirectory())
      .map(e => e.name);
    res.json(tilesets);
  } catch (err) {
    console.error('Error listing tilesets:', err);
    res.status(500).json({ error: 'Failed to list tilesets' });
  }
});

// GET /api/tilesets/:name/image — serve the PNG file
app.get('/api/tilesets/:name/image', (req, res) => {
  const name = req.params.name;
  const tilesetDir = path.join(ASSETS_DIR, name);

  if (!fs.existsSync(tilesetDir)) {
    return res.status(404).json({ error: `Tileset '${name}' not found` });
  }

  // Look for <name>.png in the tileset folder
  const pngPath = path.join(tilesetDir, `${name}.png`);
  if (!fs.existsSync(pngPath)) {
    // Try to find any PNG in the folder
    const files = fs.readdirSync(tilesetDir);
    const png = files.find(f => f.toLowerCase().endsWith('.png'));
    if (png) {
      return res.sendFile(path.join(tilesetDir, png));
    }
    return res.status(404).json({ error: `No PNG found in tileset '${name}'` });
  }

  res.sendFile(pngPath);
});

// GET /api/tilesets/:name/json — read the sidecar JSON
app.get('/api/tilesets/:name/json', (req, res) => {
  const name = req.params.name;
  const jsonPath = path.join(ASSETS_DIR, name, `${name}.json`);

  if (!fs.existsSync(jsonPath)) {
    return res.status(404).json({ error: `Sidecar JSON not found for tileset '${name}'` });
  }

  try {
    const content = fs.readFileSync(jsonPath, 'utf-8');
    const data = JSON.parse(content);
    res.json(data);
  } catch (err) {
    console.error(`Error reading sidecar JSON for '${name}':`, err);
    res.status(500).json({ error: 'Failed to parse sidecar JSON' });
  }
});

// PUT /api/tilesets/:name/json — write updated sidecar JSON
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
    console.error(`Error writing sidecar JSON for '${name}':`, err);
    res.status(500).json({ error: 'Failed to write sidecar JSON' });
  }
});

app.listen(PORT, () => {
  console.log(`Particluar Web Editor running at http://localhost:${PORT}`);
  console.log(`Assets directory: ${ASSETS_DIR}`);
});
