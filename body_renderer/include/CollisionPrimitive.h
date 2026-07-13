#pragma once

#include "BodyTypes.h"
#include <vector>
#include <memory>
#include <cmath>

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
    float distance; // distance along ray to hit point
    Vec3 point;     // world-space hit point
    Vec3 normal;    // surface normal at hit point

    RayHit() : hit(false), distance(1e30f) {}
};

// ============================================================================
// Collision primitive base class (polymorphic)
// ============================================================================

class CollisionPrimitive {
public:
    virtual ~CollisionPrimitive() {}

    // Ray intersection test. Returns closest hit along the ray.
    virtual RayHit Raycast(const Ray& ray) const = 0;

    // Overlap test against another primitive.
    virtual bool Overlaps(const CollisionPrimitive& other) const = 0;

    // Get bounding sphere center and radius (for broad-phase)
    virtual Vec3 GetCenter() const = 0;
    virtual float GetBoundingRadius() const = 0;

    // Clone for polymorphic copy
    virtual std::unique_ptr<CollisionPrimitive> Clone() const = 0;

    // Type tag for double-dispatch in overlap tests
    enum class Type { Sphere, Capsule, Cylinder, Cone };
    virtual Type GetType() const = 0;
};

// ============================================================================
// Collision Sphere
// ============================================================================

class CollisionSphere : public CollisionPrimitive {
public:
    Vec3 center;
    float radius;

    CollisionSphere() : radius(0.0f) {}
    CollisionSphere(const Vec3& c, float r) : center(c), radius(r) {}

    RayHit Raycast(const Ray& ray) const override;
    bool Overlaps(const CollisionPrimitive& other) const override;
    Vec3 GetCenter() const override { return center; }
    float GetBoundingRadius() const override { return radius; }
    std::unique_ptr<CollisionPrimitive> Clone() const override {
        return std::unique_ptr<CollisionPrimitive>(new CollisionSphere(*this));
    }
    Type GetType() const override { return Type::Sphere; }
};

// ============================================================================
// Collision Capsule (two hemispheres + cylinder along Y axis in local space)
// ============================================================================

class CollisionCapsule : public CollisionPrimitive {
public:
    Vec3 pointA;  // center of bottom hemisphere
    Vec3 pointB;  // center of top hemisphere
    float radius;

    CollisionCapsule() : radius(0.0f) {}
    CollisionCapsule(const Vec3& a, const Vec3& b, float r) : pointA(a), pointB(b), radius(r) {}

    RayHit Raycast(const Ray& ray) const override;
    bool Overlaps(const CollisionPrimitive& other) const override;
    Vec3 GetCenter() const override {
        return Vec3((pointA.x + pointB.x) * 0.5f,
                    (pointA.y + pointB.y) * 0.5f,
                    (pointA.z + pointB.z) * 0.5f);
    }
    float GetBoundingRadius() const override {
        Vec3 mid = GetCenter();
        float half_len = (pointB - pointA).Length() * 0.5f;
        return half_len + radius;
    }
    std::unique_ptr<CollisionPrimitive> Clone() const override {
        return std::unique_ptr<CollisionPrimitive>(new CollisionCapsule(*this));
    }
    Type GetType() const override { return Type::Capsule; }
};

// ============================================================================
// Collision Cylinder (axis-aligned along local Y)
// ============================================================================

class CollisionCylinder : public CollisionPrimitive {
public:
    Vec3 baseCenter;  // center of bottom cap
    Vec3 topCenter;   // center of top cap
    float radius;

    CollisionCylinder() : radius(0.0f) {}
    CollisionCylinder(const Vec3& base, const Vec3& top, float r)
        : baseCenter(base), topCenter(top), radius(r) {}

    RayHit Raycast(const Ray& ray) const override;
    bool Overlaps(const CollisionPrimitive& other) const override;
    Vec3 GetCenter() const override {
        return Vec3((baseCenter.x + topCenter.x) * 0.5f,
                    (baseCenter.y + topCenter.y) * 0.5f,
                    (baseCenter.z + topCenter.z) * 0.5f);
    }
    float GetBoundingRadius() const override {
        float half_height = (topCenter - baseCenter).Length() * 0.5f;
        return std::sqrt(half_height * half_height + radius * radius);
    }
    std::unique_ptr<CollisionPrimitive> Clone() const override {
        return std::unique_ptr<CollisionPrimitive>(new CollisionCylinder(*this));
    }
    Type GetType() const override { return Type::Cylinder; }
};

// ============================================================================
// Collision Cone (tip at top, base at bottom along local Y)
// ============================================================================

class CollisionCone : public CollisionPrimitive {
public:
    Vec3 baseCenter; // center of the base circle
    Vec3 tip;        // apex point
    float baseRadius;

    CollisionCone() : baseRadius(0.0f) {}
    CollisionCone(const Vec3& base, const Vec3& t, float r)
        : baseCenter(base), tip(t), baseRadius(r) {}

    RayHit Raycast(const Ray& ray) const override;
    bool Overlaps(const CollisionPrimitive& other) const override;
    Vec3 GetCenter() const override {
        // Centroid of a cone is 1/4 from the base
        return Vec3(baseCenter.x + (tip.x - baseCenter.x) * 0.25f,
                    baseCenter.y + (tip.y - baseCenter.y) * 0.25f,
                    baseCenter.z + (tip.z - baseCenter.z) * 0.25f);
    }
    float GetBoundingRadius() const override {
        Vec3 center = GetCenter();
        float to_tip = (tip - center).Length();
        float to_base_edge = std::sqrt(
            (baseCenter - center).Dot(baseCenter - center) + baseRadius * baseRadius);
        return (to_tip > to_base_edge) ? to_tip : to_base_edge;
    }
    std::unique_ptr<CollisionPrimitive> Clone() const override {
        return std::unique_ptr<CollisionPrimitive>(new CollisionCone(*this));
    }
    Type GetType() const override { return Type::Cone; }
};

// ============================================================================
// Factory: create collision primitive from shape params + world transform
// ============================================================================

// Creates the cheapest appropriate collision primitive for a given shape type:
//   Sphere   -> CollisionSphere
//   Cylinder -> CollisionCylinder
//   Cone     -> CollisionCone
//   Capsule  -> CollisionCapsule
//   Torus    -> CollisionSphere (bounding sphere approximation)
std::unique_ptr<CollisionPrimitive> CreateCollisionPrimitive(
    const ShapeParams& shape, const Mat4& worldTransform);

// ============================================================================
// Utility: Build collision primitives for all nodes in a body tree
// ============================================================================

struct NodeCollider {
    std::string nodeName;
    std::unique_ptr<CollisionPrimitive> primitive;
};

// Traverses the body tree and creates collision primitives for every node
std::vector<NodeCollider> BuildBodyColliders(const Body& body);

// Raycast against all colliders in a body, return closest hit + node name
struct BodyRayHit {
    bool hit;
    float distance;
    Vec3 point;
    Vec3 normal;
    std::string nodeName;

    BodyRayHit() : hit(false), distance(1e30f) {}
};

BodyRayHit RaycastBody(const std::vector<NodeCollider>& colliders, const Ray& ray);

// Check if any two nodes in two bodies overlap
struct BodyOverlapResult {
    bool overlaps;
    std::string nodeA;
    std::string nodeB;

    BodyOverlapResult() : overlaps(false) {}
};

BodyOverlapResult CheckBodyOverlap(
    const std::vector<NodeCollider>& collidersA,
    const std::vector<NodeCollider>& collidersB);

} // namespace BodyRenderer
