#include "ConnectionFaceMatcher.h"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace BodyRenderer {

// ============================================================================
// Connection radius computation
// ============================================================================

float ConnectionFaceMatcher::ComputeConnectionRadius(const ShapeParams& shape, const AttachmentPoint& attach) const
{
    switch (shape.type) {
    case ShapeType::Cylinder:
        // For top/bottom: the natural face is the cap N-gon = full cylinder radius
        // For side: it's a small rectangular face — approximate as arc segment
        if (attach.region == AttachRegion::Top || attach.region == AttachRegion::Bottom) {
            return shape.radius;
        } else {
            // Side face: approximate face width/height
            float segment_angle = 2.0f * static_cast<float>(M_PI) / shape.segments;
            return shape.radius * std::sin(segment_angle * 0.5f);
        }
        break;

    case ShapeType::Sphere:
        return ComputeSphereLocalRadius(shape, attach);

    case ShapeType::Capsule:
        if (attach.region == AttachRegion::Top || attach.region == AttachRegion::TopCap ||
            attach.region == AttachRegion::Bottom || attach.region == AttachRegion::BottomCap) {
            // At the pole, the "natural" face is a small triangle fan
            // At the equator, it's full radius. Use latitude to interpolate.
            float v = attach.v; // 0 = pole, 1 = equator for caps
            // At pole, face radius is small (first ring). At equator, it's full radius.
            // First ring radius on a hemisphere with N segments and hemi_stacks:
            int hemi_stacks = shape.segments / 2;
            if (hemi_stacks < 2) hemi_stacks = 2;
            float phi = v * 0.5f * static_cast<float>(M_PI);
            return shape.radius * std::sin(phi);
        } else {
            // Side: same as cylinder
            return shape.radius;
        }
        break;

    case ShapeType::Cone:
        if (attach.region == AttachRegion::Base) {
            return shape.radius;
        } else {
            // Side: radius varies with height
            float radius_at_height = shape.radius * (1.0f - attach.v);
            return radius_at_height;
        }
        break;

    case ShapeType::Torus:
        // Torus local face size is approximately the minor radius arc segment
        return shape.minor_radius;
        break;
    }

    return 0.1f; // fallback
}

float ConnectionFaceMatcher::ComputeSphereLocalRadius(const ShapeParams& shape, const AttachmentPoint& attach) const
{
    // On a sphere, the face size varies with latitude.
    // At the equator, faces are largest. At poles, they're triangles converging to a point.
    // v = 0 is top pole, v = 1 is bottom pole, v = 0.5 is equator.
    // The "ring radius" at latitude v is: R * sin(phi) where phi = v * PI
    float phi = attach.v * static_cast<float>(M_PI);
    float ring_radius = shape.radius * std::sin(phi);

    // But the actual face at that latitude spans one segment worth of longitude
    // The chord length for one segment at that ring:
    // face_radius ~= ring_radius * sin(PI / lon_segments)
    // But for the connection ring, we want the ring itself (all segments), not one face.
    // So the connection ring radius = ring_radius (the full circle at that latitude).
    // However, that would be the entire latitude ring which is too large.
    // 
    // The user's intent: where a sphere connects to a cylinder, the sphere should
    // produce a face (N-gon) that matches the cylinder's cap N-gon. Both faces
    // meet flush. The sphere's face ring at the connection point should match.
    //
    // For a sphere, the natural connection ring radius at latitude v is the
    // circumscribed radius of the face ring at that latitude:
    // = R * sin(phi) where phi = v * PI
    //
    // This is the largest ring we can inscribe at that latitude.
    // If the child's connection radius is smaller, we use the child's.
    // If larger, we cap at this value.
    return ring_radius;
}

float ConnectionFaceMatcher::ComputeMatchedRadius(
    const ShapeParams& parent_shape, const AttachmentPoint& parent_attach,
    const ShapeParams& child_shape, const AttachmentPoint& child_attach) const
{
    float parent_r = ComputeConnectionRadius(parent_shape, parent_attach);
    float child_r = ComputeConnectionRadius(child_shape, child_attach);

    // Use the smaller radius so neither shape expands beyond natural bounds.
    // Both shapes will contract to this radius at the junction.
    float matched = std::min(parent_r, child_r);

    // Ensure minimum viable radius
    if (matched < 0.01f) matched = 0.01f;

    return matched;
}

int ConnectionFaceMatcher::GetSegmentsAtRegion(const ShapeParams& shape, const AttachmentPoint& /*attach*/) const
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

int ConnectionFaceMatcher::ComputeMatchedSegments(
    const ShapeParams& parent_shape, const AttachmentPoint& parent_attach,
    const ShapeParams& child_shape, const AttachmentPoint& child_attach) const
{
    int parent_n = GetSegmentsAtRegion(parent_shape, parent_attach);
    int child_n = GetSegmentsAtRegion(child_shape, child_attach);

    // Use the minimum so both shapes can produce it without exceeding their resolution
    int matched = std::min(parent_n, child_n);
    if (matched < 3) matched = 3;

    return matched;
}

// ============================================================================
// Ring face generation
// ============================================================================

Face ConnectionFaceMatcher::GenerateRingFace(const Vec3& center, const Vec3& normal, float radius, int segments) const
{
    Face face;
    face.normal = normal.Normalized();

    // Build a coordinate frame on the plane perpendicular to normal
    Vec3 up = (std::fabs(normal.y) < 0.9f) ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
    Vec3 tangent = normal.Cross(up).Normalized();
    Vec3 bitangent = normal.Cross(tangent).Normalized();

    // Generate N-gon vertices
    for (int i = 0; i < segments; ++i) {
        float angle = 2.0f * static_cast<float>(M_PI) * i / segments;
        float cx = std::cos(angle);
        float cz = std::sin(angle);
        Vec3 v = center + tangent * (radius * cx) + bitangent * (radius * cz);
        face.vertices.push_back(v);
    }

    return face;
}

std::vector<Face> ConnectionFaceMatcher::GenerateBridgeFaces(
    const std::vector<Vec3>& inner_ring,
    const std::vector<Vec3>& outer_ring,
    const Vec3& normal) const
{
    std::vector<Face> bridge_faces;

    int inner_n = static_cast<int>(inner_ring.size());
    int outer_n = static_cast<int>(outer_ring.size());

    if (inner_n == 0 || outer_n == 0) return bridge_faces;

    if (inner_n == outer_n) {
        // Same count: simple quad strips
        for (int i = 0; i < inner_n; ++i) {
            int next = (i + 1) % inner_n;
            Face f;
            f.vertices = { inner_ring[i], inner_ring[next], outer_ring[next], outer_ring[i] };
            Vec3 e1 = inner_ring[next] - inner_ring[i];
            Vec3 e2 = outer_ring[i] - inner_ring[i];
            f.normal = e1.Cross(e2).Normalized();
            // Ensure normal points outward (same hemisphere as surface normal)
            if (f.normal.Dot(normal) < 0) {
                f.normal = f.normal * (-1.0f);
                // Reverse winding
                std::reverse(f.vertices.begin(), f.vertices.end());
            }
            bridge_faces.push_back(f);
        }
    } else {
        // Different counts: use triangle fan bridging
        // Map outer vertices to inner vertices proportionally
        for (int i = 0; i < outer_n; ++i) {
            int next_outer = (i + 1) % outer_n;
            // Map to inner ring index
            float inner_frac = static_cast<float>(i) / outer_n * inner_n;
            int inner_idx = static_cast<int>(inner_frac) % inner_n;
            float inner_frac_next = static_cast<float>(next_outer) / outer_n * inner_n;
            int inner_idx_next = static_cast<int>(inner_frac_next) % inner_n;

            if (inner_idx == inner_idx_next) {
                // Triangle: two outer vertices map to same inner vertex
                Face f;
                f.vertices = { outer_ring[i], outer_ring[next_outer], inner_ring[inner_idx] };
                Vec3 e1 = outer_ring[next_outer] - outer_ring[i];
                Vec3 e2 = inner_ring[inner_idx] - outer_ring[i];
                f.normal = e1.Cross(e2).Normalized();
                if (f.normal.Dot(normal) < 0) {
                    f.normal = f.normal * (-1.0f);
                    std::reverse(f.vertices.begin(), f.vertices.end());
                }
                bridge_faces.push_back(f);
            } else {
                // Quad
                Face f;
                f.vertices = { outer_ring[i], outer_ring[next_outer], inner_ring[inner_idx_next], inner_ring[inner_idx] };
                Vec3 e1 = outer_ring[next_outer] - outer_ring[i];
                Vec3 e2 = inner_ring[inner_idx] - outer_ring[i];
                f.normal = e1.Cross(e2).Normalized();
                if (f.normal.Dot(normal) < 0) {
                    f.normal = f.normal * (-1.0f);
                    std::reverse(f.vertices.begin(), f.vertices.end());
                }
                bridge_faces.push_back(f);
            }
        }
    }

    return bridge_faces;
}

// ============================================================================
// Face generation with connection rings
// ============================================================================

MatchedFaces ConnectionFaceMatcher::GenerateWithConnections(
    const BodyNode& node,
    const std::vector<ConnectionRing>& rings) const
{
    MatchedFaces result;

    if (rings.empty()) {
        // No connections to match — just generate normal faces
        result.faces = m_faceGen.Generate(node.shape);
        return result;
    }

    // Strategy: Generate base faces, then for each connection ring,
    // replace the face nearest the connection point with a ring + bridge geometry.
    result.faces = m_faceGen.Generate(node.shape);

    for (const auto& ring : rings) {
        int ring_face_index = -1;

        switch (node.shape.type) {
        case ShapeType::Cylinder:
            InsertCylinderRing(result.faces, node.shape, ring, ring_face_index);
            break;
        case ShapeType::Sphere:
            InsertSphereRing(result.faces, node.shape, ring, ring_face_index);
            break;
        case ShapeType::Capsule:
            InsertCapsuleRing(result.faces, node.shape, ring, ring_face_index);
            break;
        default:
            // For cone and torus, use the same sphere approach (find nearest face)
            InsertSphereRing(result.faces, node.shape, ring, ring_face_index);
            break;
        }

        result.connection_face_indices.push_back(ring_face_index);
    }

    return result;
}

// ============================================================================
// Ring insertion into specific shape types
// ============================================================================

void ConnectionFaceMatcher::InsertCylinderRing(
    std::vector<Face>& faces, const ShapeParams& shape,
    const ConnectionRing& ring, int& out_ring_face_index) const
{
    // For cylinder top/bottom, the cap is already an N-gon.
    // If the ring radius differs from the cylinder radius, we need to:
    // 1. Replace the cap N-gon with a smaller inner N-gon (the connection face)
    // 2. Bridge from the inner N-gon to the cylinder wall with an annular ring of quads

    float hh = shape.height * 0.5f;
    bool is_top = (ring.normal.y > 0.5f);
    bool is_bottom = (ring.normal.y < -0.5f);

    if (!is_top && !is_bottom) {
        // Side connection — use sphere-style face replacement
        InsertSphereRing(faces, shape, ring, out_ring_face_index);
        return;
    }

    float cap_y = is_top ? hh : -hh;

    // Find and remove the existing cap face
    int cap_index = -1;
    for (int i = 0; i < static_cast<int>(faces.size()); ++i) {
        if (faces[i].vertices.size() > 4) { // N-gon caps
            // Check if this face is at the right height
            float avg_y = 0;
            for (const auto& v : faces[i].vertices) avg_y += v.y;
            avg_y /= faces[i].vertices.size();
            if (std::fabs(avg_y - cap_y) < 0.01f) {
                cap_index = i;
                break;
            }
        }
    }

    if (cap_index < 0) {
        // No cap found — fallback
        out_ring_face_index = -1;
        return;
    }

    // Get the outer ring (existing cap vertices)
    std::vector<Vec3> outer_ring = faces[cap_index].vertices;
    Vec3 cap_normal = faces[cap_index].normal;

    // Remove the old cap
    faces.erase(faces.begin() + cap_index);

    // If ring radius >= cylinder radius, no modification needed — just add the cap back
    if (ring.radius >= shape.radius * 0.99f) {
        Face cap_face;
        cap_face.normal = cap_normal;
        cap_face.vertices = outer_ring;
        faces.push_back(cap_face);
        out_ring_face_index = static_cast<int>(faces.size()) - 1;
        return;
    }

    // Generate the inner connection ring (smaller N-gon at the connection point)
    Vec3 center(ring.center.x, cap_y, ring.center.z);
    Face inner_face = GenerateRingFace(center, cap_normal, ring.radius, ring.segments);

    // If the inner ring has different segment count than outer, we need bridging
    std::vector<Vec3> inner_ring_verts = inner_face.vertices;

    // Generate bridge faces (annular quads between inner ring and outer cap edge)
    std::vector<Face> bridge = GenerateBridgeFaces(inner_ring_verts, outer_ring, cap_normal);

    // Add the inner face (this is the connection face)
    faces.push_back(inner_face);
    out_ring_face_index = static_cast<int>(faces.size()) - 1;

    // Add bridge faces
    for (auto& bf : bridge) {
        faces.push_back(bf);
    }
}

void ConnectionFaceMatcher::InsertSphereRing(
    std::vector<Face>& faces, const ShapeParams& shape,
    const ConnectionRing& ring, int& out_ring_face_index) const
{
    // For a sphere (or general shape), find the face closest to the ring center
    // and replace it with the connection ring + surrounding bridge geometry.
    (void)shape; // Operates on pre-generated faces, not shape params directly

    Vec3 ring_center = ring.center;
    Vec3 ring_dir = ring.normal.Normalized();

    // Find the face whose center is closest to the ring center
    int closest_face = -1;
    float closest_dist = 1e9f;
    for (int i = 0; i < static_cast<int>(faces.size()); ++i) {
        Vec3 face_center(0, 0, 0);
        for (const auto& v : faces[i].vertices) {
            face_center = face_center + v;
        }
        face_center = face_center * (1.0f / faces[i].vertices.size());

        float dist = (face_center - ring_center).Length();
        if (dist < closest_dist) {
            closest_dist = dist;
            closest_face = i;
        }
    }

    if (closest_face < 0) {
        out_ring_face_index = -1;
        return;
    }

    // Find all faces within a radius of the ring (faces that overlap with the ring area)
    float search_radius = ring.radius * 1.5f;
    std::vector<int> affected_faces;
    for (int i = 0; i < static_cast<int>(faces.size()); ++i) {
        Vec3 face_center(0, 0, 0);
        for (const auto& v : faces[i].vertices) {
            face_center = face_center + v;
        }
        face_center = face_center * (1.0f / faces[i].vertices.size());

        float dist = (face_center - ring_center).Length();
        if (dist < search_radius) {
            affected_faces.push_back(i);
        }
    }

    if (affected_faces.empty()) {
        out_ring_face_index = -1;
        return;
    }

    // Collect all vertices from affected faces to form the "outer boundary"
    // Then remove affected faces and replace with ring + bridge
    std::vector<Vec3> boundary_verts;
    for (int idx : affected_faces) {
        for (const auto& v : faces[idx].vertices) {
            // Only include vertices that are outside the ring radius
            float dist_from_center = (v - ring_center).Length();
            if (dist_from_center > ring.radius * 0.8f) {
                // Check if this vertex is already in the boundary
                bool duplicate = false;
                for (const auto& bv : boundary_verts) {
                    if ((bv - v).Length() < 0.001f) {
                        duplicate = true;
                        break;
                    }
                }
                if (!duplicate) {
                    boundary_verts.push_back(v);
                }
            }
        }
    }

    // Sort boundary vertices by angle around the ring normal
    if (!boundary_verts.empty()) {
        Vec3 up = (std::fabs(ring_dir.y) < 0.9f) ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
        Vec3 tangent = ring_dir.Cross(up).Normalized();
        Vec3 bitangent = ring_dir.Cross(tangent).Normalized();

        std::sort(boundary_verts.begin(), boundary_verts.end(),
            [&](const Vec3& a, const Vec3& b) {
                Vec3 da = a - ring_center;
                Vec3 db = b - ring_center;
                float angle_a = std::atan2(da.Dot(bitangent), da.Dot(tangent));
                float angle_b = std::atan2(db.Dot(bitangent), db.Dot(tangent));
                return angle_a < angle_b;
            });
    }

    // Remove affected faces (in reverse order to preserve indices)
    std::sort(affected_faces.begin(), affected_faces.end(), std::greater<int>());
    for (int idx : affected_faces) {
        faces.erase(faces.begin() + idx);
    }

    // Generate the connection ring face
    Face ring_face = GenerateRingFace(ring_center, ring_dir, ring.radius, ring.segments);
    faces.push_back(ring_face);
    out_ring_face_index = static_cast<int>(faces.size()) - 1;

    // Generate bridge faces from ring to boundary (if we have enough boundary vertices)
    if (boundary_verts.size() >= 3) {
        std::vector<Face> bridge = GenerateBridgeFaces(ring_face.vertices, boundary_verts, ring_dir);
        for (auto& bf : bridge) {
            faces.push_back(bf);
        }
    }
}

void ConnectionFaceMatcher::InsertCapsuleRing(
    std::vector<Face>& faces, const ShapeParams& shape,
    const ConnectionRing& ring, int& out_ring_face_index) const
{
    // For capsule, delegate to sphere-style insertion in all cases.
    // The hemisphere geometry is handled the same way.
    (void)shape; // Shape info not needed here since InsertSphereRing works generically
    InsertSphereRing(faces, shape, ring, out_ring_face_index);
}

} // namespace BodyRenderer
