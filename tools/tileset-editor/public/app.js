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

  // Helper to enable/disable sidebar buttons visually
  function enableBtn(id, enabled) {
    const btn = document.getElementById(id);
    if (!btn) return;
    btn.disabled = !enabled;
    if (enabled) {
      if (id === 'open-explorer-btn') {
        btn.style.cssText = 'width:100%;margin-top:6px;padding:6px;background:#0f3460;color:#e0e0e0;border:1px solid #4fc3f7;border-radius:3px;cursor:pointer;font-size:11px;';
      } else if (id === 'import-tmx-btn') {
        btn.style.cssText = 'width:100%;margin-top:6px;padding:6px;background:#0f3460;color:#ff9800;border:1px solid #ff9800;border-radius:3px;cursor:pointer;font-size:11px;';
      }
    } else {
      btn.style.cssText = 'width:100%;margin-top:6px;padding:6px;background:#0a0a1a;color:#555;border:1px solid #333;border-radius:3px;cursor:not-allowed;font-size:11px;';
    }
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
        div.innerHTML = `<span style="opacity:0.5;font-size:11px">${escapeHtml(pathPrefix)}</span>${escapeHtml(folderName)} 📁`;
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
    // Enable "Open in Explorer" now that a folder is picked
    enableBtn('open-explorer-btn', true);
    enableBtn('import-tmx-btn', false); // need a sheet first
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
      if (typeof renderBlockersList === 'function') renderBlockersList();
      // Enable import button now that a sheet is loaded
      enableBtn('import-tmx-btn', true);
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

  // Import TMX button — shows in-app picker defaulting to TMX files in current folder
  const importTmxBtn = document.getElementById('import-tmx-btn');
  if (importTmxBtn) {
    importTmxBtn.addEventListener('click', async () => {
      if (!currentTilesetName || !currentSheetFilename) return;
      try {
        // Fetch TMX files in the current tileset folder
        const res = await fetch(`/api/tilesets/${currentTilesetName}/tmx-files`);
        const tmxFiles = await res.json();

        // Show picker: list folder TMX files + option to browse elsewhere
        const picked = await showTmxFilePicker(tmxFiles);
        if (!picked) return;

        let xmlText;
        if (picked.source === 'server') {
          // Load from server
          const tmxRes = await fetch(`/api/tilesets/${currentTilesetName}/tmx-raw/${picked.filename}`);
          if (!tmxRes.ok) { alert('Failed to load TMX file from server'); return; }
          xmlText = await tmxRes.text();
        } else {
          // Local file from native picker
          xmlText = await picked.file.text();
        }

        // Parse and import
        const tilesets = parseTmxTilesets(xmlText);
        if (tilesets.length === 0) { alert('No tileset definitions found in this TMX.'); return; }
        let targetTs = tilesets.find(ts => ts.image === currentSheetFilename);
        if (!targetTs) {
          alert(`No tileset references "${currentSheetFilename}" in this TMX.\n\nFound: ${tilesets.map(ts => ts.image).join(', ')}`);
          return;
        }
        const tw = targetTs.tilewidth, th = targetTs.tileheight;
        const cols = targetTs.columns, count = targetTs.tilecount;
        // Build a map of tileid -> animation frames from parsed animations
        const animMap = {};
        if (targetTs.animations) {
          targetTs.animations.forEach(anim => { animMap[anim.tileid] = anim.frames; });
        }
        const newTiles = [];
        for (let i = 0; i < count; i++) {
          const tile = {
            id: `${targetTs.name}_${i}`,
            source_rect: { x: (i % cols) * tw, y: Math.floor(i / cols) * th, w: tw, h: th },
            adjacency: { up: [], down: [], left: [], right: [] }, labels: []
          };
          // Attach animation data if this tile has an animation defined
          if (animMap[i]) {
            tile.animation = animMap[i].map(f => ({
              source_rect: f.source_rect,
              duration_ms: f.duration_ms
            }));
          }
          newTiles.push(tile);
        }
        if (!confirm(`Import ${newTiles.length} tiles from "${targetTs.name}" (${tw}x${th}, ${cols} cols)?\nThis replaces current tiles.`)) return;
        if (!currentTilesetData) currentTilesetData = { tiles: [], labels: [] };
        currentTilesetData.tiles = newTiles;
        selectedTileIndex = -1; highlightedCell = null;
        cellWidthInput.value = tw; cellHeightInput.value = th;
        renderTilesetCanvas(); renderTileInfo(); renderCreatedTilesList();
        if (typeof renderBlockersList === 'function') renderBlockersList();
        setStatus(`Imported ${newTiles.length} tiles from "${targetTs.name}" — Save to persist.`);
      } catch (err) { alert(`TMX import failed: ${err.message}`); }
    });
  }

  // In-app TMX file picker: shows folder files with first preselected, plus "Browse..." option
  function showTmxFilePicker(folderFiles) {
    return new Promise((resolve) => {
      const overlay = document.createElement('div');
      overlay.style.cssText = 'position:fixed;top:0;left:0;width:100%;height:100%;background:rgba(0,0,0,0.7);display:flex;align-items:center;justify-content:center;z-index:9999';
      const box = document.createElement('div');
      box.style.cssText = 'background:#16213e;border:2px solid #ff9800;border-radius:8px;padding:20px;min-width:320px;max-width:500px';
      box.innerHTML = '<h3 style="color:#ff9800;margin-bottom:12px">Import TMX File</h3>';
      if (folderFiles.length > 0) {
        const hint = document.createElement('p');
        hint.style.cssText = 'color:#a0a0a0;font-size:11px;margin-bottom:10px';
        hint.textContent = 'TMX files found in tileset folder:';
        box.appendChild(hint);
        folderFiles.forEach((f, i) => {
          const btn = document.createElement('button');
          btn.textContent = f;
          btn.style.cssText = 'display:block;width:100%;padding:10px;margin-bottom:6px;background:' + (i === 0 ? '#ff9800;color:#1a1a2e' : '#0f3460;color:#e0e0e0') + ';border:1px solid #ff9800;border-radius:4px;cursor:pointer;font-size:13px;text-align:left;font-weight:' + (i === 0 ? '700' : '400');
          btn.addEventListener('click', () => { document.body.removeChild(overlay); resolve({ source: 'server', filename: f }); });
          box.appendChild(btn);
        });
        const sep = document.createElement('hr');
        sep.style.cssText = 'border-color:#0f3460;margin:12px 0';
        box.appendChild(sep);
      } else {
        const hint = document.createElement('p');
        hint.style.cssText = 'color:#a0a0a0;font-size:11px;margin-bottom:10px';
        hint.textContent = 'No TMX files in tileset folder. Browse to select one:';
        box.appendChild(hint);
      }
      // Browse button for native file picker
      const browseBtn = document.createElement('button');
      browseBtn.textContent = '📂 Browse for .tmx file...';
      browseBtn.style.cssText = 'display:block;width:100%;padding:10px;margin-bottom:6px;background:#1a1a2e;color:#4fc3f7;border:1px solid #4fc3f7;border-radius:4px;cursor:pointer;font-size:12px';
      browseBtn.addEventListener('click', () => {
        document.body.removeChild(overlay);
        const fileInput = document.createElement('input');
        fileInput.type = 'file'; fileInput.accept = '.tmx'; fileInput.style.display = 'none';
        document.body.appendChild(fileInput);
        fileInput.addEventListener('change', () => {
          const file = fileInput.files[0];
          document.body.removeChild(fileInput);
          if (file) resolve({ source: 'local', file });
          else resolve(null);
        });
        fileInput.click();
      });
      box.appendChild(browseBtn);
      // Cancel
      const cancelBtn = document.createElement('button');
      cancelBtn.textContent = 'Cancel';
      cancelBtn.style.cssText = 'display:block;width:100%;padding:8px;margin-top:8px;background:#1a1a2e;color:#a0a0a0;border:1px solid #0f3460;border-radius:4px;cursor:pointer;font-size:12px';
      cancelBtn.addEventListener('click', () => { document.body.removeChild(overlay); resolve(null); });
      box.appendChild(cancelBtn);
      overlay.appendChild(box);
      document.body.appendChild(overlay);
    });
  }

  // Parse TMX XML string and extract tileset partition info (client-side)
  function parseTmxTilesets(xml) {
    const results = [];
    const blocks = xml.split(/<tileset\s+/);
    blocks.shift(); // remove content before first tileset
    for (const block of blocks) {
      const attrStr = block.substring(0, block.indexOf('>'));
      const getAttr = (name) => { const m = attrStr.match(new RegExp(name + '="([^"]+)"')); return m ? m[1] : ''; };
      const imgMatch = block.match(/<image\s+([^>]+)\/?>/) ;
      let imgSource = '', imgW = 0, imgH = 0;
      if (imgMatch) {
        const ia = imgMatch[1];
        const getIA = (n) => { const m2 = ia.match(new RegExp(n + '="([^"]+)"')); return m2 ? m2[1] : ''; };
        imgSource = getIA('source');
        imgW = parseInt(getIA('width')) || 0;
        imgH = parseInt(getIA('height')) || 0;
      }

      const tilewidth = parseInt(getAttr('tilewidth')) || 16;
      const tileheight = parseInt(getAttr('tileheight')) || 16;
      const columns = parseInt(getAttr('columns')) || 1;

      // Extract <animation> blocks from <tile> elements
      const animations = [];
      const tileBlockRegex = /<tile\s+id="(\d+)"[^>]*>([\s\S]*?)<\/tile>/g;
      let tileMatch;
      while ((tileMatch = tileBlockRegex.exec(block)) !== null) {
        const tileId = parseInt(tileMatch[1]);
        const tileContent = tileMatch[2];
        const animMatch = tileContent.match(/<animation>([\s\S]*?)<\/animation>/);
        if (animMatch) {
          const animContent = animMatch[1];
          const frames = [];
          const frameRegex = /<frame\s+tileid="(\d+)"\s+duration="(\d+)"\s*\/?>/g;
          let frameMatch;
          while ((frameMatch = frameRegex.exec(animContent)) !== null) {
            const frameTileId = parseInt(frameMatch[1]);
            const duration = parseInt(frameMatch[2]);
            const col = frameTileId % columns;
            const row = Math.floor(frameTileId / columns);
            frames.push({
              tileid: frameTileId,
              duration_ms: duration,
              source_rect: { x: col * tilewidth, y: row * tileheight, w: tilewidth, h: tileheight }
            });
          }
          if (frames.length > 0) {
            animations.push({ tileid: tileId, frames });
          }
        }
      }

      results.push({
        name: getAttr('name'),
        tilewidth,
        tileheight,
        tilecount: parseInt(getAttr('tilecount')) || 0,
        columns,
        image: imgSource,
        imagewidth: imgW,
        imageheight: imgH,
        animations
      });
    }
    return results;
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
      if (leJigsawMode) renderLEPaletteJigsaw();
      else renderLEPalette();
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

  // ============================================================
  // JIGSAW MODE — snap-to-edge variable-size tile placement
  // ============================================================

  let leJigsawMode = false;
  let leJigsawTiles = []; // Array of {tile_id, x, y, w, h}

  const leJigsawToggle = document.getElementById('le-jigsaw-toggle');

  leJigsawToggle.addEventListener('click', () => {
    leJigsawMode = !leJigsawMode;
    leJigsawToggle.textContent = leJigsawMode ? 'Jigsaw Mode: On' : 'Jigsaw Mode: Off';
    leJigsawToggle.style.background = leJigsawMode ? '#ff9800' : '';
    leJigsawToggle.style.color = leJigsawMode ? '#1a1a2e' : '';
    if (leJigsawMode) {
      renderLEPaletteJigsaw();
      renderLECanvasJigsaw();
    } else {
      renderLEPalette();
      renderLECanvas();
    }
  });

  function findSnapPosition(candidate, mouseX, mouseY, existingTiles, threshold) {
    // candidate = {w, h}
    // Returns {x, y, valid}
    if (existingTiles.length === 0) {
      return { x: 0, y: 0, valid: true };
    }
    let bestX = mouseX, bestY = mouseY;
    let bestDist = Infinity;
    let found = false;

    for (const t of existingTiles) {
      // Snap to right edge of existing tile
      const snapRightX = t.x + t.w;
      // Align top edges
      const snapRightY = t.y;
      const dRightTop = Math.hypot(snapRightX - mouseX, snapRightY - mouseY);
      if (dRightTop < threshold && dRightTop < bestDist) {
        bestX = snapRightX; bestY = snapRightY; bestDist = dRightTop; found = true;
      }
      // Align bottom of existing with top of new
      const snapRightYB = t.y + t.h - candidate.h;
      const dRightBot = Math.hypot(snapRightX - mouseX, snapRightYB - mouseY);
      if (dRightBot < threshold && dRightBot < bestDist) {
        bestX = snapRightX; bestY = snapRightYB; bestDist = dRightBot; found = true;
      }

      // Snap to left edge of existing tile
      const snapLeftX = t.x - candidate.w;
      const dLeftTop = Math.hypot(snapLeftX - mouseX, t.y - mouseY);
      if (dLeftTop < threshold && dLeftTop < bestDist) {
        bestX = snapLeftX; bestY = t.y; bestDist = dLeftTop; found = true;
      }

      // Snap to bottom edge of existing tile
      const snapBotY = t.y + t.h;
      const dBotLeft = Math.hypot(t.x - mouseX, snapBotY - mouseY);
      if (dBotLeft < threshold && dBotLeft < bestDist) {
        bestX = t.x; bestY = snapBotY; bestDist = dBotLeft; found = true;
      }
      // Align right of existing with right of new
      const snapBotXR = t.x + t.w - candidate.w;
      const dBotRight = Math.hypot(snapBotXR - mouseX, snapBotY - mouseY);
      if (dBotRight < threshold && dBotRight < bestDist) {
        bestX = snapBotXR; bestY = snapBotY; bestDist = dBotRight; found = true;
      }

      // Snap to top edge of existing tile
      const snapTopY = t.y - candidate.h;
      const dTopLeft = Math.hypot(t.x - mouseX, snapTopY - mouseY);
      if (dTopLeft < threshold && dTopLeft < bestDist) {
        bestX = t.x; bestY = snapTopY; bestDist = dTopLeft; found = true;
      }
    }

    if (!found) {
      // Fall back to mouse position (no snap)
      return { x: mouseX, y: mouseY, valid: false };
    }
    return { x: bestX, y: bestY, valid: true };
  }

  function jigsawWouldOverlap(candidate, existingTiles) {
    // candidate = {x, y, w, h}
    for (const t of existingTiles) {
      const overlapX = Math.max(0, Math.min(candidate.x + candidate.w, t.x + t.w) - Math.max(candidate.x, t.x));
      const overlapY = Math.max(0, Math.min(candidate.y + candidate.h, t.y + t.h) - Math.max(candidate.y, t.y));
      if (overlapX > 0 && overlapY > 0) return true;
    }
    return false;
  }

  function renderLEPaletteJigsaw() {
    lePalette.innerHTML = '';
    if (!leTilesetData || !leTilesetImage) return;
    const tiles = leTilesetData.tiles.filter(t => {
      if (!leActiveLabelFilter) return true;
      return (t.labels || []).includes(leActiveLabelFilter);
    });
    tiles.forEach(tile => {
      const div = document.createElement('div');
      div.className = 'palette-tile';
      div.title = tile.id;
      const c = document.createElement('canvas');
      // Show at proportional size (capped for display)
      const maxDim = 64;
      const scale = Math.min(1, maxDim / Math.max(tile.source_rect.w, tile.source_rect.h));
      c.width = Math.round(tile.source_rect.w * scale);
      c.height = Math.round(tile.source_rect.h * scale);
      c.style.width = c.width + 'px';
      c.style.height = c.height + 'px';
      c.getContext('2d').drawImage(leTilesetImage,
        tile.source_rect.x, tile.source_rect.y, tile.source_rect.w, tile.source_rect.h,
        0, 0, c.width, c.height);
      div.appendChild(c);
      if (leSelectedTileId === tile.id) div.classList.add('selected');
      div.addEventListener('click', () => {
        document.querySelectorAll('.palette-tile').forEach(el => el.classList.remove('selected'));
        div.classList.add('selected');
        leSelectedTileId = tile.id;
        setStatus(`Selected: ${tile.id} (${tile.source_rect.w}x${tile.source_rect.h})`);
      });
      lePalette.appendChild(div);
    });
    if (tiles.length === 0) {
      lePalette.innerHTML = '<p style="color:#a0a0a0;font-size:11px">No tiles match filter</p>';
    }
  }

  function renderLECanvasJigsaw() {
    // Compute canvas size from placed tiles bounding box
    let maxX = 320, maxY = 240;
    for (const t of leJigsawTiles) {
      maxX = Math.max(maxX, t.x + t.w + 32);
      maxY = Math.max(maxY, t.y + t.h + 32);
    }
    levelCanvas.width = maxX;
    levelCanvas.height = maxY;
    lCtx.imageSmoothingEnabled = false;
    lCtx.clearRect(0, 0, levelCanvas.width, levelCanvas.height);

    // Draw background grid hint
    lCtx.strokeStyle = 'rgba(79,195,247,0.15)'; lCtx.lineWidth = 1;
    for (let x = 0; x <= levelCanvas.width; x += 32) { lCtx.beginPath(); lCtx.moveTo(x+0.5,0); lCtx.lineTo(x+0.5,levelCanvas.height); lCtx.stroke(); }
    for (let y = 0; y <= levelCanvas.height; y += 32) { lCtx.beginPath(); lCtx.moveTo(0,y+0.5); lCtx.lineTo(levelCanvas.width,y+0.5); lCtx.stroke(); }

    // Draw placed jigsaw tiles
    for (const pt of leJigsawTiles) {
      if (leTilesetData && leTilesetImage) {
        const td = leTilesetData.tiles.find(t => t.id === pt.tile_id);
        if (td) {
          lCtx.drawImage(leTilesetImage,
            td.source_rect.x, td.source_rect.y, td.source_rect.w, td.source_rect.h,
            pt.x, pt.y, pt.w, pt.h);
        } else {
          lCtx.fillStyle = '#ff00ff';
          lCtx.fillRect(pt.x, pt.y, pt.w, pt.h);
        }
      }
      // Outline
      lCtx.strokeStyle = 'rgba(79,195,247,0.6)'; lCtx.lineWidth = 1;
      lCtx.strokeRect(pt.x + 0.5, pt.y + 0.5, pt.w - 1, pt.h - 1);
    }
  }

  // Jigsaw mode click handler
  levelCanvas.addEventListener('click', (e) => {
    if (!leJigsawMode) return;
    if (e.button !== 0) return;
    if (!leSelectedTileId || !leTilesetData) { setStatus('Select a tile first'); return; }

    const td = leTilesetData.tiles.find(t => t.id === leSelectedTileId);
    if (!td) return;
    const candW = td.source_rect.w, candH = td.source_rect.h;

    const rect = levelCanvas.getBoundingClientRect();
    const sx = levelCanvas.width / rect.width, sy = levelCanvas.height / rect.height;
    const mouseX = (e.clientX - rect.left) * sx;
    const mouseY = (e.clientY - rect.top) * sy;

    const snap = findSnapPosition({ w: candW, h: candH }, mouseX, mouseY, leJigsawTiles, 16);
    const placement = { tile_id: leSelectedTileId, x: snap.x, y: snap.y, w: candW, h: candH };

    if (jigsawWouldOverlap(placement, leJigsawTiles)) {
      setStatus('Cannot place: would overlap existing tile');
      return;
    }

    leJigsawTiles.push(placement);
    renderLECanvasJigsaw();
    setStatus(`Placed ${leSelectedTileId} at (${snap.x},${snap.y})`);
  });

  // Jigsaw mode right-click to remove
  levelCanvas.addEventListener('contextmenu', (e) => {
    if (!leJigsawMode) return;
    e.preventDefault();
    const rect = levelCanvas.getBoundingClientRect();
    const sx = levelCanvas.width / rect.width, sy = levelCanvas.height / rect.height;
    const mx = (e.clientX - rect.left) * sx;
    const my = (e.clientY - rect.top) * sy;

    // Find tile at cursor (last placed on top)
    for (let i = leJigsawTiles.length - 1; i >= 0; i--) {
      const t = leJigsawTiles[i];
      if (mx >= t.x && mx < t.x + t.w && my >= t.y && my < t.y + t.h) {
        leJigsawTiles.splice(i, 1);
        renderLECanvasJigsaw();
        setStatus('Removed tile');
        return;
      }
    }
  });

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
  levelCanvas.addEventListener('mousedown', (e) => { if (leJigsawMode) return; isDrawing = true; paintTile(e); });
  levelCanvas.addEventListener('mousemove', (e) => { if (leJigsawMode) return; if (isDrawing) paintTile(e); });
  levelCanvas.addEventListener('mouseup', () => { if (leJigsawMode) return; isDrawing = false; });
  levelCanvas.addEventListener('mouseleave', () => { if (leJigsawMode) return; isDrawing = false; });
  levelCanvas.addEventListener('contextmenu', (e) => e.preventDefault());

  // Export
  leExportBtn.addEventListener('click', () => {
    if (!leTilesetName) { setStatus('No tileset loaded'); return; }

    if (leJigsawMode) {
      // Jigsaw export format
      if (leJigsawTiles.length === 0) { setStatus('No jigsaw tiles placed'); return; }
      const mapFile = {
        format: 'jigsaw',
        tileset_id: leTilesetName,
        tiles: leJigsawTiles.map(t => ({ tile_id: t.tile_id, x: t.x, y: t.y, w: t.w, h: t.h }))
      };
      const blob = new Blob([JSON.stringify(mapFile, null, 2)], { type: 'application/json' });
      const a = document.createElement('a');
      a.href = URL.createObjectURL(blob);
      a.download = `jigsaw_map.json`;
      a.click(); URL.revokeObjectURL(a.href);
      setStatus(`Exported jigsaw map (${leJigsawTiles.length} tiles)`);
      return;
    }

    // Grid export (original)
    if (leGrid.length === 0) { setStatus('No grid'); return; }
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
