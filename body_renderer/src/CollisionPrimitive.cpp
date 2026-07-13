#include "CollisionPrimitive.h"
#include "GeomUtil.h"
#include <cmath>
#include <algorithm>
#include <cfloat>

namespace BodyRenderer {

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

Ray TransformRayToLocal(const Ray& worldRay, const Mat4& worldTransform)
{
    Mat4 inv = InverseRigidTransform(worldTransform);
    Ray localRay;
    localRay.origin = inv.TransformPoint(worldRay.origin);
    localRay.direction = inv.TransformDirection(worldRay.direction).Normalized();
    return localRay;
}

// ============================================================================
// CollisionSphere::Raycast
// Standard ray-sphere intersection (sphere at origin with given radius)
// ============================================================================

RayHit CollisionSphere::Raycast(const Ray& localRay) const
{
    RayHit result;

    // Solve: |O + t*D|^2 = r^2
    // t^2*(D.D) + 2t*(O.D) + (O.O - r^2) = 0
    float a = localRay.direction.Dot(localRay.direction);
    float b = 2.0f * localRay.origin.Dot(localRay.direction);
    float c = localRay.origin.Dot(localRay.origin) - radius * radius;

    float discriminant = b * b - 4.0f * a * c;
    if (discriminant < 0.0f) return result;

    float sqrtDisc = std::sqrt(discriminant);
    float t0 = (-b - sqrtDisc) / (2.0f * a);
    float t1 = (-b + sqrtDisc) / (2.0f * a);

    float t = t0;
    if (t < 0.0f) t = t1;
    if (t < 0.0f) return result;

    result.hit = true;
    result.distance = t;
    result.point = localRay.origin + localRay.direction * t;
    result.normal = result.point * (1.0f / radius);
    return result;
}

// ============================================================================
// CollisionSphere::TestOverlap
// ============================================================================

OverlapResult CollisionSphere::TestOverlap(const CollisionPrimitive& other,
                                            const Mat4& thisWorld,
                                            const Mat4& otherWorld) const
{
    OverlapResult result;
    Vec3 thisPos = GeomUtil::GetWorldPosition(thisWorld);

    switch (other.GetType()) {
    case PRIM_SPHERE: {
        const CollisionSphere& otherSphere = static_cast<const CollisionSphere&>(other);
        Vec3 otherPos = GeomUtil::GetWorldPosition(otherWorld);
        Vec3 diff = thisPos - otherPos;
        float dist = diff.Length();
        float sumRadii = radius + otherSphere.radius;

        if (dist < sumRadii) {
            result.overlapping = true;
            result.penetrationDepth = sumRadii - dist;
            if (dist > 1e-9f) {
                result.separationAxis = diff * (1.0f / dist);
            } else {
                result.separationAxis = Vec3(0, 1, 0);
            }
        }
        break;
    }
    case PRIM_CAPSULE: {
        const CollisionCapsule& cap = static_cast<const CollisionCapsule&>(other);
        Vec3 otherPos = GeomUtil::GetWorldPosition(otherWorld);

        // Capsule segment endpoints in world space
        Vec3 localA(0, -cap.halfHeight, 0);
        Vec3 localB(0, cap.halfHeight, 0);
        Vec3 capA = otherWorld.TransformPoint(localA);
        Vec3 capB = otherWorld.TransformPoint(localB);

        // Closest point on capsule segment to sphere center
        Vec3 closest;
        GeomUtil::ClosestPointOnSegment(capA, capB, thisPos, closest);

        Vec3 diff = thisPos - closest;
        float dist = diff.Length();
        float sumRadii = radius + cap.radius;

        if (dist < sumRadii) {
            result.overlapping = true;
            result.penetrationDepth = sumRadii - dist;
            if (dist > 1e-9f) {
                result.separationAxis = diff * (1.0f / dist);
            } else {
                result.separationAxis = Vec3(0, 1, 0);
            }
        }
        break;
    }
    case PRIM_CYLINDER: {
        // Approximate cylinder as capsule for overlap (conservative)
        const CollisionCylinder& cyl = static_cast<const CollisionCylinder&>(other);
        CollisionCapsule approx(cyl.radius, cyl.halfHeight);
        return TestOverlap(approx, thisWorld, otherWorld);
    }
    }

    return result;
}

// ============================================================================
// CollisionCapsule::Raycast
// Ray vs capsule: test infinite cylinder + two hemispherical caps
// ============================================================================

RayHit CollisionCapsule::Raycast(const Ray& localRay) const
{
    RayHit result;
    float bestT = FLT_MAX;

    // The capsule is a line segment from (0, -halfHeight, 0) to (0, halfHeight, 0)
    // with radius.

    // Step 1: Ray vs infinite cylinder along Y axis
    float dx = localRay.direction.x;
    float dz = localRay.direction.z;
    float ox = localRay.origin.x;
    float oz = localRay.origin.z;

    float a = dx * dx + dz * dz;
    float b = 2.0f * (ox * dx + oz * dz);
    float c = ox * ox + oz * oz - radius * radius;

    if (a > 1e-12f) {
        float disc = b * b - 4.0f * a * c;
        if (disc >= 0.0f) {
            float sqrtDisc = std::sqrt(disc);
            float t0 = (-b - sqrtDisc) / (2.0f * a);
            float t1 = (-b + sqrtDisc) / (2.0f * a);

            // Check both t values against the cylinder's Y bounds
            for (int i = 0; i < 2; ++i) {
                float t = (i == 0) ? t0 : t1;
                if (t < 0.0f) continue;
                float y = localRay.origin.y + t * localRay.direction.y;
                if (y >= -halfHeight && y <= halfHeight) {
                    if (t < bestT) {
                        bestT = t;
                        result.hit = true;
                        result.distance = t;
                        result.point = localRay.origin + localRay.direction * t;
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
        Vec3 sphereCenter(0, halfHeight, 0);
        Vec3 oc = localRay.origin - sphereCenter;
        float sa = localRay.direction.Dot(localRay.direction);
        float sb = 2.0f * oc.Dot(localRay.direction);
        float sc = oc.Dot(oc) - radius * radius;
        float disc = sb * sb - 4.0f * sa * sc;
        if (disc >= 0.0f) {
            float sqrtDisc = std::sqrt(disc);
            float t0 = (-sb - sqrtDisc) / (2.0f * sa);
            float t1 = (-sb + sqrtDisc) / (2.0f * sa);
            for (int i = 0; i < 2; ++i) {
                float t = (i == 0) ? t0 : t1;
                if (t < 0.0f) continue;
                Vec3 p = localRay.origin + localRay.direction * t;
                // Must be in the top hemisphere (y >= halfHeight)
                if (p.y >= halfHeight && t < bestT) {
                    bestT = t;
                    result.hit = true;
                    result.distance = t;
                    result.point = p;
                    result.normal = (p - sphereCenter) * (1.0f / radius);
                    break;
                }
            }
        }
    }

    // Step 3: Ray vs bottom hemisphere (center at (0, -halfHeight, 0))
    {
        Vec3 sphereCenter(0, -halfHeight, 0);
        Vec3 oc = localRay.origin - sphereCenter;
        float sa = localRay.direction.Dot(localRay.direction);
        float sb = 2.0f * oc.Dot(localRay.direction);
        float sc = oc.Dot(oc) - radius * radius;
        float disc = sb * sb - 4.0f * sa * sc;
        if (disc >= 0.0f) {
            float sqrtDisc = std::sqrt(disc);
            float t0 = (-sb - sqrtDisc) / (2.0f * sa);
            float t1 = (-sb + sqrtDisc) / (2.0f * sa);
            for (int i = 0; i < 2; ++i) {
                float t = (i == 0) ? t0 : t1;
                if (t < 0.0f) continue;
                Vec3 p = localRay.origin + localRay.direction * t;
                // Must be in the bottom hemisphere (y <= -halfHeight)
                if (p.y <= -halfHeight && t < bestT) {
                    bestT = t;
                    result.hit = true;
                    result.distance = t;
                    result.point = p;
                    result.normal = (p - sphereCenter) * (1.0f / radius);
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
                                             const Mat4& thisWorld,
                                             const Mat4& otherWorld) const
{
    OverlapResult result;

    // This capsule's segment endpoints in world space
    Vec3 thisA = thisWorld.TransformPoint(Vec3(0, -halfHeight, 0));
    Vec3 thisB = thisWorld.TransformPoint(Vec3(0, halfHeight, 0));

    switch (other.GetType()) {
    case PRIM_SPHERE: {
        const CollisionSphere& sphere = static_cast<const CollisionSphere&>(other);
        Vec3 spherePos = GeomUtil::GetWorldPosition(otherWorld);

        Vec3 closest;
        GeomUtil::ClosestPointOnSegment(thisA, thisB, spherePos, closest);

        Vec3 diff = spherePos - closest;
        float dist = diff.Length();
        float sumRadii = radius + sphere.radius;

        if (dist < sumRadii) {
            result.overlapping = true;
            result.penetrationDepth = sumRadii - dist;
            if (dist > 1e-9f) {
                result.separationAxis = diff * (-1.0f / dist);
            } else {
                result.separationAxis = Vec3(0, 1, 0);
            }
        }
        break;
    }
    case PRIM_CAPSULE: {
        const CollisionCapsule& otherCap = static_cast<const CollisionCapsule&>(other);
        Vec3 otherA = otherWorld.TransformPoint(Vec3(0, -otherCap.halfHeight, 0));
        Vec3 otherB = otherWorld.TransformPoint(Vec3(0, otherCap.halfHeight, 0));

        Vec3 closest1, closest2;
        GeomUtil::ClosestDistSegmentSegment(thisA, thisB, otherA, otherB, closest1, closest2);

        Vec3 diff = closest1 - closest2;
        float dist = diff.Length();
        float sumRadii = radius + otherCap.radius;

        if (dist < sumRadii) {
            result.overlapping = true;
            result.penetrationDepth = sumRadii - dist;
            if (dist > 1e-9f) {
                result.separationAxis = diff * (1.0f / dist);
            } else {
                result.separationAxis = Vec3(0, 1, 0);
            }
        }
        break;
    }
    case PRIM_CYLINDER: {
        // Approximate cylinder as capsule for overlap
        const CollisionCylinder& cyl = static_cast<const CollisionCylinder&>(other);
        CollisionCapsule approx(cyl.radius, cyl.halfHeight);
        return TestOverlap(approx, thisWorld, otherWorld);
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

RayHit CollisionCylinder::Raycast(const Ray& localRay) const
{
    RayHit result;
    float bestT = FLT_MAX;

    float dx = localRay.direction.x;
    float dy = localRay.direction.y;
    float dz = localRay.direction.z;
    float ox = localRay.origin.x;
    float oy = localRay.origin.y;
    float oz = localRay.origin.z;

    // Side: infinite cylinder along Y
    float a = dx * dx + dz * dz;
    float b = 2.0f * (ox * dx + oz * dz);
    float c = ox * ox + oz * oz - radius * radius;

    if (a > 1e-12f) {
        float disc = b * b - 4.0f * a * c;
        if (disc >= 0.0f) {
            float sqrtDisc = std::sqrt(disc);
            float t0 = (-b - sqrtDisc) / (2.0f * a);
            float t1 = (-b + sqrtDisc) / (2.0f * a);

            for (int i = 0; i < 2; ++i) {
                float t = (i == 0) ? t0 : t1;
                if (t < 0.0f) continue;
                float y = oy + t * dy;
                if (y >= -halfHeight && y <= halfHeight && t < bestT) {
                    bestT = t;
                    result.hit = true;
                    result.distance = t;
                    result.point = localRay.origin + localRay.direction * t;
                    result.normal = Vec3(result.point.x, 0, result.point.z) * (1.0f / radius);
                    break;
                }
            }
        }
    }

    // Top cap (y = +halfHeight, disk of given radius)
    if (std::fabs(dy) > 1e-9f) {
        float t = (halfHeight - oy) / dy;
        if (t >= 0.0f && t < bestT) {
            float hx = ox + t * dx;
            float hz = oz + t * dz;
            if (hx * hx + hz * hz <= radius * radius) {
                bestT = t;
                result.hit = true;
                result.distance = t;
                result.point = localRay.origin + localRay.direction * t;
                result.normal = Vec3(0, 1, 0);
            }
        }
    }

    // Bottom cap (y = -halfHeight)
    if (std::fabs(dy) > 1e-9f) {
        float t = (-halfHeight - oy) / dy;
        if (t >= 0.0f && t < bestT) {
            float hx = ox + t * dx;
            float hz = oz + t * dz;
            if (hx * hx + hz * hz <= radius * radius) {
                bestT = t;
                result.hit = true;
                result.distance = t;
                result.point = localRay.origin + localRay.direction * t;
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
                                              const Mat4& thisWorld,
                                              const Mat4& otherWorld) const
{
    // Approximate this cylinder as a capsule for overlap testing.
    CollisionCapsule approx(radius, halfHeight);
    return approx.TestOverlap(other, thisWorld, otherWorld);
}

// ============================================================================
// Factory: CreateCollisionPrimitive
// ============================================================================

CollisionPrimitive* CreateCollisionPrimitive(const ShapeParams& shape)
{
    switch (shape.type) {
    case ShapeType::Sphere:
        return new CollisionSphere(shape.radius);

    case ShapeType::Torus:
        return new CollisionSphere(shape.majorRadius + shape.minorRadius);

    case ShapeType::Cylinder:
        return new CollisionCapsule(shape.radius, shape.height * 0.5f);

    case ShapeType::Cone:
        return new CollisionCapsule(shape.radius, shape.height * 0.5f);

    case ShapeType::Capsule:
        return new CollisionCapsule(shape.radius, shape.height * 0.5f);
    }

    // Fallback: bounding sphere
    return new CollisionSphere(1.0f);
}

} // namespace BodyRenderer
