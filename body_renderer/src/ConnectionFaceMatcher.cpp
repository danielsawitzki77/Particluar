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
        if (attach.region == AttachRegion::Top || attach.region == AttachRegion::Bottom) {
            return shape.radius;
        } else {
            float segment_angle = 2.0f * static_cast<float>(M_PI) / shape.segments;
            return shape.radius * std::sin(segment_angle * 0.5f);
        }
        break;

    case ShapeType::Sphere:
        return ComputeSphereLocalRadius(shape, attach);

    case ShapeType::Capsule:
        if (attach.region == AttachRegion::Top || attach.region == AttachRegion::TopCap ||
            attach.region == AttachRegion::Bottom || attach.region == AttachRegion::BottomCap) {
            float v = attach.v;
            float phi = v * 0.5f * static_cast<float>(M_PI);
            float ring_r = shape.radius * std::sin(phi);
            // At pole (v=0), ring radius approaches 0; at equator (v=1), it's full radius
            if (ring_r < 0.01f) ring_r = shape.radius * 0.1f; // minimum for pole
            return ring_r;
        } else {
            return shape.radius;
        }
        break;

    case ShapeType::Cone:
        if (attach.region == AttachRegion::Base) {
            return shape.radius;
        } else {
            float radius_at_height = shape.radius * (1.0f - attach.v);
            return radius_at_height;
        }
        break;

    case ShapeType::Torus:
        return shape.minor_radius;
        break;
    }

    return 0.1f;
}

float ConnectionFaceMatcher::ComputeSphereLocalRadius(const ShapeParams& shape, const AttachmentPoint& attach) const
{
    // The ring radius at latitude v on a sphere: R * sin(phi) where phi = v * PI
    // v = 0 is top pole, v = 1 is bottom pole, v = 0.5 is equator
    float phi = attach.v * static_cast<float>(M_PI);
    float ring_radius = shape.radius * std::sin(phi);

    // At poles, the ring radius approaches 0 — use a minimum
    if (ring_radius < shape.radius * 0.05f) {
        ring_radius = shape.radius * 0.15f;
    }

    return ring_radius;
}

float ConnectionFaceMatcher::ComputeMatchedRadius(
    const ShapeParams& parent_shape, const AttachmentPoint& parent_attach,
    const ShapeParams& child_shape, const AttachmentPoint& child_attach) const
{
    float parent_r = ComputeConnectionRadius(parent_shape, parent_attach);
    float child_r = ComputeConnectionRadius(child_shape, child_attach);

    // Use the smaller radius so both shapes taper toward the junction
    float matched = std::min(parent_r, child_r);

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

    // Generate N-gon vertices in CCW order when viewed from the normal direction
    for (int i = 0; i < segments; ++i) {
        float angle = 2.0f * static_cast<float>(M_PI) * i / segments;
        float cx = std::cos(angle);
        float cz = std::sin(angle);
        Vec3 v = center + tangent * (radius * cx) + bitangent * (radius * cz);
        face.vertices.push_back(v);
    }

    return face;
}

// ============================================================================
// Tapering: deform shape geometry toward connection points
// ============================================================================

void ConnectionFaceMatcher::TaperCylinderEnd(
    std::vector<Face>& faces, const ShapeParams& shape,
    const ConnectionRing& ring, int& out_ring_face_index) const
{
    float hh = shape.height * 0.5f;
    bool is_top = (ring.normal.y > 0.5f);
    bool is_bottom = (ring.normal.y < -0.5f);

    if (!is_top && !is_bottom) {
        // Side connection — use sphere-style tapering
        TaperSphereRegion(faces, shape, ring, out_ring_face_index);
        return;
    }

    float cap_y = is_top ? hh : -hh;

    // If the ring radius is approximately equal to cylinder radius, no tapering needed.
    // Just find the cap face and mark it as the connection face.
    if (ring.radius >= shape.radius * 0.95f) {
        // Find the cap face
        for (int i = 0; i < static_cast<int>(faces.size()); ++i) {
            if (faces[i].vertices.size() > 4) {
                float avg_y = 0;
                for (const auto& v : faces[i].vertices) avg_y += v.y;
                avg_y /= faces[i].vertices.size();
                if (std::fabs(avg_y - cap_y) < 0.01f) {
                    out_ring_face_index = i;
                    return;
                }
            }
        }
        out_ring_face_index = -1;
        return;
    }

    // Tapering: deform the lateral faces near the connection end so they
    // converge from the full cylinder radius down to the matched ring radius.
    // Then replace the cap with a smaller N-gon at the ring radius.
    
    int h_segs = shape.height_segments;
    if (h_segs < 1) h_segs = 1;

    // Determine how many height segments to taper over.
    // Taper the closest 1/3 of height segments (minimum 1).
    int taper_segs = std::max(1, h_segs / 3);
    
    float taper_ratio = ring.radius / shape.radius;

    // Walk all faces and deform vertices near the cap
    for (auto& face : faces) {
        for (auto& v : face.vertices) {
            // Check if this vertex is on the taper side
            float dist_from_cap;
            if (is_top) {
                dist_from_cap = hh - v.y;
            } else {
                dist_from_cap = v.y - (-hh);
            }

            // Only affect vertices within the taper zone
            float taper_height = shape.height * taper_segs / h_segs;
            if (dist_from_cap < taper_height + 0.001f) {
                // t = 0 at the cap edge, t = 1 at the end of the taper zone
                float t = dist_from_cap / taper_height;
                if (t > 1.0f) t = 1.0f;

                // Smoothstep interpolation for smooth taper
                float smooth_t = t * t * (3.0f - 2.0f * t);

                // Scale factor: at cap edge (t=0) we want taper_ratio, at taper end (t=1) we want 1.0
                float scale = taper_ratio + (1.0f - taper_ratio) * smooth_t;

                // Apply radial scaling (only xz, not y)
                float current_r = std::sqrt(v.x * v.x + v.z * v.z);
                if (current_r > 0.001f) {
                    float new_r = current_r * scale;
                    float factor = new_r / current_r;
                    v.x *= factor;
                    v.z *= factor;
                }
            }
        }

        // Recompute face normal after deformation (for lateral faces only — skip caps)
        if (face.vertices.size() >= 3 && face.vertices.size() <= 4) {
            // Skip cap/polygon faces (N-gons > 4 are handled separately)
            // Determine if this face was originally a lateral face by checking if
            // vertices span multiple Y levels (caps have all vertices at same Y).
            float min_y = face.vertices[0].y, max_y = face.vertices[0].y;
            for (const auto& fv : face.vertices) {
                if (fv.y < min_y) min_y = fv.y;
                if (fv.y > max_y) max_y = fv.y;
            }
            bool is_lateral = (max_y - min_y) > 0.001f;

            if (is_lateral) {
                // Use Newell's method for robust normal on possibly non-planar quads
                Vec3 computed(0, 0, 0);
                int nv = static_cast<int>(face.vertices.size());
                for (int vi = 0; vi < nv; ++vi) {
                    const Vec3& cur = face.vertices[vi];
                    const Vec3& nxt = face.vertices[(vi + 1) % nv];
                    computed.x += (cur.y - nxt.y) * (cur.z + nxt.z);
                    computed.y += (cur.z - nxt.z) * (cur.x + nxt.x);
                    computed.z += (cur.x - nxt.x) * (cur.y + nxt.y);
                }
                if (computed.Length() > 0.0001f) {
                    computed = computed.Normalized();
                    // Ensure outward: radial direction from Y axis
                    Vec3 face_center(0, 0, 0);
                    for (const auto& fv : face.vertices) face_center = face_center + fv;
                    face_center = face_center * (1.0f / face.vertices.size());
                    Vec3 radial_out(face_center.x, 0, face_center.z);
                    if (radial_out.Length() > 0.01f) {
                        if (computed.Dot(radial_out) < 0) {
                            computed = computed * (-1.0f);
                        }
                    }
                    face.normal = computed;
                }
            }
        }
    }

    // Find and replace the cap face with a properly-sized connection N-gon
    int cap_index = -1;
    for (int i = 0; i < static_cast<int>(faces.size()); ++i) {
        if (faces[i].vertices.size() > 4) {
            float avg_y = 0;
            for (const auto& v : faces[i].vertices) avg_y += v.y;
            avg_y /= faces[i].vertices.size();
            if (std::fabs(avg_y - cap_y) < 0.01f) {
                cap_index = i;
                break;
            }
        }
    }

    if (cap_index >= 0) {
        // Two cases:
        // 1. ring.segments == shape.segments: The tapered lateral vertices at the cap level
        //    directly form the connection face. Use them for watertight geometry.
        // 2. ring.segments != shape.segments: Generate the ring face at the matched segment
        //    count, then add bridge triangles between the outer lateral ring and the inner
        //    connection ring.

        float cap_y_actual = cap_y; // may slightly differ from ring.center.y

        // Collect the lateral vertices at the cap edge (y ≈ cap_y)
        std::vector<Vec3> outer_ring;
        for (const auto& face : faces) {
            if (face.vertices.size() == 3 || face.vertices.size() == 4) {
                for (const auto& v : face.vertices) {
                    if (std::fabs(v.y - cap_y_actual) < 0.001f) {
                        bool found = false;
                        for (const auto& rv : outer_ring) {
                            float dx = rv.x - v.x, dz = rv.z - v.z;
                            if (dx * dx + dz * dz < 0.0001f) {
                                found = true;
                                break;
                            }
                        }
                        if (!found) {
                            outer_ring.push_back(v);
                        }
                    }
                }
            }
        }

        // Sort outer ring by angle
        Vec3 centroid(0, cap_y_actual, 0);
        for (const auto& v : outer_ring) {
            centroid.x += v.x;
            centroid.z += v.z;
        }
        if (!outer_ring.empty()) {
            centroid.x /= outer_ring.size();
            centroid.z /= outer_ring.size();
        }
        std::sort(outer_ring.begin(), outer_ring.end(),
            [&centroid](const Vec3& a, const Vec3& b) {
                float angle_a = std::atan2(a.z - centroid.z, a.x - centroid.x);
                float angle_b = std::atan2(b.z - centroid.z, b.x - centroid.x);
                return angle_a < angle_b;
            });

        if (ring.segments == static_cast<int>(outer_ring.size())) {
            // Case 1: segments match — use lateral vertices directly as the cap
            Face new_cap;
            new_cap.normal = is_top ? Vec3(0, 1, 0) : Vec3(0, -1, 0);
            if (is_top) {
                new_cap.vertices = outer_ring;
            } else {
                new_cap.vertices.assign(outer_ring.rbegin(), outer_ring.rend());
            }
            faces[cap_index] = new_cap;
            out_ring_face_index = cap_index;
        } else if (!outer_ring.empty()) {
            // Case 2: segments differ — generate inner ring at matched count,
            // replace cap with inner ring face, add bridge annular faces.
            Vec3 cap_center(0, cap_y_actual, 0);
            Vec3 cap_normal = is_top ? Vec3(0, 1, 0) : Vec3(0, -1, 0);
            Face inner_ring = GenerateRingFace(cap_center, cap_normal, ring.radius, ring.segments);
            if (!is_top) {
                std::reverse(inner_ring.vertices.begin(), inner_ring.vertices.end());
                inner_ring.normal = Vec3(0, -1, 0);
            }

            // Replace the old cap with the inner ring face
            faces[cap_index] = inner_ring;
            out_ring_face_index = cap_index;

            // Add bridge triangles between outer_ring (N_outer) and inner_ring (N_inner).
            // Use a stitching algorithm to connect two rings with different vertex counts.
            int n_outer = static_cast<int>(outer_ring.size());
            int n_inner = ring.segments;
            // IMPORTANT: Copy inner vertices — do NOT take a reference to faces[cap_index].vertices
            // because faces.push_back() below can reallocate and invalidate references.
            const std::vector<Vec3> inner_verts = faces[cap_index].vertices;

            // Stitch the two rings using proportional advancement.
            // Total triangles needed = n_outer + n_inner (each edge becomes a tri).
            // We advance around each ring proportionally so triangles stay well-shaped.
            if (n_outer >= 3 && n_inner >= 3 &&
                static_cast<int>(inner_verts.size()) == n_inner) {
                int total_steps = n_outer + n_inner;
                int io = 0, ii = 0;
                int outer_steps = 0, inner_steps = 0;

                for (int step = 0; step < total_steps; ++step) {
                    // Decide whether to advance outer or inner ring next.
                    // Advance the ring that is proportionally "behind" the other.
                    bool advance_outer;
                    if (outer_steps >= n_outer) {
                        advance_outer = false;
                    } else if (inner_steps >= n_inner) {
                        advance_outer = true;
                    } else {
                        // Compare progress ratios: advance whichever is more behind
                        advance_outer = (static_cast<float>(outer_steps) / n_outer <=
                                         static_cast<float>(inner_steps) / n_inner);
                    }

                    Face bridge;
                    bridge.normal = cap_normal;
                    if (advance_outer) {
                        int next_o = (io + 1) % n_outer;
                        if (is_top) {
                            bridge.vertices = {outer_ring[io], outer_ring[next_o], inner_verts[ii]};
                        } else {
                            bridge.vertices = {inner_verts[ii], outer_ring[next_o], outer_ring[io]};
                        }
                        io = next_o;
                        outer_steps++;
                    } else {
                        int next_i = (ii + 1) % n_inner;
                        if (is_top) {
                            bridge.vertices = {outer_ring[io], inner_verts[next_i], inner_verts[ii]};
                        } else {
                            bridge.vertices = {inner_verts[ii], inner_verts[next_i], outer_ring[io]};
                        }
                        ii = next_i;
                        inner_steps++;
                    }
                    faces.push_back(bridge);
                }
            }
        } else {
            // Fallback: generate analytically
            Vec3 cap_center(ring.center.x, cap_y_actual, ring.center.z);
            Vec3 cap_normal = is_top ? Vec3(0, 1, 0) : Vec3(0, -1, 0);
            Face new_cap = GenerateRingFace(cap_center, cap_normal, ring.radius, ring.segments);
            if (!is_top) {
                std::reverse(new_cap.vertices.begin(), new_cap.vertices.end());
                new_cap.normal = Vec3(0, -1, 0);
            }
            faces[cap_index] = new_cap;
            out_ring_face_index = cap_index;
        }
    } else {
        out_ring_face_index = -1;
    }
}

void ConnectionFaceMatcher::TaperSphereRegion(
    std::vector<Face>& faces, const ShapeParams& /*shape*/,
    const ConnectionRing& ring, int& out_ring_face_index) const
{
    Vec3 ring_center = ring.center;
    Vec3 ring_dir = ring.normal.Normalized();

    // Compute the "influence radius" — how far from the ring center we deform vertices.
    // Use 2x the ring radius as the taper zone.
    float influence_radius = ring.radius * 2.5f;

    // Walk all faces and deform vertices within the influence zone
    for (auto& face : faces) {
        for (auto& v : face.vertices) {
            // Distance from vertex to ring center (on the surface, using arc-like metric)
            Vec3 to_v = v - ring_center;
            float dist = to_v.Length();

            if (dist < influence_radius && dist > 0.001f) {
                // t = 0 at ring center (max taper), t = 1 at edge of influence (no taper)
                float t = dist / influence_radius;
                float smooth_t = t * t * (3.0f - 2.0f * t); // smoothstep

                float proj_on_axis = to_v.Dot(ring_dir);
                Vec3 radial_from_axis = to_v - ring_dir * proj_on_axis;
                float radial_dist = radial_from_axis.Length();

                if (radial_dist > 0.001f) {
                    // At the connection face (t close to 0), vertices should be at ring.radius
                    // from the ring center in the ring plane
                    float desired_radial = ring.radius + (radial_dist - ring.radius) * smooth_t;
                    float factor = desired_radial / radial_dist;
                    
                    // Apply: move vertex radially (keep projection along ring axis fixed)
                    Vec3 new_radial = radial_from_axis * factor;
                    Vec3 new_pos = ring_center + ring_dir * proj_on_axis + new_radial;
                    
                    // Project back onto the sphere surface (scaled)
                    // Actually, don't project back — allow the deformation to break
                    // the perfect sphere shape. This is the "melting" effect requested.
                    v = new_pos;
                }
            }
        }

        // Recompute face normal after deformation using Newell's method
        if (face.vertices.size() >= 3) {
            Vec3 computed(0, 0, 0);
            int nv = static_cast<int>(face.vertices.size());
            for (int vi = 0; vi < nv; ++vi) {
                const Vec3& cur = face.vertices[vi];
                const Vec3& nxt = face.vertices[(vi + 1) % nv];
                computed.x += (cur.y - nxt.y) * (cur.z + nxt.z);
                computed.y += (cur.z - nxt.z) * (cur.x + nxt.x);
                computed.z += (cur.x - nxt.x) * (cur.y + nxt.y);
            }
            if (computed.Length() > 0.0001f) {
                computed = computed.Normalized();
                // Ensure outward from shape center (origin)
                Vec3 face_center(0, 0, 0);
                for (const auto& fv : face.vertices) face_center = face_center + fv;
                face_center = face_center * (1.0f / face.vertices.size());
                if (computed.Dot(face_center) < 0) {
                    computed = computed * (-1.0f);
                }
                face.normal = computed;
            }
        }
    }

    // Find the face closest to the ring center and replace it with the connection ring face
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

    if (closest_face >= 0) {
        // Instead of generating a brand new ring face (which disconnects from neighbors),
        // deform the existing closest face to become the connection ring.
        // If the face vertex count doesn't match ring.segments, replace it but
        // snap its vertices to neighboring deformed vertices for continuity.

        Face& target_face = faces[closest_face];
        
        // If the existing face has enough vertices, just reshape it in-place
        if (static_cast<int>(target_face.vertices.size()) == ring.segments) {
            // Redistribute vertices around the ring at the matched radius
            Vec3 up = (std::fabs(ring_dir.y) < 0.9f) ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
            Vec3 tangent = ring_dir.Cross(up).Normalized();
            Vec3 bitangent = ring_dir.Cross(tangent).Normalized();
            for (int i = 0; i < ring.segments; ++i) {
                float angle = 2.0f * static_cast<float>(M_PI) * i / ring.segments;
                target_face.vertices[i] = ring_center
                    + tangent * (ring.radius * std::cos(angle))
                    + bitangent * (ring.radius * std::sin(angle));
            }
            target_face.normal = ring_dir;
            out_ring_face_index = closest_face;
        } else {
            // Replace with a new ring face — snap each vertex to the nearest
            // existing deformed neighbor vertex for continuity
            Face ring_face = GenerateRingFace(ring_center, ring_dir, ring.radius, ring.segments);
            
            // For each ring vertex, find the closest vertex in adjacent faces and snap
            for (auto& rv : ring_face.vertices) {
                float best_dist = 1e9f;
                Vec3 best_v = rv;
                for (int fi = 0; fi < static_cast<int>(faces.size()); ++fi) {
                    if (fi == closest_face) continue;
                    for (const auto& fv : faces[fi].vertices) {
                        float d = (fv - rv).Length();
                        if (d < best_dist && d < ring.radius * 0.3f) {
                            best_dist = d;
                            best_v = fv;
                        }
                    }
                }
                if (best_dist < ring.radius * 0.3f) {
                    rv = best_v; // snap to neighbor
                }
            }
            
            faces[closest_face] = ring_face;
            out_ring_face_index = closest_face;
        }
    } else {
        out_ring_face_index = -1;
    }
}

void ConnectionFaceMatcher::TaperCapsuleRegion(
    std::vector<Face>& faces, const ShapeParams& shape,
    const ConnectionRing& ring, int& out_ring_face_index) const
{
    // Capsule: determine if connection is at a cap or on the side
    float cylinder_height = shape.height - 2.0f * shape.radius;
    if (cylinder_height < 0.0f) cylinder_height = 0.0f;
    float hh = cylinder_height * 0.5f;

    bool is_top = (ring.normal.y > 0.5f && ring.center.y > hh - 0.01f);
    bool is_bottom = (ring.normal.y < -0.5f && ring.center.y < -hh + 0.01f);

    if (is_top || is_bottom) {
        // Cap connection — taper the hemisphere region similar to cylinder end tapering
        // but with spherical influence

        float taper_ratio = ring.radius / shape.radius;
        if (taper_ratio >= 0.95f) {
            // No tapering needed — find the pole face
            out_ring_face_index = -1;
            return;
        }

        // Taper influence zone: from cap pole outward by about 1/3 of hemisphere
        float cap_pole_y = is_top ? (hh + shape.radius) : (-hh - shape.radius);
        (void)cap_pole_y; // may be used for future refinement

        for (auto& face : faces) {
            for (auto& v : face.vertices) {
                float dist_from_pole;
                if (is_top) {
                    dist_from_pole = cap_pole_y - v.y;
                } else {
                    dist_from_pole = v.y - cap_pole_y;
                }
                // Also consider radial distance to ensure we only affect the cap region
                float hemisphere_center_y = is_top ? hh : -hh;
                Vec3 from_hemi_center = v - Vec3(0, hemisphere_center_y, 0);
                float angular_dist = std::acos(std::min(1.0f, std::max(-1.0f,
                    from_hemi_center.Normalized().Dot(is_top ? Vec3(0, 1, 0) : Vec3(0, -1, 0)))));
                
                // Only affect vertices within ~60 degrees of the pole
                if (angular_dist < static_cast<float>(M_PI) / 3.0f) {
                    float t = angular_dist / (static_cast<float>(M_PI) / 3.0f);
                    float smooth_t = t * t * (3.0f - 2.0f * t);
                    float scale = taper_ratio + (1.0f - taper_ratio) * smooth_t;
                    
                    float current_r = std::sqrt(v.x * v.x + v.z * v.z);
                    if (current_r > 0.001f) {
                        float new_r = current_r * scale;
                        float factor = new_r / current_r;
                        v.x *= factor;
                        v.z *= factor;
                    }
                }
            }

            // Recompute normal using Newell's method
            if (face.vertices.size() >= 3) {
                Vec3 computed(0, 0, 0);
                int nv = static_cast<int>(face.vertices.size());
                for (int vi = 0; vi < nv; ++vi) {
                    const Vec3& cur = face.vertices[vi];
                    const Vec3& nxt = face.vertices[(vi + 1) % nv];
                    computed.x += (cur.y - nxt.y) * (cur.z + nxt.z);
                    computed.y += (cur.z - nxt.z) * (cur.x + nxt.x);
                    computed.z += (cur.x - nxt.x) * (cur.y + nxt.y);
                }
                if (computed.Length() > 0.0001f) {
                    computed = computed.Normalized();
                    Vec3 face_center(0, 0, 0);
                    for (const auto& fv : face.vertices) face_center = face_center + fv;
                    face_center = face_center * (1.0f / face.vertices.size());
                    if (computed.Dot(face_center) < 0) {
                        computed = computed * (-1.0f);
                    }
                    face.normal = computed;
                }
            }
        }

        // Replace the pole triangle(s) with a connection ring face.
        // Snap ring vertices to nearby deformed vertices for continuity.
        Vec3 pole_pos = ring.center;
        int closest_face = -1;
        float closest_dist = 1e9f;
        for (int i = 0; i < static_cast<int>(faces.size()); ++i) {
            Vec3 face_center(0, 0, 0);
            for (const auto& fv : faces[i].vertices) face_center = face_center + fv;
            face_center = face_center * (1.0f / faces[i].vertices.size());
            float dist = (face_center - pole_pos).Length();
            if (dist < closest_dist) {
                closest_dist = dist;
                closest_face = i;
            }
        }

        if (closest_face >= 0) {
            Face ring_face = GenerateRingFace(ring.center, ring.normal, ring.radius, ring.segments);
            // Snap ring vertices to nearby mesh vertices for watertight connections
            for (auto& rv : ring_face.vertices) {
                float best_d = 1e9f;
                Vec3 best_v = rv;
                for (int fi = 0; fi < static_cast<int>(faces.size()); ++fi) {
                    if (fi == closest_face) continue;
                    for (const auto& fv : faces[fi].vertices) {
                        float d = (fv - rv).Length();
                        if (d < best_d && d < ring.radius * 0.3f) {
                            best_d = d;
                            best_v = fv;
                        }
                    }
                }
                if (best_d < ring.radius * 0.3f) {
                    rv = best_v;
                }
            }
            faces[closest_face] = ring_face;
            out_ring_face_index = closest_face;
        } else {
            out_ring_face_index = -1;
        }
    } else {
        // Side connection — use sphere-style tapering
        TaperSphereRegion(faces, shape, ring, out_ring_face_index);
    }
}

// ============================================================================
// Face generation with connection ring modifications
// ============================================================================

MatchedFaces ConnectionFaceMatcher::GenerateWithConnections(
    const BodyNode& node,
    const std::vector<ConnectionRing>& rings) const
{
    MatchedFaces result;

    if (rings.empty()) {
        result.faces = m_faceGen.Generate(node.shape);
        return result;
    }

    // Generate base faces first
    result.faces = m_faceGen.Generate(node.shape);

    // Apply tapering for each connection ring
    for (const auto& ring : rings) {
        int ring_face_index = -1;

        switch (node.shape.type) {
        case ShapeType::Cylinder:
            TaperCylinderEnd(result.faces, node.shape, ring, ring_face_index);
            break;
        case ShapeType::Sphere:
            TaperSphereRegion(result.faces, node.shape, ring, ring_face_index);
            break;
        case ShapeType::Capsule:
            TaperCapsuleRegion(result.faces, node.shape, ring, ring_face_index);
            break;
        case ShapeType::Cone:
            // Cones already taper naturally — just find/replace the nearest face
            TaperSphereRegion(result.faces, node.shape, ring, ring_face_index);
            break;
        case ShapeType::Torus:
            TaperSphereRegion(result.faces, node.shape, ring, ring_face_index);
            break;
        }

        result.connection_face_indices.push_back(ring_face_index);
    }

    return result;
}

} // namespace BodyRenderer
