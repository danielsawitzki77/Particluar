// ============================================================
// Particluar Tileset Editor — app.js
// Single-page app with Tileset Configurator and Level Editor
// ============================================================

(function() {
  'use strict';

  // --- Tab Switching (also syncs URL) ---
  const tabBtns = document.querySelectorAll('.tab-btn');
  const tabContents = document.querySelectorAll('.tab-content');

  function activateTab(tabName) {
    tabBtns.forEach(b => b.classList.remove('active'));
    tabContents.forEach(c => c.classList.remove('active'));
    const btn = document.querySelector(`.tab-btn[data-tab="${tabName}"]`);
    if (btn) btn.classList.add('active');
    const tabEl = document.getElementById('tab-' + tabName);
    if (tabEl) tabEl.classList.add('active');
    // Update URL without reload
    const url = tabName === 'configurator' ? '/tileset-configurator' : '/level-editor';
    history.replaceState(null, '', url);
  }

  tabBtns.forEach(btn => {
    btn.addEventListener('click', () => activateTab(btn.dataset.tab));
  });

  // Activate tab based on current URL
  if (window.location.pathname.includes('level-editor')) {
    activateTab('level-editor');
  } else {
    activateTab('configurator');
  }

  // --- Close Button ---
  document.getElementById('close-server-btn').addEventListener('click', async () => {
    if (!confirm('Shut down the Tileset Editor server?')) return;
    try {
      await fetch('/api/shutdown', { method: 'POST' });
      document.body.innerHTML = '<div style="display:flex;align-items:center;justify-content:center;height:100vh;background:#1a1a2e;color:#e0e0e0;font-family:sans-serif;"><h1>Editor closed. You can close this tab.</h1></div>';
    } catch (e) {
      // Server already gone
      document.body.innerHTML = '<div style="display:flex;align-items:center;justify-content:center;height:100vh;background:#1a1a2e;color:#e0e0e0;font-family:sans-serif;"><h1>Editor closed. You can close this tab.</h1></div>';
    }
  });

  function setStatus(msg) {
    document.getElementById('status-bar').textContent = msg;
  }

  // ============================================================
  // TILESET CONFIGURATOR
  // ============================================================

  const tilesetList = document.getElementById('tileset-list');
  const sheetSelect = document.getElementById('sheet-select');
  const tilesetCanvas = document.getElementById('tileset-canvas');
  const ctx = tilesetCanvas.getContext('2d');
  const cellWidthInput = document.getElementById('cell-width');
  const cellHeightInput = document.getElementById('cell-height');
  const offsetXInput = document.getElementById('offset-x');
  const offsetYInput = document.getElementById('offset-y');
  const saveTilesetBtn = document.getElementById('save-tileset-btn');
  const tileInfoPanel = document.getElementById('tile-info-panel');

  let currentTilesetName = null;
  let currentSheetFilename = null; // e.g. "ground_grasss.png"
  let currentSheetBase = null;     // e.g. "ground_grasss" (JSON sidecar name)
  let currentTilesetImage = null;  // HTMLImageElement
  let currentTilesetData = null;   // parsed sidecar JSON
  let selectedTileIndex = -1;

  // Load tileset list
  async function loadTilesetList() {
    try {
      const res = await fetch('/api/tilesets');
      const tilesets = await res.json();
      tilesetList.innerHTML = '';
      tilesets.forEach(name => {
        const div = document.createElement('div');
        div.className = 'tileset-item';
        div.textContent = name;
        div.addEventListener('click', () => selectTileset(name));
        tilesetList.appendChild(div);
      });
      populateLETilesetSelect(tilesets);
    } catch (err) {
      setStatus('Error loading tileset list');
      console.error(err);
    }
  }

  async function selectTileset(name) {
    document.querySelectorAll('.tileset-item').forEach(el => {
      el.classList.toggle('selected', el.textContent === name);
    });

    currentTilesetName = name;
    currentSheetFilename = null;
    currentSheetBase = null;
    selectedTileIndex = -1;
    setStatus(`Loading tileset: ${name}...`);

    try {
      // Fetch list of PNG sheets in this tileset folder
      const res = await fetch(`/api/tilesets/${name}/sheets`);
      const sheets = await res.json();

      // Populate sheet dropdown
      sheetSelect.innerHTML = '';
      sheetSelect.disabled = false;

      if (sheets.length === 0) {
        sheetSelect.innerHTML = '<option value="">No PNG files found</option>';
        sheetSelect.disabled = true;
        setStatus(`Tileset '${name}' has no PNG files.`);
        return;
      }

      if (sheets.length === 1) {
        // Auto-select the only sheet
        const opt = document.createElement('option');
        opt.value = sheets[0];
        opt.textContent = sheets[0];
        sheetSelect.appendChild(opt);
        await loadSheet(name, sheets[0]);
      } else {
        // Multiple sheets — let user pick
        const placeholder = document.createElement('option');
        placeholder.value = '';
        placeholder.textContent = `-- ${sheets.length} sheets available --`;
        sheetSelect.appendChild(placeholder);
        sheets.forEach(s => {
          const opt = document.createElement('option');
          opt.value = s;
          opt.textContent = s;
          sheetSelect.appendChild(opt);
        });
        setStatus(`Tileset '${name}': ${sheets.length} sheets. Select one.`);
      }
    } catch (err) {
      setStatus(`Error loading tileset: ${err.message}`);
      console.error(err);
    }
  }

  // Sheet dropdown change
  sheetSelect.addEventListener('change', async () => {
    const filename = sheetSelect.value;
    if (!filename || !currentTilesetName) return;
    await loadSheet(currentTilesetName, filename);
  });

  async function loadSheet(tilesetName, filename) {
    currentSheetFilename = filename;
    currentSheetBase = filename.replace(/\.[^/.]+$/, ''); // strip extension
    selectedTileIndex = -1;

    try {
      // Load image
      const img = new Image();
      img.crossOrigin = 'anonymous';
      await new Promise((resolve, reject) => {
        img.onload = resolve;
        img.onerror = reject;
        img.src = `/api/tilesets/${tilesetName}/sheets/${filename}?t=${Date.now()}`;
      });
      currentTilesetImage = img;

      // Load per-sheet JSON sidecar
      const res = await fetch(`/api/tilesets/${tilesetName}/json/${currentSheetBase}`);
      currentTilesetData = await res.json();

      // Auto-detect cell size from first tile
      if (currentTilesetData.tiles && currentTilesetData.tiles.length > 0) {
        const firstTile = currentTilesetData.tiles[0];
        if (firstTile.source_rect) {
          cellWidthInput.value = firstTile.source_rect.w || 32;
          cellHeightInput.value = firstTile.source_rect.h || 32;
        }
      }

      renderTilesetCanvas();
      renderTileInfo();
      setStatus(`Sheet '${filename}' loaded (${currentTilesetData.tiles.length} tiles defined)`);
    } catch (err) {
      setStatus(`Error loading sheet: ${err.message}`);
      console.error(err);
    }
  }

  function renderTilesetCanvas() {
    if (!currentTilesetImage) return;

    const img = currentTilesetImage;
    tilesetCanvas.width = img.naturalWidth;
    tilesetCanvas.height = img.naturalHeight;

    // Draw image
    ctx.clearRect(0, 0, tilesetCanvas.width, tilesetCanvas.height);
    ctx.drawImage(img, 0, 0);

    // Draw grid overlay
    const cellW = parseInt(cellWidthInput.value) || 32;
    const cellH = parseInt(cellHeightInput.value) || 32;
    const offX = parseInt(offsetXInput.value) || 0;
    const offY = parseInt(offsetYInput.value) || 0;

    ctx.strokeStyle = 'rgba(79, 195, 247, 0.5)';
    ctx.lineWidth = 1;

    for (let x = offX; x <= tilesetCanvas.width; x += cellW) {
      ctx.beginPath();
      ctx.moveTo(x + 0.5, 0);
      ctx.lineTo(x + 0.5, tilesetCanvas.height);
      ctx.stroke();
    }
    for (let y = offY; y <= tilesetCanvas.height; y += cellH) {
      ctx.beginPath();
      ctx.moveTo(0, y + 0.5);
      ctx.lineTo(tilesetCanvas.width, y + 0.5);
      ctx.stroke();
    }

    // Highlight selected tile
    if (selectedTileIndex >= 0 && currentTilesetData && currentTilesetData.tiles[selectedTileIndex]) {
      const tile = currentTilesetData.tiles[selectedTileIndex];
      const sr = tile.source_rect;
      ctx.strokeStyle = '#ff6b6b';
      ctx.lineWidth = 2;
      ctx.strokeRect(sr.x + 1, sr.y + 1, sr.w - 2, sr.h - 2);
    }
  }

  // Canvas click — select tile
  tilesetCanvas.addEventListener('click', (e) => {
    if (!currentTilesetData || !currentTilesetData.tiles) return;

    const rect = tilesetCanvas.getBoundingClientRect();
    const scaleX = tilesetCanvas.width / rect.width;
    const scaleY = tilesetCanvas.height / rect.height;
    const clickX = (e.clientX - rect.left) * scaleX;
    const clickY = (e.clientY - rect.top) * scaleY;

    const cellW = parseInt(cellWidthInput.value) || 32;
    const cellH = parseInt(cellHeightInput.value) || 32;
    const offX = parseInt(offsetXInput.value) || 0;
    const offY = parseInt(offsetYInput.value) || 0;

    // Find which tile the click is in
    const col = Math.floor((clickX - offX) / cellW);
    const row = Math.floor((clickY - offY) / cellH);
    const tileX = offX + col * cellW;
    const tileY = offY + row * cellH;

    // Match against tiles
    let foundIndex = -1;
    for (let i = 0; i < currentTilesetData.tiles.length; i++) {
      const sr = currentTilesetData.tiles[i].source_rect;
      if (sr.x === tileX && sr.y === tileY) {
        foundIndex = i;
        break;
      }
    }

    // If no exact match found, select by grid position anyway and create a tile entry
    if (foundIndex === -1) {
      // Check if click is within image bounds
      if (tileX >= 0 && tileY >= 0 && tileX + cellW <= tilesetCanvas.width && tileY + cellH <= tilesetCanvas.height) {
        // Create a new tile entry
        const newId = `tile_${col}_${row}`;
        currentTilesetData.tiles.push({
          id: newId,
          source_rect: { x: tileX, y: tileY, w: cellW, h: cellH },
          adjacency: { up: [], down: [], left: [], right: [] }
        });
        foundIndex = currentTilesetData.tiles.length - 1;
        setStatus(`Created new tile '${newId}' at (${tileX}, ${tileY})`);
      }
    }

    selectedTileIndex = foundIndex;
    renderTilesetCanvas();
    renderTileInfo();
  });

  function renderTileInfo() {
    if (selectedTileIndex < 0 || !currentTilesetData || !currentTilesetData.tiles[selectedTileIndex]) {
      tileInfoPanel.innerHTML = '<p style="color:#a0a0a0; font-size:12px;">Click a tile on the canvas to select it.</p>';
      return;
    }

    const tile = currentTilesetData.tiles[selectedTileIndex];
    const adj = tile.adjacency || { up: [], down: [], left: [], right: [] };

    let html = `
      <div class="tile-info">
        <strong>ID:</strong> <input type="text" id="tile-id-input" value="${escapeHtml(tile.id)}" style="width:140px; padding:2px 4px; background:#0d0d1a; border:1px solid #0f3460; color:#e0e0e0; border-radius:3px; font-size:12px;">
        <br><strong>Source:</strong> (${tile.source_rect.x}, ${tile.source_rect.y}) ${tile.source_rect.w}x${tile.source_rect.h}
      </div>
    `;

    const directions = ['up', 'down', 'left', 'right'];
    directions.forEach(dir => {
      const neighbors = adj[dir] || [];
      html += `<div class="adjacency-section">
        <h4>${dir}</h4>
        <div class="adjacency-list">`;
      neighbors.forEach((n, i) => {
        html += `<span class="adj-tag">${escapeHtml(n)} <span class="remove-btn" data-dir="${dir}" data-idx="${i}">&times;</span></span>`;
      });
      html += `</div>
        <div class="add-adj-row">
          <input type="text" placeholder="Add neighbor ID" id="add-adj-${dir}">
          <button data-dir="${dir}" class="add-adj-btn">+</button>
        </div>
      </div>`;
    });

    tileInfoPanel.innerHTML = html;

    // Bind tile ID change
    const tileIdInput = document.getElementById('tile-id-input');
    tileIdInput.addEventListener('change', () => {
      currentTilesetData.tiles[selectedTileIndex].id = tileIdInput.value;
      setStatus(`Tile ID updated to '${tileIdInput.value}'`);
    });

    // Bind remove buttons
    tileInfoPanel.querySelectorAll('.remove-btn').forEach(btn => {
      btn.addEventListener('click', () => {
        const dir = btn.dataset.dir;
        const idx = parseInt(btn.dataset.idx);
        currentTilesetData.tiles[selectedTileIndex].adjacency[dir].splice(idx, 1);
        renderTileInfo();
      });
    });

    // Bind add buttons
    tileInfoPanel.querySelectorAll('.add-adj-btn').forEach(btn => {
      btn.addEventListener('click', () => {
        const dir = btn.dataset.dir;
        const input = document.getElementById(`add-adj-${dir}`);
        const val = input.value.trim();
        if (val) {
          if (!currentTilesetData.tiles[selectedTileIndex].adjacency[dir]) {
            currentTilesetData.tiles[selectedTileIndex].adjacency[dir] = [];
          }
          currentTilesetData.tiles[selectedTileIndex].adjacency[dir].push(val);
          input.value = '';
          renderTileInfo();
        }
      });
    });

    // Allow Enter key to add
    directions.forEach(dir => {
      const input = document.getElementById(`add-adj-${dir}`);
      input.addEventListener('keydown', (e) => {
        if (e.key === 'Enter') {
          e.preventDefault();
          const val = input.value.trim();
          if (val) {
            if (!currentTilesetData.tiles[selectedTileIndex].adjacency[dir]) {
              currentTilesetData.tiles[selectedTileIndex].adjacency[dir] = [];
            }
            currentTilesetData.tiles[selectedTileIndex].adjacency[dir].push(val);
            input.value = '';
            renderTileInfo();
          }
        }
      });
    });
  }

  // Save tileset
  saveTilesetBtn.addEventListener('click', async () => {
    if (!currentTilesetName || !currentTilesetData || !currentSheetBase) {
      setStatus('No sheet loaded to save');
      return;
    }
    try {
      const res = await fetch(`/api/tilesets/${currentTilesetName}/json/${currentSheetBase}`, {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(currentTilesetData)
      });
      if (res.ok) {
        setStatus(`Saved '${currentSheetBase}.json' in tileset '${currentTilesetName}'`);
      } else {
        setStatus('Error saving tileset');
      }
    } catch (err) {
      setStatus(`Save error: ${err.message}`);
    }
  });

  // Grid control changes — re-render
  [cellWidthInput, cellHeightInput, offsetXInput, offsetYInput].forEach(input => {
    input.addEventListener('input', renderTilesetCanvas);
  });

  function escapeHtml(str) {
    return String(str).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
  }

  // ============================================================
  // LEVEL EDITOR
  // ============================================================

  const leWidthInput = document.getElementById('le-width');
  const leHeightInput = document.getElementById('le-height');
  const leTilesetSelect = document.getElementById('le-tileset-select');
  const leLoadBtn = document.getElementById('le-load-btn');
  const leNewGridBtn = document.getElementById('le-new-grid-btn');
  const leExportBtn = document.getElementById('le-export-btn');
  const lePalette = document.getElementById('le-palette');
  const levelCanvas = document.getElementById('level-canvas');
  const lCtx = levelCanvas.getContext('2d');

  let leGrid = [];          // 2D array [row][col] of tile IDs (or "")
  let leGridW = 16;
  let leGridH = 16;
  let leTilesetName = null;
  let leTilesetData = null;
  let leTilesetImage = null;
  let leSelectedTileId = null;
  let leCellW = 32;
  let leCellH = 32;

  function populateLETilesetSelect(tilesets) {
    leTilesetSelect.innerHTML = '<option value="">-- Select Tileset --</option>';
    tilesets.forEach(name => {
      const opt = document.createElement('option');
      opt.value = name;
      opt.textContent = name;
      leTilesetSelect.appendChild(opt);
    });
  }

  leLoadBtn.addEventListener('click', async () => {
    const name = leTilesetSelect.value;
    if (!name) {
      setStatus('Select a tileset first');
      return;
    }

    try {
      // Load image
      const img = new Image();
      img.crossOrigin = 'anonymous';
      await new Promise((resolve, reject) => {
        img.onload = resolve;
        img.onerror = reject;
        img.src = `/api/tilesets/${name}/image?t=${Date.now()}`;
      });
      leTilesetImage = img;

      // Load JSON
      const res = await fetch(`/api/tilesets/${name}/json`);
      if (res.ok) {
        leTilesetData = await res.json();
      } else {
        leTilesetData = { tiles: [] };
      }

      leTilesetName = name;
      leSelectedTileId = null;

      // Auto-detect cell size
      if (leTilesetData.tiles.length > 0 && leTilesetData.tiles[0].source_rect) {
        leCellW = leTilesetData.tiles[0].source_rect.w || 32;
        leCellH = leTilesetData.tiles[0].source_rect.h || 32;
      }

      renderLEPalette();
      renderLECanvas();
      setStatus(`Level Editor: Loaded tileset '${name}' (${leTilesetData.tiles.length} tiles)`);
    } catch (err) {
      setStatus(`Error loading tileset: ${err.message}`);
    }
  });

  leNewGridBtn.addEventListener('click', () => {
    leGridW = Math.max(1, Math.min(256, parseInt(leWidthInput.value) || 16));
    leGridH = Math.max(1, Math.min(256, parseInt(leHeightInput.value) || 16));
    leWidthInput.value = leGridW;
    leHeightInput.value = leGridH;

    leGrid = [];
    for (let r = 0; r < leGridH; r++) {
      leGrid.push(new Array(leGridW).fill(''));
    }
    renderLECanvas();
    setStatus(`New grid created: ${leGridW}x${leGridH}`);
  });

  function renderLEPalette() {
    lePalette.innerHTML = '';
    if (!leTilesetData || !leTilesetImage) return;

    leTilesetData.tiles.forEach(tile => {
      const div = document.createElement('div');
      div.className = 'palette-tile';
      div.title = tile.id;

      const c = document.createElement('canvas');
      c.width = tile.source_rect.w;
      c.height = tile.source_rect.h;
      const tileCtx = c.getContext('2d');
      tileCtx.drawImage(leTilesetImage,
        tile.source_rect.x, tile.source_rect.y, tile.source_rect.w, tile.source_rect.h,
        0, 0, tile.source_rect.w, tile.source_rect.h);
      div.appendChild(c);

      div.addEventListener('click', () => {
        document.querySelectorAll('.palette-tile').forEach(el => el.classList.remove('selected'));
        div.classList.add('selected');
        leSelectedTileId = tile.id;
        setStatus(`Selected tile: ${tile.id}`);
      });

      lePalette.appendChild(div);
    });
  }

  function renderLECanvas() {
    if (leGrid.length === 0) {
      // Initialize grid
      leGridW = Math.max(1, Math.min(256, parseInt(leWidthInput.value) || 16));
      leGridH = Math.max(1, Math.min(256, parseInt(leHeightInput.value) || 16));
      leGrid = [];
      for (let r = 0; r < leGridH; r++) {
        leGrid.push(new Array(leGridW).fill(''));
      }
    }

    levelCanvas.width = leGridW * leCellW;
    levelCanvas.height = leGridH * leCellH;

    lCtx.clearRect(0, 0, levelCanvas.width, levelCanvas.height);

    // Draw tiles
    for (let r = 0; r < leGridH; r++) {
      for (let c = 0; c < leGridW; c++) {
        const tileId = leGrid[r][c];
        const dx = c * leCellW;
        const dy = r * leCellH;

        if (tileId && leTilesetData && leTilesetImage) {
          const tileDef = leTilesetData.tiles.find(t => t.id === tileId);
          if (tileDef) {
            lCtx.drawImage(leTilesetImage,
              tileDef.source_rect.x, tileDef.source_rect.y, tileDef.source_rect.w, tileDef.source_rect.h,
              dx, dy, leCellW, leCellH);
          } else {
            // Unresolved tile — magenta
            lCtx.fillStyle = '#ff00ff';
            lCtx.fillRect(dx, dy, leCellW, leCellH);
          }
        }
      }
    }

    // Draw grid lines
    lCtx.strokeStyle = 'rgba(79, 195, 247, 0.3)';
    lCtx.lineWidth = 1;
    for (let x = 0; x <= levelCanvas.width; x += leCellW) {
      lCtx.beginPath();
      lCtx.moveTo(x + 0.5, 0);
      lCtx.lineTo(x + 0.5, levelCanvas.height);
      lCtx.stroke();
    }
    for (let y = 0; y <= levelCanvas.height; y += leCellH) {
      lCtx.beginPath();
      lCtx.moveTo(0, y + 0.5);
      lCtx.lineTo(levelCanvas.width, y + 0.5);
      lCtx.stroke();
    }
  }

  // Paint / Erase on level canvas
  let isDrawing = false;

  function getLEGridPos(e) {
    const rect = levelCanvas.getBoundingClientRect();
    const scaleX = levelCanvas.width / rect.width;
    const scaleY = levelCanvas.height / rect.height;
    const x = (e.clientX - rect.left) * scaleX;
    const y = (e.clientY - rect.top) * scaleY;
    const col = Math.floor(x / leCellW);
    const row = Math.floor(y / leCellH);
    return { row, col };
  }

  function paintTile(e) {
    if (leGrid.length === 0) return;
    const { row, col } = getLEGridPos(e);
    if (row < 0 || row >= leGridH || col < 0 || col >= leGridW) return;

    if (e.buttons === 1) {
      // Left click — paint
      if (!leSelectedTileId) {
        setStatus('Select a tile from the palette first');
        return;
      }
      if (leGrid[row][col] !== leSelectedTileId) {
        leGrid[row][col] = leSelectedTileId;
        renderLECanvas();
      }
    } else if (e.buttons === 2) {
      // Right click — erase
      if (leGrid[row][col] !== '') {
        leGrid[row][col] = '';
        renderLECanvas();
      }
    }
  }

  levelCanvas.addEventListener('mousedown', (e) => {
    isDrawing = true;
    paintTile(e);
  });

  levelCanvas.addEventListener('mousemove', (e) => {
    if (isDrawing) paintTile(e);
  });

  levelCanvas.addEventListener('mouseup', () => { isDrawing = false; });
  levelCanvas.addEventListener('mouseleave', () => { isDrawing = false; });

  // Prevent context menu on right click
  levelCanvas.addEventListener('contextmenu', (e) => e.preventDefault());

  // Export Map JSON
  leExportBtn.addEventListener('click', () => {
    if (leGrid.length === 0) {
      setStatus('No grid to export. Create a grid first.');
      return;
    }
    if (!leTilesetName) {
      setStatus('No tileset loaded. Load a tileset first.');
      return;
    }

    // Validate: check for empty cells
    let hasEmptyCells = false;
    for (let r = 0; r < leGridH; r++) {
      for (let c = 0; c < leGridW; c++) {
        if (!leGrid[r][c]) {
          hasEmptyCells = true;
          break;
        }
      }
      if (hasEmptyCells) break;
    }

    if (hasEmptyCells) {
      if (!confirm('Some cells are empty. Empty cells will be exported as empty strings. Continue?')) {
        return;
      }
    }

    const mapFile = {
      width: leGridW,
      height: leGridH,
      tileset: leTilesetName,
      grid: leGrid
    };

    const jsonStr = JSON.stringify(mapFile, null, 2);
    const blob = new Blob([jsonStr], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `map_${leGridW}x${leGridH}.json`;
    a.click();
    URL.revokeObjectURL(url);
    setStatus(`Map exported: ${leGridW}x${leGridH}, tileset: ${leTilesetName}`);
  });

  // ============================================================
  // INITIALIZATION
  // ============================================================

  loadTilesetList();
  leNewGridBtn.click(); // Initialize default grid

})();
