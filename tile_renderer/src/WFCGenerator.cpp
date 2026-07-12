#include "WFCGenerator.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <set>
#include <cmath>

WFCResult WFCGenerator::Generate(const WFCParams& params)
{
    WFCResult result;
    result.status = WFCStatus::InvalidInput;

    // --- Validate inputs ---
    if (!params.tileset) {
        SDL_Log("[WFC] Invalid input: tileset is null");
        return result;
    }
    if (params.tileset->tiles.empty()) {
        SDL_Log("[WFC] Invalid input: tileset has zero tiles");
        return result;
    }
    if (params.width < 1 || params.width > 1024) {
        SDL_Log("[WFC] Invalid input: width %d outside range 1-1024", params.width);
        return result;
    }
    if (params.height < 1 || params.height > 1024) {
        SDL_Log("[WFC] Invalid input: height %d outside range 1-1024", params.height);
        return result;
    }

    const TilesetDef& tileset = *params.tileset;
    const int numTiles = static_cast<int>(tileset.tiles.size());

    // --- Seed RNG ---
    std::mt19937 rng;
    if (params.seed == 0) {
        std::random_device rd;
        rng.seed(rd());
    } else {
        rng.seed(params.seed);
    }

    // --- Initialize grid ---
    std::vector<std::vector<Cell>> grid(params.height, std::vector<Cell>(params.width));
    for (int r = 0; r < params.height; ++r) {
        for (int c = 0; c < params.width; ++c) {
            grid[r][c].possibilities.assign(numTiles, true);
            grid[r][c].entropy = numTiles;
            grid[r][c].collapsed = false;
        }
    }

    // --- Main loop ---
    while (true) {
        // Select minimum entropy cell
        std::pair<int, int> pos = SelectMinEntropy(grid, rng);

        // All cells collapsed -> success
        if (pos.first == -1) {
            break;
        }

        // Collapse chosen cell
        Collapse(grid[pos.first][pos.second], rng);

        // Propagate constraints
        if (!Propagate(grid, pos.first, pos.second, tileset)) {
            result.status = WFCStatus::Contradiction;
            SDL_Log("[WFC] Contradiction encountered during propagation");
            return result;
        }
    }

    // --- Build MapData on success ---
    result.status = WFCStatus::Success;
    result.map.width = params.width;
    result.map.height = params.height;
    result.map.tileset_id = tileset.name;
    result.map.grid.resize(params.height);

    for (int r = 0; r < params.height; ++r) {
        result.map.grid[r].resize(params.width);
        for (int c = 0; c < params.width; ++c) {
            // Find the single true bit
            for (int t = 0; t < numTiles; ++t) {
                if (grid[r][c].possibilities[t]) {
                    result.map.grid[r][c] = tileset.tiles[t].id;
                    break;
                }
            }
        }
    }

    return result;
}

std::pair<int, int> WFCGenerator::SelectMinEntropy(
    const std::vector<std::vector<Cell>>& grid, std::mt19937& rng) const
{
    int minEntropy = std::numeric_limits<int>::max();
    std::vector<std::pair<int, int>> candidates;

    int height = static_cast<int>(grid.size());
    int width = static_cast<int>(grid[0].size());

    for (int r = 0; r < height; ++r) {
        for (int c = 0; c < width; ++c) {
            if (grid[r][c].collapsed) {
                continue;
            }
            int e = grid[r][c].entropy;
            if (e < minEntropy) {
                minEntropy = e;
                candidates.clear();
                candidates.push_back({ r, c });
            } else if (e == minEntropy) {
                candidates.push_back({ r, c });
            }
        }
    }

    if (candidates.empty()) {
        return { -1, -1 };
    }

    // Break ties randomly
    std::uniform_int_distribution<int> dist(0, static_cast<int>(candidates.size()) - 1);
    return candidates[dist(rng)];
}

void WFCGenerator::Collapse(Cell& cell, std::mt19937& rng)
{
    // Collect indices of remaining possibilities
    std::vector<int> remaining;
    for (int i = 0; i < static_cast<int>(cell.possibilities.size()); ++i) {
        if (cell.possibilities[i]) {
            remaining.push_back(i);
        }
    }

    // Pick one randomly
    std::uniform_int_distribution<int> dist(0, static_cast<int>(remaining.size()) - 1);
    int chosen = remaining[dist(rng)];

    // Set all others to false
    cell.possibilities.assign(cell.possibilities.size(), false);
    cell.possibilities[chosen] = true;
    cell.entropy = 1;
    cell.collapsed = true;
}

bool WFCGenerator::Propagate(std::vector<std::vector<Cell>>& grid,
                             int startRow, int startCol,
                             const TilesetDef& tileset)
{
    int height = static_cast<int>(grid.size());
    int width = static_cast<int>(grid[0].size());
    int numTiles = static_cast<int>(tileset.tiles.size());

    // Direction offsets: 0=up(row-1), 1=down(row+1), 2=left(col-1), 3=right(col+1)
    static const int DR[] = { -1, 1, 0, 0 };
    static const int DC[] = { 0, 0, -1, 1 };
    // Opposite direction index: up<->down, left<->right
    static const int OPPOSITE[] = { 1, 0, 3, 2 };

    // BFS queue of cells that were modified and need to propagate
    std::queue<std::pair<int, int>> queue;
    queue.push({ startRow, startCol });

    while (!queue.empty()) {
        auto current = queue.front();
        queue.pop();
        int cr = current.first;
        int cc = current.second;

        for (int dir = 0; dir < 4; ++dir) {
            int nr = cr + DR[dir];
            int nc = cc + DC[dir];

            if (nr < 0 || nr >= height || nc < 0 || nc >= width) {
                continue;
            }

            Cell& neighbor = grid[nr][nc];
            if (neighbor.collapsed) {
                continue;
            }

            // For each neighbor tile N still possible, check if it's compatible
            // with at least one tile C still possible in the current cell.
            //
            // Compatibility (C in direction dir, N is the neighbor):
            //   - C's adjacency[dir] is empty (unconstrained) OR C's adjacency[dir] contains N.id
            //   - AND N's adjacency[opposite_dir] is empty (unconstrained) OR N's adjacency[opposite_dir] contains C.id
            //
            // N stays possible only if at least one such C exists.

            int oppDir = OPPOSITE[dir];
            bool changed = false;

            for (int nt = 0; nt < numTiles; ++nt) {
                if (!neighbor.possibilities[nt]) {
                    continue;
                }

                const TileDef& nTile = tileset.tiles[nt];
                const std::vector<std::string>* nOppList = nullptr;
                switch (oppDir) {
                    case 0: nOppList = &nTile.adjacency.up; break;
                    case 1: nOppList = &nTile.adjacency.down; break;
                    case 2: nOppList = &nTile.adjacency.left; break;
                    case 3: nOppList = &nTile.adjacency.right; break;
                }

                // Check if any current cell tile supports this neighbor tile
                bool supported = false;
                for (int ct = 0; ct < numTiles && !supported; ++ct) {
                    if (!grid[cr][cc].possibilities[ct]) {
                        continue;
                    }

                    const TileDef& cTile = tileset.tiles[ct];
                    const std::vector<std::string>* cDirList = nullptr;
                    switch (dir) {
                        case 0: cDirList = &cTile.adjacency.up; break;
                        case 1: cDirList = &cTile.adjacency.down; break;
                        case 2: cDirList = &cTile.adjacency.left; break;
                        case 3: cDirList = &cTile.adjacency.right; break;
                    }

                    // Check C allows N in this direction
                    bool cAllowsN = false;
                    if (cDirList->empty()) {
                        cAllowsN = true; // unconstrained
                    } else {
                        for (const std::string& id : *cDirList) {
                            if (id == nTile.id) { cAllowsN = true; break; }
                        }
                    }
                    if (!cAllowsN) continue;

                    // Check N allows C in the opposite direction (reciprocity)
                    bool nAllowsC = false;
                    if (nOppList->empty()) {
                        nAllowsC = true; // unconstrained
                    } else {
                        for (const std::string& id : *nOppList) {
                            if (id == cTile.id) { nAllowsC = true; break; }
                        }
                    }

                    if (nAllowsC) {
                        supported = true;
                    }
                }

                if (!supported) {
                    neighbor.possibilities[nt] = false;
                    neighbor.entropy--;
                    changed = true;
                }
            }

            // Check for contradiction
            if (neighbor.entropy <= 0) {
                return false;
            }

            if (changed) {
                queue.push({ nr, nc });
            }
        }
    }

    return true;
}


// =============================================================================
// Jigsaw WFC Implementation — Gap-Queue Algorithm with Backtracking
// =============================================================================

static const float EPSILON = 0.001f;

std::vector<size_t> WFCGenerator::GetCandidates(
    const Gap& gap,
    const TilesetDef& tileset,
    const JigsawMap& placed,
    float sheetScale, float layerScale) const
{
    std::vector<size_t> candidates;
    const int numTiles = static_cast<int>(tileset.tiles.size());

    for (int i = 0; i < numTiles; ++i) {
        const TileDef& def = tileset.tiles[i];

        // Compute effective size for this tile
        auto eff = ComputeEffectiveSize(
            static_cast<float>(def.source_rect.w),
            static_cast<float>(def.source_rect.h),
            def.scale, sheetScale, layerScale);
        float ew = eff.first;
        float eh = eff.second;

        // Tile must fit within the gap (with tolerance)
        if (ew > gap.w + EPSILON || eh > gap.h + EPSILON) {
            continue;
        }

        // Construct a hypothetical placed tile at the gap's position
        PlacedTile hypothetical;
        hypothetical.tile_id = def.id;
        hypothetical.x = gap.x;
        hypothetical.y = gap.y;
        hypothetical.w = ew;
        hypothetical.h = eh;

        // Validate adjacency with already-placed neighbors
        if (!ValidateAdjacency(hypothetical, def, placed, tileset)) {
            continue;
        }

        candidates.push_back(static_cast<size_t>(i));
    }

    return candidates;
}

bool WFCGenerator::ValidateAdjacency(
    const PlacedTile& candidate,
    const TileDef& candidateDef,
    const JigsawMap& placed,
    const TilesetDef& tileset) const
{
    // Get edge neighbors of the hypothetical placement
    std::vector<const PlacedTile*> neighbors = placed.GetEdgeNeighbors(candidate);

    for (const PlacedTile* neighbor : neighbors) {
        // Find the neighbor's TileDef
        auto it = tileset.id_index.find(neighbor->tile_id);
        if (it == tileset.id_index.end()) {
            continue;  // unknown tile, skip (shouldn't happen)
        }
        const TileDef& neighborDef = tileset.tiles[it->second];

        // Determine the direction relationship between candidate and neighbor
        // Check if neighbor is to the LEFT of candidate
        if (std::fabs((neighbor->x + neighbor->w) - candidate.x) < EPSILON) {
            // neighbor's right edge touches candidate's left edge
            // Check overlap in Y
            float overlapY = std::min(neighbor->y + neighbor->h, candidate.y + candidate.h)
                           - std::max(neighbor->y, candidate.y);
            if (overlapY > EPSILON) {
                // neighbor.right must allow candidate
                const std::vector<std::string>& nRight = neighborDef.adjacency.right;
                if (!nRight.empty()) {
                    bool found = false;
                    for (const std::string& id : nRight) {
                        if (id == candidateDef.id) { found = true; break; }
                    }
                    if (!found) return false;
                }
                // candidate.left must allow neighbor
                const std::vector<std::string>& cLeft = candidateDef.adjacency.left;
                if (!cLeft.empty()) {
                    bool found = false;
                    for (const std::string& id : cLeft) {
                        if (id == neighborDef.id) { found = true; break; }
                    }
                    if (!found) return false;
                }
            }
        }

        // Check if neighbor is to the RIGHT of candidate
        if (std::fabs((candidate.x + candidate.w) - neighbor->x) < EPSILON) {
            float overlapY = std::min(neighbor->y + neighbor->h, candidate.y + candidate.h)
                           - std::max(neighbor->y, candidate.y);
            if (overlapY > EPSILON) {
                // candidate.right must allow neighbor
                const std::vector<std::string>& cRight = candidateDef.adjacency.right;
                if (!cRight.empty()) {
                    bool found = false;
                    for (const std::string& id : cRight) {
                        if (id == neighborDef.id) { found = true; break; }
                    }
                    if (!found) return false;
                }
                // neighbor.left must allow candidate
                const std::vector<std::string>& nLeft = neighborDef.adjacency.left;
                if (!nLeft.empty()) {
                    bool found = false;
                    for (const std::string& id : nLeft) {
                        if (id == candidateDef.id) { found = true; break; }
                    }
                    if (!found) return false;
                }
            }
        }

        // Check if neighbor is ABOVE candidate
        if (std::fabs((neighbor->y + neighbor->h) - candidate.y) < EPSILON) {
            float overlapX = std::min(neighbor->x + neighbor->w, candidate.x + candidate.w)
                           - std::max(neighbor->x, candidate.x);
            if (overlapX > EPSILON) {
                // neighbor.down must allow candidate
                const std::vector<std::string>& nDown = neighborDef.adjacency.down;
                if (!nDown.empty()) {
                    bool found = false;
                    for (const std::string& id : nDown) {
                        if (id == candidateDef.id) { found = true; break; }
                    }
                    if (!found) return false;
                }
                // candidate.up must allow neighbor
                const std::vector<std::string>& cUp = candidateDef.adjacency.up;
                if (!cUp.empty()) {
                    bool found = false;
                    for (const std::string& id : cUp) {
                        if (id == neighborDef.id) { found = true; break; }
                    }
                    if (!found) return false;
                }
            }
        }

        // Check if neighbor is BELOW candidate
        if (std::fabs((candidate.y + candidate.h) - neighbor->y) < EPSILON) {
            float overlapX = std::min(neighbor->x + neighbor->w, candidate.x + candidate.w)
                           - std::max(neighbor->x, candidate.x);
            if (overlapX > EPSILON) {
                // candidate.down must allow neighbor
                const std::vector<std::string>& cDown = candidateDef.adjacency.down;
                if (!cDown.empty()) {
                    bool found = false;
                    for (const std::string& id : cDown) {
                        if (id == neighborDef.id) { found = true; break; }
                    }
                    if (!found) return false;
                }
                // neighbor.up must allow candidate
                const std::vector<std::string>& nUp = neighborDef.adjacency.up;
                if (!nUp.empty()) {
                    bool found = false;
                    for (const std::string& id : nUp) {
                        if (id == candidateDef.id) { found = true; break; }
                    }
                    if (!found) return false;
                }
            }
        }
    }

    return true;
}

std::vector<Gap> WFCGenerator::SplitGap(const Gap& original, const PlacedTile& placed) const
{
    std::vector<Gap> subGaps;

    float rightW = original.w - placed.w;
    float bottomH = original.h - placed.h;

    // Right remainder: to the right of the placed tile, same height as placed tile
    if (rightW > EPSILON) {
        Gap right;
        right.x = original.x + placed.w;
        right.y = original.y;
        right.w = rightW;
        right.h = placed.h;
        subGaps.push_back(right);
    }

    // Bottom remainder: below the placed tile, full width of original gap
    if (bottomH > EPSILON) {
        Gap bottom;
        bottom.x = original.x;
        bottom.y = original.y + placed.h;
        bottom.w = original.w;
        bottom.h = bottomH;
        subGaps.push_back(bottom);
    }

    return subGaps;
}

JigsawWFCResult WFCGenerator::GenerateJigsaw(const JigsawWFCParams& params)
{
    JigsawWFCResult result;
    result.status = WFCStatus::InvalidInput;

    // --- Validate inputs ---
    if (!params.tileset) {
        SDL_Log("[JigsawWFC] Invalid input: tileset is null");
        return result;
    }
    if (params.tileset->tiles.empty()) {
        SDL_Log("[JigsawWFC] Invalid input: tileset has zero tiles");
        return result;
    }
    if (params.target_width <= 0.0f || params.target_height <= 0.0f) {
        SDL_Log("[JigsawWFC] Invalid input: target area must be > 0");
        return result;
    }

    const TilesetDef& tileset = *params.tileset;
    float sheetScale = tileset.sheet_scale;
    float layerScale = params.layer_scale;

    // --- Seed RNG ---
    std::mt19937 rng;
    if (params.seed == 0) {
        std::random_device rd;
        rng.seed(rd());
    } else {
        rng.seed(params.seed);
    }

    // --- Set up the map with boundary ---
    result.map.SetTilesetId(tileset.name);
    MapBoundary boundary;
    boundary.width_pixels = params.target_width;
    boundary.height_pixels = params.target_height;
    result.map.SetBoundary(boundary);

    // --- Initialize gap queue with one gap = entire target area ---
    GapQueue gapQueue;
    Gap initialGap;
    initialGap.x = params.origin_x;
    initialGap.y = params.origin_y;
    initialGap.w = params.target_width;
    initialGap.h = params.target_height;
    gapQueue.push(initialGap);

    // --- Backtracking state ---
    // Simple approach: for each gap, try all candidates. If all fail, contradiction.
    // A snapshot records the state before a placement so we can undo it.
    struct Snapshot {
        Gap gap;
        std::vector<size_t> remainingCandidates;
        GapQueue savedQueue;
        size_t tileCountBefore;
    };
    std::vector<Snapshot> backtrackStack;

    static const int MAX_BACKTRACKS = 100;
    int backtrackCount = 0;

    // --- Main loop ---
    while (!gapQueue.empty()) {
        Gap currentGap = gapQueue.top();
        gapQueue.pop();

        // Skip negligibly small gaps (floating point remnants)
        if (currentGap.w < EPSILON || currentGap.h < EPSILON) {
            continue;
        }

        // Get candidates for this gap
        std::vector<size_t> candidates = GetCandidates(
            currentGap, tileset, result.map, sheetScale, layerScale);

        if (candidates.empty()) {
            // Try backtracking
            bool recovered = false;
            while (!backtrackStack.empty() && backtrackCount < MAX_BACKTRACKS) {
                Snapshot& snap = backtrackStack.back();

                if (snap.remainingCandidates.empty()) {
                    // No more alternatives at this level, pop further
                    backtrackStack.pop_back();
                    continue;
                }

                // Undo: remove tiles placed after this snapshot
                while (result.map.GetTileCount() > snap.tileCountBefore) {
                    const auto& allTiles = result.map.GetAllTiles();
                    if (allTiles.empty()) break;
                    const PlacedTile& last = allTiles.back();
                    result.map.RemoveTile(last.x, last.y);
                }

                // Restore the gap queue
                gapQueue = snap.savedQueue;

                // Try next candidate
                backtrackCount++;
                size_t nextIdx = snap.remainingCandidates.back();
                snap.remainingCandidates.pop_back();

                const TileDef& def = tileset.tiles[nextIdx];
                auto eff = ComputeEffectiveSize(
                    static_cast<float>(def.source_rect.w),
                    static_cast<float>(def.source_rect.h),
                    def.scale, sheetScale, layerScale);

                PlacedTile tile;
                tile.tile_id = def.id;
                tile.x = snap.gap.x;
                tile.y = snap.gap.y;
                tile.w = eff.first;
                tile.h = eff.second;

                if (result.map.AddTile(tile)) {
                    // Split remaining gap
                    std::vector<Gap> subGaps = SplitGap(snap.gap, tile);
                    for (const Gap& sg : subGaps) {
                        gapQueue.push(sg);
                    }
                    recovered = true;
                    break;
                }
                // If AddTile failed (overlap), try next candidate
            }

            if (!recovered) {
                result.status = WFCStatus::Contradiction;
                SDL_Log("[JigsawWFC] Contradiction: no candidates for gap at (%.1f, %.1f) size %.1f x %.1f",
                        currentGap.x, currentGap.y, currentGap.w, currentGap.h);
                return result;
            }
            continue;
        }

        // Shuffle candidates for randomness
        std::shuffle(candidates.begin(), candidates.end(), rng);

        // Pick the first candidate
        size_t chosenIdx = candidates[0];
        const TileDef& chosenDef = tileset.tiles[chosenIdx];
        auto eff = ComputeEffectiveSize(
            static_cast<float>(chosenDef.source_rect.w),
            static_cast<float>(chosenDef.source_rect.h),
            chosenDef.scale, sheetScale, layerScale);

        PlacedTile placedTile;
        placedTile.tile_id = chosenDef.id;
        placedTile.x = currentGap.x;
        placedTile.y = currentGap.y;
        placedTile.w = eff.first;
        placedTile.h = eff.second;

        // Save snapshot for backtracking (remaining candidates minus the chosen one)
        Snapshot snap;
        snap.gap = currentGap;
        snap.remainingCandidates.assign(candidates.begin() + 1, candidates.end());
        snap.savedQueue = gapQueue;
        snap.tileCountBefore = result.map.GetTileCount();
        backtrackStack.push_back(snap);

        // Place the tile
        if (!result.map.AddTile(placedTile)) {
            // Overlap detected — shouldn't happen if gap tracking is correct, but handle gracefully
            // Remove this snapshot and try next candidate via backtracking
            backtrackStack.pop_back();
            // Re-push this gap to try again
            gapQueue.push(currentGap);
            continue;
        }

        // Split remaining gap into sub-gaps
        std::vector<Gap> subGaps = SplitGap(currentGap, placedTile);
        for (const Gap& sg : subGaps) {
            gapQueue.push(sg);
        }
    }

    // --- Success: all gaps filled ---
    result.status = WFCStatus::Success;
    SDL_Log("[JigsawWFC] Success: placed %zu tiles", result.map.GetTileCount());
    return result;
}
