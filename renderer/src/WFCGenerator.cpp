#include "WFCGenerator.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <set>

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
