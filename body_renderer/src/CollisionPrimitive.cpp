#include "CollisionPrimitive.h"
#include <cmath>
#include <algorithm>
#include <cfloat>

namespace BodyRenderer {

// ============================================================================
// Helper: closest point on line segment to a point
// Segment from A to B, returns parameter t in [0,1] and the closest point
// ============================================================================

static float ClosestPointOnSegment(const Vec3& a, const Vec3& b, const Vec3& point, Vec3& closest)
{
    Vec3 ab = b - a;
    float ab_len_sq = ab.Dot(ab);
    if (ab_len_sq < 1e-12f) {
        closest = a;
        return 0.0f;
    }
    float t = (point - a).Dot(ab) / ab_len_sq;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    closest = a + ab * t;
    return t;
}

// ============================================================================
// Helper: closest distance between two line segments
// Returns the squared distance and the closest points on each segment
// ============================================================================

static float ClosestDistSegmentSegment(
    const Vec3& p1, const Vec3& q1,  // segment 1: p1 to q1
    const Vec3& p2, const Vec3& q2,  // segment 2: p2 to q2
    Vec3& closest1, Vec3& closest2)
{
    Vec3 d1 = q1 - p1; // direction of segment 1
    Vec3 d2 = q2 - p2; // direction of segment 2
    Vec3 r = p1 - p2;

    float a = d1.Dot(d1); // squared length of segment 1
    float e = d2.Dot(d2); // squared length of segment 2
    float f = d2.Dot(r);

    float s, t;

    if (a <= 1e-12f && e <= 1e-12f) {
        // Both segments degenerate to points
        closest1 = p1;
        closest2 = p2;
        Vec3 diff = closest1 - closest2;
        return diff.Dot(diff);
    }

    if (a <= 1e-12f) {
        // First segment degenerates to a point
        s = 0.0f;
        t = f / e;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
    } else {
        float c = d1.Dot(r);
        if (e <= 1e-12f) {
            // Second segment degenerates to a point
            t = 0.0f;
            s = -c / a;
            if (s < 0.0f) s = 0.0f;
            if (s > 1.0f) s = 1.0f;
        } else {
            // General case
            float b = d1.Dot(d2);
            float denom = a * e - b * b;

            if (denom != 0.0f) {
                s = (b * f - c * e) / denom;
                if (s < 0.0f) s = 0.0f;
                if (s > 1.0f) s = 1.0f;
            } else {
                s = 0.0f;
            }

            t = (b * s + f) / e;

            if (t < 0.0f) {
                t = 0.0f;
                s = -c / a;
                if (s < 0.0f) s = 0.0f;
                if (s > 1.0f) s = 1.0f;
            } else if (t > 1.0f) {
                t = 1.0f;
                s = (b - c) / a;
                if (s < 0.0f) s = 0.0f;
                if (s > 1.0f) s = 1.0f;
            }
        }
    }

    closest1 = p1 + d1 * s;
    closest2 = p2 + d2 * t;
    Vec3 diff = closest1 - closest2;
    return diff.Dot(diff);
}

// ============================================================================
// Helper: extract position from world matrix
// ============================================================================

static Vec3 GetWorldPosition(const Mat4& world)
{
    return Vec3(world.m[12], world.m[13], world.m[14]);
}

// ============================================================================
// InverseRigidTransform
// For a matrix composed of rotation R and translation T:
// Inverse = [R^T | -R^T * T]
// ============================================================================

Mat4 InverseRigidTransform(const Mat4& m)
{
    Mat4 inv;
    // Transpose the 3x3 rotation part
    inv.m[0] = m.m[0];  inv.m[1] = m.m[4];  inv.m[2] = m.m[8];
    inv.m[4] = m.m[1];  inv.m[5] = m.m[5];  inv.m[6] = m.m[9];
    inv.m[8] = m.m[2];  inv.m[9] = m.m[6];  inv.m[10] = m.m[10];

    // Translation = -R^T * T
    float tx = m.m[12], ty = m.m[13], tz = m.m[14];
    inv.m[12] = -(inv.m[0] * tx + inv.m[4] * ty + inv.m[8] * tz);
    inv.m[13] = -(inv.m[1] * tx + inv.m[5] * ty + inv.m[9] * tz);
    inv.m[14] = -(inv.m[2] * tx + inv.m[6] * ty + inv.m[10] * tz);

    inv.m[3] = 0.0f; inv.m[7] = 0.0f; inv.m[11] = 0.0f; inv.m[15] = 1.0f;
    return inv;
}

// ============================================================================
// TransformRayToLocal
// ============================================================================

Ray TransformRayToLocal(const Ray& world_ray, const Mat4& world_transform)
{
    Mat4 inv = InverseRigidTransform(world_transform);
    Ray local;
    local.origin = inv.TransformPoint(world_ray.origin);
    local.direction = inv.TransformDirection(world_ray.direction).Normalized();
    return local;
}

// ============================================================================
// CollisionSphere::Raycast
// Standard ray-sphere intersection (sphere at origin with given radius)
// ============================================================================

RayHit CollisionSphere::Raycast(const Ray& local_ray) const
{
    RayHit result;

    // Solve: |O + t*D|^2 = r^2
    // t^2*(D.D) + 2t*(O.D) + (O.O - r^2) = 0
    float a = local_ray.direction.Dot(local_ray.direction);
    float b = 2.0f * local_ray.origin.Dot(local_ray.direction);
    float c = local_ray.origin.Dot(local_ray.origin) - radius * radius;

    float discriminant = b * b - 4.0f * a * c;
    if (discriminant < 0.0f) return result;

    float sqrt_disc = std::sqrt(discriminant);
    float t0 = (-b - sqrt_disc) / (2.0f * a);
    float t1 = (-b + sqrt_disc) / (2.0f * a);

    float t = t0;
    if (t < 0.0f) t = t1;
    if (t < 0.0f) return result;

    result.hit = true;
    result.distance = t;
    result.point = local_ray.origin + local_ray.direction * t;
    result.normal = result.point * (1.0f / radius); // normalized position = normal for unit sphere
    return result;
}

// ============================================================================
// CollisionSphere::TestOverlap
// ============================================================================

OverlapResult CollisionSphere::TestOverlap(const CollisionPrimitive& other,
                                            const Mat4& this_world,
                                            const Mat4& other_world) const
{
    OverlapResult result;
    Vec3 this_pos = GetWorldPosition(this_world);

    switch (other.GetType()) {
    case PRIM_SPHERE: {
        const CollisionSphere& other_sphere = static_cast<const CollisionSphere&>(other);
        Vec3 other_pos = GetWorldPosition(other_world);
        Vec3 diff = this_pos - other_pos;
        float dist = diff.Length();
        float sum_radii = radius + other_sphere.radius;

        if (dist < sum_radii) {
            result.overlapping = true;
            result.penetration_depth = sum_radii - dist;
            if (dist > 1e-9f) {
                result.separation_axis = diff * (1.0f / dist);
            } else {
                result.separation_axis = Vec3(0, 1, 0);
            }
        }
        break;
    }
    case PRIM_CAPSULE: {
        const CollisionCapsule& cap = static_cast<const CollisionCapsule&>(other);
        Vec3 other_pos = GetWorldPosition(other_world);

        // Capsule segment endpoints in world space
        Vec3 local_a(0, -cap.halfHeight, 0);
        Vec3 local_b(0, cap.halfHeight, 0);
        Vec3 cap_a = other_world.TransformPoint(local_a);
        Vec3 cap_b = other_world.TransformPoint(local_b);

        // Closest point on capsule segment to sphere center
        Vec3 closest;
        ClosestPointOnSegment(cap_a, cap_b, this_pos, closest);

        Vec3 diff = this_pos - closest;
        float dist = diff.Length();
        float sum_radii = radius + cap.radius;

        if (dist < sum_radii) {
            result.overlapping = true;
            result.penetration_depth = sum_radii - dist;
            if (dist > 1e-9f) {
                result.separation_axis = diff * (1.0f / dist);
            } else {
                result.separation_axis = Vec3(0, 1, 0);
            }
        }
        break;
    }
    case PRIM_CYLINDER: {
        // Approximate cylinder as capsule for overlap (conservative)
        const CollisionCylinder& cyl = static_cast<const CollisionCylinder&>(other);
        CollisionCapsule approx(cyl.radius, cyl.halfHeight);
        return TestOverlap(approx, this_world, other_world);
    }
    }

    return result;
}

// ============================================================================
// CollisionCapsule::Raycast
// Ray vs capsule: test infinite cylinder + two hemispherical caps
// ============================================================================

RayHit CollisionCapsule::Raycast(const Ray& local_ray) const
{
    RayHit result;
    float best_t = FLT_MAX;

    // The capsule is a line segment from (0, -halfHeight, 0) to (0, halfHeight, 0)
    // with radius.

    // Step 1: Ray vs infinite cylinder along Y axis
    // Project ray onto XZ plane: solve (ox + t*dx)^2 + (oz + t*dz)^2 = r^2
    float dx = local_ray.direction.x;
    float dz = local_ray.direction.z;
    float ox = local_ray.origin.x;
    float oz = local_ray.origin.z;

    float a = dx * dx + dz * dz;
    float b = 2.0f * (ox * dx + oz * dz);
    float c = ox * ox + oz * oz - radius * radius;

    if (a > 1e-12f) {
        float disc = b * b - 4.0f * a * c;
        if (disc >= 0.0f) {
            float sqrt_disc = std::sqrt(disc);
            float t0 = (-b - sqrt_disc) / (2.0f * a);
            float t1 = (-b + sqrt_disc) / (2.0f * a);

            // Check both t values against the cylinder's Y bounds
            for (int i = 0; i < 2; ++i) {
                float t = (i == 0) ? t0 : t1;
                if (t < 0.0f) continue;
                float y = local_ray.origin.y + t * local_ray.direction.y;
                if (y >= -halfHeight && y <= halfHeight) {
                    if (t < best_t) {
                        best_t = t;
                        result.hit = true;
                        result.distance = t;
                        result.point = local_ray.origin + local_ray.direction * t;
                        // Normal is radial in XZ plane
                        result.normal = Vec3(result.point.x, 0, result.point.z) * (1.0f / radius);
                    }
                    break; // first valid hit
                }
            }
        }
    }

    // Step 2: Ray vs top hemisphere (center at (0, halfHeight, 0))
    {
        Vec3 sphere_center(0, halfHeight, 0);
        Vec3 oc = local_ray.origin - sphere_center;
        float sa = local_ray.direction.Dot(local_ray.direction);
        float sb = 2.0f * oc.Dot(local_ray.direction);
        float sc = oc.Dot(oc) - radius * radius;
        float disc = sb * sb - 4.0f * sa * sc;
        if (disc >= 0.0f) {
            float sqrt_disc = std::sqrt(disc);
            float t0 = (-sb - sqrt_disc) / (2.0f * sa);
            float t1 = (-sb + sqrt_disc) / (2.0f * sa);
            for (int i = 0; i < 2; ++i) {
                float t = (i == 0) ? t0 : t1;
                if (t < 0.0f) continue;
                Vec3 p = local_ray.origin + local_ray.direction * t;
                // Must be in the top hemisphere (y >= halfHeight)
                if (p.y >= halfHeight && t < best_t) {
                    best_t = t;
                    result.hit = true;
                    result.distance = t;
                    result.point = p;
                    result.normal = (p - sphere_center) * (1.0f / radius);
                    break;
                }
            }
        }
    }

    // Step 3: Ray vs bottom hemisphere (center at (0, -halfHeight, 0))
    {
        Vec3 sphere_center(0, -halfHeight, 0);
        Vec3 oc = local_ray.origin - sphere_center;
        float sa = local_ray.direction.Dot(local_ray.direction);
        float sb = 2.0f * oc.Dot(local_ray.direction);
        float sc = oc.Dot(oc) - radius * radius;
        float disc = sb * sb - 4.0f * sa * sc;
        if (disc >= 0.0f) {
            float sqrt_disc = std::sqrt(disc);
            float t0 = (-sb - sqrt_disc) / (2.0f * sa);
            float t1 = (-sb + sqrt_disc) / (2.0f * sa);
            for (int i = 0; i < 2; ++i) {
                float t = (i == 0) ? t0 : t1;
                if (t < 0.0f) continue;
                Vec3 p = local_ray.origin + local_ray.direction * t;
                // Must be in the bottom hemisphere (y <= -halfHeight)
                if (p.y <= -halfHeight && t < best_t) {
                    best_t = t;
                    result.hit = true;
                    result.distance = t;
                    result.point = p;
                    result.normal = (p - sphere_center) * (1.0f / radius);
                    break;
                }
            }
        }
    }

    return result;
}

// ============================================================================
// CollisionCapsule::TestOverlap
// ============================================================================

OverlapResult CollisionCapsule::TestOverlap(const CollisionPrimitive& other,
                                             const Mat4& this_world,
                                             const Mat4& other_world) const
{
    OverlapResult result;

    // This capsule's segment endpoints in world space
    Vec3 this_a = this_world.TransformPoint(Vec3(0, -halfHeight, 0));
    Vec3 this_b = this_world.TransformPoint(Vec3(0, halfHeight, 0));

    switch (other.GetType()) {
    case PRIM_SPHERE: {
        const CollisionSphere& sphere = static_cast<const CollisionSphere&>(other);
        Vec3 sphere_pos = GetWorldPosition(other_world);

        Vec3 closest;
        ClosestPointOnSegment(this_a, this_b, sphere_pos, closest);

        Vec3 diff = sphere_pos - closest;
        float dist = diff.Length();
        float sum_radii = radius + sphere.radius;

        if (dist < sum_radii) {
            result.overlapping = true;
            result.penetration_depth = sum_radii - dist;
            if (dist > 1e-9f) {
                result.separation_axis = diff * (-1.0f / dist); // push this away from sphere
            } else {
                result.separation_axis = Vec3(0, 1, 0);
            }
        }
        break;
    }
    case PRIM_CAPSULE: {
        const CollisionCapsule& other_cap = static_cast<const CollisionCapsule&>(other);
        Vec3 other_a = other_world.TransformPoint(Vec3(0, -other_cap.halfHeight, 0));
        Vec3 other_b = other_world.TransformPoint(Vec3(0, other_cap.halfHeight, 0));

        Vec3 closest1, closest2;
        ClosestDistSegmentSegment(this_a, this_b, other_a, other_b, closest1, closest2);

        Vec3 diff = closest1 - closest2;
        float dist = diff.Length();
        float sum_radii = radius + other_cap.radius;

        if (dist < sum_radii) {
            result.overlapping = true;
            result.penetration_depth = sum_radii - dist;
            if (dist > 1e-9f) {
                result.separation_axis = diff * (1.0f / dist);
            } else {
                result.separation_axis = Vec3(0, 1, 0);
            }
        }
        break;
    }
    case PRIM_CYLINDER: {
        // Approximate cylinder as capsule for overlap
        const CollisionCylinder& cyl = static_cast<const CollisionCylinder&>(other);
        CollisionCapsule approx(cyl.radius, cyl.halfHeight);
        return TestOverlap(approx, this_world, other_world);
    }
    }

    return result;
}

// ============================================================================
// CollisionCylinder::GetBoundingRadius
// ============================================================================

float CollisionCylinder::GetBoundingRadius() const
{
    return std::sqrt(halfHeight * halfHeight + radius * radius);
}

// ============================================================================
// CollisionCylinder::Raycast
// Ray vs finite cylinder (caps + sides)
// ============================================================================

RayHit CollisionCylinder::Raycast(const Ray& local_ray) const
{
    RayHit result;
    float best_t = FLT_MAX;

    float dx = local_ray.direction.x;
    float dy = local_ray.direction.y;
    float dz = local_ray.direction.z;
    float ox = local_ray.origin.x;
    float oy = local_ray.origin.y;
    float oz = local_ray.origin.z;

    // Side: infinite cylinder along Y
    float a = dx * dx + dz * dz;
    float b = 2.0f * (ox * dx + oz * dz);
    float c = ox * ox + oz * oz - radius * radius;

    if (a > 1e-12f) {
        float disc = b * b - 4.0f * a * c;
        if (disc >= 0.0f) {
            float sqrt_disc = std::sqrt(disc);
            float t0 = (-b - sqrt_disc) / (2.0f * a);
            float t1 = (-b + sqrt_disc) / (2.0f * a);

            for (int i = 0; i < 2; ++i) {
                float t = (i == 0) ? t0 : t1;
                if (t < 0.0f) continue;
                float y = oy + t * dy;
                if (y >= -halfHeight && y <= halfHeight && t < best_t) {
                    best_t = t;
                    result.hit = true;
                    result.distance = t;
                    result.point = local_ray.origin + local_ray.direction * t;
                    result.normal = Vec3(result.point.x, 0, result.point.z) * (1.0f / radius);
                    break;
                }
            }
        }
    }

    // Top cap (y = +halfHeight, disk of given radius)
    if (std::fabs(dy) > 1e-9f) {
        float t = (halfHeight - oy) / dy;
        if (t >= 0.0f && t < best_t) {
            float hx = ox + t * dx;
            float hz = oz + t * dz;
            if (hx * hx + hz * hz <= radius * radius) {
                best_t = t;
                result.hit = true;
                result.distance = t;
                result.point = local_ray.origin + local_ray.direction * t;
                result.normal = Vec3(0, 1, 0);
            }
        }
    }

    // Bottom cap (y = -halfHeight)
    if (std::fabs(dy) > 1e-9f) {
        float t = (-halfHeight - oy) / dy;
        if (t >= 0.0f && t < best_t) {
            float hx = ox + t * dx;
            float hz = oz + t * dz;
            if (hx * hx + hz * hz <= radius * radius) {
                best_t = t;
                result.hit = true;
                result.distance = t;
                result.point = local_ray.origin + local_ray.direction * t;
                result.normal = Vec3(0, -1, 0);
            }
        }
    }

    return result;
}

// ============================================================================
// CollisionCylinder::TestOverlap
// ============================================================================

OverlapResult CollisionCylinder::TestOverlap(const CollisionPrimitive& other,
                                              const Mat4& this_world,
                                              const Mat4& other_world) const
{
    // Approximate this cylinder as a capsule for overlap testing.
    // This is the standard industry approach for real-time collision.
    CollisionCapsule approx(radius, halfHeight);
    return approx.TestOverlap(other, this_world, other_world);
}

// ============================================================================
// Factory: CreateCollisionPrimitive
// ============================================================================

CollisionPrimitive* CreateCollisionPrimitive(const ShapeParams& shape)
{
    switch (shape.type) {
    case ShapeType::Sphere:
        // Exact sphere collision
        return new CollisionSphere(shape.radius);

    case ShapeType::Torus:
        // Bounding sphere: majorRadius + minorRadius
        return new CollisionSphere(shape.majorRadius + shape.minorRadius);

    case ShapeType::Cylinder:
        // Capsule approximation (cheapest for elongated shape)
        // Cylinder height goes from -height/2 to +height/2, so halfHeight = height/2
        return new CollisionCapsule(shape.radius, shape.height * 0.5f);

    case ShapeType::Cone:
        // Capsule using the base radius (conservative bounding)
        // The cone tapers to a point, so capsule with base radius is a superset
        return new CollisionCapsule(shape.radius, shape.height * 0.5f);

    case ShapeType::Capsule:
        // Exact capsule collision — the capsule shape IS a collision capsule
        // Capsule total: hemisphere(bottom) + cylinder(height) + hemisphere(top)
        // Line segment halfHeight = height/2 (the straight cylinder portion)
        return new CollisionCapsule(shape.radius, shape.height * 0.5f);
    }

    // Fallback: bounding sphere
    return new CollisionSphere(1.0f);
}

} // namespace BodyRenderer
