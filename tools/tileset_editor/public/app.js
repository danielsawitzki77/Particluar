// ============================================================
// Tile-O-Matic — app.js
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
    if (!confirm('Shut down the Tile-O-Matic server?')) return;
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
      } else if (id === 'auto-neighbor-btn-global') {
        btn.style.cssText = 'padding:5px 10px;background:#4caf50;color:#fff;border:none;border-radius:3px;cursor:pointer;font-size:11px;font-weight:600;';
      }
    } else {
      if (id === 'auto-neighbor-btn-global') {
        btn.style.cssText = 'padding:5px 10px;background:#0a0a1a;color:#555;border:1px solid #333;border-radius:3px;cursor:not-allowed;font-size:11px;font-weight:600;';
      } else {
        btn.style.cssText = 'width:100%;margin-top:6px;padding:6px;background:#0a0a1a;color:#555;border:1px solid #333;border-radius:3px;cursor:not-allowed;font-size:11px;';
      }
    }
  }

  // Update Auto-Neighbor button enabled state based on current tileset
  function updateAutoNeighborBtnState() {
    const hasEnoughTiles = currentTilesetData && currentTilesetImage && currentTilesetData.tiles && currentTilesetData.tiles.length >= 2;
    enableBtn('auto-neighbor-btn-global', !!hasEnoughTiles);
    const thresholdInput = document.getElementById('auto-neighbor-threshold');
    if (thresholdInput) {
      thresholdInput.disabled = !hasEnoughTiles;
      thresholdInput.style.opacity = hasEnoughTiles ? '1' : '0.4';
    }
    const expandedCheckbox = document.getElementById('auto-neighbor-expanded');
    if (expandedCheckbox) {
      expandedCheckbox.disabled = !hasEnoughTiles;
      expandedCheckbox.style.opacity = hasEnoughTiles ? '1' : '0.4';
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

  // --- Pixel comparison helpers (for Remove Duplicates) ---
  // Extract raw pixel data (Uint8ClampedArray) for a given source_rect from the current tileset image
  function getTilePixelData(srcRect) {
    if (!currentTilesetImage) return null;
    const offscreen = document.createElement('canvas');
    offscreen.width = srcRect.w;
    offscreen.height = srcRect.h;
    const octx = offscreen.getContext('2d');
    octx.drawImage(currentTilesetImage, srcRect.x, srcRect.y, srcRect.w, srcRect.h, 0, 0, srcRect.w, srcRect.h);
    return octx.getImageData(0, 0, srcRect.w, srcRect.h).data;
  }

  // Compare two Uint8ClampedArray pixel buffers for exact equality
  function pixelDataEqual(a, b) {
    if (a.length !== b.length) return false;
    for (let i = 0; i < a.length; i++) {
      if (a[i] !== b[i]) return false;
    }
    return true;
  }

  // --- Edge pixel helpers (for Auto-Neighbor) ---
  // Extract a single pixel column (x offset within srcRect) as Uint8ClampedArray [r,g,b,a, r,g,b,a, ...]
  function getEdgeColumn(srcRect, xOffset) {
    if (!currentTilesetImage) return null;
    const offscreen = document.createElement('canvas');
    offscreen.width = 1;
    offscreen.height = srcRect.h;
    const octx = offscreen.getContext('2d');
    octx.drawImage(currentTilesetImage, srcRect.x + xOffset, srcRect.y, 1, srcRect.h, 0, 0, 1, srcRect.h);
    return octx.getImageData(0, 0, 1, srcRect.h).data;
  }

  // Extract a single pixel row (y offset within srcRect) as Uint8ClampedArray [r,g,b,a, r,g,b,a, ...]
  function getEdgeRow(srcRect, yOffset) {
    if (!currentTilesetImage) return null;
    const offscreen = document.createElement('canvas');
    offscreen.width = srcRect.w;
    offscreen.height = 1;
    const octx = offscreen.getContext('2d');
    octx.drawImage(currentTilesetImage, srcRect.x, srcRect.y + yOffset, srcRect.w, 1, 0, 0, srcRect.w, 1);
    return octx.getImageData(0, 0, srcRect.w, 1).data;
  }

  // Compare two edge pixel arrays using RGBA vector similarity.
  // Returns the fraction of pixels that are "similar" (0.0 to 1.0).
  // Two pixels are similar if their RGBA Euclidean distance is within threshold (max distance = 510 = sqrt(255^2*4)).
  function edgeSimilarity(edgeA, edgeB) {
    if (!edgeA || !edgeB || edgeA.length !== edgeB.length) return 0;
    const pixelCount = edgeA.length / 4;
    if (pixelCount === 0) return 0;
    let matchingPixels = 0;
    // Max distance for a single pixel: sqrt(255^2 + 255^2 + 255^2 + 255^2) â‰ˆ 510
    const maxDist = 510;
    // Per-pixel threshold: allow ~10% color deviation per channel
    const pixelThreshold = maxDist * 0.1; // ~51 distance units
    for (let i = 0; i < pixelCount; i++) {
      const off = i * 4;
      const dr = edgeA[off] - edgeB[off];
      const dg = edgeA[off + 1] - edgeB[off + 1];
      const db = edgeA[off + 2] - edgeB[off + 2];
      const da = edgeA[off + 3] - edgeB[off + 3];
      const dist = Math.sqrt(dr * dr + dg * dg + db * db + da * da);
      if (dist <= pixelThreshold) matchingPixels++;
    }
    return matchingPixels / pixelCount;
  }

  // Auto-Neighbor (direct): for all tile pairs, if they share an edge on the tileset image
  // and their edge pixels are similar enough, add adjacency metadata to both.
  function autoNeighborAll(similarityThreshold) {
    if (!currentTilesetData || !currentTilesetImage) return 0;
    const tiles = currentTilesetData.tiles;
    let addedCount = 0;

    for (let i = 0; i < tiles.length; i++) {
      for (let j = i + 1; j < tiles.length; j++) {
        const a = tiles[i], b = tiles[j];
        const arect = a.source_rect, brect = b.source_rect;

        // Ensure adjacency objects exist
        if (!a.adjacency) a.adjacency = { up: [], down: [], left: [], right: [] };
        if (!b.adjacency) b.adjacency = { up: [], down: [], left: [], right: [] };

        // Check: A's right edge touches B's left edge (A is left of B)
        if (arect.x + arect.w === brect.x && arect.h === brect.h && arect.y === brect.y) {
          const edgeA = getEdgeColumn(arect, arect.w - 1); // rightmost column of A
          const edgeB = getEdgeColumn(brect, 0);            // leftmost column of B
          const sim = edgeSimilarity(edgeA, edgeB);
          if (sim >= similarityThreshold) {
            if (!a.adjacency.right.includes(b.id)) { a.adjacency.right.push(b.id); addedCount++; }
            if (!b.adjacency.left.includes(a.id)) { b.adjacency.left.push(a.id); addedCount++; }
          }
        }

        // Check: B's right edge touches A's left edge (B is left of A)
        if (brect.x + brect.w === arect.x && arect.h === brect.h && arect.y === brect.y) {
          const edgeB = getEdgeColumn(brect, brect.w - 1); // rightmost column of B
          const edgeA = getEdgeColumn(arect, 0);            // leftmost column of A
          const sim = edgeSimilarity(edgeB, edgeA);
          if (sim >= similarityThreshold) {
            if (!b.adjacency.right.includes(a.id)) { b.adjacency.right.push(a.id); addedCount++; }
            if (!a.adjacency.left.includes(b.id)) { a.adjacency.left.push(b.id); addedCount++; }
          }
        }

        // Check: A's bottom edge touches B's top edge (A is above B)
        if (arect.y + arect.h === brect.y && arect.w === brect.w && arect.x === brect.x) {
          const edgeA = getEdgeRow(arect, arect.h - 1); // bottom row of A
          const edgeB = getEdgeRow(brect, 0);            // top row of B
          const sim = edgeSimilarity(edgeA, edgeB);
          if (sim >= similarityThreshold) {
            if (!a.adjacency.down.includes(b.id)) { a.adjacency.down.push(b.id); addedCount++; }
            if (!b.adjacency.up.includes(a.id)) { b.adjacency.up.push(a.id); addedCount++; }
          }
        }

        // Check: B's bottom edge touches A's top edge (B is above A)
        if (brect.y + brect.h === arect.y && arect.w === brect.w && arect.x === brect.x) {
          const edgeB = getEdgeRow(brect, brect.h - 1); // bottom row of B
          const edgeA = getEdgeRow(arect, 0);            // top row of A
          const sim = edgeSimilarity(edgeB, edgeA);
          if (sim >= similarityThreshold) {
            if (!b.adjacency.down.includes(a.id)) { b.adjacency.down.push(a.id); addedCount++; }
            if (!a.adjacency.up.includes(b.id)) { a.adjacency.up.push(b.id); addedCount++; }
          }
        }
      }
    }
    return addedCount;
  }

  // Auto-Neighbor (expanded): for all tile pairs, compare edge pixels regardless of
  // position on the tileset image. Searches in concentric circles of distance from each
  // tile, checking candidates that have compatible dimensions.
  // "Concentric circles" here means: we sort candidate tiles by their distance from the
  // reference tile on the sheet, and check them in expanding order. This makes nearer tiles
  // get priority when multiple candidates pass the threshold.
  // Progress modal helpers
  let progressCancelled = false;
  function showProgressModal(title) {
    progressCancelled = false;
    const modal = document.getElementById('progress-modal');
    document.getElementById('progress-title').textContent = title;
    document.getElementById('progress-bar').style.width = '0%';
    document.getElementById('progress-text').textContent = '0%';
    modal.style.display = 'flex';
  }
  function updateProgress(pct, detail) {
    document.getElementById('progress-bar').style.width = pct + '%';
    document.getElementById('progress-text').textContent = detail || (Math.round(pct) + '%');
  }
  function hideProgressModal() {
    document.getElementById('progress-modal').style.display = 'none';
  }
  document.getElementById('progress-cancel-btn').addEventListener('click', () => {
    progressCancelled = true;
  });

  async function autoNeighborExpanded(similarityThreshold) {
    if (!currentTilesetData || !currentTilesetImage) return 0;
    const tiles = currentTilesetData.tiles;
    let addedCount = 0;
    const totalTiles = tiles.length;
    const totalPairs = totalTiles * (totalTiles - 1);

    // Helper: center point of a tile's source_rect
    function tileCenter(t) {
      return { x: t.source_rect.x + t.source_rect.w / 2, y: t.source_rect.y + t.source_rect.h / 2 };
    }

    // Helper: Euclidean distance between two tile centers
    function tileDist(a, b) {
      const ca = tileCenter(a), cb = tileCenter(b);
      const dx = ca.x - cb.x, dy = ca.y - cb.y;
      return Math.sqrt(dx * dx + dy * dy);
    }

    showProgressModal('Expanded Neighbor Search');

    // For each tile, sort all other tiles by distance and check edge compatibility
    for (let i = 0; i < tiles.length; i++) {
      if (progressCancelled) { hideProgressModal(); return addedCount; }

      const a = tiles[i];
      const arect = a.source_rect;
      if (!a.adjacency) a.adjacency = { up: [], down: [], left: [], right: [] };

      // Build candidate list sorted by distance from tile a
      const candidates = [];
      for (let j = 0; j < tiles.length; j++) {
        if (j === i) continue;
        candidates.push({ idx: j, dist: tileDist(a, tiles[j]) });
      }
      candidates.sort((x, y) => x.dist - y.dist);

      // Check each candidate in distance order for each direction
      for (let ci = 0; ci < candidates.length; ci++) {
        const cand = candidates[ci];
        const b = tiles[cand.idx];
        const brect = b.source_rect;
        if (!b.adjacency) b.adjacency = { up: [], down: [], left: [], right: [] };

        // Horizontal adjacency: A right → B left (requires same height)
        if (arect.h === brect.h && !a.adjacency.right.includes(b.id)) {
          const edgeA = getEdgeColumn(arect, arect.w - 1);
          const edgeB = getEdgeColumn(brect, 0);
          const sim = edgeSimilarity(edgeA, edgeB);
          if (sim >= similarityThreshold) {
            a.adjacency.right.push(b.id); addedCount++;
            if (!b.adjacency.left.includes(a.id)) { b.adjacency.left.push(a.id); addedCount++; }
          }
        }

        // Horizontal adjacency: B right → A left (requires same height)
        if (arect.h === brect.h && !a.adjacency.left.includes(b.id)) {
          const edgeB = getEdgeColumn(brect, brect.w - 1);
          const edgeA = getEdgeColumn(arect, 0);
          const sim = edgeSimilarity(edgeB, edgeA);
          if (sim >= similarityThreshold) {
            a.adjacency.left.push(b.id); addedCount++;
            if (!b.adjacency.right.includes(a.id)) { b.adjacency.right.push(a.id); addedCount++; }
          }
        }

        // Vertical adjacency: A bottom → B top (requires same width)
        if (arect.w === brect.w && !a.adjacency.down.includes(b.id)) {
          const edgeA = getEdgeRow(arect, arect.h - 1);
          const edgeB = getEdgeRow(brect, 0);
          const sim = edgeSimilarity(edgeA, edgeB);
          if (sim >= similarityThreshold) {
            a.adjacency.down.push(b.id); addedCount++;
            if (!b.adjacency.up.includes(a.id)) { b.adjacency.up.push(a.id); addedCount++; }
          }
        }

        // Vertical adjacency: B bottom → A top (requires same width)
        if (arect.w === brect.w && !a.adjacency.up.includes(b.id)) {
          const edgeB = getEdgeRow(brect, brect.h - 1);
          const edgeA = getEdgeRow(arect, 0);
          const sim = edgeSimilarity(edgeB, edgeA);
          if (sim >= similarityThreshold) {
            a.adjacency.up.push(b.id); addedCount++;
            if (!b.adjacency.down.includes(a.id)) { b.adjacency.down.push(a.id); addedCount++; }
          }
        }

        // Yield to UI every 50 candidates to keep progress bar responsive
        if (ci % 50 === 0) {
          const pairsProcessed = i * (totalTiles - 1) + ci;
          const pct = (pairsProcessed / totalPairs) * 100;
          updateProgress(pct, `${Math.round(pct)}% — tile ${i + 1}/${totalTiles} (${addedCount} links found)`);
          await new Promise(r => setTimeout(r, 0));
          if (progressCancelled) { hideProgressModal(); return addedCount; }
        }
      }

      // Update progress after each tile completes
      const pct = ((i + 1) / totalTiles) * 100;
      updateProgress(pct, `${Math.round(pct)}% — tile ${i + 1}/${totalTiles} (${addedCount} links found)`);
      await new Promise(r => setTimeout(r, 0));
    }

    hideProgressModal();
    return addedCount;
  }

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
        div.innerHTML = `<span style="opacity:0.5;font-size:11px">${escapeHtml(pathPrefix)}</span>${escapeHtml(folderName)} &#x1F4C1;`;
        div.dataset.path = name;
        div.addEventListener('click', () => selectTileset(name));
        tilesetList.appendChild(div);
      });
      populateLEFolderList(tilesets);
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
    enableBtn('auto-neighbor-btn-global', false); // need tiles first
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
    if (!container || !currentTilesetData) {
      updateAutoNeighborBtnState();
      return;
    }
    const tiles = currentTilesetData.tiles;
    updateAutoNeighborBtnState();
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
          adjacency: {up:[],down:[],left:[],right:[]}, labels: [], chance: 1
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

  // --- Navigate to a tile by ID: select it and scroll the canvas to show it ---
  function navigateToTile(tileId) {
    if (!currentTilesetData) return;
    const idx = currentTilesetData.tiles.findIndex(t => t.id === tileId);
    if (idx < 0) {
      setStatus(`Tile "${tileId}" not found in current tileset.`);
      return;
    }
    selectedTileIndex = idx;
    highlightedCell = null;
    renderTilesetCanvas();
    renderTileInfo();
    renderCreatedTilesList();

    // Scroll canvas wrapper to center the selected tile
    const tile = currentTilesetData.tiles[idx];
    const zoom = Math.max(1, parseInt(zoomInput.value) || 1);
    const wrapper = document.querySelector('.canvas-wrapper');
    if (wrapper && tile.source_rect) {
      const sr = tile.source_rect;
      const tileCenterX = (sr.x + sr.w / 2) * zoom;
      const tileCenterY = (sr.y + sr.h / 2) * zoom;
      wrapper.scrollLeft = tileCenterX - wrapper.clientWidth / 2;
      wrapper.scrollTop = tileCenterY - wrapper.clientHeight / 2;
    }

    // Also scroll the created tiles list to show the active item
    const listContainer = document.getElementById('created-tiles-list');
    if (listContainer) {
      const activeItem = listContainer.querySelector('.created-tile-item.active');
      if (activeItem) activeItem.scrollIntoView({ block: 'nearest' });
    }

    setStatus(`Navigated to tile "${tileId}".`);
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

    const tileChance = (tile.chance !== undefined && tile.chance !== null) ? tile.chance : 1;
    let html = `<div class="tile-info">
      <strong>ID:</strong> <input type="text" id="tile-id-input" value="${escapeHtml(tile.id)}" style="width:130px;padding:2px 4px;background:#0d0d1a;border:1px solid #0f3460;color:#e0e0e0;border-radius:3px;font-size:12px;">
      <div style="color:#a0a0a0;">Src: (${tile.source_rect.x},${tile.source_rect.y}) ${tile.source_rect.w}x${tile.source_rect.h}</div>
      <div style="margin-top:6px;"><strong>Chance:</strong> <input type="number" id="tile-chance-input" value="${tileChance}" min="0" step="1" style="width:60px;padding:2px 4px;background:#0d0d1a;border:1px solid #0f3460;color:#e0e0e0;border-radius:3px;font-size:12px;" title="Relative probability weight for random map generation (0 = excluded, higher = more likely)"></div>
      <br><button id="remove-duplicates-btn" style="margin-top:6px;padding:5px 10px;background:#ff6b6b;color:#fff;border:none;border-radius:3px;cursor:pointer;font-size:11px;font-weight:600;" title="Remove all other tiles with identical pixel content to this one">&#x1F5D1; Remove Duplicates</button>
    </div>`;

    // Labels assignment
    html += '<div class="adjacency-section"><h4>Labels</h4><div class="adjacency-list">';
    if (tile.labels.length === 0) {
      html += '<span style="color:#a0a0a0;font-size:11px;font-style:italic;">- No Labels Set -</span>';
    } else {
      tile.labels.forEach((l, i) => {
        html += `<span class="adj-tag">${escapeHtml(l)} <span class="remove-tile-label" data-idx="${i}">&times;</span></span>`;
      });
    }
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
        html += `<span class="adj-tag"><span class="neighbor-link" data-tile-id="${escapeHtml(n)}">${escapeHtml(n)}</span> <span class="remove-btn" data-dir="${dir}" data-idx="${i}">&times;</span></span>`;
      });
      html += `</div><div class="add-adj-row"><input type="text" placeholder="Neighbor ID" id="add-adj-${dir}"><button data-dir="${dir}" class="add-adj-btn">+</button></div></div>`;
    });
    tileInfoPanel.innerHTML = html;

    // Bind "Remove Duplicates" button
    const removeDupsBtn = document.getElementById('remove-duplicates-btn');
    if (removeDupsBtn) {
      removeDupsBtn.addEventListener('click', () => {
        if (selectedTileIndex < 0 || !currentTilesetData || !currentTilesetImage) return;
        const refTile = currentTilesetData.tiles[selectedTileIndex];
        const refPixels = getTilePixelData(refTile.source_rect);
        if (!refPixels) { setStatus('Could not read pixel data for selected tile'); return; }

        // Find all tiles with identical pixel content (excluding the selected one)
        const toRemove = [];
        for (let i = 0; i < currentTilesetData.tiles.length; i++) {
          if (i === selectedTileIndex) continue;
          const t = currentTilesetData.tiles[i];
          // Quick size check
          if (t.source_rect.w !== refTile.source_rect.w || t.source_rect.h !== refTile.source_rect.h) continue;
          const pixels = getTilePixelData(t.source_rect);
          if (pixels && pixelDataEqual(refPixels, pixels)) {
            toRemove.push(i);
          }
        }

        if (toRemove.length === 0) {
          setStatus('No duplicate tiles found matching the selected tile.');
          return;
        }

        if (!confirm(`Remove ${toRemove.length} tile(s) with identical pixel content to "${refTile.id}"?`)) return;

        // Remove in reverse order to keep indices stable
        for (let i = toRemove.length - 1; i >= 0; i--) {
          const idx = toRemove[i];
          currentTilesetData.tiles.splice(idx, 1);
          if (selectedTileIndex > idx) selectedTileIndex--;
        }

        setStatus(`Removed ${toRemove.length} duplicate tile(s).`);
        renderTilesetCanvas();
        renderTileInfo();
        renderCreatedTilesList();
      });
    }

    // Bind ID change
    document.getElementById('tile-id-input').addEventListener('change', (e) => {
      currentTilesetData.tiles[selectedTileIndex].id = e.target.value;
      renderCreatedTilesList();
    });
    // Bind chance change
    document.getElementById('tile-chance-input').addEventListener('change', (e) => {
      const val = parseInt(e.target.value);
      currentTilesetData.tiles[selectedTileIndex].chance = isNaN(val) ? 1 : Math.max(0, val);
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
    // Bind neighbor link click (navigate to tile)
    tileInfoPanel.querySelectorAll('.neighbor-link').forEach(link => {
      link.addEventListener('click', (e) => {
        e.stopPropagation();
        navigateToTile(link.dataset.tileId);
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

  // --- Global Auto-Neighbor button (tileset-level operation) ---
  document.getElementById('auto-neighbor-btn-global').addEventListener('click', async () => {
    if (!currentTilesetData || !currentTilesetImage) {
      setStatus('Load a tileset sheet first.');
      return;
    }
    if (currentTilesetData.tiles.length < 2) {
      setStatus('Need at least 2 tiles for auto-neighbor detection.');
      return;
    }
    // Check if any tiles already have adjacency data
    const tilesWithAdjacency = currentTilesetData.tiles.filter(t => t.adjacency &&
      (t.adjacency.up?.length || t.adjacency.down?.length || t.adjacency.left?.length || t.adjacency.right?.length));
    if (tilesWithAdjacency.length > 0) {
      if (!confirm(`Warning: ${tilesWithAdjacency.length} tile(s) already have neighbor/adjacency data. Auto-Neighbor will add new links (existing links are preserved). Continue?`)) {
        return;
      }
    }
    const thresholdInput = document.getElementById('auto-neighbor-threshold');
    const thresholdPct = Math.max(0, Math.min(100, parseInt(thresholdInput.value) || 90));
    const threshold = thresholdPct / 100;
    const expandedCheckbox = document.getElementById('auto-neighbor-expanded');
    const useExpanded = expandedCheckbox && expandedCheckbox.checked;
    const modeLabel = useExpanded ? 'expanded' : 'direct';
    // Disable button during async expanded search to prevent double-click
    const btn = document.getElementById('auto-neighbor-btn-global');
    if (useExpanded) btn.disabled = true;
    const added = useExpanded ? await autoNeighborExpanded(threshold) : autoNeighborAll(threshold);
    if (useExpanded) enableBtn('auto-neighbor-btn-global', true);
    if (progressCancelled) {
      setStatus(`Auto-Neighbor (${modeLabel}) cancelled. ${added} link(s) were added before cancellation.`);
    } else if (added === 0) {
      alert(`Auto-Neighbor (${modeLabel}) complete: no new adjacency links found at ${thresholdPct}% similarity.`);
      setStatus(`Auto-Neighbor (${modeLabel}): no new adjacency links found at ${thresholdPct}% similarity.`);
    } else {
      alert(`Auto-Neighbor (${modeLabel}) complete: added ${added} adjacency link(s) at ${thresholdPct}% similarity.`);
      setStatus(`Auto-Neighbor (${modeLabel}): added ${added} adjacency link(s) at ${thresholdPct}% similarity.`);
    }
    renderTileInfo();
  });

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
            adjacency: { up: [], down: [], left: [], right: [] }, labels: [], chance: 1
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
      browseBtn.textContent = '\u{1F4C2} Browse for .tmx file...';
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

  // Step buttons (â–²â–¼ Â±1, â–²â–²â–¼â–¼ Â±8, â–²â–²â–²â–¼â–¼â–¼ Â±16)
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
      drawBlockerBtn.textContent = 'â–  Blocker ON';
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
    setStatus(`Blocker added: (${x},${y}) ${w}x${h}`);
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
        <span>(${r.x},${r.y}) ${r.w}x${r.h}</span>
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
        setStatus(`Blocker removed: (${removed.x},${removed.y}) ${removed.w}x${removed.h}`);
      });
    });
  }


  // ============================================================
  // LEVEL EDITOR — Redesigned with Selection Slot workflow
  // ============================================================

  const leFolderList = document.getElementById('le-folder-list');
  const leJsonSelect = document.getElementById('le-json-select');
  const leLoadBtn = document.getElementById('le-load-btn');
  const leExportBtn = document.getElementById('le-export-btn');
  const lePalette = document.getElementById('le-palette');
  const levelCanvas = document.getElementById('level-canvas');
  const lCtx = levelCanvas.getContext('2d');
  const leLabelFilter = document.getElementById('le-label-filter');
  const leSlotEl = document.getElementById('le-selection-slot');
  const leDetailPanel = document.getElementById('le-tile-detail');

  // State
  let leTilesetName = null;
  let leTilesetData = null;
  let leTilesetImage = null;
  let leSelectedPaletteId = null;
  let leActiveLabelFilter = '';
  let leZoom = 1.0;
  let leGridCellW = 16, leGridCellH = 16, leGridOffX = 0, leGridOffY = 0;
  let leMapW = 20, leMapH = 15; // map size in cells
  let leShowBlockers = false;
  let leMode = 'free'; // 'free' | 'picking' | 'constrained'
  let leSlottedTile = null; // { tile_id, x, y, w, h, mapLabels? }
  let lePlacedTiles = []; // Array of { tile_id, x, y, w, h, mapLabels: [], blocked: false }
  let leHoveredPaletteId = null;
  let leHoverWarningTile = null; // tile that would be removed on placement
  let leSelectedFolder = null; // currently selected folder name
  let leBlockerPaintMode = false; // toggle for per-tile blocker painting

  // --- Folder list population (matching configurator style) ---
  function populateLEFolderList(tilesets) {
    if (!leFolderList) return;
    leFolderList.innerHTML = '';
    // Extract unique folder paths
    const folders = [...new Set(tilesets.map(name => name.split('/')[0]))];
    folders.forEach(folder => {
      const div = document.createElement('div');
      div.className = 'tileset-item';
      div.textContent = folder;
      div.dataset.folder = folder;
      div.addEventListener('click', () => selectLEFolder(folder));
      leFolderList.appendChild(div);
    });
  }

  async function selectLEFolder(folder) {
    // Highlight selected folder
    if (leFolderList) {
      leFolderList.querySelectorAll('.tileset-item').forEach(el => {
        el.classList.toggle('selected', el.dataset.folder === folder);
      });
    }
    leSelectedFolder = folder;
    // Fetch JSON files in this folder
    try {
      const res = await fetch(`/api/tilesets/${folder}/jsons`);
      const jsons = await res.json();
      leJsonSelect.innerHTML = '';
      leJsonSelect.disabled = false;
      if (jsons.length === 0) {
        leJsonSelect.innerHTML = '<option value="">No JSON files found</option>';
        leJsonSelect.disabled = true;
        return;
      }
      if (jsons.length === 1) {
        const opt = document.createElement('option');
        opt.value = jsons[0]; opt.textContent = jsons[0];
        leJsonSelect.appendChild(opt);
      } else {
        jsons.forEach(jsonFile => {
          const opt = document.createElement('option');
          opt.value = jsonFile; opt.textContent = jsonFile;
          leJsonSelect.appendChild(opt);
        });
      }
    } catch (err) {
      leJsonSelect.innerHTML = '<option value="">Error loading JSONs</option>';
      leJsonSelect.disabled = true;
    }
  }

  // --- Load tileset ---
  leLoadBtn.addEventListener('click', async () => {
    if (!leSelectedFolder) { setStatus('Select a tileset folder first'); return; }
    const jsonFile = leJsonSelect.value;
    if (!jsonFile) { setStatus('Select a JSON tileset file first'); return; }
    const name = leSelectedFolder;
    const sheetBase = jsonFile.replace(/\.json$/i, '');
    try {
      const img = new Image();
      img.crossOrigin = 'anonymous';
      await new Promise((resolve, reject) => {
        img.onload = resolve; img.onerror = reject;
        img.src = `/api/tilesets/${name}/sheets/${sheetBase}.png?t=${Date.now()}`;
      });
      leTilesetImage = img;
      const res = await fetch(`/api/tilesets/${name}/json/${sheetBase}`);
      leTilesetData = res.ok ? await res.json() : { tiles: [], labels: [], blockers: [] };
      if (!leTilesetData.tiles) leTilesetData.tiles = [];
      if (!leTilesetData.labels) leTilesetData.labels = [];
      if (!leTilesetData.blockers) leTilesetData.blockers = [];
      leTilesetName = name;
      leSelectedPaletteId = null;
      leSlottedTile = null;
      leMode = 'free';
      lePlacedTiles = [];
      if (leTilesetData.tiles.length > 0 && leTilesetData.tiles[0].source_rect) {
        leGridCellW = leTilesetData.tiles[0].source_rect.w || 16;
        leGridCellH = leTilesetData.tiles[0].source_rect.h || 16;
        document.getElementById('le-cell-w').value = leGridCellW;
        document.getElementById('le-cell-h').value = leGridCellH;
      }
      populateLELabelFilter();
      updateSlotUI();
      renderLEPalette();
      renderLECanvas();
      renderLEDetail();
      renderTilesetLabels();
      renderMapLabels();
      setStatus(`Level Editor: Loaded '${name}/${jsonFile}' (${leTilesetData.tiles.length} tiles)`);
    } catch (err) { setStatus(`Error: ${err.message}`); }
  });

  // --- Label filter ---
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

  // --- Per-Tile Map Labels (instance labels on placed tiles) ---
  const leMapLabelsListEl = document.getElementById('le-map-labels-list');
  const leMapLabelInput = document.getElementById('le-map-label-input');
  const leMapLabelAddBtn = document.getElementById('le-map-label-add-btn');
  const leTilesetLabelsEl = document.getElementById('le-tileset-labels');

  function getSlottedPlacedTile() {
    // Find the placed tile that matches the current slotted tile position
    if (!leSlottedTile) return null;
    return lePlacedTiles.find(t => t.x === leSlottedTile.x && t.y === leSlottedTile.y && t.tile_id === leSlottedTile.tile_id) || null;
  }

  function renderTilesetLabels() {
    if (!leTilesetLabelsEl) return;
    if (!leSlottedTile || !leTilesetData) {
      leTilesetLabelsEl.innerHTML = '<p style="color:#a0a0a0;font-size:11px;font-style:italic;">Double-click a placed tile to see its tileset labels.</p>';
      return;
    }
    const td = leTilesetData.tiles.find(t => t.id === leSlottedTile.tile_id);
    const tilesetLabels = (td && td.labels) ? td.labels : [];
    if (tilesetLabels.length === 0) {
      leTilesetLabelsEl.innerHTML = '<p style="color:#a0a0a0;font-size:11px;font-style:italic;">No tileset labels defined for this tile.</p>';
      return;
    }
    let html = '<div style="margin-bottom:2px;color:#a0a0a0;font-size:10px;">From tileset definition (read-only):</div>';
    html += '<div style="display:flex;flex-wrap:wrap;gap:4px;">';
    tilesetLabels.forEach(label => {
      html += `<span class="adj-tag" style="background:#1a1a2e;padding:2px 6px;border-radius:3px;font-size:11px;">${escapeHtml(label)}</span>`;
    });
    html += '</div>';
    leTilesetLabelsEl.innerHTML = html;
  }

  function renderMapLabels() {
    if (!leMapLabelsListEl) return;
    const placedTile = getSlottedPlacedTile();
    if (!placedTile) {
      leMapLabelsListEl.innerHTML = '<p style="color:#a0a0a0;font-size:11px;font-style:italic;">Double-click a placed tile on the map to select it, then add labels here.</p>';
      return;
    }
    // Hide the hint once a tile is selected
    const hint = document.getElementById('le-map-labels-hint');
    if (hint) hint.style.display = 'none';
    const labels = placedTile.mapLabels || [];
    if (labels.length === 0) {
      leMapLabelsListEl.innerHTML = '<p style="color:#a0a0a0;font-size:11px;font-style:italic;">No map labels on this tile yet. Type a label below and click + to add.</p>';
      return;
    }
    let html = '';
    labels.forEach((label, idx) => {
      html += `<div style="display:flex;align-items:center;justify-content:space-between;padding:2px 6px;margin-bottom:2px;background:#1a1a2e;border-radius:3px;">`;
      html += `<span class="adj-tag" style="color:#81d4fa;">${escapeHtml(label)}</span>`;
      html += `<span class="del-map-label" data-idx="${idx}" style="color:#ff6b6b;cursor:pointer;font-weight:bold;padding:0 4px;">&times;</span>`;
      html += `</div>`;
    });
    leMapLabelsListEl.innerHTML = html;
    // Attach delete handlers
    leMapLabelsListEl.querySelectorAll('.del-map-label').forEach(el => {
      el.addEventListener('click', () => {
        const idx = parseInt(el.dataset.idx);
        const pt = getSlottedPlacedTile();
        if (pt && pt.mapLabels) {
          pt.mapLabels.splice(idx, 1);
          renderMapLabels();
        }
      });
    });
  }

  if (leMapLabelAddBtn && leMapLabelInput) {
    leMapLabelAddBtn.addEventListener('click', () => {
      const val = leMapLabelInput.value.trim();
      const placedTile = getSlottedPlacedTile();
      if (!placedTile) { setStatus('Pick a placed tile first to add labels'); return; }
      if (!placedTile.mapLabels) placedTile.mapLabels = [];
      if (val && !placedTile.mapLabels.includes(val)) {
        placedTile.mapLabels.push(val);
        leMapLabelInput.value = '';
        renderMapLabels();
      }
    });
    leMapLabelInput.addEventListener('keydown', (e) => {
      if (e.key === 'Enter') {
        e.preventDefault();
        leMapLabelAddBtn.click();
      }
    });
  }
  renderMapLabels();
  renderTilesetLabels();

  // --- Zoom ---
  const leZoomIn = document.getElementById('le-zoom-in');
  const leZoomOut = document.getElementById('le-zoom-out');
  const leZoomLabel = document.getElementById('le-zoom-label');
  function updateLeZoom(delta) {
    leZoom = Math.max(0.25, Math.min(4, +(leZoom + delta).toFixed(2)));
    leZoomLabel.textContent = leZoom + 'x';
    renderLECanvas();
  }
  leZoomIn.addEventListener('click', () => updateLeZoom(0.25));
  leZoomOut.addEventListener('click', () => updateLeZoom(-0.25));

  // --- Blocker toggle ---
  const leBlockerMode = document.getElementById('le-blocker-mode');
  const leBlockerBtn = document.getElementById('le-blocker-btn');
  leBlockerMode.addEventListener('change', () => {
    leShowBlockers = leBlockerMode.value !== 'off';
    renderLECanvas();
  });
  leBlockerBtn.addEventListener('click', () => {
    leBlockerPaintMode = !leBlockerPaintMode;
    leBlockerBtn.style.background = leBlockerPaintMode ? '#ff6b6b' : '#1a1a2e';
    leBlockerBtn.style.color = leBlockerPaintMode ? '#fff' : '#ff6b6b';
    leBlockerBtn.textContent = leBlockerPaintMode ? 'Blockers: ON' : 'Blockers:';
    if (leBlockerPaintMode) setStatus('Blocker paint mode ON — click tiles to toggle blocking');
    else setStatus('Blocker paint mode OFF');
  });

  // --- Grid param step buttons (reuse configurator pattern) ---
  document.querySelectorAll('.le-controls-bar .step-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      const target = document.getElementById(btn.dataset.target);
      if (!target) return;
      const step = parseInt(btn.dataset.step) || 0;
      target.value = Math.max(1, (parseInt(target.value) || 0) + step);
      leGridCellW = Math.max(1, parseInt(document.getElementById('le-cell-w').value) || 16);
      leGridCellH = Math.max(1, parseInt(document.getElementById('le-cell-h').value) || 16);
      leGridOffX = Math.max(0, parseInt(document.getElementById('le-off-x').value) || 0);
      leGridOffY = Math.max(0, parseInt(document.getElementById('le-off-y').value) || 0);
      leMapW = Math.max(1, parseInt(document.getElementById('le-map-w').value) || 20);
      leMapH = Math.max(1, parseInt(document.getElementById('le-map-h').value) || 15);
      renderLECanvas();
    });
  });

  // --- Map/grid size inputs: sync on direct typing ---
  ['le-map-w', 'le-map-h', 'le-cell-w', 'le-cell-h', 'le-off-x', 'le-off-y'].forEach(id => {
    const el = document.getElementById(id);
    if (el) el.addEventListener('change', () => {
      leGridCellW = Math.max(1, parseInt(document.getElementById('le-cell-w').value) || 16);
      leGridCellH = Math.max(1, parseInt(document.getElementById('le-cell-h').value) || 16);
      leGridOffX = Math.max(0, parseInt(document.getElementById('le-off-x').value) || 0);
      leGridOffY = Math.max(0, parseInt(document.getElementById('le-off-y').value) || 0);
      leMapW = Math.max(1, parseInt(document.getElementById('le-map-w').value) || 20);
      leMapH = Math.max(1, parseInt(document.getElementById('le-map-h').value) || 15);
      renderLECanvas();
    });
  });

  // --- Selection Slot ---
  function updateSlotUI() {
    if (leMode === 'picking') {
      leSlotEl.className = 'le-slot picking';
      leSlotEl.innerHTML = '<span class="le-slot-placeholder">&#x1F3AF; Click a tile on the map...</span>';
    } else if (leSlottedTile && leTilesetData && leTilesetImage) {
      leSlotEl.className = 'le-slot occupied';
      const td = leTilesetData.tiles.find(t => t.id === leSlottedTile.tile_id);
      let html = '<div class="le-slot-content">';
      if (td) {
        html += `<canvas width="${td.source_rect.w}" height="${td.source_rect.h}" style="width:48px;height:48px;image-rendering:pixelated;" id="le-slot-canvas"></canvas>`;
        html += `<span style="font-size:11px;color:#e0e0e0;">${escapeHtml(td.id)}</span>`;
      } else {
        html += `<span style="font-size:11px;color:#ff6b6b;">Unknown: ${escapeHtml(leSlottedTile.tile_id)}</span>`;
      }
      html += '</div>';
      leSlotEl.innerHTML = html;
      // Draw thumbnail
      const slotCanvas = document.getElementById('le-slot-canvas');
      if (slotCanvas && td) {
        const sctx = slotCanvas.getContext('2d');
        sctx.drawImage(leTilesetImage, td.source_rect.x, td.source_rect.y, td.source_rect.w, td.source_rect.h, 0, 0, td.source_rect.w, td.source_rect.h);
      }
    } else {
      leSlotEl.className = 'le-slot';
      leSlotEl.innerHTML = '<span class="le-slot-placeholder">Click to pick from map</span>';
    }
  }

  leSlotEl.addEventListener('click', () => {
    if (leMode === 'picking') {
      // Cancel picking
      leMode = 'free';
      updateSlotUI();
      renderLECanvas();
      setStatus('Pick cancelled');
    } else if (leSlottedTile) {
      // Clear slot -> free mode
      leSlottedTile = null;
      leSelectedPaletteId = null;
      leMode = 'free';
      updateSlotUI();
      renderLEPalette();
      renderTilesetLabels();
      renderMapLabels();
      renderLECanvas();
      setStatus('Slot cleared — free placement mode');
    } else {
      // Enter picking mode
      leMode = 'picking';
      updateSlotUI();
      renderLECanvas();
      setStatus('Click a placed tile on the map to select it');
    }
  });

  // --- Adjacency filtering ---
  function getCompatibleTiles(slottedTileDef) {
    if (!slottedTileDef || !leTilesetData) return [];
    const adj = slottedTileDef.adjacency || { up: [], down: [], left: [], right: [] };
    const results = []; // { tileDef, direction }
    const seen = new Set();
    const directions = ['up', 'down', 'left', 'right'];
    const opposites = { up: 'down', down: 'up', left: 'right', right: 'left' };
    const arrows = { up: '\u2191', down: '\u2193', left: '\u2190', right: '\u2192' };

    for (const dir of directions) {
      const list = adj[dir] || [];
      if (list.length === 0) continue; // unconstrained = skip
      for (const neighborId of list) {
        if (seen.has(neighborId)) continue;
        const nDef = leTilesetData.tiles.find(t => t.id === neighborId);
        if (!nDef) continue;
        // Reciprocity check
        const nAdj = nDef.adjacency || {};
        const nOppList = nAdj[opposites[dir]] || [];
        if (nOppList.length === 0 || nOppList.includes(slottedTileDef.id)) {
          results.push({ tileDef: nDef, direction: dir, arrow: arrows[dir] });
          seen.add(neighborId);
        }
      }
    }
    return results;
  }

  // --- Compute placement position ---
  function computePlacement(slottedTile, candidateDef, direction) {
    const cw = candidateDef.source_rect.w;
    const ch = candidateDef.source_rect.h;
    let x, y;
    switch (direction) {
      case 'right': x = slottedTile.x + slottedTile.w; y = slottedTile.y; break;
      case 'left':  x = slottedTile.x - cw; y = slottedTile.y; break;
      case 'down':  x = slottedTile.x; y = slottedTile.y + slottedTile.h; break;
      case 'up':    x = slottedTile.x; y = slottedTile.y - ch; break;
    }
    return { tile_id: candidateDef.id, x, y, w: cw, h: ch, mapLabels: [], blocked: false };
  }

  // --- Find tile at position (overlap check) ---
  function findTileAtRect(placement) {
    for (let i = lePlacedTiles.length - 1; i >= 0; i--) {
      const t = lePlacedTiles[i];
      // AABB overlap
      if (t.x < placement.x + placement.w && t.x + t.w > placement.x &&
          t.y < placement.y + placement.h && t.y + t.h > placement.y) {
        return i;
      }
    }
    return -1;
  }

  // --- Render palette ---
  function renderLEPalette() {
    lePalette.innerHTML = '';
    if (!leTilesetData || !leTilesetImage) return;

    let tilesToShow = [];
    if (leMode === 'constrained' && leSlottedTile) {
      const slottedDef = leTilesetData.tiles.find(t => t.id === leSlottedTile.tile_id);
      if (slottedDef) {
        tilesToShow = getCompatibleTiles(slottedDef);
      }
    } else {
      // Free mode: show all (filtered by label)
      tilesToShow = leTilesetData.tiles
        .filter(t => !leActiveLabelFilter || (t.labels || []).includes(leActiveLabelFilter))
        .map(t => ({ tileDef: t, direction: null, arrow: null }));
    }

    if (tilesToShow.length === 0) {
      lePalette.innerHTML = '<p style="color:#a0a0a0;font-size:11px;">' +
        (leMode === 'constrained' ? 'No compatible tiles found' : 'No tiles match filter') + '</p>';
      return;
    }

    tilesToShow.forEach(({ tileDef, direction, arrow }) => {
      const div = document.createElement('div');
      div.className = 'palette-tile';
      if (leSelectedPaletteId === tileDef.id) div.classList.add('selected');
      div.title = tileDef.id + (arrow ? ` (${arrow} ${direction})` : '');
      div.dataset.tileId = tileDef.id;
      div.dataset.direction = direction || '';

      const c = document.createElement('canvas');
      c.width = tileDef.source_rect.w;
      c.height = tileDef.source_rect.h;
      c.getContext('2d').drawImage(leTilesetImage,
        tileDef.source_rect.x, tileDef.source_rect.y, tileDef.source_rect.w, tileDef.source_rect.h,
        0, 0, tileDef.source_rect.w, tileDef.source_rect.h);
      div.appendChild(c);

      // Direction arrow overlay
      if (arrow) {
        const arrowEl = document.createElement('span');
        arrowEl.className = 'dir-arrow';
        arrowEl.textContent = arrow;
        div.appendChild(arrowEl);
      }

      // Click handler
      div.addEventListener('click', () => {
        leSelectedPaletteId = tileDef.id;
        if (leMode === 'constrained' && direction && leSlottedTile) {
          // Auto-place
          const placement = computePlacement(leSlottedTile, tileDef, direction);
          const overlapIdx = findTileAtRect(placement);
          if (overlapIdx >= 0) lePlacedTiles.splice(overlapIdx, 1);
          lePlacedTiles.push(placement);
          // Keep the slotted tile unchanged — user explicitly picks a new slot
          renderLEPalette();
          renderLECanvas();
          setStatus(`Placed ${tileDef.id} ${arrow} of slotted tile`);
        } else {
          // Free mode: just select for manual placement
          renderLEPalette();
          renderLEDetail();
          setStatus(`Selected: ${tileDef.id}`);
        }
      });

      // Hover warning (constrained mode)
      if (leMode === 'constrained' && direction && leSlottedTile) {
        div.addEventListener('mouseenter', () => {
          const placement = computePlacement(leSlottedTile, tileDef, direction);
          const overlapIdx = findTileAtRect(placement);
          leHoverWarningTile = overlapIdx >= 0 ? lePlacedTiles[overlapIdx] : null;
          renderLECanvas();
        });
        div.addEventListener('mouseleave', () => {
          leHoverWarningTile = null;
          renderLECanvas();
        });
      }

      lePalette.appendChild(div);
    });
  }

  // --- Tile detail panel (read-only) ---
  function renderLEDetail() {
    if (!leSelectedPaletteId || !leTilesetData) {
      leDetailPanel.innerHTML = '<p style="color:#a0a0a0;font-size:11px;">Select a tile to see its data.</p>';
      return;
    }
    const td = leTilesetData.tiles.find(t => t.id === leSelectedPaletteId);
    if (!td) { leDetailPanel.innerHTML = '<p style="color:#ff6b6b;font-size:11px;">Tile not found.</p>'; return; }
    const adj = td.adjacency || { up: [], down: [], left: [], right: [] };
    let html = `<div style="margin-bottom:4px;"><strong>${escapeHtml(td.id)}</strong></div>`;
    html += `<div style="color:#a0a0a0;">Src: (${td.source_rect.x},${td.source_rect.y}) ${td.source_rect.w}x${td.source_rect.h}</div>`;
    if (td.labels && td.labels.length > 0) {
      html += `<div style="margin-top:3px;">Labels: ${td.labels.map(l => '<span class="adj-tag">' + escapeHtml(l) + '</span>').join(' ')}</div>`;
    }
    html += '<div class="adj-section"><h5>^ Up</h5><div class="adj-ids">' + (adj.up.length ? adj.up.join(', ') : '<em>any</em>') + '</div></div>';
    html += '<div class="adj-section"><h5>v Down</h5><div class="adj-ids">' + (adj.down.length ? adj.down.join(', ') : '<em>any</em>') + '</div></div>';
    html += '<div class="adj-section"><h5>&lt; Left</h5><div class="adj-ids">' + (adj.left.length ? adj.left.join(', ') : '<em>any</em>') + '</div></div>';
    html += '<div class="adj-section"><h5>&gt; Right</h5><div class="adj-ids">' + (adj.right.length ? adj.right.join(', ') : '<em>any</em>') + '</div></div>';
    leDetailPanel.innerHTML = html;
  }

  // --- Canvas rendering ---
  function renderLECanvas() {
    // Map size determines the full canvas area — zoom scales the entire area (like tileset configurator)
    const mapPixelW = leMapW * leGridCellW;
    const mapPixelH = leMapH * leGridCellH;
    levelCanvas.width = Math.round(mapPixelW * leZoom);
    levelCanvas.height = Math.round(mapPixelH * leZoom);
    lCtx.imageSmoothingEnabled = false;
    lCtx.clearRect(0, 0, levelCanvas.width, levelCanvas.height);

    // Fill map background so it's visually distinct from the outer dark area
    lCtx.fillStyle = '#141428';
    lCtx.fillRect(0, 0, levelCanvas.width, levelCanvas.height);

    lCtx.save();
    lCtx.scale(leZoom, leZoom);

    // Grid (free mode only)
    if (leMode === 'free') {
      lCtx.strokeStyle = 'rgba(79,195,247,0.2)';
      lCtx.lineWidth = 1 / leZoom;
      for (let x = leGridOffX; x <= mapPixelW; x += leGridCellW) {
        lCtx.beginPath(); lCtx.moveTo(x + 0.5, 0); lCtx.lineTo(x + 0.5, mapPixelH); lCtx.stroke();
      }
      for (let y = leGridOffY; y <= mapPixelH; y += leGridCellH) {
        lCtx.beginPath(); lCtx.moveTo(0, y + 0.5); lCtx.lineTo(mapPixelW, y + 0.5); lCtx.stroke();
      }
    }

    // Placed tiles
    for (const pt of lePlacedTiles) {
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
    }

    // Slotted tile highlight
    if (leSlottedTile) {
      lCtx.strokeStyle = '#ff9800';
      lCtx.lineWidth = 3 / leZoom;
      lCtx.strokeRect(leSlottedTile.x + 1, leSlottedTile.y + 1, leSlottedTile.w - 2, leSlottedTile.h - 2);
    }

    // Hover warning (red highlight on tile that would be removed)
    if (leHoverWarningTile) {
      lCtx.fillStyle = 'rgba(255, 0, 0, 0.3)';
      lCtx.fillRect(leHoverWarningTile.x, leHoverWarningTile.y, leHoverWarningTile.w, leHoverWarningTile.h);
      lCtx.strokeStyle = '#ff0000';
      lCtx.lineWidth = 2 / leZoom;
      lCtx.strokeRect(leHoverWarningTile.x, leHoverWarningTile.y, leHoverWarningTile.w, leHoverWarningTile.h);
    }

    // Blocker overlay
    if (leShowBlockers && leTilesetData && leTilesetData.blockers && leTilesetData.blockers.length > 0) {
      lCtx.fillStyle = 'rgba(255, 0, 0, 0.25)';
      lCtx.strokeStyle = 'rgba(255, 0, 0, 0.6)';
      lCtx.lineWidth = 1 / leZoom;
      for (const pt of lePlacedTiles) {
        const td = leTilesetData.tiles.find(t => t.id === pt.tile_id);
        if (!td) continue;
        const scaleX = pt.w / td.source_rect.w;
        const scaleY = pt.h / td.source_rect.h;
        for (const b of leTilesetData.blockers) {
          // Check if blocker intersects this tile's source_rect
          if (b.x + b.w <= td.source_rect.x || b.x >= td.source_rect.x + td.source_rect.w) continue;
          if (b.y + b.h <= td.source_rect.y || b.y >= td.source_rect.y + td.source_rect.h) continue;
          // Clip blocker to tile's source_rect
          const clippedX = Math.max(b.x, td.source_rect.x) - td.source_rect.x;
          const clippedY = Math.max(b.y, td.source_rect.y) - td.source_rect.y;
          const clippedW = Math.min(b.x + b.w, td.source_rect.x + td.source_rect.w) - Math.max(b.x, td.source_rect.x);
          const clippedH = Math.min(b.y + b.h, td.source_rect.y + td.source_rect.h) - Math.max(b.y, td.source_rect.y);
          const mapX = pt.x + clippedX * scaleX;
          const mapY = pt.y + clippedY * scaleY;
          const mapW = clippedW * scaleX;
          const mapH = clippedH * scaleY;
          lCtx.fillRect(mapX, mapY, mapW, mapH);
          lCtx.strokeRect(mapX + 0.5, mapY + 0.5, mapW - 1, mapH - 1);
        }
      }
    }

    // Per-tile blocking overlay (map-level blocked tiles)
    if (leShowBlockers) {
      lCtx.fillStyle = 'rgba(200, 0, 0, 0.35)';
      lCtx.strokeStyle = 'rgba(200, 0, 0, 0.8)';
      lCtx.lineWidth = 2 / leZoom;
      for (const pt of lePlacedTiles) {
        if (pt.blocked) {
          lCtx.fillRect(pt.x, pt.y, pt.w, pt.h);
          lCtx.strokeRect(pt.x + 0.5, pt.y + 0.5, pt.w - 1, pt.h - 1);
        }
      }
    }

    // Map border
    lCtx.strokeStyle = 'rgba(79,195,247,0.5)';
    lCtx.lineWidth = 2 / leZoom;
    lCtx.strokeRect(0, 0, mapPixelW, mapPixelH);

    lCtx.restore();
  }

  // --- Canvas mouse handlers ---
  function getCanvasWorldPos(e) {
    const rect = levelCanvas.getBoundingClientRect();
    const sx = levelCanvas.width / rect.width, sy = levelCanvas.height / rect.height;
    return {
      x: (e.clientX - rect.left) * sx / leZoom,
      y: (e.clientY - rect.top) * sy / leZoom
    };
  }

  function findPlacedTileAt(wx, wy) {
    for (let i = lePlacedTiles.length - 1; i >= 0; i--) {
      const t = lePlacedTiles[i];
      if (wx >= t.x && wx < t.x + t.w && wy >= t.y && wy < t.y + t.h) return i;
    }
    return -1;
  }

  levelCanvas.addEventListener('click', (e) => {
    if (e.button !== 0) return;
    const { x: wx, y: wy } = getCanvasWorldPos(e);

    // Blocker paint mode: toggle per-tile blocking
    if (leBlockerPaintMode) {
      const idx = findPlacedTileAt(wx, wy);
      if (idx >= 0) {
        lePlacedTiles[idx].blocked = !lePlacedTiles[idx].blocked;
        renderLECanvas();
        setStatus(`Tile ${lePlacedTiles[idx].tile_id} ${lePlacedTiles[idx].blocked ? 'BLOCKED' : 'unblocked'}`);
      } else {
        setStatus('No tile at that position');
      }
      return;
    }

    if (leMode === 'picking') {
      // Pick tile from map
      const idx = findPlacedTileAt(wx, wy);
      if (idx >= 0) {
        leSlottedTile = { ...lePlacedTiles[idx] };
        leSelectedPaletteId = leSlottedTile.tile_id;
        leMode = 'constrained';
        updateSlotUI();
        renderLEPalette();
        renderLEDetail();
        renderTilesetLabels();
        renderMapLabels();
        renderLECanvas();
        setStatus(`Picked: ${leSlottedTile.tile_id} — palette filtered to compatible tiles`);
      } else {
        setStatus('No tile at that position — click a placed tile');
      }
      return;
    }

    if (leMode === 'free') {
      // If no palette tile is selected, clicking on a placed tile selects it for label editing
      if (!leSelectedPaletteId || !leTilesetData) {
        const idx = findPlacedTileAt(wx, wy);
        if (idx >= 0) {
          leSlottedTile = { ...lePlacedTiles[idx] };
          leSelectedPaletteId = leSlottedTile.tile_id;
          leMode = 'constrained';
          updateSlotUI();
          renderLEPalette();
          renderLEDetail();
          renderTilesetLabels();
          renderMapLabels();
          renderLECanvas();
          const hint = document.getElementById('le-map-labels-hint');
          if (hint) hint.style.display = 'none';
          setStatus(`Selected: ${leSlottedTile.tile_id} — edit map labels in the right panel`);
        } else {
          setStatus('Select a tile from the palette first');
        }
        return;
      }
      const td = leTilesetData.tiles.find(t => t.id === leSelectedPaletteId);
      if (!td) return;
      // Snap to grid
      const col = Math.floor((wx - leGridOffX) / leGridCellW);
      const row = Math.floor((wy - leGridOffY) / leGridCellH);
      const px = leGridOffX + col * leGridCellW;
      const py = leGridOffY + row * leGridCellH;
      const placement = { tile_id: td.id, x: px, y: py, w: td.source_rect.w, h: td.source_rect.h, mapLabels: [], blocked: false };
      // Remove existing at that position
      const overlapIdx = findTileAtRect(placement);
      if (overlapIdx >= 0) lePlacedTiles.splice(overlapIdx, 1);
      lePlacedTiles.push(placement);
      renderLECanvas();
      setStatus(`Placed ${td.id} at (${px},${py})`);
    }
  });

  // Double-click to select a placed tile directly for label editing (shortcut for slot picking)
  levelCanvas.addEventListener('dblclick', (e) => {
    if (e.button !== 0) return;
    const { x: wx, y: wy } = getCanvasWorldPos(e);
    const idx = findPlacedTileAt(wx, wy);
    if (idx >= 0) {
      leSlottedTile = { ...lePlacedTiles[idx] };
      leSelectedPaletteId = leSlottedTile.tile_id;
      leMode = 'constrained';
      updateSlotUI();
      renderLEPalette();
      renderLEDetail();
      renderTilesetLabels();
      renderMapLabels();
      renderLECanvas();
      // Hide the hint now that user has discovered the workflow
      const hint = document.getElementById('le-map-labels-hint');
      if (hint) hint.style.display = 'none';
      setStatus(`Selected: ${leSlottedTile.tile_id} — edit map labels in the right panel`);
    }
  });

  // Right-click to remove
  levelCanvas.addEventListener('contextmenu', (e) => {
    e.preventDefault();
    const { x: wx, y: wy } = getCanvasWorldPos(e);
    const idx = findPlacedTileAt(wx, wy);
    if (idx >= 0) {
      const removed = lePlacedTiles.splice(idx, 1)[0];
      renderLECanvas();
      setStatus(`Removed: ${removed.tile_id}`);
    }
  });

  // Mouse wheel zoom
  levelCanvas.addEventListener('wheel', (e) => {
    e.preventDefault();
    updateLeZoom(e.deltaY < 0 ? 0.25 : -0.25);
  }, { passive: false });

  // ESC to cancel picking
  document.addEventListener('keydown', (e) => {
    if (e.key === 'Escape' && leMode === 'picking') {
      leMode = 'free';
      updateSlotUI();
      renderLECanvas();
      setStatus('Pick cancelled');
    }
  });

  // --- Export ---
  leExportBtn.addEventListener('click', () => {
    if (!leTilesetName) { setStatus('No tileset loaded'); return; }
    if (lePlacedTiles.length === 0) { setStatus('No tiles placed'); return; }
    let tilesWithLabels = 0;
    const mapFile = {
      format: 'jigsaw',
      tileset_id: leTilesetName,
      map_width: leMapW,
      map_height: leMapH,
      cell_width: leGridCellW,
      cell_height: leGridCellH,
      tiles: lePlacedTiles.map(t => {
        const entry = { tile_id: t.tile_id, x: t.x, y: t.y, w: t.w, h: t.h };
        if (t.mapLabels && t.mapLabels.length > 0) {
          entry.labels = t.mapLabels.slice();
          tilesWithLabels++;
        }
        if (t.blocked) {
          entry.blocked = true;
        }
        return entry;
      })
    };
    const blob = new Blob([JSON.stringify(mapFile, null, 2)], { type: 'application/json' });
    const a = document.createElement('a');
    a.href = URL.createObjectURL(blob);
    a.download = `map_${leTilesetName.replace(/[/\\]/g, '_')}.json`;
    a.click(); URL.revokeObjectURL(a.href);
    setStatus(`Exported map (${lePlacedTiles.length} tiles, ${leMapW}x${leMapH} cells${tilesWithLabels > 0 ? ', ' + tilesWithLabels + ' tiles with map labels' : ''})`);
  });

  // ============================================================
  // INIT
  // ============================================================
  loadTilesetList();

})();
