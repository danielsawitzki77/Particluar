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
        shape.lonSegments = segs;
        break;
    case ShapeType::Torus:
        shape.sideSegments = segs;
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
        return shape.lonSegments;
    case ShapeType::Torus:
        return shape.sideSegments;
    }
    return 8;
}

static void DeriveSegmentsRecursive(BodyNode& node, int base_res, int forced_angular = 0)
{
    // Collect all connection u-positions on this node (from children connecting to its side/surface)
    std::vector<float> side_u_positions;
    std::vector<float> side_v_positions;
    for (const auto& child : node.children) {
        if (child.connection.isLegacy) continue;
        if (child.connection.parentAttach.region == AttachRegion::Side ||
            child.connection.parentAttach.region == AttachRegion::Surface) {
            side_u_positions.push_back(child.connection.parentAttach.u);
            side_v_positions.push_back(child.connection.parentAttach.v);
        }
    }

    // Compute angular segments needed for this shape based on connection positions
    int min_angular = (std::max)(base_res, forced_angular);

    // Raise min_angular for radius-ratio face-width matching.
    // If this (parent) shape is larger than a connected child at a side/surface joint,
    // the parent needs ceil(parent_r / child_r) * child_segments so its angular face
    // width shrinks to match the child's. Using a multiple of the child's segments
    // preserves face-center alignment at the connection point.
    for (const auto& child : node.children) {
        if (child.connection.isLegacy) continue;
        if (child.connection.parentAttach.region != AttachRegion::Side &&
            child.connection.parentAttach.region != AttachRegion::Surface) continue;
        if (child.connection.childAttach.region != AttachRegion::Side &&
            child.connection.childAttach.region != AttachRegion::Surface) continue;
        // Skip torus parents (handled via ringSegments separately)
        if (node.shape.type == ShapeType::Torus) continue;

        // Effective radius at the connection point on each shape
        float parent_r = node.shape.radius;
        if (node.shape.type == ShapeType::Sphere) {
            float phi = child.connection.parentAttach.v * static_cast<float>(M_PI);
            float local_r = node.shape.radius * std::sin(phi);
            if (local_r > 0.05f * node.shape.radius) parent_r = local_r;
        } else if (node.shape.type == ShapeType::Cone) {
            parent_r = node.shape.radius * (1.0f - child.connection.parentAttach.v);
        }

        float child_r = child.shape.radius;
        if (child.shape.type == ShapeType::Sphere) {
            float phi = child.connection.childAttach.v * static_cast<float>(M_PI);
            float local_r = child.shape.radius * std::sin(phi);
            if (local_r > 0.05f * child.shape.radius) child_r = local_r;
        } else if (child.shape.type == ShapeType::Cone) {
            child_r = child.shape.radius * (1.0f - child.connection.childAttach.v);
        }

        if (parent_r > 0.001f && child_r > 0.001f && parent_r > child_r * 1.05f) {
            // Parent is larger: needs more segments.
            // Use base_res as the child's reference segment count (it will be at least this).
            int child_base_segs = (std::max)(base_res, forced_angular);
            int ratio_mult = static_cast<int>(std::ceil(parent_r / child_r));
            int needed = ratio_mult * child_base_segs;
            if (needed > 128) needed = 128;
            if (needed > min_angular) min_angular = needed;
        }
    }

    int required_angular = ComputeRequiredSegments(side_u_positions, min_angular);

    // Set the shape's angular segments
    SetShapeAngularSegments(node.shape, required_angular);

    // Also set latSegments for spheres and ringSegments for tori based on v-positions
    if (node.shape.type == ShapeType::Sphere) {
        node.shape.latSegments = ComputeRequiredSphereLat(side_v_positions, (std::max)(3, base_res));
    } else if (node.shape.type == ShapeType::Torus) {
        node.shape.ringSegments = ComputeRequiredSegments(side_u_positions, (std::max)(3, base_res));
        node.shape.sideSegments = ComputeRequiredSegments(side_v_positions, (std::max)(3, base_res));
    } else if (node.shape.type == ShapeType::Cylinder ||
               node.shape.type == ShapeType::Capsule ||
               node.shape.type == ShapeType::Cone) {
        if (!side_v_positions.empty()) {
            node.shape.heightSegments = ComputeRequiredSegments(side_v_positions, (std::max)(1, base_res / 4));
        }
    }

    // For each child: ensure child uses compatible segments at the connection boundary
    for (auto& child : node.children) {
        if (child.connection.isLegacy) {
            DeriveSegmentsRecursive(child, base_res, 0);
            continue;
        }

        // Child's angular segments must be at least parent's
        // For torus parents: do NOT force the child's segments to torus ringSegments.
        // The child keeps its natural segments; the torus adapts instead.
        int parent_segs;
        if (node.shape.type == ShapeType::Torus &&
            (child.connection.parentAttach.region == AttachRegion::Surface)) {
            parent_segs = 0; // Don't force child segments from torus
        } else {
            parent_segs = GetShapeAngularSegments(node.shape);
        }

        // Set child latSegments / ringSegments from base_res
        if (child.shape.type == ShapeType::Sphere) {
            child.shape.latSegments = (std::max)(3, base_res);
        } else if (child.shape.type == ShapeType::Torus) {
            child.shape.ringSegments = (std::max)(3, base_res);
        }

        // If child connects via its side or surface, compute heightSegments from v-positions
        if (child.connection.childAttach.region == AttachRegion::Side ||
            child.connection.childAttach.region == AttachRegion::Surface) {
            std::vector<float> child_v_positions;
            child_v_positions.push_back(child.connection.childAttach.v);
            for (const auto& grandchild : child.children) {
                if (grandchild.connection.isLegacy) continue;
                if (grandchild.connection.parentAttach.region == AttachRegion::Side ||
                    grandchild.connection.parentAttach.region == AttachRegion::Surface) {
                    child_v_positions.push_back(grandchild.connection.parentAttach.v);
                }
            }
            int h_segs = ComputeRequiredSegments(child_v_positions, (std::max)(1, base_res / 4));
            child.shape.heightSegments = h_segs;
        } else {
            child.shape.heightSegments = (std::max)(1, base_res / 4);
        }

        // Special case: cylinder/capsule/cone side connects to a torus surface
        // The torus needs enough ringSegments so ONE of its faces matches the
        // cylinder's natural face width. The cylinder stays unchanged.
        // Formula: torus face width = 2π*major/ring_segs should equal
        //          cylinder face width = 2π*cyl_radius/cyl_segs
        // => ring_segs = majorRadius / cyl_radius * cyl_segs
        if (child.connection.parentAttach.region == AttachRegion::Surface &&
            node.shape.type == ShapeType::Torus) {
            if (child.shape.type == ShapeType::Cylinder ||
                child.shape.type == ShapeType::Capsule ||
                child.shape.type == ShapeType::Cone) {
                if (child.connection.childAttach.region == AttachRegion::Side ||
                    child.connection.childAttach.region == AttachRegion::Surface) {
                    // Match height: cylinder row height = torus tube face height
                    child.shape.heightSegments = node.shape.sideSegments;
                    // Increase torus ringSegments so its face width matches cylinder's
                    float child_radius = child.shape.radius;
                    if (child_radius > 0.001f) {
                        int needed_ring = static_cast<int>(
                            std::ceil(node.shape.majorRadius / child_radius * child.shape.segments));
                        if (needed_ring < node.shape.ringSegments) needed_ring = node.shape.ringSegments;
                        if (needed_ring > 128) needed_ring = 128;
                        node.shape.ringSegments = needed_ring;
                    }
                    // Do NOT change child.shape.segments — the cylinder keeps its natural geometry
                }
            }
        }

        // Sync cylinder heightSegments with sphere latSegments at side<->surface connections
        if (child.connection.childAttach.region == AttachRegion::Side ||
            child.connection.childAttach.region == AttachRegion::Surface) {
            if (node.shape.type == ShapeType::Sphere) {
                int synced = (std::max)(node.shape.latSegments, child.shape.heightSegments);
                node.shape.latSegments = synced;
                child.shape.heightSegments = synced;

                // Compute non-uniform row boundaries so cylinder rows match sphere latitude bands
                int H = child.shape.heightSegments;
                std::vector<float> boundaries;
                boundaries.reserve(H + 1);
                for (int k = 0; k <= H; ++k) {
                    float phi = static_cast<float>(M_PI) * (H - k) / H;
                    float t = (1.0f + std::cos(phi)) * 0.5f;
                    boundaries.push_back(t);
                }
                child.shape.rowBoundaries = boundaries;
            }
        }
        if (child.connection.parentAttach.region == AttachRegion::Side ||
            child.connection.parentAttach.region == AttachRegion::Surface) {
            if (child.shape.type == ShapeType::Sphere) {
                int synced = (std::max)(child.shape.latSegments, node.shape.heightSegments);
                child.shape.latSegments = synced;
                node.shape.heightSegments = synced;

                // Compute non-uniform row boundaries so parent cylinder rows match sphere latitude bands
                int H = node.shape.heightSegments;
                std::vector<float> boundaries;
                boundaries.reserve(H + 1);
                for (int k = 0; k <= H; ++k) {
                    float phi = static_cast<float>(M_PI) * (H - k) / H;
                    float t = (1.0f + std::cos(phi)) * 0.5f;
                    boundaries.push_back(t);
                }
                node.shape.rowBoundaries = boundaries;
            }
        }

        // Generalized non-uniform row spacing: match the child cylinder's connection
        // row to the parent's actual face height at the junction point.
#ifdef _DEBUG
        printf("[SubdivSolver] Checking generalized row spacing for %s -> %s: "
               "child_side=%d, parent_surface=%d, not_sphere=%d\n",
               node.name.c_str(), child.name.c_str(),
               (child.connection.childAttach.region == AttachRegion::Side ||
                child.connection.childAttach.region == AttachRegion::Surface),
               (child.connection.parentAttach.region == AttachRegion::Surface || 
                child.connection.parentAttach.region == AttachRegion::Side),
               node.shape.type != ShapeType::Sphere);
#endif
        if ((child.connection.childAttach.region == AttachRegion::Side ||
             child.connection.childAttach.region == AttachRegion::Surface) &&
            (child.connection.parentAttach.region == AttachRegion::Surface ||
             child.connection.parentAttach.region == AttachRegion::Side) &&
            node.shape.type != ShapeType::Sphere) {

            // Generate parent faces to measure actual face height at connection point
            FaceGenerator faceGen;
            std::vector<Face> parent_faces = faceGen.Generate(node.shape);

            ConnectionFaceMatcher matcher;
            int parent_grid_idx = matcher.ComputeGridIndex(node.shape, child.connection.parentAttach);

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
                        int H = child.shape.heightSegments;
                        float v_center = child.connection.childAttach.v;

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

                        child.shape.rowBoundaries = boundaries;
#ifdef _DEBUG
                        printf("[SubdivSolver] %s -> %s: H=%d, v_center=%.4f, row_span=%.4f, conn_row=%d, row_bottom=%.4f, row_top=%.4f\n",
                               node.name.c_str(), child.name.c_str(), H, v_center, row_span, conn_row, row_bottom, row_top);
                        printf("[SubdivSolver] rowBoundaries: [");
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
                   (child.connection.childAttach.region == AttachRegion::Side ||
                    child.connection.childAttach.region == AttachRegion::Surface),
                   (child.connection.parentAttach.region == AttachRegion::Surface || 
                    child.connection.parentAttach.region == AttachRegion::Side),
                   node.shape.type != ShapeType::Sphere);
#endif
        }

        // Radius-ratio face-width matching for the child side:
        // If the child is larger than the parent at a side/surface connection,
        // inflate child's angular segments so its face width matches the parent's.
        if ((child.connection.childAttach.region == AttachRegion::Side ||
             child.connection.childAttach.region == AttachRegion::Surface) &&
            (child.connection.parentAttach.region == AttachRegion::Side ||
             child.connection.parentAttach.region == AttachRegion::Surface)) {

            float parent_r = node.shape.radius;
            if (node.shape.type == ShapeType::Torus) parent_r = node.shape.minorRadius;
            if (node.shape.type == ShapeType::Sphere) {
                float phi = child.connection.parentAttach.v * static_cast<float>(M_PI);
                float local_r = node.shape.radius * std::sin(phi);
                if (local_r > 0.05f * node.shape.radius) parent_r = local_r;
            } else if (node.shape.type == ShapeType::Cone) {
                parent_r = node.shape.radius * (1.0f - child.connection.parentAttach.v);
            }

            float child_r = child.shape.radius;
            if (child.shape.type == ShapeType::Sphere) {
                float phi = child.connection.childAttach.v * static_cast<float>(M_PI);
                float local_r = child.shape.radius * std::sin(phi);
                if (local_r > 0.05f * child.shape.radius) child_r = local_r;
            } else if (child.shape.type == ShapeType::Cone) {
                child_r = child.shape.radius * (1.0f - child.connection.childAttach.v);
            }

            if (child_r > parent_r * 1.05f && parent_r > 0.001f) {
                // Child is larger: needs ceil(child_r/parent_r) * parent_segments
                int parent_base_segs = GetShapeAngularSegments(node.shape);
                int ratio_mult = static_cast<int>(std::ceil(child_r / parent_r));
                int needed = ratio_mult * parent_base_segs;
                if (needed > 128) needed = 128;
                int current = GetShapeAngularSegments(child.shape);
                if (needed > current) {
                    SetShapeAngularSegments(child.shape, needed);
                }
            }

            // When parent was inflated for radius ratio, scale parent_segs down
            // proportionally for the child so the smaller child isn't over-inflated.
            if (parent_r > child_r * 1.05f && child_r > 0.001f) {
                // Parent is larger — child needs base_res (not the inflated parent count)
                // because the child's natural segments already produce the right face width.
                int child_natural = static_cast<int>(std::ceil(
                    static_cast<float>(parent_segs) * child_r / parent_r));
                if (child_natural < base_res) child_natural = base_res;
                parent_segs = child_natural;
            }
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
