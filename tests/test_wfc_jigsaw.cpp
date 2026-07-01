// Feature: jigsaw-tile-layout
// Property-based tests for WFC Jigsaw Generator
// Properties 4, 6, 7 validated using RapidCheck

#include <rapidcheck.h>
#include "WFCGenerator.h"
#include "EffectiveSize.h"
#include "TilesetLoader.h"
#include "JigsawMap.h"

#include <cmath>
#include <algorithm>
#include <set>

static const float EPSILON = 0.001f;

// ============================================================================
// Test helpers: build simple tilesets for property testing
// ============================================================================

// Creates a minimal tileset with uniform-size tiles and unconstrained adjacency.
// All tiles are square with the given pixel dimension.
static TilesetDef MakeUniformTileset(int tilePixels, int numTiles) {
    TilesetDef ts;
    ts.name = "test_uniform";
    ts.sheet_scale = 1.0f;
    ts.texture_width = tilePixels * numTiles;
    ts.texture_height = tilePixels;

    for (int i = 0; i < numTiles; ++i) {
        TileDef def;
        def.id = "tile_" + std::to_string(i);
        def.source_rect.x = i * tilePixels;
        def.source_rect.y = 0;
        def.source_rect.w = tilePixels;
        def.source_rect.h = tilePixels;
        def.scale = 1.0f;
        // Empty adjacency lists = unconstrained (wildcard)
        ts.tiles.push_back(def);
        ts.id_index[def.id] = static_cast<size_t>(i);
    }
    return ts;
}

// Creates a tileset with variable-size tiles (widths and heights from the given arrays).
// All tiles have unconstrained adjacency.
static TilesetDef MakeVariableTileset(const std::vector<std::pair<int, int>>& sizes) {
    TilesetDef ts;
    ts.name = "test_variable";
    ts.sheet_scale = 1.0f;
    ts.texture_width = 1024;
    ts.texture_height = 1024;

    for (size_t i = 0; i < sizes.size(); ++i) {
        TileDef def;
        def.id = "tile_" + std::to_string(i);
        def.source_rect.x = 0;
        def.source_rect.y = 0;
        def.source_rect.w = sizes[i].first;
        def.source_rect.h = sizes[i].second;
        def.scale = 1.0f;
        ts.tiles.push_back(def);
        ts.id_index[def.id] = i;
    }
    return ts;
}

// Creates a tileset with specific adjacency constraints for testing.
// Two tile types: "A" can have "B" to the right, "B" can have "A" to the left.
static TilesetDef MakeConstrainedTileset(int tilePixels) {
    TilesetDef ts;
    ts.name = "test_constrained";
    ts.sheet_scale = 1.0f;
    ts.texture_width = tilePixels * 2;
    ts.texture_height = tilePixels;

    // Tile A: right=B, down=A|B, left=B, up=A|B
    TileDef a;
    a.id = "A";
    a.source_rect = {0, 0, tilePixels, tilePixels};
    a.scale = 1.0f;
    a.adjacency.right = {"B"};
    a.adjacency.left = {"B"};
    a.adjacency.up = {"A", "B"};
    a.adjacency.down = {"A", "B"};
    ts.tiles.push_back(a);
    ts.id_index["A"] = 0;

    // Tile B: right=A, down=A|B, left=A, up=A|B
    TileDef b;
    b.id = "B";
    b.source_rect = {tilePixels, 0, tilePixels, tilePixels};
    b.scale = 1.0f;
    b.adjacency.right = {"A"};
    b.adjacency.left = {"A"};
    b.adjacency.up = {"A", "B"};
    b.adjacency.down = {"A", "B"};
    ts.tiles.push_back(b);
    ts.id_index["B"] = 1;

    return ts;
}

// ============================================================================
// Property 4: Gap-Free Coverage for Finite Maps
// ============================================================================
// For any successfully generated finite map (status == Success) with boundary (W, H),
// for any point (px, py) where 0 <= px < W and 0 <= py < H, there SHALL exist at
// least one placed tile T such that T.x <= px < T.x + T.w and T.y <= py < T.y + T.h.
//
// **Validates: Requirements 4.4, 7.1, 7.3**
// ============================================================================

// Feature: jigsaw-tile-layout, Property 4: Gap-Free Coverage for Finite Maps
static void testProperty4_GapFreeCoverage() {
    rc::check("Property 4: Gap-Free Coverage — every interior point is covered by a tile",
        []() {
            // Generate parameters: small area with uniform tiles for tractability
            const int tileSize = *rc::gen::inRange(16, 65);
            const int numTiles = *rc::gen::inRange(1, 5);
            // Target area is a multiple of tile size to guarantee coverage is possible
            const int gridW = *rc::gen::inRange(1, 6);
            const int gridH = *rc::gen::inRange(1, 6);
            const float targetW = static_cast<float>(gridW * tileSize);
            const float targetH = static_cast<float>(gridH * tileSize);
            const unsigned int seed = *rc::gen::inRange(1, 100000);

            TilesetDef tileset = MakeUniformTileset(tileSize, numTiles);

            JigsawWFCParams params;
            params.target_width = targetW;
            params.target_height = targetH;
            params.origin_x = 0.0f;
            params.origin_y = 0.0f;
            params.seed = seed;
            params.tileset = &tileset;
            params.layer_scale = 1.0f;

            WFCGenerator gen;
            JigsawWFCResult result = gen.GenerateJigsaw(params);

            // Only check coverage property if generation succeeded
            RC_PRE(result.status == WFCStatus::Success);

            // Sample random points within the boundary
            const int numSamples = 20;
            for (int s = 0; s < numSamples; ++s) {
                float px = *rc::gen::inRange(0, static_cast<int>(targetW));
                float py = *rc::gen::inRange(0, static_cast<int>(targetH));

                // Check that some tile covers this point
                const PlacedTile* hit = result.map.QueryPoint(px, py);
                RC_ASSERT(hit != nullptr);
            }
        });
}

// ============================================================================
// Property 6: Adjacency Validation
// ============================================================================
// For any pair of tiles in a successfully generated JigsawMap that share a
// non-zero-length boundary segment, the adjacency rules of both tiles SHALL be
// satisfied.
//
// **Validates: Requirements 5.1, 5.2, 5.3, 5.4, 12.1, 12.2**
// ============================================================================

// Feature: jigsaw-tile-layout, Property 6: Adjacency Validation
static void testProperty6_AdjacencyValidation() {
    rc::check("Property 6: Adjacency Validation — all placed neighbor pairs satisfy adjacency rules",
        []() {
            const int tileSize = *rc::gen::inRange(16, 65);
            const int gridW = *rc::gen::inRange(2, 5);
            const int gridH = *rc::gen::inRange(2, 5);
            const float targetW = static_cast<float>(gridW * tileSize);
            const float targetH = static_cast<float>(gridH * tileSize);
            const unsigned int seed = *rc::gen::inRange(1, 100000);

            // Use constrained tileset to test real adjacency enforcement
            TilesetDef tileset = MakeConstrainedTileset(tileSize);

            JigsawWFCParams params;
            params.target_width = targetW;
            params.target_height = targetH;
            params.origin_x = 0.0f;
            params.origin_y = 0.0f;
            params.seed = seed;
            params.tileset = &tileset;
            params.layer_scale = 1.0f;

            WFCGenerator gen;
            JigsawWFCResult result = gen.GenerateJigsaw(params);

            RC_PRE(result.status == WFCStatus::Success);

            const auto& allTiles = result.map.GetAllTiles();

            // For each placed tile, get its edge neighbors and verify adjacency
            for (const PlacedTile& tile : allTiles) {
                auto neighbors = result.map.GetEdgeNeighbors(tile);

                // Find this tile's definition
                auto tileIt = tileset.id_index.find(tile.tile_id);
                RC_ASSERT(tileIt != tileset.id_index.end());
                const TileDef& tileDef = tileset.tiles[tileIt->second];

                for (const PlacedTile* neighbor : neighbors) {
                    auto neighborIt = tileset.id_index.find(neighbor->tile_id);
                    RC_ASSERT(neighborIt != tileset.id_index.end());
                    const TileDef& neighborDef = tileset.tiles[neighborIt->second];

                    // Determine direction relationship
                    // Is neighbor to the RIGHT of tile?
                    if (std::fabs((tile.x + tile.w) - neighbor->x) < EPSILON) {
                        float overlapY = std::min(tile.y + tile.h, neighbor->y + neighbor->h)
                                       - std::max(tile.y, neighbor->y);
                        if (overlapY > EPSILON) {
                            // tile.right must allow neighbor (or be unconstrained)
                            if (!tileDef.adjacency.right.empty()) {
                                bool found = false;
                                for (const auto& id : tileDef.adjacency.right) {
                                    if (id == neighborDef.id) { found = true; break; }
                                }
                                RC_ASSERT(found);
                            }
                            // neighbor.left must allow tile (or be unconstrained)
                            if (!neighborDef.adjacency.left.empty()) {
                                bool found = false;
                                for (const auto& id : neighborDef.adjacency.left) {
                                    if (id == tileDef.id) { found = true; break; }
                                }
                                RC_ASSERT(found);
                            }
                        }
                    }

                    // Is neighbor to the LEFT of tile?
                    if (std::fabs((neighbor->x + neighbor->w) - tile.x) < EPSILON) {
                        float overlapY = std::min(tile.y + tile.h, neighbor->y + neighbor->h)
                                       - std::max(tile.y, neighbor->y);
                        if (overlapY > EPSILON) {
                            if (!tileDef.adjacency.left.empty()) {
                                bool found = false;
                                for (const auto& id : tileDef.adjacency.left) {
                                    if (id == neighborDef.id) { found = true; break; }
                                }
                                RC_ASSERT(found);
                            }
                            if (!neighborDef.adjacency.right.empty()) {
                                bool found = false;
                                for (const auto& id : neighborDef.adjacency.right) {
                                    if (id == tileDef.id) { found = true; break; }
                                }
                                RC_ASSERT(found);
                            }
                        }
                    }

                    // Is neighbor BELOW tile?
                    if (std::fabs((tile.y + tile.h) - neighbor->y) < EPSILON) {
                        float overlapX = std::min(tile.x + tile.w, neighbor->x + neighbor->w)
                                       - std::max(tile.x, neighbor->x);
                        if (overlapX > EPSILON) {
                            if (!tileDef.adjacency.down.empty()) {
                                bool found = false;
                                for (const auto& id : tileDef.adjacency.down) {
                                    if (id == neighborDef.id) { found = true; break; }
                                }
                                RC_ASSERT(found);
                            }
                            if (!neighborDef.adjacency.up.empty()) {
                                bool found = false;
                                for (const auto& id : neighborDef.adjacency.up) {
                                    if (id == tileDef.id) { found = true; break; }
                                }
                                RC_ASSERT(found);
                            }
                        }
                    }

                    // Is neighbor ABOVE tile?
                    if (std::fabs((neighbor->y + neighbor->h) - tile.y) < EPSILON) {
                        float overlapX = std::min(tile.x + tile.w, neighbor->x + neighbor->w)
                                       - std::max(tile.x, neighbor->x);
                        if (overlapX > EPSILON) {
                            if (!tileDef.adjacency.up.empty()) {
                                bool found = false;
                                for (const auto& id : tileDef.adjacency.up) {
                                    if (id == neighborDef.id) { found = true; break; }
                                }
                                RC_ASSERT(found);
                            }
                            if (!neighborDef.adjacency.down.empty()) {
                                bool found = false;
                                for (const auto& id : neighborDef.adjacency.down) {
                                    if (id == tileDef.id) { found = true; break; }
                                }
                                RC_ASSERT(found);
                            }
                        }
                    }
                }
            }
        });
}

// ============================================================================
// Property 7: Deterministic Generation
// ============================================================================
// For any valid JigsawWFCParams with seed > 0, calling GenerateJigsaw twice with
// identical parameters SHALL produce identical JigsawWFCResult instances.
//
// **Validates: Requirements 6.5**
// ============================================================================

// Feature: jigsaw-tile-layout, Property 7: Deterministic Generation
static void testProperty7_DeterministicGeneration() {
    rc::check("Property 7: Deterministic Generation — same seed produces same output",
        []() {
            const int tileSize = *rc::gen::inRange(16, 65);
            const int numTiles = *rc::gen::inRange(1, 5);
            const int gridW = *rc::gen::inRange(1, 5);
            const int gridH = *rc::gen::inRange(1, 5);
            const float targetW = static_cast<float>(gridW * tileSize);
            const float targetH = static_cast<float>(gridH * tileSize);
            // seed > 0 for determinism
            const unsigned int seed = *rc::gen::inRange(1, 100000);

            TilesetDef tileset = MakeUniformTileset(tileSize, numTiles);

            JigsawWFCParams params;
            params.target_width = targetW;
            params.target_height = targetH;
            params.origin_x = 0.0f;
            params.origin_y = 0.0f;
            params.seed = seed;
            params.tileset = &tileset;
            params.layer_scale = 1.0f;

            WFCGenerator gen1;
            JigsawWFCResult result1 = gen1.GenerateJigsaw(params);

            WFCGenerator gen2;
            JigsawWFCResult result2 = gen2.GenerateJigsaw(params);

            // Status must match
            RC_ASSERT(result1.status == result2.status);

            if (result1.status == WFCStatus::Success) {
                const auto& tiles1 = result1.map.GetAllTiles();
                const auto& tiles2 = result2.map.GetAllTiles();

                // Same number of tiles
                RC_ASSERT(tiles1.size() == tiles2.size());

                // Each tile must match exactly
                for (size_t i = 0; i < tiles1.size(); ++i) {
                    RC_ASSERT(tiles1[i].tile_id == tiles2[i].tile_id);
                    RC_ASSERT(std::fabs(tiles1[i].x - tiles2[i].x) < EPSILON);
                    RC_ASSERT(std::fabs(tiles1[i].y - tiles2[i].y) < EPSILON);
                    RC_ASSERT(std::fabs(tiles1[i].w - tiles2[i].w) < EPSILON);
                    RC_ASSERT(std::fabs(tiles1[i].h - tiles2[i].h) < EPSILON);
                }
            }
        });
}

// ============================================================================
// Main entry point
// ============================================================================

int main() {
    // Property 4: Gap-Free Coverage for Finite Maps
    // **Validates: Requirements 4.4, 7.1, 7.3**
    testProperty4_GapFreeCoverage();

    // Property 6: Adjacency Validation
    // **Validates: Requirements 5.1, 5.2, 5.3, 5.4, 12.1, 12.2**
    testProperty6_AdjacencyValidation();

    // Property 7: Deterministic Generation
    // **Validates: Requirements 6.5**
    testProperty7_DeterministicGeneration();

    return 0;
}
