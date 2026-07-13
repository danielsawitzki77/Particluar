#include "CollisionPrimitive.h"
#include "ConnectionSolver.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace BodyRenderer {

// ============================================================================
// Helper: closest point on line segment to a point
// ============================================================================

static Vec3 ClosestPointOnSegment(const Vec3& a, const Vec3& b, const Vec3& p)
{
    Vec3 ab = b - a;
    float len_sq = ab.Dot(ab);
    if (len_sq < 1e-12f) return a;
    float t = (p - a).Dot(ab) / len_sq;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return a + ab * t;
}

// ============================================================================
// Helper: distance between two line segments (for capsule-capsule)
// ============================================================================

static float SegmentSegmentDistanceSq(
    const Vec3& a0, const Vec3& a1,
    const Vec3& b0, const Vec3& b1,
    float& outS, float& outT)
{
    Vec3 d1 = a1 - a0;
    Vec3 d2 = b1 - b0;
    Vec3 r = a0 - b0;

    float a = d1.Dot(d1);
    float e = d2.Dot(d2);
    float f = d2.Dot(r);

    float s, t;

    if (a < 1e-12f && e < 1e-12f) {
        s = t = 0.0f;
    } else if (a < 1e-12f) {
        s = 0.0f;
        t = f / e;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
    } else {
        float c = d1.Dot(r);
        if (e < 1e-12f) {
            t = 0.0f;
            s = -c / a;
            if (s < 0.0f) s = 0.0f;
            if (s > 1.0f) s = 1.0f;
        } else {
            float b_val = d1.Dot(d2);
            float denom = a * e - b_val * b_val;

            if (denom > 1e-12f) {
                s = (b_val * f - c * e) / denom;
                if (s < 0.0f) s = 0.0f;
                if (s > 1.0f) s = 1.0f;
            } else {
                s = 0.0f;
            }

            t = (b_val * s + f) / e;

            if (t < 0.0f) {
                t = 0.0f;
                s = -c / a;
                if (s < 0.0f) s = 0.0f;
                if (s > 1.0f) s = 1.0f;
            } else if (t > 1.0f) {
                t = 1.0f;
                s = (b_val - c) / a;
                if (s < 0.0f) s = 0.0f;
                if (s > 1.0f) s = 1.0f;
            }
        }
    }

    outS = s;
    outT = t;

    Vec3 closest = r + d1 * s - d2 * t;
    return closest.Dot(closest);
}

// ============================================================================
// Helper: solve quadratic equation, return smallest positive root
// ============================================================================

static bool SolveQuadratic(float a, float b, float c, float& t)
{
    if (std::fabs(a) < 1e-12f) {
        if (std::fabs(b) < 1e-12f) return false;
        t = -c / b;
        return t >= 0.0f;
    }

    float disc = b * b - 4.0f * a * c;
    if (disc < 0.0f) return false;

    float sqrt_disc = std::sqrt(disc);
    float t0 = (-b - sqrt_disc) / (2.0f * a);
    float t1 = (-b + sqrt_disc) / (2.0f * a);

    if (t0 > t1) { float tmp = t0; t0 = t1; t1 = tmp; }

    if (t0 >= 0.0f) { t = t0; return true; }
    if (t1 >= 0.0f) { t = t1; return true; }
    return false;
}

// ============================================================================
// CollisionSphere
// ============================================================================

RayHit CollisionSphere::Raycast(const Ray& ray) const
{
    RayHit result;

    Vec3 oc = ray.origin - center;
    float a = ray.direction.Dot(ray.direction);
    float b = 2.0f * oc.Dot(ray.direction);
    float c = oc.Dot(oc) - radius * radius;

    float t;
    if (!SolveQuadratic(a, b, c, t)) return result;

    result.hit = true;
    result.distance = t;
    result.point = ray.origin + ray.direction * t;
    result.normal = (result.point - center).Normalized();
    return result;
}

bool CollisionSphere::Overlaps(const CollisionPrimitive& other) const
{
    switch (other.GetType()) {
    case Type::Sphere: {
        const CollisionSphere& s = static_cast<const CollisionSphere&>(other);
        float dist = (center - s.center).Length();
        return dist < (radius + s.radius);
    }
    case Type::Capsule: {
        const CollisionCapsule& cap = static_cast<const CollisionCapsule&>(other);
        Vec3 closest = ClosestPointOnSegment(cap.pointA, cap.pointB, center);
        float dist = (center - closest).Length();
        return dist < (radius + cap.radius);
    }
    case Type::Cylinder: {
        // Approximate: treat cylinder as capsule for overlap
        const CollisionCylinder& cyl = static_cast<const CollisionCylinder&>(other);
        Vec3 closest = ClosestPointOnSegment(cyl.baseCenter, cyl.topCenter, center);
        float dist = (center - closest).Length();
        return dist < (radius + cyl.radius);
    }
    case Type::Cone: {
        // Approximate: use bounding sphere of cone
        const CollisionCone& cone = static_cast<const CollisionCone&>(other);
        Vec3 cone_center = cone.GetCenter();
        float cone_radius = cone.GetBoundingRadius();
        float dist = (center - cone_center).Length();
        return dist < (radius + cone_radius);
    }
    }
    return false;
}

// ============================================================================
// CollisionCapsule
// ============================================================================

RayHit CollisionCapsule::Raycast(const Ray& ray) const
{
    RayHit result;

    // Capsule raycast: test infinite cylinder along segment, clamp, then hemispheres
    Vec3 ab = pointB - pointA;
    float ab_len = ab.Length();
    if (ab_len < 1e-9f) {
        // Degenerate to sphere
        CollisionSphere s(pointA, radius);
        return s.Raycast(ray);
    }

    Vec3 axis = ab * (1.0f / ab_len);

    // Project ray onto plane perpendicular to capsule axis
    Vec3 ao = ray.origin - pointA;
    Vec3 d_perp = ray.direction - axis * ray.direction.Dot(axis);
    Vec3 ao_perp = ao - axis * ao.Dot(axis);

    float a = d_perp.Dot(d_perp);
    float b = 2.0f * d_perp.Dot(ao_perp);
    float c = ao_perp.Dot(ao_perp) - radius * radius;

    float t_cyl;
    if (SolveQuadratic(a, b, c, t_cyl)) {
        // Check if hit is within the cylinder portion
        Vec3 hit = ray.origin + ray.direction * t_cyl;
        float proj = (hit - pointA).Dot(axis);
        if (proj >= 0.0f && proj <= ab_len) {
            result.hit = true;
            result.distance = t_cyl;
            result.point = hit;
            // Normal is perpendicular to axis
            Vec3 on_axis = pointA + axis * proj;
            result.normal = (hit - on_axis).Normalized();
            // Don't return yet; check if hemispheres give closer hit
        }
    }

    // Check hemisphere A (bottom)
    CollisionSphere sA(pointA, radius);
    RayHit hitA = sA.Raycast(ray);
    if (hitA.hit) {
        // Ensure hit is on the hemisphere facing away from B
        float proj = (hitA.point - pointA).Dot(axis);
        if (proj <= 0.0f && hitA.distance < result.distance) {
            result = hitA;
        }
    }

    // Check hemisphere B (top)
    CollisionSphere sB(pointB, radius);
    RayHit hitB = sB.Raycast(ray);
    if (hitB.hit) {
        float proj = (hitB.point - pointB).Dot(axis);
        if (proj >= 0.0f && hitB.distance < result.distance) {
            result = hitB;
        }
    }

    return result;
}

bool CollisionCapsule::Overlaps(const CollisionPrimitive& other) const
{
    switch (other.GetType()) {
    case Type::Sphere: {
        const CollisionSphere& s = static_cast<const CollisionSphere&>(other);
        Vec3 closest = ClosestPointOnSegment(pointA, pointB, s.center);
        float dist = (s.center - closest).Length();
        return dist < (radius + s.radius);
    }
    case Type::Capsule: {
        const CollisionCapsule& cap = static_cast<const CollisionCapsule&>(other);
        float s_param, t_param;
        float dist_sq = SegmentSegmentDistanceSq(pointA, pointB, cap.pointA, cap.pointB, s_param, t_param);
        float combined = radius + cap.radius;
        return dist_sq < (combined * combined);
    }
    case Type::Cylinder: {
        // Approximate cylinder as capsule
        const CollisionCylinder& cyl = static_cast<const CollisionCylinder&>(other);
        float s_param, t_param;
        float dist_sq = SegmentSegmentDistanceSq(pointA, pointB, cyl.baseCenter, cyl.topCenter, s_param, t_param);
        float combined = radius + cyl.radius;
        return dist_sq < (combined * combined);
    }
    case Type::Cone: {
        // Approximate: bounding sphere overlap
        const CollisionCone& cone = static_cast<const CollisionCone&>(other);
        Vec3 cone_center = cone.GetCenter();
        float cone_radius = cone.GetBoundingRadius();
        Vec3 closest = ClosestPointOnSegment(pointA, pointB, cone_center);
        float dist = (cone_center - closest).Length();
        return dist < (radius + cone_radius);
    }
    }
    return false;
}

// ============================================================================
// CollisionCylinder
// ============================================================================

RayHit CollisionCylinder::Raycast(const Ray& ray) const
{
    RayHit result;

    Vec3 ab = topCenter - baseCenter;
    float ab_len = ab.Length();
    if (ab_len < 1e-9f) {
        // Degenerate: treat as sphere
        CollisionSphere s(baseCenter, radius);
        return s.Raycast(ray);
    }

    Vec3 axis = ab * (1.0f / ab_len);

    // Test infinite cylinder
    Vec3 ao = ray.origin - baseCenter;
    Vec3 d_perp = ray.direction - axis * ray.direction.Dot(axis);
    Vec3 ao_perp = ao - axis * ao.Dot(axis);

    float a = d_perp.Dot(d_perp);
    float b = 2.0f * d_perp.Dot(ao_perp);
    float c = ao_perp.Dot(ao_perp) - radius * radius;

    float t_cyl;
    if (SolveQuadratic(a, b, c, t_cyl)) {
        Vec3 hit = ray.origin + ray.direction * t_cyl;
        float proj = (hit - baseCenter).Dot(axis);
        if (proj >= 0.0f && proj <= ab_len) {
            result.hit = true;
            result.distance = t_cyl;
            result.point = hit;
            Vec3 on_axis = baseCenter + axis * proj;
            result.normal = (hit - on_axis).Normalized();
        }
    }

    // Test bottom cap (disc at baseCenter)
    float denom_base = ray.direction.Dot(axis * (-1.0f));
    if (std::fabs(denom_base) > 1e-9f) {
        float t_base = (baseCenter - ray.origin).Dot(axis * (-1.0f)) / denom_base;
        if (t_base >= 0.0f && t_base < result.distance) {
            Vec3 hit = ray.origin + ray.direction * t_base;
            Vec3 to_hit = hit - baseCenter;
            if (to_hit.Dot(to_hit) <= radius * radius) {
                result.hit = true;
                result.distance = t_base;
                result.point = hit;
                result.normal = axis * (-1.0f);
            }
        }
    }

    // Test top cap (disc at topCenter)
    float denom_top = ray.direction.Dot(axis);
    if (std::fabs(denom_top) > 1e-9f) {
        float t_top = (topCenter - ray.origin).Dot(axis) / denom_top;
        if (t_top >= 0.0f && t_top < result.distance) {
            Vec3 hit = ray.origin + ray.direction * t_top;
            Vec3 to_hit = hit - topCenter;
            if (to_hit.Dot(to_hit) <= radius * radius) {
                result.hit = true;
                result.distance = t_top;
                result.point = hit;
                result.normal = axis;
            }
        }
    }

    return result;
}

bool CollisionCylinder::Overlaps(const CollisionPrimitive& other) const
{
    switch (other.GetType()) {
    case Type::Sphere: {
        const CollisionSphere& s = static_cast<const CollisionSphere&>(other);
        Vec3 closest = ClosestPointOnSegment(baseCenter, topCenter, s.center);
        float dist = (s.center - closest).Length();
        return dist < (radius + s.radius);
    }
    case Type::Capsule: {
        const CollisionCapsule& cap = static_cast<const CollisionCapsule&>(other);
        float s_param, t_param;
        float dist_sq = SegmentSegmentDistanceSq(baseCenter, topCenter, cap.pointA, cap.pointB, s_param, t_param);
        float combined = radius + cap.radius;
        return dist_sq < (combined * combined);
    }
    case Type::Cylinder: {
        const CollisionCylinder& cyl = static_cast<const CollisionCylinder&>(other);
        float s_param, t_param;
        float dist_sq = SegmentSegmentDistanceSq(baseCenter, topCenter, cyl.baseCenter, cyl.topCenter, s_param, t_param);
        float combined = radius + cyl.radius;
        return dist_sq < (combined * combined);
    }
    case Type::Cone: {
        const CollisionCone& cone = static_cast<const CollisionCone&>(other);
        Vec3 cone_center = cone.GetCenter();
        float cone_radius = cone.GetBoundingRadius();
        Vec3 closest = ClosestPointOnSegment(baseCenter, topCenter, cone_center);
        float dist = (cone_center - closest).Length();
        return dist < (radius + cone_radius);
    }
    }
    return false;
}

// ============================================================================
// CollisionCone
// ============================================================================

RayHit CollisionCone::Raycast(const Ray& ray) const
{
    RayHit result;

    Vec3 ab = tip - baseCenter;
    float height = ab.Length();
    if (height < 1e-9f) return result;

    Vec3 axis = ab * (1.0f / height);

    // Cone equation: for a point P on the cone surface,
    // the angle from axis satisfies tan(alpha) = baseRadius / height
    float cos_alpha = height / std::sqrt(height * height + baseRadius * baseRadius);
    float cos2 = cos_alpha * cos_alpha;

    Vec3 co = ray.origin - tip;
    float d_dot_a = ray.direction.Dot(axis);
    float co_dot_a = co.Dot(axis);

    float a = d_dot_a * d_dot_a - cos2 * ray.direction.Dot(ray.direction);
    float b = 2.0f * (d_dot_a * co_dot_a - cos2 * co.Dot(ray.direction));
    float c = co_dot_a * co_dot_a - cos2 * co.Dot(co);

    // Note: negate a,b,c since we want the cone below the tip
    a = -a; b = -b; c = -c;

    float disc = b * b - 4.0f * a * c;
    if (disc >= 0.0f) {
        float sqrt_disc = std::sqrt(disc);
        float t0 = (-b - sqrt_disc) / (2.0f * a);
        float t1 = (-b + sqrt_disc) / (2.0f * a);

        // Check both intersections
        for (int i = 0; i < 2; ++i) {
            float t = (i == 0) ? t0 : t1;
            if (t < 0.0f) continue;
            if (t >= result.distance) continue;

            Vec3 hit = ray.origin + ray.direction * t;
            float proj = (hit - tip).Dot(axis * (-1.0f)); // distance from tip toward base
            if (proj >= 0.0f && proj <= height) {
                result.hit = true;
                result.distance = t;
                result.point = hit;
                // Normal: perpendicular to cone surface
                Vec3 on_axis = tip + axis * (-(proj)); // Nope, from tip downward
                Vec3 to_surface = hit - (tip - axis * proj);
                float surface_len = to_surface.Length();
                if (surface_len > 1e-9f) {
                    // Cone normal: tilt outward by slant angle
                    Vec3 radial = to_surface * (1.0f / surface_len);
                    result.normal = (radial * height + axis * baseRadius).Normalized();
                } else {
                    result.normal = axis * (-1.0f);
                }
            }
        }
    }

    // Test base cap
    float denom = ray.direction.Dot(axis * (-1.0f));
    if (std::fabs(denom) > 1e-9f) {
        float t_cap = (baseCenter - ray.origin).Dot(axis * (-1.0f)) / denom;
        if (t_cap >= 0.0f && t_cap < result.distance) {
            Vec3 hit = ray.origin + ray.direction * t_cap;
            Vec3 to_hit = hit - baseCenter;
            if (to_hit.Dot(to_hit) <= baseRadius * baseRadius) {
                result.hit = true;
                result.distance = t_cap;
                result.point = hit;
                result.normal = axis * (-1.0f);
            }
        }
    }

    return result;
}

bool CollisionCone::Overlaps(const CollisionPrimitive& other) const
{
    // Use bounding sphere for all overlap tests (cone is complex)
    Vec3 my_center = GetCenter();
    float my_radius = GetBoundingRadius();

    switch (other.GetType()) {
    case Type::Sphere: {
        const CollisionSphere& s = static_cast<const CollisionSphere&>(other);
        float dist = (my_center - s.center).Length();
        return dist < (my_radius + s.radius);
    }
    case Type::Capsule: {
        const CollisionCapsule& cap = static_cast<const CollisionCapsule&>(other);
        Vec3 closest = ClosestPointOnSegment(cap.pointA, cap.pointB, my_center);
        float dist = (my_center - closest).Length();
        return dist < (my_radius + cap.radius);
    }
    case Type::Cylinder: {
        const CollisionCylinder& cyl = static_cast<const CollisionCylinder&>(other);
        Vec3 closest = ClosestPointOnSegment(cyl.baseCenter, cyl.topCenter, my_center);
        float dist = (my_center - closest).Length();
        return dist < (my_radius + cyl.radius);
    }
    case Type::Cone: {
        const CollisionCone& cone = static_cast<const CollisionCone&>(other);
        Vec3 other_center = cone.GetCenter();
        float other_radius = cone.GetBoundingRadius();
        float dist = (my_center - other_center).Length();
        return dist < (my_radius + other_radius);
    }
    }
    return false;
}

// ============================================================================
// Factory
// ============================================================================

std::unique_ptr<CollisionPrimitive> CreateCollisionPrimitive(
    const ShapeParams& shape, const Mat4& worldTransform)
{
    switch (shape.type) {
    case ShapeType::Sphere: {
        Vec3 local_center(0.0f, 0.0f, 0.0f);
        Vec3 world_center = worldTransform.TransformPoint(local_center);
        return std::unique_ptr<CollisionPrimitive>(
            new CollisionSphere(world_center, shape.radius));
    }
    case ShapeType::Cylinder: {
        float half_h = shape.height * 0.5f;
        Vec3 local_base(0.0f, -half_h, 0.0f);
        Vec3 local_top(0.0f, half_h, 0.0f);
        Vec3 world_base = worldTransform.TransformPoint(local_base);
        Vec3 world_top = worldTransform.TransformPoint(local_top);
        return std::unique_ptr<CollisionPrimitive>(
            new CollisionCylinder(world_base, world_top, shape.radius));
    }
    case ShapeType::Cone: {
        float half_h = shape.height * 0.5f;
        Vec3 local_base(0.0f, -half_h, 0.0f);
        Vec3 local_tip(0.0f, half_h, 0.0f);
        Vec3 world_base = worldTransform.TransformPoint(local_base);
        Vec3 world_tip = worldTransform.TransformPoint(local_tip);
        return std::unique_ptr<CollisionPrimitive>(
            new CollisionCone(world_base, world_tip, shape.radius));
    }
    case ShapeType::Capsule: {
        float half_h = shape.height * 0.5f;
        // Capsule: hemispheres sit at pointA and pointB, cylinder between them
        // Total height = height, so hemisphere centers are at +/- (height/2 - radius)
        float half_cyl = half_h - shape.radius;
        if (half_cyl < 0.0f) half_cyl = 0.0f;
        Vec3 local_a(0.0f, -half_cyl, 0.0f);
        Vec3 local_b(0.0f, half_cyl, 0.0f);
        Vec3 world_a = worldTransform.TransformPoint(local_a);
        Vec3 world_b = worldTransform.TransformPoint(local_b);
        return std::unique_ptr<CollisionPrimitive>(
            new CollisionCapsule(world_a, world_b, shape.radius));
    }
    case ShapeType::Torus: {
        // Torus: approximate with bounding sphere
        // The bounding sphere has radius = majorRadius + minorRadius
        Vec3 local_center(0.0f, 0.0f, 0.0f);
        Vec3 world_center = worldTransform.TransformPoint(local_center);
        float bounding_r = shape.majorRadius + shape.minorRadius;
        return std::unique_ptr<CollisionPrimitive>(
            new CollisionSphere(world_center, bounding_r));
    }
    }

    // Fallback: bounding sphere
    Vec3 world_center = worldTransform.TransformPoint(Vec3(0, 0, 0));
    return std::unique_ptr<CollisionPrimitive>(
        new CollisionSphere(world_center, shape.radius));
}

// ============================================================================
// Body tree traversal
// ============================================================================

static void CollectColliders(
    const BodyNode* node, const Mat4& parentWorld,
    std::vector<NodeCollider>& out)
{
    if (!node) return;

    Mat4 world = parentWorld * node->localTransform;

    NodeCollider nc;
    nc.nodeName = node->name;
    nc.primitive = CreateCollisionPrimitive(node->shape, world);
    out.push_back(std::move(nc));

    for (const auto& child : node->children) {
        CollectColliders(&child, world, out);
    }
}

std::vector<NodeCollider> BuildBodyColliders(const Body& body)
{
    std::vector<NodeCollider> colliders;
    Mat4 identity;
    identity.Identity();
    CollectColliders(&body.root, identity, colliders);
    return colliders;
}

// ============================================================================
// Body-level raycast
// ============================================================================

BodyRayHit RaycastBody(const std::vector<NodeCollider>& colliders, const Ray& ray)
{
    BodyRayHit best;

    for (const auto& nc : colliders) {
        if (!nc.primitive) continue;
        RayHit hit = nc.primitive->Raycast(ray);
        if (hit.hit && hit.distance < best.distance) {
            best.hit = true;
            best.distance = hit.distance;
            best.point = hit.point;
            best.normal = hit.normal;
            best.nodeName = nc.nodeName;
        }
    }

    return best;
}

// ============================================================================
// Body-vs-body overlap
// ============================================================================

BodyOverlapResult CheckBodyOverlap(
    const std::vector<NodeCollider>& collidersA,
    const std::vector<NodeCollider>& collidersB)
{
    BodyOverlapResult result;

    for (const auto& a : collidersA) {
        if (!a.primitive) continue;
        for (const auto& b : collidersB) {
            if (!b.primitive) continue;

            // Broad phase: bounding sphere check
            Vec3 ca = a.primitive->GetCenter();
            Vec3 cb = b.primitive->GetCenter();
            float ra = a.primitive->GetBoundingRadius();
            float rb = b.primitive->GetBoundingRadius();
            float dist = (ca - cb).Length();
            if (dist >= ra + rb) continue;

            // Narrow phase
            if (a.primitive->Overlaps(*b.primitive)) {
                result.overlaps = true;
                result.nodeA = a.nodeName;
                result.nodeB = b.nodeName;
                return result;
            }
        }
    }

    return result;
}

} // namespace BodyRenderer
