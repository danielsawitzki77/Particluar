#include "SubdivisionSolver.h"
#include "FaceGenerator.h"
#include "ConnectionFaceMatcher.h"
#include "ConnectionSolver.h"
#include "BodyTypes.h"

#include <cmath>
#include <cstdio>
#include <algorithm>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace BodyRenderer {

// ============================================================================
// Internal helpers
// ============================================================================

// Computes the minimum N such that every connection position lands exactly
// on a face center, guaranteeing alignment by construction.
static int ComputeRequiredSegments(const std::vector<float>& positions, int min_segments)
{
    if (positions.empty()) return min_segments;
    for (int N = min_segments; N <= 128; ++N) {
        bool all_valid = true;
        for (float u : positions) {
            // u=0.0 and u=1.0 always land near face 0 center for any N — skip check
            if (u < 0.001f || u > 0.999f) continue;
            float face_pos = N * u - 0.5f;
            float rounded = std::round(face_pos);
            if (std::fabs(face_pos - rounded) > 0.01f) {
                all_valid = false;
                break;
            }
        }
        if (all_valid) return N;
    }
    return min_segments; // fallback
}

// For sphere latitude: face centers are at (row + 1.5) / T for mid-band quads,
// and pole triangles occupy [0, 1/T] and [(T-1)/T, 1].
static int ComputeRequiredSphereLat(const std::vector<float>& v_positions, int min_segments)
{
    if (v_positions.empty()) return min_segments;
    for (int T = min_segments; T <= 128; ++T) {
        bool all_valid = true;
        for (float v : v_positions) {
            float stack_f = v * T;
            // Check if it lands in pole region (which is fine — pole tris just need col)
            if (stack_f < 1.0f || stack_f >= static_cast<float>(T - 1)) {
                continue; // pole connections always align
            }
            // Mid-band: face center at (row + 1.5) where row = int(stack_f) - 1
            // So we need stack_f - 0.5 to be integer
            float face_pos = stack_f - 0.5f;
            float rounded = std::round(face_pos);
            if (std::fabs(face_pos - rounded) > 0.01f) {
                all_valid = false;
                break;
            }
        }
        if (all_valid) return T;
    }
    return min_segments;
}

static void SetShapeAngularSegments(ShapeParams& shape, int segs)
{
    switch (shape.type) {
    case ShapeType::Cylinder:
    case ShapeType::Capsule:
    case ShapeType::Cone:
        shape.segments = segs;
        break;
    case ShapeType::Sphere:
        shape.lon_segments = segs;
        break;
    case ShapeType::Torus:
        shape.side_segments = segs;
        break;
    }
}

static int GetShapeAngularSegments(const ShapeParams& shape)
{
    switch (shape.type) {
    case ShapeType::Cylinder:
    case ShapeType::Capsule:
    case ShapeType::Cone:
        return shape.segments;
    case ShapeType::Sphere:
        return shape.lon_segments;
    case ShapeType::Torus:
        return shape.side_segments;
    }
    return 8;
}

static void DeriveSegmentsRecursive(BodyNode& node, int base_res, int forced_angular = 0)
{
    // Collect all connection u-positions on this node (from children connecting to its side/surface)
    std::vector<float> side_u_positions;
    std::vector<float> side_v_positions;
    for (const auto& child : node.children) {
        if (child.connection.is_legacy) continue;
        if (child.connection.parent_attach.region == AttachRegion::Side ||
            child.connection.parent_attach.region == AttachRegion::Surface) {
            side_u_positions.push_back(child.connection.parent_attach.u);
            side_v_positions.push_back(child.connection.parent_attach.v);
        }
    }

    // Compute angular segments needed for this shape based on connection positions
    int min_angular = (std::max)(base_res, forced_angular);
    int required_angular = ComputeRequiredSegments(side_u_positions, min_angular);

    // Set the shape's angular segments
    SetShapeAngularSegments(node.shape, required_angular);

    // Also set lat_segments for spheres and ring_segments for tori based on v-positions
    if (node.shape.type == ShapeType::Sphere) {
        node.shape.lat_segments = ComputeRequiredSphereLat(side_v_positions, (std::max)(3, base_res));
    } else if (node.shape.type == ShapeType::Torus) {
        node.shape.ring_segments = ComputeRequiredSegments(side_u_positions, (std::max)(3, base_res));
        node.shape.side_segments = ComputeRequiredSegments(side_v_positions, (std::max)(3, base_res));
    } else if (node.shape.type == ShapeType::Cylinder ||
               node.shape.type == ShapeType::Capsule ||
               node.shape.type == ShapeType::Cone) {
        if (!side_v_positions.empty()) {
            node.shape.height_segments = ComputeRequiredSegments(side_v_positions, (std::max)(1, base_res / 4));
        }
    }

    // For each child: ensure child uses compatible segments at the connection boundary
    for (auto& child : node.children) {
        if (child.connection.is_legacy) {
            DeriveSegmentsRecursive(child, base_res, 0);
            continue;
        }

        // Child's angular segments must be at least parent's
        // For torus parents: the child's angular dimension maps to the torus's
        // ring direction, so use ring_segments as the forced minimum (not side_segments
        // which GetShapeAngularSegments returns for torus).
        int parent_segs;
        if (node.shape.type == ShapeType::Torus &&
            (child.connection.parent_attach.region == AttachRegion::Surface)) {
            parent_segs = node.shape.ring_segments;
        } else {
            parent_segs = GetShapeAngularSegments(node.shape);
        }

        // Set child lat_segments / ring_segments from base_res
        if (child.shape.type == ShapeType::Sphere) {
            child.shape.lat_segments = (std::max)(3, base_res);
        } else if (child.shape.type == ShapeType::Torus) {
            child.shape.ring_segments = (std::max)(3, base_res);
        }

        // If child connects via its side, compute height_segments from v-positions
        if (child.connection.child_attach.region == AttachRegion::Side) {
            std::vector<float> child_v_positions;
            child_v_positions.push_back(child.connection.child_attach.v);
            for (const auto& grandchild : child.children) {
                if (grandchild.connection.is_legacy) continue;
                if (grandchild.connection.parent_attach.region == AttachRegion::Side ||
                    grandchild.connection.parent_attach.region == AttachRegion::Surface) {
                    child_v_positions.push_back(grandchild.connection.parent_attach.v);
                }
            }
            int h_segs = ComputeRequiredSegments(child_v_positions, (std::max)(1, base_res / 4));
            child.shape.height_segments = h_segs;
        } else {
            child.shape.height_segments = (std::max)(1, base_res / 4);
        }

        // Special case: cylinder/capsule/cone side connects to a torus surface
        // The torus needs enough ring_segments so its face width matches the
        // cylinder's face width: 2π*major/ring_segs = 2π*cyl_radius/cyl_segs
        // => ring_segs = major_radius / cyl_radius * cyl_segs
        if (child.connection.parent_attach.region == AttachRegion::Surface &&
            node.shape.type == ShapeType::Torus) {
            if (child.shape.type == ShapeType::Cylinder ||
                child.shape.type == ShapeType::Capsule ||
                child.shape.type == ShapeType::Cone) {
                if (child.connection.child_attach.region == AttachRegion::Side) {
                    child.shape.height_segments = node.shape.side_segments;
                    // Compute ring_segments needed for face width matching
                    float child_radius = child.shape.radius;
                    if (child_radius > 0.001f) {
                        int needed_ring = static_cast<int>(
                            std::ceil(node.shape.major_radius / child_radius * child.shape.segments));
                        if (needed_ring < node.shape.ring_segments) needed_ring = node.shape.ring_segments;
                        if (needed_ring > 128) needed_ring = 128;
                        node.shape.ring_segments = needed_ring;
                    }
                    child.shape.segments = node.shape.ring_segments;
                }
            }
        }

        // Sync cylinder height_segments with sphere lat_segments at side<->surface connections
        if (child.connection.child_attach.region == AttachRegion::Side) {
            if (node.shape.type == ShapeType::Sphere) {
                int synced = (std::max)(node.shape.lat_segments, child.shape.height_segments);
                node.shape.lat_segments = synced;
                child.shape.height_segments = synced;

                // Compute non-uniform row boundaries so cylinder rows match sphere latitude bands
                int H = child.shape.height_segments;
                std::vector<float> boundaries;
                boundaries.reserve(H + 1);
                for (int k = 0; k <= H; ++k) {
                    float phi = static_cast<float>(M_PI) * (H - k) / H;
                    float t = (1.0f + std::cos(phi)) * 0.5f;
                    boundaries.push_back(t);
                }
                child.shape.row_boundaries = boundaries;
            }
        }
        if (child.connection.parent_attach.region == AttachRegion::Side ||
            child.connection.parent_attach.region == AttachRegion::Surface) {
            if (child.shape.type == ShapeType::Sphere) {
                int synced = (std::max)(child.shape.lat_segments, node.shape.height_segments);
                child.shape.lat_segments = synced;
                node.shape.height_segments = synced;

                // Compute non-uniform row boundaries so parent cylinder rows match sphere latitude bands
                int H = node.shape.height_segments;
                std::vector<float> boundaries;
                boundaries.reserve(H + 1);
                for (int k = 0; k <= H; ++k) {
                    float phi = static_cast<float>(M_PI) * (H - k) / H;
                    float t = (1.0f + std::cos(phi)) * 0.5f;
                    boundaries.push_back(t);
                }
                node.shape.row_boundaries = boundaries;
            }
        }

        // Generalized non-uniform row spacing: match the child cylinder's connection
        // row to the parent's actual face height at the junction point.
#ifdef _DEBUG
        printf("[SubdivSolver] Checking generalized row spacing for %s -> %s: "
               "child_side=%d, parent_surface=%d, not_sphere=%d\n",
               node.name.c_str(), child.name.c_str(),
               child.connection.child_attach.region == AttachRegion::Side,
               (child.connection.parent_attach.region == AttachRegion::Surface || 
                child.connection.parent_attach.region == AttachRegion::Side),
               node.shape.type != ShapeType::Sphere);
#endif
        if (child.connection.child_attach.region == AttachRegion::Side &&
            (child.connection.parent_attach.region == AttachRegion::Surface ||
             child.connection.parent_attach.region == AttachRegion::Side) &&
            node.shape.type != ShapeType::Sphere) {

            // Generate parent faces to measure actual face height at connection point
            FaceGenerator faceGen;
            std::vector<Face> parent_faces = faceGen.Generate(node.shape);

            ConnectionFaceMatcher matcher;
            int parent_grid_idx = matcher.ComputeGridIndex(node.shape, child.connection.parent_attach);

#ifdef _DEBUG
            printf("[SubdivSolver] %s -> %s: parent_faces=%d, parent_grid_idx=%d\n",
                   node.name.c_str(), child.name.c_str(), (int)parent_faces.size(), parent_grid_idx);
#endif
            if (parent_grid_idx >= 0 && parent_grid_idx < static_cast<int>(parent_faces.size())) {
                const auto& conn_face = parent_faces[parent_grid_idx];

                // Only proceed for quad faces (lateral faces on torus, cylinder, etc.)
                if (conn_face.vertices.size() == 4) {
                    Vec3 bot_mid(
                        (conn_face.vertices[0].x + conn_face.vertices[1].x) * 0.5f,
                        (conn_face.vertices[0].y + conn_face.vertices[1].y) * 0.5f,
                        (conn_face.vertices[0].z + conn_face.vertices[1].z) * 0.5f
                    );
                    Vec3 top_mid(
                        (conn_face.vertices[2].x + conn_face.vertices[3].x) * 0.5f,
                        (conn_face.vertices[2].y + conn_face.vertices[3].y) * 0.5f,
                        (conn_face.vertices[2].z + conn_face.vertices[3].z) * 0.5f
                    );
                    float dx = top_mid.x - bot_mid.x;
                    float dy = top_mid.y - bot_mid.y;
                    float dz = top_mid.z - bot_mid.z;
                    float parent_face_height = std::sqrt(dx * dx + dy * dy + dz * dz);

#ifdef _DEBUG
                    printf("[SubdivSolver] %s -> %s: parent_face_height=%.4f, child_height=%.4f, parent_grid_idx=%d, face_verts=%d\n",
                           node.name.c_str(), child.name.c_str(), parent_face_height, child.shape.height,
                           parent_grid_idx, (int)conn_face.vertices.size());
#endif
                    if (child.shape.height > 0.0f && parent_face_height > 0.0f) {
                        int H = child.shape.height_segments;
                        float v_center = child.connection.child_attach.v;

                        // Compute the row span that should match parent face height
                        float row_span = parent_face_height / child.shape.height;
                        if (row_span > 1.0f) row_span = 1.0f;

                        // For edge cases (v=0 or v=1), the connection row is at the
                        // boundary, so we place the full row_span starting from that edge.
                        float row_bottom, row_top;
                        if (v_center < 0.001f) {
                            // Connection at bottom edge: row extends upward
                            row_bottom = 0.0f;
                            row_top = row_span;
                        } else if (v_center > 0.999f) {
                            // Connection at top edge: row extends downward
                            row_bottom = 1.0f - row_span;
                            row_top = 1.0f;
                        } else {
                            // Connection in middle: row centered on v
                            float row_half = row_span * 0.5f;
                            row_bottom = v_center - row_half;
                            row_top = v_center + row_half;

                            // Clamp to [0, 1]
                            if (row_bottom < 0.0f) { row_top += -row_bottom; row_bottom = 0.0f; }
                            if (row_top > 1.0f) { row_bottom -= (row_top - 1.0f); row_top = 1.0f; }
                            if (row_bottom < 0.0f) row_bottom = 0.0f;
                            if (row_top > 1.0f) row_top = 1.0f;
                        }

                        // Determine how many uniform rows above/below the connection row
                        int conn_row;
                        if (v_center < 0.001f) {
                            conn_row = 0;
                        } else if (v_center > 0.999f) {
                            conn_row = H - 1;
                        } else {
                            conn_row = static_cast<int>(std::round(v_center * (H - 1)));
                        }
                        if (conn_row < 0) conn_row = 0;
                        if (conn_row >= H) conn_row = H - 1;

                        int rows_below = conn_row;
                        int rows_above = H - 1 - conn_row;

                        std::vector<float> boundaries;
                        boundaries.reserve(H + 1);
                        boundaries.push_back(0.0f);

                        // Uniform rows below the connection row
                        for (int i = 1; i <= rows_below; ++i) {
                            boundaries.push_back(row_bottom * static_cast<float>(i) / rows_below);
                        }

                        // Connection row boundaries
                        if (rows_below == 0 && row_bottom > 0.001f) {
                            // If no rows below but row_bottom > 0, add it
                            boundaries.push_back(row_bottom);
                        }
                        boundaries.push_back(row_top);

                        // Uniform rows above the connection row
                        for (int i = 1; i <= rows_above; ++i) {
                            boundaries.push_back(row_top + (1.0f - row_top) * static_cast<float>(i) / rows_above);
                        }

                        // Ensure exactly H+1 entries
                        while (static_cast<int>(boundaries.size()) < H + 1)
                            boundaries.push_back(1.0f);
                        while (static_cast<int>(boundaries.size()) > H + 1)
                            boundaries.pop_back();
                        boundaries.back() = 1.0f;

                        child.shape.row_boundaries = boundaries;
#ifdef _DEBUG
                        printf("[SubdivSolver] %s -> %s: H=%d, v_center=%.4f, row_span=%.4f, conn_row=%d, row_bottom=%.4f, row_top=%.4f\n",
                               node.name.c_str(), child.name.c_str(), H, v_center, row_span, conn_row, row_bottom, row_top);
                        printf("[SubdivSolver] row_boundaries: [");
                        for (float b : boundaries) printf("%.3f ", b);
                        printf("]\n");
#endif
                    }
                }
            }
        } else {
#ifdef _DEBUG
            printf("[SubdivSolver] SKIPPED generalized row spacing for %s -> %s: "
                   "child_side=%d, parent_surface_or_side=%d, not_sphere=%d\n",
                   node.name.c_str(), child.name.c_str(),
                   child.connection.child_attach.region == AttachRegion::Side,
                   (child.connection.parent_attach.region == AttachRegion::Surface || 
                    child.connection.parent_attach.region == AttachRegion::Side),
                   node.shape.type != ShapeType::Sphere);
#endif
        }

        // Recurse — pass parent_segs as forced minimum so child doesn't reduce below it
        DeriveSegmentsRecursive(child, base_res, parent_segs);
    }
}

// ============================================================================
// Public API
// ============================================================================

void SubdivisionSolver::Solve(Body& body, int base_resolution) const
{
    DeriveSegmentsRecursive(body.root, base_resolution);
}

void SubdivisionSolver::PrepareBody(Body& body, int base_resolution) const
{
    Solve(body, base_resolution);

    ConnectionSolver solver;
    solver.ResolveTree(&body.root);
}

} // namespace BodyRenderer
