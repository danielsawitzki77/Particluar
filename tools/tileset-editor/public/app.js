// ============================================================
// Particluar Tileset Editor — app.js
// Tileset Configurator + Level Editor with zoom, labels, filters
// ============================================================

(function() {
  'use strict';

  // --- Tab Switching (syncs URL) ---
  const tabBtns = document.querySelectorAll('.tab-btn');
  const tabContents = document.querySelectorAll('.tab-content');

  function activateTab(tabName) {
    tabBtns.forEach(b => b.classList.remove('active'));
    tabContents.forEach(c => c.classList.remove('active'));
    const btn = document.querySelector(`.tab-btn[data-tab="${tabName}"]`);
    if (btn) btn.classList.add('active');
    const tabEl = document.getElementById('tab-' + tabName);
    if (tabEl) tabEl.classList.add('active');
    const url = tabName === 'configurator' ? '/tileset-configurator' : '/level-editor';
    history.replaceState(null, '', url);
  }

  tabBtns.forEach(btn => {
    btn.addEventListener('click', () => activateTab(btn.dataset.tab));
  });

  if (window.location.pathname.includes('level-editor')) {
    activateTab('level-editor');
  } else {
    activateTab('configurator');
  }

  // --- Close Button ---
  document.getElementById('close-server-btn').addEventListener('click', async () => {
    if (!confirm('Shut down the Tileset Editor server?')) return;
    try { await fetch('/api/shutdown', { method: 'POST' }); } catch(e) {}
    document.body.innerHTML = '<div style="display:flex;align-items:center;justify-content:center;height:100vh;background:#1a1a2e;color:#e0e0e0;font-family:sans-serif"><h1>Editor closed.</h1></div>';
    setTimeout(() => window.close(), 1000);
  });

  function setStatus(msg) {
    document.getElementById('status-bar').textContent = msg;
  }
  function escapeHtml(s) {
    return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');
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
  const zoomInput = document.getElementById('zoom-level');
  const saveTilesetBtn = document.getElementById('save-tileset-btn');
  const tileInfoPanel = document.getElementById('tile-info-panel');

  let currentTilesetName = null;
  let currentSheetFilename = null;
  let currentSheetBase = null;
  let currentTilesetImage = null;
  let currentTilesetData = null; // { tiles: [], labels: [] }
  let selectedTileIndex = -1;
  let highlightedCell = null; // {x, y, w, h} for a grid cell with no existing tile

  // --- Tileset list ---
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
    highlightedCell = null;
    setStatus(`Loading tileset: ${name}...`);
    try {
      const res = await fetch(`/api/tilesets/${name}/sheets`);
      const sheets = await res.json();
      sheetSelect.innerHTML = '';
      sheetSelect.disabled = false;
      if (sheets.length === 0) {
        sheetSelect.innerHTML = '<option value="">No PNG files found</option>';
        sheetSelect.disabled = true;
        return;
      }
      if (sheets.length === 1) {
        const opt = document.createElement('option');
        opt.value = sheets[0]; opt.textContent = sheets[0];
        sheetSelect.appendChild(opt);
        await loadSheet(name, sheets[0]);
      } else {
        const ph = document.createElement('option');
        ph.value = ''; ph.textContent = `-- ${sheets.length} sheets --`;
        sheetSelect.appendChild(ph);
        sheets.forEach(s => {
          const opt = document.createElement('option');
          opt.value = s; opt.textContent = s;
          sheetSelect.appendChild(opt);
        });
        setStatus(`Tileset '${name}': ${sheets.length} sheets. Select one.`);
      }
    } catch (err) { setStatus(`Error: ${err.message}`); }
  }

  sheetSelect.addEventListener('change', async () => {
    const f = sheetSelect.value;
    if (f && currentTilesetName) await loadSheet(currentTilesetName, f);
  });

  async function loadSheet(tilesetName, filename) {
    currentSheetFilename = filename;
    currentSheetBase = filename.replace(/\.[^/.]+$/, '');
    selectedTileIndex = -1;
    highlightedCell = null;
    try {
      const img = new Image();
      img.crossOrigin = 'anonymous';
      await new Promise((resolve, reject) => {
        img.onload = resolve; img.onerror = reject;
        img.src = `/api/tilesets/${tilesetName}/sheets/${filename}?t=${Date.now()}`;
      });
      currentTilesetImage = img;
      const res = await fetch(`/api/tilesets/${tilesetName}/json/${currentSheetBase}`);
      currentTilesetData = await res.json();
      if (!currentTilesetData.tiles) currentTilesetData.tiles = [];
      if (!currentTilesetData.labels) currentTilesetData.labels = [];
      if (currentTilesetData.tiles.length > 0 && currentTilesetData.tiles[0].source_rect) {
        cellWidthInput.value = currentTilesetData.tiles[0].source_rect.w || 32;
        cellHeightInput.value = currentTilesetData.tiles[0].source_rect.h || 32;
      }
      renderTilesetCanvas();
      renderTileInfo();
      renderLabelsPanel();
      renderCreatedTilesList();
      setStatus(`Sheet '${filename}' loaded (${currentTilesetData.tiles.length} tiles)`);
    } catch (err) { setStatus(`Error loading sheet: ${err.message}`); }
  }

  // --- Canvas rendering with zoom ---
  function renderTilesetCanvas() {
    if (!currentTilesetImage) return;
    const img = currentTilesetImage;
    const zoom = Math.max(1, parseInt(zoomInput.value) || 1);
    tilesetCanvas.width = img.naturalWidth * zoom;
    tilesetCanvas.height = img.naturalHeight * zoom;
    ctx.imageSmoothingEnabled = false;
    ctx.clearRect(0, 0, tilesetCanvas.width, tilesetCanvas.height);
    ctx.drawImage(img, 0, 0, tilesetCanvas.width, tilesetCanvas.height);
    const cellW = (parseInt(cellWidthInput.value) || 32) * zoom;
    const cellH = (parseInt(cellHeightInput.value) || 32) * zoom;
    const offX = (parseInt(offsetXInput.value) || 0) * zoom;
    const offY = (parseInt(offsetYInput.value) || 0) * zoom;
    ctx.strokeStyle = 'rgba(79, 195, 247, 0.5)';
    ctx.lineWidth = 1;
    for (let x = offX; x <= tilesetCanvas.width; x += cellW) {
      ctx.beginPath(); ctx.moveTo(x+0.5, 0); ctx.lineTo(x+0.5, tilesetCanvas.height); ctx.stroke();
    }
    for (let y = offY; y <= tilesetCanvas.height; y += cellH) {
      ctx.beginPath(); ctx.moveTo(0, y+0.5); ctx.lineTo(tilesetCanvas.width, y+0.5); ctx.stroke();
    }
    // Draw selected tile highlight (red)
    if (selectedTileIndex >= 0 && currentTilesetData && currentTilesetData.tiles[selectedTileIndex]) {
      const sr = currentTilesetData.tiles[selectedTileIndex].source_rect;
      ctx.strokeStyle = '#ff6b6b'; ctx.lineWidth = 2;
      ctx.strokeRect(sr.x*zoom+1, sr.y*zoom+1, sr.w*zoom-2, sr.h*zoom-2);
    }
    // Draw highlighted cell (yellow/orange) for potential new tile
    if (highlightedCell) {
      ctx.strokeStyle = '#ff9800'; ctx.lineWidth = 3;
      ctx.strokeRect(highlightedCell.x*zoom+1, highlightedCell.y*zoom+1, highlightedCell.w*zoom-2, highlightedCell.h*zoom-2);
      ctx.fillStyle = 'rgba(255, 152, 0, 0.2)';
      ctx.fillRect(highlightedCell.x*zoom+1, highlightedCell.y*zoom+1, highlightedCell.w*zoom-2, highlightedCell.h*zoom-2);
    }
    // Draw outlines for all created tiles (subtle)
    if (currentTilesetData && currentTilesetData.tiles) {
      ctx.strokeStyle = 'rgba(79, 195, 247, 0.8)'; ctx.lineWidth = 1;
      currentTilesetData.tiles.forEach((t, i) => {
        if (i === selectedTileIndex) return;
        const sr = t.source_rect;
        ctx.strokeRect(sr.x*zoom+0.5, sr.y*zoom+0.5, sr.w*zoom-1, sr.h*zoom-1);
      });
    }
  }

  // --- Canvas click: select existing tile OR highlight grid cell for creation ---
  tilesetCanvas.addEventListener('click', (e) => {
    if (!currentTilesetData || !currentTilesetImage) return;
    const zoom = Math.max(1, parseInt(zoomInput.value) || 1);
    const rect = tilesetCanvas.getBoundingClientRect();
    const scaleX = tilesetCanvas.width / rect.width;
    const scaleY = tilesetCanvas.height / rect.height;
    const clickX = (e.clientX - rect.left) * scaleX / zoom;
    const clickY = (e.clientY - rect.top) * scaleY / zoom;
    const cellW = parseInt(cellWidthInput.value) || 32;
    const cellH = parseInt(cellHeightInput.value) || 32;
    const offX = parseInt(offsetXInput.value) || 0;
    const offY = parseInt(offsetYInput.value) || 0;
    const col = Math.floor((clickX - offX) / cellW);
    const row = Math.floor((clickY - offY) / cellH);
    const tileX = offX + col * cellW;
    const tileY = offY + row * cellH;

    // Check if an existing tile matches this position
    let foundIndex = -1;
    for (let i = 0; i < currentTilesetData.tiles.length; i++) {
      const sr = currentTilesetData.tiles[i].source_rect;
      if (sr.x === tileX && sr.y === tileY && sr.w === cellW && sr.h === cellH) {
        foundIndex = i; break;
      }
    }

    if (foundIndex >= 0) {
      // Select existing tile
      selectedTileIndex = foundIndex;
      highlightedCell = null;
    } else if (tileX >= 0 && tileY >= 0 &&
               tileX + cellW <= currentTilesetImage.naturalWidth &&
               tileY + cellH <= currentTilesetImage.naturalHeight) {
      // Highlight this empty cell (do NOT auto-create)
      highlightedCell = { x: tileX, y: tileY, w: cellW, h: cellH };
      selectedTileIndex = -1;
      setStatus(`Cell highlighted at (${tileX},${tileY}) ${cellW}x${cellH} — click "Create Tile" to add it.`);
    }

    renderTilesetCanvas();
    renderTileInfo();
    renderCreatedTilesList();
  });

  // --- Created Tiles list (persists across grid changes) ---
  function renderCreatedTilesList() {
    const container = document.getElementById('created-tiles-list');
    if (!container || !currentTilesetData) return;
    const tiles = currentTilesetData.tiles;
    let html = '';

    // Show "Create Tile" button if a cell is highlighted but no tile exists there
    if (highlightedCell) {
      html += `<button class="create-tile-btn" id="create-tile-btn">Create Tile at (${highlightedCell.x},${highlightedCell.y})</button>`;
    }

    if (tiles.length === 0 && !highlightedCell) {
      html += '<p style="color:#a0a0a0;font-size:11px;">Click grid cells to highlight, then create tiles.</p>';
      container.innerHTML = html;
      return;
    }
    tiles.forEach((t, i) => {
      const active = (i === selectedTileIndex) ? ' active' : '';
      html += `<div class="created-tile-item${active}" data-idx="${i}">
        <span>${escapeHtml(t.id)} <span style="color:#666">(${t.source_rect.w}x${t.source_rect.h})</span></span>
        <span class="del-tile" data-idx="${i}">&times;</span>
      </div>`;
    });
    container.innerHTML = html;

    // Bind Create Tile button
    const createBtn = document.getElementById('create-tile-btn');
    if (createBtn) {
      createBtn.addEventListener('click', () => {
        if (!highlightedCell || !currentTilesetData) return;
        const hc = highlightedCell;
        const newId = `tile_${hc.x}_${hc.y}_${hc.w}x${hc.h}`;
        currentTilesetData.tiles.push({
          id: newId, source_rect: {x: hc.x, y: hc.y, w: hc.w, h: hc.h},
          adjacency: {up:[],down:[],left:[],right:[]}, labels: []
        });
        selectedTileIndex = currentTilesetData.tiles.length - 1;
        highlightedCell = null;
        setStatus(`Created tile '${newId}' (${hc.w}x${hc.h} at ${hc.x},${hc.y})`);
        renderTilesetCanvas();
        renderTileInfo();
        renderCreatedTilesList();
      });
    }

    // Click to select
    container.querySelectorAll('.created-tile-item').forEach(el => {
      el.addEventListener('click', (e) => {
        if (e.target.classList.contains('del-tile')) return;
        selectedTileIndex = parseInt(el.dataset.idx);
        highlightedCell = null;
        renderTilesetCanvas();
        renderTileInfo();
        renderCreatedTilesList();
      });
    });
    // Delete button
    container.querySelectorAll('.del-tile').forEach(btn => {
      btn.addEventListener('click', (e) => {
        e.stopPropagation();
        const idx = parseInt(btn.dataset.idx);
        currentTilesetData.tiles.splice(idx, 1);
        if (selectedTileIndex === idx) selectedTileIndex = -1;
        else if (selectedTileIndex > idx) selectedTileIndex--;
        renderTilesetCanvas();
        renderTileInfo();
        renderCreatedTilesList();
      });
    });
  }

  // --- Labels panel (tileset-level labels definition) ---
  function renderLabelsPanel() {
    const panel = document.getElementById('labels-panel');
    if (!panel || !currentTilesetData) { if(panel) panel.innerHTML=''; return; }
    const labels = currentTilesetData.labels || [];
    let html = '<div class="add-adj-row" style="margin-bottom:8px"><input type="text" id="new-label-input" placeholder="New label name"><button id="add-label-btn">+</button></div>';
    html += '<div class="adjacency-list">';
    labels.forEach((l, i) => {
      html += `<span class="adj-tag">${escapeHtml(l)} <span class="remove-label-btn" data-idx="${i}">&times;</span></span>`;
    });
    html += '</div>';
    panel.innerHTML = html;
    document.getElementById('add-label-btn').addEventListener('click', () => {
      const input = document.getElementById('new-label-input');
      const val = input.value.trim();
      if (val && !currentTilesetData.labels.includes(val)) {
        currentTilesetData.labels.push(val);
        input.value = '';
        renderLabelsPanel();
        renderTileInfo();
      }
    });
    document.getElementById('new-label-input').addEventListener('keydown', (e) => {
      if (e.key === 'Enter') { e.preventDefault(); document.getElementById('add-label-btn').click(); }
    });
    panel.querySelectorAll('.remove-label-btn').forEach(btn => {
      btn.addEventListener('click', () => {
        const idx = parseInt(btn.dataset.idx);
        const removed = currentTilesetData.labels.splice(idx, 1)[0];
        currentTilesetData.tiles.forEach(t => {
          if (t.labels) t.labels = t.labels.filter(l => l !== removed);
        });
        renderLabelsPanel();
        renderTileInfo();
      });
    });
  }

  // --- Tile info panel (with label assignment) ---
  function renderTileInfo() {
    if (selectedTileIndex < 0 || !currentTilesetData || !currentTilesetData.tiles[selectedTileIndex]) {
      if (highlightedCell) {
        tileInfoPanel.innerHTML = `<p style="color:#ff9800;font-size:12px;">Cell (${highlightedCell.x},${highlightedCell.y}) ${highlightedCell.w}x${highlightedCell.h} highlighted.<br>Click "Create Tile" above to add it.</p>`;
      } else {
        tileInfoPanel.innerHTML = '<p style="color:#a0a0a0;font-size:12px;">Click a tile on the canvas to select it.</p>';
      }
      return;
    }
    const tile = currentTilesetData.tiles[selectedTileIndex];
    if (!tile.labels) tile.labels = [];
    const adj = tile.adjacency || {up:[],down:[],left:[],right:[]};
    const allLabels = currentTilesetData.labels || [];

    let html = `<div class="tile-info">
      <strong>ID:</strong> <input type="text" id="tile-id-input" value="${escapeHtml(tile.id)}" style="width:130px;padding:2px 4px;background:#0d0d1a;border:1px solid #0f3460;color:#e0e0e0;border-radius:3px;font-size:12px;">
      <br><strong>Src:</strong> (${tile.source_rect.x},${tile.source_rect.y}) ${tile.source_rect.w}x${tile.source_rect.h}
    </div>`;

    // Labels assignment
    html += '<div class="adjacency-section"><h4>Labels</h4><div class="adjacency-list">';
    tile.labels.forEach((l, i) => {
      html += `<span class="adj-tag">${escapeHtml(l)} <span class="remove-tile-label" data-idx="${i}">&times;</span></span>`;
    });
    html += '</div>';
    if (allLabels.length > 0) {
      const available = allLabels.filter(l => !tile.labels.includes(l));
      if (available.length > 0) {
        html += '<select id="add-tile-label-select" style="width:100%;padding:4px;background:#1a1a2e;border:1px solid #0f3460;color:#e0e0e0;border-radius:3px;font-size:12px;margin-top:4px">';
        html += '<option value="">+ Add label...</option>';
        available.forEach(l => { html += `<option value="${escapeHtml(l)}">${escapeHtml(l)}</option>`; });
        html += '</select>';
      }
    }
    html += '</div>';

    // Adjacency rules
    const directions = ['up','down','left','right'];
    directions.forEach(dir => {
      const neighbors = adj[dir] || [];
      html += `<div class="adjacency-section"><h4>${dir}</h4><div class="adjacency-list">`;
      neighbors.forEach((n, i) => {
        html += `<span class="adj-tag">${escapeHtml(n)} <span class="remove-btn" data-dir="${dir}" data-idx="${i}">&times;</span></span>`;
      });
      html += `</div><div class="add-adj-row"><input type="text" placeholder="Neighbor ID" id="add-adj-${dir}"><button data-dir="${dir}" class="add-adj-btn">+</button></div></div>`;
    });
    tileInfoPanel.innerHTML = html;

    // Bind ID change
    document.getElementById('tile-id-input').addEventListener('change', (e) => {
      currentTilesetData.tiles[selectedTileIndex].id = e.target.value;
      renderCreatedTilesList();
    });
    // Bind label add
    const labelSelect = document.getElementById('add-tile-label-select');
    if (labelSelect) {
      labelSelect.addEventListener('change', () => {
        const val = labelSelect.value;
        if (val) { tile.labels.push(val); renderTileInfo(); }
      });
    }
    // Bind label remove
    tileInfoPanel.querySelectorAll('.remove-tile-label').forEach(btn => {
      btn.addEventListener('click', () => {
        tile.labels.splice(parseInt(btn.dataset.idx), 1);
        renderTileInfo();
      });
    });
    // Bind adjacency remove
    tileInfoPanel.querySelectorAll('.remove-btn').forEach(btn => {
      btn.addEventListener('click', () => {
        const dir = btn.dataset.dir;
        currentTilesetData.tiles[selectedTileIndex].adjacency[dir].splice(parseInt(btn.dataset.idx), 1);
        renderTileInfo();
      });
    });
    // Bind adjacency add
    tileInfoPanel.querySelectorAll('.add-adj-btn').forEach(btn => {
      btn.addEventListener('click', () => {
        const dir = btn.dataset.dir;
        const input = document.getElementById(`add-adj-${dir}`);
        const val = input.value.trim();
        if (val) {
          if (!currentTilesetData.tiles[selectedTileIndex].adjacency[dir])
            currentTilesetData.tiles[selectedTileIndex].adjacency[dir] = [];
          currentTilesetData.tiles[selectedTileIndex].adjacency[dir].push(val);
          input.value = ''; renderTileInfo();
        }
      });
    });
    // Enter key for adjacency
    const directions2 = ['up','down','left','right'];
    directions2.forEach(dir => {
      const input = document.getElementById(`add-adj-${dir}`);
      if (input) input.addEventListener('keydown', (e) => {
        if (e.key === 'Enter') { e.preventDefault(); tileInfoPanel.querySelector(`.add-adj-btn[data-dir="${dir}"]`).click(); }
      });
    });
  }

  // Save
  saveTilesetBtn.addEventListener('click', async () => {
    if (!currentTilesetName || !currentTilesetData || !currentSheetBase) { setStatus('No sheet loaded'); return; }
    try {
      const res = await fetch(`/api/tilesets/${currentTilesetName}/json/${currentSheetBase}`, {
        method: 'PUT', headers: {'Content-Type':'application/json'}, body: JSON.stringify(currentTilesetData)
      });
      if (res.ok) setStatus(`Saved '${currentSheetBase}.json'`);
      else setStatus('Error saving');
    } catch (err) { setStatus(`Save error: ${err.message}`); }
  });

  // Re-render on control changes
  [cellWidthInput, cellHeightInput, offsetXInput, offsetYInput, zoomInput].forEach(input => {
    if (input) input.addEventListener('input', renderTilesetCanvas);
  });

  // ============================================================
  // LEVEL EDITOR (with label filter)
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
  const leLabelFilter = document.getElementById('le-label-filter');

  let leGrid = [];
  let leGridW = 16, leGridH = 16;
  let leTilesetName = null;
  let leTilesetData = null;
  let leTilesetImage = null;
  let leSelectedTileId = null;
  let leCellW = 32, leCellH = 32;
  let leActiveLabelFilter = '';

  function populateLETilesetSelect(tilesets) {
    leTilesetSelect.innerHTML = '<option value="">-- Select Tileset --</option>';
    tilesets.forEach(name => {
      const opt = document.createElement('option');
      opt.value = name; opt.textContent = name;
      leTilesetSelect.appendChild(opt);
    });
  }

  leLoadBtn.addEventListener('click', async () => {
    const name = leTilesetSelect.value;
    if (!name) { setStatus('Select a tileset first'); return; }
    try {
      const img = new Image();
      img.crossOrigin = 'anonymous';
      await new Promise((resolve, reject) => {
        img.onload = resolve; img.onerror = reject;
        img.src = `/api/tilesets/${name}/image?t=${Date.now()}`;
      });
      leTilesetImage = img;
      const res = await fetch(`/api/tilesets/${name}/json`);
      leTilesetData = res.ok ? await res.json() : { tiles: [], labels: [] };
      if (!leTilesetData.tiles) leTilesetData.tiles = [];
      if (!leTilesetData.labels) leTilesetData.labels = [];
      leTilesetName = name;
      leSelectedTileId = null;
      if (leTilesetData.tiles.length > 0 && leTilesetData.tiles[0].source_rect) {
        leCellW = leTilesetData.tiles[0].source_rect.w || 32;
        leCellH = leTilesetData.tiles[0].source_rect.h || 32;
      }
      populateLELabelFilter();
      renderLEPalette();
      renderLECanvas();
      setStatus(`Level Editor: Loaded '${name}' (${leTilesetData.tiles.length} tiles)`);
    } catch (err) { setStatus(`Error: ${err.message}`); }
  });

  function populateLELabelFilter() {
    if (!leLabelFilter || !leTilesetData) return;
    leLabelFilter.innerHTML = '<option value="">All tiles</option>';
    (leTilesetData.labels || []).forEach(l => {
      const opt = document.createElement('option');
      opt.value = l; opt.textContent = l;
      leLabelFilter.appendChild(opt);
    });
  }

  if (leLabelFilter) {
    leLabelFilter.addEventListener('change', () => {
      leActiveLabelFilter = leLabelFilter.value;
      renderLEPalette();
    });
  }

  leNewGridBtn.addEventListener('click', () => {
    leGridW = Math.max(1, Math.min(256, parseInt(leWidthInput.value) || 16));
    leGridH = Math.max(1, Math.min(256, parseInt(leHeightInput.value) || 16));
    leWidthInput.value = leGridW; leHeightInput.value = leGridH;
    leGrid = [];
    for (let r = 0; r < leGridH; r++) leGrid.push(new Array(leGridW).fill(''));
    renderLECanvas();
    setStatus(`New grid: ${leGridW}x${leGridH}`);
  });

  function renderLEPalette() {
    lePalette.innerHTML = '';
    if (!leTilesetData || !leTilesetImage) return;
    const tiles = leTilesetData.tiles.filter(t => {
      if (!leActiveLabelFilter) return true;
      return (t.labels || []).includes(leActiveLabelFilter);
    });
    tiles.forEach(tile => {
      const div = document.createElement('div');
      div.className = 'palette-tile';
      div.title = tile.id + (tile.labels && tile.labels.length ? ' [' + tile.labels.join(', ') + ']' : '');
      const c = document.createElement('canvas');
      c.width = tile.source_rect.w; c.height = tile.source_rect.h;
      c.getContext('2d').drawImage(leTilesetImage,
        tile.source_rect.x, tile.source_rect.y, tile.source_rect.w, tile.source_rect.h,
        0, 0, tile.source_rect.w, tile.source_rect.h);
      div.appendChild(c);
      div.addEventListener('click', () => {
        document.querySelectorAll('.palette-tile').forEach(el => el.classList.remove('selected'));
        div.classList.add('selected');
        leSelectedTileId = tile.id;
        setStatus(`Selected: ${tile.id}`);
      });
      lePalette.appendChild(div);
    });
    if (tiles.length === 0) {
      lePalette.innerHTML = '<p style="color:#a0a0a0;font-size:11px">No tiles match filter</p>';
    }
  }

  function renderLECanvas() {
    if (leGrid.length === 0) {
      leGridW = Math.max(1, Math.min(256, parseInt(leWidthInput.value) || 16));
      leGridH = Math.max(1, Math.min(256, parseInt(leHeightInput.value) || 16));
      leGrid = [];
      for (let r = 0; r < leGridH; r++) leGrid.push(new Array(leGridW).fill(''));
    }
    levelCanvas.width = leGridW * leCellW;
    levelCanvas.height = leGridH * leCellH;
    lCtx.imageSmoothingEnabled = false;
    lCtx.clearRect(0, 0, levelCanvas.width, levelCanvas.height);
    for (let r = 0; r < leGridH; r++) {
      for (let c = 0; c < leGridW; c++) {
        const tileId = leGrid[r][c];
        const dx = c * leCellW, dy = r * leCellH;
        if (tileId && leTilesetData && leTilesetImage) {
          const td = leTilesetData.tiles.find(t => t.id === tileId);
          if (td) lCtx.drawImage(leTilesetImage, td.source_rect.x, td.source_rect.y, td.source_rect.w, td.source_rect.h, dx, dy, leCellW, leCellH);
          else { lCtx.fillStyle = '#ff00ff'; lCtx.fillRect(dx, dy, leCellW, leCellH); }
        }
      }
    }
    lCtx.strokeStyle = 'rgba(79,195,247,0.3)'; lCtx.lineWidth = 1;
    for (let x = 0; x <= levelCanvas.width; x += leCellW) { lCtx.beginPath(); lCtx.moveTo(x+0.5,0); lCtx.lineTo(x+0.5,levelCanvas.height); lCtx.stroke(); }
    for (let y = 0; y <= levelCanvas.height; y += leCellH) { lCtx.beginPath(); lCtx.moveTo(0,y+0.5); lCtx.lineTo(levelCanvas.width,y+0.5); lCtx.stroke(); }
  }

  // Paint / Erase
  let isDrawing = false;
  function getLEGridPos(e) {
    const rect = levelCanvas.getBoundingClientRect();
    const sx = levelCanvas.width / rect.width, sy = levelCanvas.height / rect.height;
    return { row: Math.floor((e.clientY-rect.top)*sy/leCellH), col: Math.floor((e.clientX-rect.left)*sx/leCellW) };
  }
  function paintTile(e) {
    if (leGrid.length === 0) return;
    const {row, col} = getLEGridPos(e);
    if (row < 0 || row >= leGridH || col < 0 || col >= leGridW) return;
    if (e.buttons === 1) {
      if (!leSelectedTileId) { setStatus('Select a tile first'); return; }
      if (leGrid[row][col] !== leSelectedTileId) { leGrid[row][col] = leSelectedTileId; renderLECanvas(); }
    } else if (e.buttons === 2) {
      if (leGrid[row][col] !== '') { leGrid[row][col] = ''; renderLECanvas(); }
    }
  }
  levelCanvas.addEventListener('mousedown', (e) => { isDrawing = true; paintTile(e); });
  levelCanvas.addEventListener('mousemove', (e) => { if (isDrawing) paintTile(e); });
  levelCanvas.addEventListener('mouseup', () => { isDrawing = false; });
  levelCanvas.addEventListener('mouseleave', () => { isDrawing = false; });
  levelCanvas.addEventListener('contextmenu', (e) => e.preventDefault());

  // Export
  leExportBtn.addEventListener('click', () => {
    if (leGrid.length === 0) { setStatus('No grid'); return; }
    if (!leTilesetName) { setStatus('No tileset loaded'); return; }
    const mapFile = { width: leGridW, height: leGridH, tileset: leTilesetName, grid: leGrid };
    const blob = new Blob([JSON.stringify(mapFile, null, 2)], {type:'application/json'});
    const a = document.createElement('a');
    a.href = URL.createObjectURL(blob);
    a.download = `map_${leGridW}x${leGridH}.json`;
    a.click(); URL.revokeObjectURL(a.href);
    setStatus(`Exported ${leGridW}x${leGridH}`);
  });

  // ============================================================
  // INIT
  // ============================================================
  loadTilesetList();
  leNewGridBtn.click();

})();
