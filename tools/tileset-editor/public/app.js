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

  // --- Blocker state ---
  let blockerRects = [];
  let blockerDrawMode = false;
  let blockerDisplay = 'outlines'; // 'off', 'outlines', 'overlay'
  let selectedBlockerIndex = -1;
  let blockerHistory = [[]]; // snapshot-based history: array of blocker rect arrays
  let blockerHistoryIndex = 0;
  let blockerDragStart = null; // {x, y} in source px
  let blockerDragCurrent = null; // {x, y} in source px

  // --- Tileset list ---
  async function loadTilesetList() {
    try {
      const res = await fetch('/api/tilesets');
      const tilesets = await res.json();
      tilesetList.innerHTML = '';
      tilesets.forEach(name => {
        const div = document.createElement('div');
        div.className = 'tileset-item';
        // Show folder icon + relative path
        const parts = name.split('/');
        const folderName = parts[parts.length - 1];
        const pathPrefix = parts.length > 1 ? parts.slice(0, -1).join('/') + '/' : '';
        div.innerHTML = `<span style="opacity:0.5;font-size:11px">${escapeHtml(pathPrefix)}</span>${escapeHtml(folderName)} <span style="opacity:0.4">📁</span>`;
        div.dataset.path = name;
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
      el.classList.toggle('selected', el.dataset.path === name);
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
      // Load blockers from JSON
      blockerRects = Array.isArray(currentTilesetData.blockers) ? currentTilesetData.blockers.slice() : [];
      selectedBlockerIndex = -1;
      blockerHistory = [blockerRects.map(r => ({...r}))];
      blockerHistoryIndex = 0;
      if (currentTilesetData.tiles.length > 0 && currentTilesetData.tiles[0].source_rect) {
        cellWidthInput.value = currentTilesetData.tiles[0].source_rect.w || 32;
        cellHeightInput.value = currentTilesetData.tiles[0].source_rect.h || 32;
      }
      renderTilesetCanvas();
      renderTileInfo();
      renderLabelsPanel();
      renderCreatedTilesList();
      renderBlockersList();
      setStatus(`Sheet '${filename}' loaded (${currentTilesetData.tiles.length} tiles, ${blockerRects.length} blockers)`);
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
    // Draw blocker rectangles (respecting display mode)
    if (blockerDisplay !== 'off' && blockerRects.length > 0) {
      blockerRects.forEach((r, i) => {
        const rx = r.x * zoom, ry = r.y * zoom, rw = r.w * zoom, rh = r.h * zoom;
        if (blockerDisplay === 'overlay') {
          // Overlay mode: semi-transparent red fill, NO outlines (except selected)
          ctx.fillStyle = 'rgba(255, 0, 0, 0.18)';
          ctx.fillRect(rx, ry, rw, rh);
          if (i === selectedBlockerIndex) {
            ctx.strokeStyle = '#ffeb3b'; ctx.lineWidth = 3;
            ctx.strokeRect(rx + 0.5, ry + 0.5, rw - 1, rh - 1);
          }
        } else {
          // Outlines mode: red outlines only
          if (i === selectedBlockerIndex) {
            ctx.strokeStyle = '#ffeb3b'; ctx.lineWidth = 3;
          } else {
            ctx.strokeStyle = '#ff0000'; ctx.lineWidth = 1.5;
          }
          ctx.strokeRect(rx + 0.5, ry + 0.5, rw - 1, rh - 1);
        }
      });
    }
    // Draw in-progress blocker drag rect
    if (blockerDragStart && blockerDragCurrent) {
      const x = Math.min(blockerDragStart.x, blockerDragCurrent.x) * zoom;
      const y = Math.min(blockerDragStart.y, blockerDragCurrent.y) * zoom;
      const w = Math.abs(blockerDragCurrent.x - blockerDragStart.x) * zoom;
      const h = Math.abs(blockerDragCurrent.y - blockerDragStart.y) * zoom;
      ctx.strokeStyle = '#ff6b6b'; ctx.lineWidth = 2;
      ctx.setLineDash([6, 3]);
      ctx.strokeRect(x + 0.5, y + 0.5, w - 1, h - 1);
      ctx.setLineDash([]);
    }
  }

  // --- Canvas click: select existing tile OR highlight grid cell for creation ---
  tilesetCanvas.addEventListener('click', (e) => {
    if (blockerDrawMode) return; // blocker mode handles its own mouse events
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

    // If no exact grid match, try to find any existing tile whose rect contains the click point
    if (foundIndex === -1) {
      for (let i = 0; i < currentTilesetData.tiles.length; i++) {
        const sr = currentTilesetData.tiles[i].source_rect;
        if (clickX >= sr.x && clickX < sr.x + sr.w && clickY >= sr.y && clickY < sr.y + sr.h) {
          foundIndex = i; break;
        }
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
  // Save with overwrite confirmation and feedback
  saveTilesetBtn.addEventListener('click', async () => {
    if (!currentTilesetName || !currentTilesetData || !currentSheetBase) { setStatus('No sheet loaded'); return; }
    // Check if file exists — ask for overwrite confirmation
    try {
      const existsRes = await fetch(`/api/tilesets/${currentTilesetName}/exists/${currentSheetBase}.json`);
      if (existsRes.ok) {
        const { exists } = await existsRes.json();
        if (exists && !confirm(`Overwrite ${currentSheetBase}.json?`)) {
          setStatus('Save cancelled'); return;
        }
      }
    } catch (e) { /* proceed anyway */ }
    try {
      const saveData = Object.assign({}, currentTilesetData, { blockers: blockerRects });
      const res = await fetch(`/api/tilesets/${currentTilesetName}/json/${currentSheetBase}`, {
        method: 'PUT', headers: {'Content-Type':'application/json'}, body: JSON.stringify(saveData)
      });
      if (res.ok) {
        const blockerNote = blockerRects.length > 0 ? ` (includes ${blockerRects.length} blockers)` : '';
        setStatus(`Saved '${currentSheetBase}.json'${blockerNote}`);
        // Export blocker bitmap if there are any blockers
        if (blockerRects.length > 0 && currentTilesetImage) {
          await exportBlockerBitmap();
        }
        alert(`Saved successfully!${blockerNote}`);
      } else {
        const msg = (await res.json().catch(() => ({}))).error || `HTTP ${res.status}`;
        setStatus(`Error: ${msg}`);
        alert(`Save failed: ${msg}`);
      }
    } catch (err) { setStatus(`Save error: ${err.message}`); alert(`Save error: ${err.message}`); }
  });

  // Generate and upload blocker bitmap (black=blocked, white=passable)
  async function exportBlockerBitmap() {
    if (!currentTilesetImage || !currentTilesetName || !currentSheetBase) return;
    const w = currentTilesetImage.naturalWidth;
    const h = currentTilesetImage.naturalHeight;
    const offscreen = document.createElement('canvas');
    offscreen.width = w;
    offscreen.height = h;
    const octx = offscreen.getContext('2d');
    // Fill white (passable)
    octx.fillStyle = '#ffffff';
    octx.fillRect(0, 0, w, h);
    // Fill black (blocked) for each blocker rect
    octx.fillStyle = '#000000';
    blockerRects.forEach(r => {
      octx.fillRect(r.x, r.y, r.w, r.h);
    });
    // Export as PNG data URL
    const dataUrl = offscreen.toDataURL('image/png');
    const base64 = dataUrl.replace(/^data:image\/png;base64,/, '');
    try {
      const res = await fetch(`/api/tilesets/${currentTilesetName}/blocker-bitmap/${currentSheetBase}`, {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ data: base64 })
      });
      if (res.ok) {
        setStatus(`Blocker bitmap saved: ${currentSheetBase}_blockers.png`);
      } else {
        const msg = (await res.json().catch(() => ({}))).error || `HTTP ${res.status}`;
        console.error('Blocker bitmap save failed:', msg);
      }
    } catch (err) {
      console.error('Blocker bitmap export error:', err.message);
    }
  }

  // Open in Explorer button
  const openExplorerBtn = document.getElementById('open-explorer-btn');
  if (openExplorerBtn) {
    openExplorerBtn.addEventListener('click', async () => {
      if (!currentTilesetName) { setStatus('No tileset selected'); return; }
      const file = currentSheetFilename || (currentSheetBase ? currentSheetBase + '.json' : '');
      try {
        await fetch(`/api/tilesets/${currentTilesetName}/open-explorer`, {
          method: 'POST', headers: {'Content-Type':'application/json'},
          body: JSON.stringify({ file })
        });
        setStatus('Opened Explorer');
      } catch (e) { setStatus('Failed to open Explorer'); }
    });
  }

  // Import TMX button
  const importTmxBtn = document.getElementById('import-tmx-btn');
  if (importTmxBtn) {
    importTmxBtn.addEventListener('click', async () => {
      if (!currentTilesetName) { setStatus('Select a tileset folder first'); return; }
      try {
        // List TMX files in this tileset folder
        const res = await fetch(`/api/tilesets/${currentTilesetName}/tmx-files`);
        const tmxFiles = await res.json();
        if (!tmxFiles || tmxFiles.length === 0) {
          alert('No .tmx files found in this tileset folder.');
          return;
        }
        // Let user pick which TMX to import
        let tmxFile = tmxFiles[0];
        if (tmxFiles.length > 1) {
          tmxFile = prompt(`Multiple TMX files found. Pick one:\n${tmxFiles.map((f,i) => `${i+1}. ${f}`).join('\n')}\n\nEnter number:`, '1');
          if (!tmxFile) return;
          const idx = parseInt(tmxFile) - 1;
          if (idx >= 0 && idx < tmxFiles.length) tmxFile = tmxFiles[idx];
          else { alert('Invalid selection'); return; }
        }
        // Parse the TMX
        const parseRes = await fetch(`/api/tilesets/${currentTilesetName}/parse-tmx/${tmxFile}`);
        const parsed = await parseRes.json();
        if (!parsed.tilesets || parsed.tilesets.length === 0) {
          alert('TMX file contains no tileset definitions.');
          return;
        }
        // Find the tileset entry matching the current sheet PNG
        let targetTs = null;
        if (currentSheetFilename) {
          targetTs = parsed.tilesets.find(ts => ts.image === currentSheetFilename);
        }
        if (!targetTs) {
          // Let user pick which tileset from the TMX
          const choices = parsed.tilesets.map((ts, i) => `${i+1}. ${ts.name} (${ts.image}, ${ts.tilewidth}x${ts.tileheight}, ${ts.tilecount} tiles)`).join('\n');
          const pick = prompt(`Which tileset to import for current sheet?\n${choices}\n\nEnter number:`, '1');
          if (!pick) return;
          const idx = parseInt(pick) - 1;
          if (idx >= 0 && idx < parsed.tilesets.length) targetTs = parsed.tilesets[idx];
          else { alert('Invalid selection'); return; }
        }
        // Generate tile definitions from the TMX tileset partition
        const tw = targetTs.tilewidth;
        const th = targetTs.tileheight;
        const cols = targetTs.columns;
        const count = targetTs.tilecount;
        const newTiles = [];
        for (let i = 0; i < count; i++) {
          const col = i % cols;
          const row = Math.floor(i / cols);
          const x = col * tw;
          const y = row * th;
          newTiles.push({
            id: `${targetTs.name}_${i}`,
            source_rect: { x, y, w: tw, h: th },
            adjacency: { up: [], down: [], left: [], right: [] },
            labels: []
          });
        }
        // Confirm import
        if (!confirm(`Import ${newTiles.length} tiles from "${targetTs.name}" (${tw}x${th} grid, ${cols} columns)?\n\nThis will REPLACE all current tiles for this sheet.`)) {
          return;
        }
        // Apply to current tileset data
        if (!currentTilesetData) currentTilesetData = { tiles: [], labels: [] };
        currentTilesetData.tiles = newTiles;
        selectedTileIndex = -1;
        highlightedCell = null;
        // Update grid controls to match
        cellWidthInput.value = tw;
        cellHeightInput.value = th;
        renderTilesetCanvas();
        renderTileInfo();
        renderCreatedTilesList();
        setStatus(`Imported ${newTiles.length} tiles from TMX "${targetTs.name}" — Save to persist.`);
      } catch (err) {
        setStatus(`TMX import error: ${err.message}`);
        alert(`TMX import failed: ${err.message}`);
      }
    });
  }

  // Re-render on control changes
  [cellWidthInput, cellHeightInput, offsetXInput, offsetYInput, zoomInput].forEach(input => {
    if (input) input.addEventListener('input', renderTilesetCanvas);
  });

  // Step buttons (▲▼ ±1, ▲▲▼▼ ±8, ▲▲▲▼▼▼ ±16)
  document.querySelectorAll('.step-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      const target = document.getElementById(btn.dataset.target);
      if (!target) return;
      const step = parseInt(btn.dataset.step) || 0;
      const min = parseInt(target.min) || 0;
      const max = target.max ? parseInt(target.max) : 9999;
      let val = parseInt(target.value) || 0;
      val = Math.max(min, Math.min(max, val + step));
      target.value = val;
      target.dispatchEvent(new Event('input'));
    });
  });

  // ============================================================
  // BLOCKER DRAWING & MANAGEMENT
  // ============================================================

  const drawBlockerBtn = document.getElementById('draw-blocker-btn');
  const blockerDisplaySelect = document.getElementById('blocker-display-mode');

  // Toggle draw blocker mode
  drawBlockerBtn.addEventListener('click', () => {
    blockerDrawMode = !blockerDrawMode;
    if (blockerDrawMode) {
      drawBlockerBtn.style.background = '#ff6b6b';
      drawBlockerBtn.style.color = '#1a1a2e';
      drawBlockerBtn.textContent = '■ Blocker ON';
      tilesetCanvas.style.cursor = 'crosshair';
    } else {
      drawBlockerBtn.style.background = '#1a1a2e';
      drawBlockerBtn.style.color = '#ff6b6b';
      drawBlockerBtn.textContent = 'Draw Blocker';
      tilesetCanvas.style.cursor = 'crosshair';
    }
  });

  // Blocker display mode
  blockerDisplaySelect.addEventListener('change', () => {
    blockerDisplay = blockerDisplaySelect.value;
    renderTilesetCanvas();
  });

  // Blocker mouse events on tileset canvas
  tilesetCanvas.addEventListener('mousedown', (e) => {
    if (!blockerDrawMode || !currentTilesetImage) return;
    e.preventDefault();
    const zoom = Math.max(1, parseInt(zoomInput.value) || 1);
    const rect = tilesetCanvas.getBoundingClientRect();
    const scaleX = tilesetCanvas.width / rect.width;
    const scaleY = tilesetCanvas.height / rect.height;
    const px = Math.round((e.clientX - rect.left) * scaleX / zoom);
    const py = Math.round((e.clientY - rect.top) * scaleY / zoom);
    blockerDragStart = { x: px, y: py };
    blockerDragCurrent = { x: px, y: py };
  });

  tilesetCanvas.addEventListener('mousemove', (e) => {
    if (!blockerDrawMode || !blockerDragStart) return;
    const zoom = Math.max(1, parseInt(zoomInput.value) || 1);
    const rect = tilesetCanvas.getBoundingClientRect();
    const scaleX = tilesetCanvas.width / rect.width;
    const scaleY = tilesetCanvas.height / rect.height;
    const px = Math.round((e.clientX - rect.left) * scaleX / zoom);
    const py = Math.round((e.clientY - rect.top) * scaleY / zoom);
    blockerDragCurrent = { x: px, y: py };
    renderTilesetCanvas();
  });

  tilesetCanvas.addEventListener('mouseup', (e) => {
    if (!blockerDrawMode || !blockerDragStart) return;
    const zoom = Math.max(1, parseInt(zoomInput.value) || 1);
    const rect = tilesetCanvas.getBoundingClientRect();
    const scaleX = tilesetCanvas.width / rect.width;
    const scaleY = tilesetCanvas.height / rect.height;
    const px = Math.round((e.clientX - rect.left) * scaleX / zoom);
    const py = Math.round((e.clientY - rect.top) * scaleY / zoom);

    const x = Math.min(blockerDragStart.x, px);
    const y = Math.min(blockerDragStart.y, py);
    const w = Math.abs(px - blockerDragStart.x);
    const h = Math.abs(py - blockerDragStart.y);

    blockerDragStart = null;
    blockerDragCurrent = null;

    if (w < 2 || h < 2) { renderTilesetCanvas(); return; } // too small, ignore

    const newRect = { x: x, y: y, w: w, h: h };
    blockerRects.push(newRect);
    pushBlockerHistory();
    selectedBlockerIndex = blockerRects.length - 1;
    setStatus(`Blocker added: (${x},${y}) ${w}×${h}`);
    renderTilesetCanvas();
    renderBlockersList();
  });

  // Helper: push current blocker state to history (call after any modification)
  function pushBlockerHistory() {
    // Trim any redo states beyond current index
    blockerHistory = blockerHistory.slice(0, blockerHistoryIndex + 1);
    blockerHistory.push(blockerRects.map(r => ({...r})));
    blockerHistoryIndex = blockerHistory.length - 1;
  }

  // Undo/Redo for blockers (Ctrl+Z / Ctrl+Y)
  document.addEventListener('keydown', (e) => {
    // Only handle when configurator tab is active
    const confTab = document.getElementById('tab-configurator');
    if (!confTab || !confTab.classList.contains('active')) return;

    if (e.ctrlKey && e.key === 'z') {
      e.preventDefault();
      if (blockerHistoryIndex <= 0) return;
      blockerHistoryIndex--;
      blockerRects = blockerHistory[blockerHistoryIndex].map(r => ({...r}));
      selectedBlockerIndex = -1;
      renderTilesetCanvas();
      renderBlockersList();
      setStatus('Blocker undo');
    } else if (e.ctrlKey && e.key === 'y') {
      e.preventDefault();
      if (blockerHistoryIndex >= blockerHistory.length - 1) return;
      blockerHistoryIndex++;
      blockerRects = blockerHistory[blockerHistoryIndex].map(r => ({...r}));
      selectedBlockerIndex = -1;
      renderTilesetCanvas();
      renderBlockersList();
      setStatus('Blocker redo');
    }
  });

  // Render blockers sidebar list
  function renderBlockersList() {
    const panel = document.getElementById('blockers-list-panel');
    if (!panel) return;
    if (blockerRects.length === 0) {
      panel.innerHTML = '<p style="color:#a0a0a0;font-size:11px;">Use "Draw Blocker" mode to add blocking rectangles.</p>';
      return;
    }
    let html = '';
    blockerRects.forEach((r, i) => {
      const active = (i === selectedBlockerIndex) ? ' active' : '';
      html += `<div class="blocker-item${active}" data-idx="${i}">
        <span>(${r.x},${r.y}) ${r.w}×${r.h}</span>
        <span class="del-blocker" data-idx="${i}">&times;</span>
      </div>`;
    });
    panel.innerHTML = html;

    // Click to select
    panel.querySelectorAll('.blocker-item').forEach(el => {
      el.addEventListener('click', (e) => {
        if (e.target.classList.contains('del-blocker')) return;
        selectedBlockerIndex = parseInt(el.dataset.idx);
        renderTilesetCanvas();
        renderBlockersList();
      });
    });
    // Delete button
    panel.querySelectorAll('.del-blocker').forEach(btn => {
      btn.addEventListener('click', (e) => {
        e.stopPropagation();
        const idx = parseInt(btn.dataset.idx);
        const removed = blockerRects.splice(idx, 1)[0];
        pushBlockerHistory();
        if (selectedBlockerIndex === idx) selectedBlockerIndex = -1;
        else if (selectedBlockerIndex > idx) selectedBlockerIndex--;
        renderTilesetCanvas();
        renderBlockersList();
        setStatus(`Blocker removed: (${removed.x},${removed.y}) ${removed.w}×${removed.h}`);
      });
    });
  }

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
