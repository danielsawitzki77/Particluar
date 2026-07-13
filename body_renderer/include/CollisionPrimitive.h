#pragma once

#include "BodyTypes.h"
#include <vector>
#include <memory>

namespace BodyRenderer {

// ============================================================================
// Ray definition
// ============================================================================

struct Ray {
    Vec3 origin;
    Vec3 direction; // should be normalized

    Ray() {}
    Ray(const Vec3& o, const Vec3& d) : origin(o), direction(d) {}
};

// ============================================================================
// Raycast hit result
// ============================================================================

struct RayHit {
    bool hit;
    float distance;   // distance along ray to hit point
    Vec3 point;       // world-space hit point
    Vec3 normal;      // surface normal at hit point

    RayHit() : hit(false), distance(0.0f) {}
};

// ============================================================================
// Overlap result
// ============================================================================

struct OverlapResult {
    bool overlapping;
    float penetrationDepth; // approximate penetration depth (0 if no overlap)
    Vec3 separationAxis;    // direction to move this primitive to resolve overlap

    OverlapResult() : overlapping(false), penetrationDepth(0.0f) {}
};

// ============================================================================
// Collision primitive base class
// ============================================================================

class CollisionPrimitive {
public:
    virtual ~CollisionPrimitive() {}

    // Raycast against this primitive in its local space.
    // The ray should be transformed into the primitive's local coordinate system
    // before calling.
    virtual RayHit Raycast(const Ray& localRay) const = 0;

    // Test overlap with another primitive.
    // Both primitives are assumed to be in world space (positions applied).
    virtual OverlapResult TestOverlap(const CollisionPrimitive& other,
                                      const Mat4& thisWorld,
                                      const Mat4& otherWorld) const = 0;

    // Get the bounding sphere radius (for broad-phase culling)
    virtual float GetBoundingRadius() const = 0;

    // Get the center offset in local space (usually origin)
    virtual Vec3 GetLocalCenter() const { return Vec3(0, 0, 0); }

    // Clone for ownership
    virtual CollisionPrimitive* Clone() const = 0;

    // Identify the type for double-dispatch overlap checks
    enum PrimitiveType {
        PRIM_SPHERE,
        PRIM_CAPSULE,
        PRIM_CYLINDER
    };
    virtual PrimitiveType GetType() const = 0;
};

// ============================================================================
// Sphere collision primitive
// Used for: Sphere shapes, Torus shapes (bounding sphere approximation)
// ============================================================================

class CollisionSphere : public CollisionPrimitive {
public:
    float radius;

    CollisionSphere() : radius(0.5f) {}
    explicit CollisionSphere(float r) : radius(r) {}

    RayHit Raycast(const Ray& localRay) const override;
    OverlapResult TestOverlap(const CollisionPrimitive& other,
                              const Mat4& thisWorld,
                              const Mat4& otherWorld) const override;
    float GetBoundingRadius() const override { return radius; }
    CollisionPrimitive* Clone() const override { return new CollisionSphere(*this); }
    PrimitiveType GetType() const override { return PRIM_SPHERE; }
};

// ============================================================================
// Capsule collision primitive
// Used for: Cylinder shapes, Cone shapes, Capsule shapes
// A line segment with a radius — cheapest approximation for elongated shapes.
// Oriented along Y axis: from (0, -halfHeight, 0) to (0, +halfHeight, 0)
// ============================================================================

class CollisionCapsule : public CollisionPrimitive {
public:
    float radius;
    float halfHeight; // half of the line segment length (total height = 2*halfHeight + 2*radius)

    CollisionCapsule() : radius(0.5f), halfHeight(0.5f) {}
    CollisionCapsule(float r, float hh) : radius(r), halfHeight(hh) {}

    RayHit Raycast(const Ray& localRay) const override;
    OverlapResult TestOverlap(const CollisionPrimitive& other,
                              const Mat4& thisWorld,
                              const Mat4& otherWorld) const override;
    float GetBoundingRadius() const override { return halfHeight + radius; }
    CollisionPrimitive* Clone() const override { return new CollisionCapsule(*this); }
    PrimitiveType GetType() const override { return PRIM_CAPSULE; }
};

// ============================================================================
// Cylinder collision primitive (axis-aligned along Y)
// Used when a tighter fit than capsule is needed for flat-ended shapes.
// ============================================================================

class CollisionCylinder : public CollisionPrimitive {
public:
    float radius;
    float halfHeight;

    CollisionCylinder() : radius(0.5f), halfHeight(0.5f) {}
    CollisionCylinder(float r, float hh) : radius(r), halfHeight(hh) {}

    RayHit Raycast(const Ray& localRay) const override;
    OverlapResult TestOverlap(const CollisionPrimitive& other,
                              const Mat4& thisWorld,
                              const Mat4& otherWorld) const override;
    float GetBoundingRadius() const override;
    CollisionPrimitive* Clone() const override { return new CollisionCylinder(*this); }
    PrimitiveType GetType() const override { return PRIM_CYLINDER; }
};

// ============================================================================
// Factory: Create the cheapest collision primitive for a given shape
// ============================================================================

// Creates an appropriate collision primitive for the given shape parameters.
// Mapping:
//   Sphere  -> CollisionSphere (exact fit)
//   Torus   -> CollisionSphere (bounding sphere: majorRadius + minorRadius)
//   Cylinder-> CollisionCapsule (cheapest elongated approximation)
//   Cone    -> CollisionCapsule (using max radius)
//   Capsule -> CollisionCapsule (exact fit)
CollisionPrimitive* CreateCollisionPrimitive(const ShapeParams& shape);

// ============================================================================
// Utility: Transform a ray into a primitive's local space
// ============================================================================

Ray TransformRayToLocal(const Ray& worldRay, const Mat4& worldTransform);

// ============================================================================
// Utility: Compute inverse of a rigid-body transform (rotation + translation only)
// ============================================================================

Mat4 InverseRigidTransform(const Mat4& m);

} // namespace BodyRenderer
