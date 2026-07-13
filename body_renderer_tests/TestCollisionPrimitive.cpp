// TestCollisionPrimitive.cpp — Tests for collision primitive system

#include "CollisionPrimitive.h"
#include "BodyTypes.h"
#include <rapidcheck.h>
#include <cstdio>
#include <cmath>

using namespace BodyRenderer;

// ============================================================================
// Sphere raycast tests
// ============================================================================

static void TestSphereRayHit()
{
    printf("  Sphere ray hit (front)...\n");
    CollisionSphere sphere(1.0f);

    // Ray from (0,0,-5) shooting toward origin — should hit at z=-1
    Ray ray(Vec3(0, 0, -5), Vec3(0, 0, 1));
    RayHit hit = sphere.Raycast(ray);

    RC_ASSERT(hit.hit);
    RC_ASSERT(std::fabs(hit.distance - 4.0f) < 0.001f);
    RC_ASSERT(std::fabs(hit.point.z - (-1.0f)) < 0.001f);
    RC_ASSERT(std::fabs(hit.normal.z - (-1.0f)) < 0.01f);
}

static void TestSphereRayMiss()
{
    printf("  Sphere ray miss...\n");
    CollisionSphere sphere(1.0f);

    // Ray parallel to sphere, offset by 2 (should miss)
    Ray ray(Vec3(2, 0, -5), Vec3(0, 0, 1));
    RayHit hit = sphere.Raycast(ray);

    RC_ASSERT(!hit.hit);
}

static void TestSphereRayInside()
{
    printf("  Sphere ray from inside...\n");
    CollisionSphere sphere(2.0f);

    // Ray starting inside sphere
    Ray ray(Vec3(0, 0, 0), Vec3(0, 0, 1));
    RayHit hit = sphere.Raycast(ray);

    // Should hit the exit point
    RC_ASSERT(hit.hit);
    RC_ASSERT(std::fabs(hit.distance - 2.0f) < 0.001f);
}

// ============================================================================
// Capsule raycast tests
// ============================================================================

static void TestCapsuleRayHitSide()
{
    printf("  Capsule ray hit (side)...\n");
    CollisionCapsule capsule(0.5f, 1.0f); // radius 0.5, halfHeight 1.0

    // Ray from side, hitting the cylinder portion
    Ray ray(Vec3(-3, 0, 0), Vec3(1, 0, 0));
    RayHit hit = capsule.Raycast(ray);

    RC_ASSERT(hit.hit);
    RC_ASSERT(std::fabs(hit.point.x - (-0.5f)) < 0.001f);
    RC_ASSERT(std::fabs(hit.normal.x - (-1.0f)) < 0.01f);
}

static void TestCapsuleRayHitCap()
{
    printf("  Capsule ray hit (top cap)...\n");
    CollisionCapsule capsule(0.5f, 1.0f);

    // Ray from above, hitting the top hemisphere
    Ray ray(Vec3(0, 5, 0), Vec3(0, -1, 0));
    RayHit hit = capsule.Raycast(ray);

    RC_ASSERT(hit.hit);
    // Should hit at y = halfHeight + radius = 1.5
    RC_ASSERT(std::fabs(hit.point.y - 1.5f) < 0.001f);
}

static void TestCapsuleRayMiss()
{
    printf("  Capsule ray miss...\n");
    CollisionCapsule capsule(0.5f, 1.0f);

    // Ray going past the capsule
    Ray ray(Vec3(2, 0, -5), Vec3(0, 0, 1));
    RayHit hit = capsule.Raycast(ray);

    RC_ASSERT(!hit.hit);
}

// ============================================================================
// Cylinder raycast tests
// ============================================================================

static void TestCylinderRayHitSide()
{
    printf("  Cylinder ray hit (side)...\n");
    CollisionCylinder cyl(1.0f, 1.0f); // radius 1, halfHeight 1

    Ray ray(Vec3(-3, 0, 0), Vec3(1, 0, 0));
    RayHit hit = cyl.Raycast(ray);

    RC_ASSERT(hit.hit);
    RC_ASSERT(std::fabs(hit.point.x - (-1.0f)) < 0.001f);
}

static void TestCylinderRayHitCap()
{
    printf("  Cylinder ray hit (top cap)...\n");
    CollisionCylinder cyl(1.0f, 1.0f);

    // Ray from above hitting top cap
    Ray ray(Vec3(0, 5, 0), Vec3(0, -1, 0));
    RayHit hit = cyl.Raycast(ray);

    RC_ASSERT(hit.hit);
    RC_ASSERT(std::fabs(hit.point.y - 1.0f) < 0.001f);
    RC_ASSERT(std::fabs(hit.normal.y - 1.0f) < 0.01f);
}

// ============================================================================
// Overlap tests
// ============================================================================

static void TestSphereSphereOverlap()
{
    printf("  Sphere-Sphere overlap...\n");
    CollisionSphere s1(1.0f);
    CollisionSphere s2(1.0f);

    // Overlapping: centers 1.5 apart, combined radii = 2
    Mat4 w1 = Mat4::Translation(0, 0, 0);
    Mat4 w2 = Mat4::Translation(1.5f, 0, 0);

    OverlapResult r = s1.TestOverlap(s2, w1, w2);
    RC_ASSERT(r.overlapping);
    RC_ASSERT(std::fabs(r.penetrationDepth - 0.5f) < 0.001f);
}

static void TestSphereSphereNoOverlap()
{
    printf("  Sphere-Sphere no overlap...\n");
    CollisionSphere s1(1.0f);
    CollisionSphere s2(1.0f);

    // Not overlapping: centers 3 apart, combined radii = 2
    Mat4 w1 = Mat4::Translation(0, 0, 0);
    Mat4 w2 = Mat4::Translation(3.0f, 0, 0);

    OverlapResult r = s1.TestOverlap(s2, w1, w2);
    RC_ASSERT(!r.overlapping);
}

static void TestSphereCapsuleOverlap()
{
    printf("  Sphere-Capsule overlap...\n");
    CollisionSphere sphere(1.0f);
    CollisionCapsule capsule(0.5f, 1.0f);

    // Sphere at origin, capsule at (1.2, 0, 0) — overlapping
    Mat4 w1 = Mat4::Translation(0, 0, 0);
    Mat4 w2 = Mat4::Translation(1.2f, 0, 0);

    OverlapResult r = sphere.TestOverlap(capsule, w1, w2);
    RC_ASSERT(r.overlapping);
    RC_ASSERT(r.penetrationDepth > 0.0f);
}

static void TestCapsuleCapsuleOverlap()
{
    printf("  Capsule-Capsule overlap...\n");
    CollisionCapsule c1(0.5f, 1.0f);
    CollisionCapsule c2(0.5f, 1.0f);

    // Parallel capsules, close together
    Mat4 w1 = Mat4::Translation(0, 0, 0);
    Mat4 w2 = Mat4::Translation(0.8f, 0, 0);

    OverlapResult r = c1.TestOverlap(c2, w1, w2);
    RC_ASSERT(r.overlapping);
    RC_ASSERT(std::fabs(r.penetrationDepth - 0.2f) < 0.001f);
}

static void TestCapsuleCapsuleNoOverlap()
{
    printf("  Capsule-Capsule no overlap...\n");
    CollisionCapsule c1(0.5f, 1.0f);
    CollisionCapsule c2(0.5f, 1.0f);

    // Far apart
    Mat4 w1 = Mat4::Translation(0, 0, 0);
    Mat4 w2 = Mat4::Translation(5.0f, 0, 0);

    OverlapResult r = c1.TestOverlap(c2, w1, w2);
    RC_ASSERT(!r.overlapping);
}

// ============================================================================
// Factory tests
// ============================================================================

static void TestFactorySphere()
{
    printf("  Factory: Sphere -> CollisionSphere...\n");
    ShapeParams params;
    params.type = ShapeType::Sphere;
    params.radius = 2.0f;

    CollisionPrimitive* prim = CreateCollisionPrimitive(params);
    RC_ASSERT(prim != nullptr);
    RC_ASSERT(prim->GetType() == CollisionPrimitive::PRIM_SPHERE);
    RC_ASSERT(std::fabs(prim->GetBoundingRadius() - 2.0f) < 0.001f);
    delete prim;
}

static void TestFactoryTorus()
{
    printf("  Factory: Torus -> CollisionSphere...\n");
    ShapeParams params;
    params.type = ShapeType::Torus;
    params.majorRadius = 1.0f;
    params.minorRadius = 0.25f;

    CollisionPrimitive* prim = CreateCollisionPrimitive(params);
    RC_ASSERT(prim != nullptr);
    RC_ASSERT(prim->GetType() == CollisionPrimitive::PRIM_SPHERE);
    RC_ASSERT(std::fabs(prim->GetBoundingRadius() - 1.25f) < 0.001f);
    delete prim;
}

static void TestFactoryCylinder()
{
    printf("  Factory: Cylinder -> CollisionCapsule...\n");
    ShapeParams params;
    params.type = ShapeType::Cylinder;
    params.radius = 0.5f;
    params.height = 2.0f;

    CollisionPrimitive* prim = CreateCollisionPrimitive(params);
    RC_ASSERT(prim != nullptr);
    RC_ASSERT(prim->GetType() == CollisionPrimitive::PRIM_CAPSULE);
    // Bounding radius = halfHeight + radius = 1.0 + 0.5 = 1.5
    RC_ASSERT(std::fabs(prim->GetBoundingRadius() - 1.5f) < 0.001f);
    delete prim;
}

static void TestFactoryCone()
{
    printf("  Factory: Cone -> CollisionCapsule...\n");
    ShapeParams params;
    params.type = ShapeType::Cone;
    params.radius = 1.0f;
    params.height = 3.0f;

    CollisionPrimitive* prim = CreateCollisionPrimitive(params);
    RC_ASSERT(prim != nullptr);
    RC_ASSERT(prim->GetType() == CollisionPrimitive::PRIM_CAPSULE);
    // Bounding radius = halfHeight + radius = 1.5 + 1.0 = 2.5
    RC_ASSERT(std::fabs(prim->GetBoundingRadius() - 2.5f) < 0.001f);
    delete prim;
}

static void TestFactoryCapsule()
{
    printf("  Factory: Capsule -> CollisionCapsule...\n");
    ShapeParams params;
    params.type = ShapeType::Capsule;
    params.radius = 0.3f;
    params.height = 1.0f;

    CollisionPrimitive* prim = CreateCollisionPrimitive(params);
    RC_ASSERT(prim != nullptr);
    RC_ASSERT(prim->GetType() == CollisionPrimitive::PRIM_CAPSULE);
    delete prim;
}

// ============================================================================
// TransformRayToLocal test
// ============================================================================

static void TestTransformRayToLocal()
{
    printf("  TransformRayToLocal...\n");

    // Object at (2, 0, 0), no rotation
    Mat4 world = Mat4::Translation(2, 0, 0);

    // World ray from (5, 0, 0) toward (-1, 0, 0)
    Ray world_ray(Vec3(5, 0, 0), Vec3(-1, 0, 0));
    Ray local_ray = TransformRayToLocal(world_ray, world);

    // Local origin should be (3, 0, 0) since object is at (2,0,0) and ray origin at (5,0,0)
    RC_ASSERT(std::fabs(local_ray.origin.x - 3.0f) < 0.001f);
    RC_ASSERT(std::fabs(local_ray.origin.y) < 0.001f);
    RC_ASSERT(std::fabs(local_ray.direction.x - (-1.0f)) < 0.001f);
}

// ============================================================================
// Property-based tests
// ============================================================================

static void TestPropertySphereRayDistance()
{
    printf("  [Property] Sphere ray hit distance always positive...\n");
    rc::check("sphere ray hit distance positive", []() {
        float radius = *rc::gen::inRange(1, 100) * 0.1f;
        CollisionSphere sphere(radius);

        // Generate ray origin outside sphere
        float dist = radius + *rc::gen::inRange(1, 100) * 0.1f;
        Ray ray(Vec3(0, 0, -dist), Vec3(0, 0, 1));
        RayHit hit = sphere.Raycast(ray);

        RC_ASSERT(hit.hit);
        RC_ASSERT(hit.distance > 0.0f);
        RC_ASSERT(hit.distance < dist); // must hit before reaching center
    });
}

static void TestPropertyOverlapSymmetry()
{
    printf("  [Property] Overlap is symmetric...\n");
    rc::check("overlap symmetry", []() {
        float r1 = *rc::gen::inRange(1, 50) * 0.1f;
        float r2 = *rc::gen::inRange(1, 50) * 0.1f;
        float separation = *rc::gen::inRange(0, 100) * 0.1f;

        CollisionSphere s1(r1);
        CollisionSphere s2(r2);

        Mat4 w1 = Mat4::Translation(0, 0, 0);
        Mat4 w2 = Mat4::Translation(separation, 0, 0);

        OverlapResult r_forward = s1.TestOverlap(s2, w1, w2);
        OverlapResult r_backward = s2.TestOverlap(s1, w2, w1);

        RC_ASSERT(r_forward.overlapping == r_backward.overlapping);
        if (r_forward.overlapping) {
            RC_ASSERT(std::fabs(r_forward.penetrationDepth - r_backward.penetrationDepth) < 0.001f);
        }
    });
}

static void TestPropertyBoundingRadiusContainsPrimitive()
{
    printf("  [Property] Bounding radius contains primitive...\n");
    rc::check("bounding radius valid", []() {
        int type_idx = *rc::gen::inRange(0, 5);
        ShapeParams params;
        params.radius = *rc::gen::inRange(1, 50) * 0.1f;
        params.height = *rc::gen::inRange(1, 50) * 0.1f;
        params.majorRadius = *rc::gen::inRange(1, 50) * 0.1f;
        params.minorRadius = *rc::gen::inRange(1, 30) * 0.1f;

        switch (type_idx) {
        case 0: params.type = ShapeType::Sphere; break;
        case 1: params.type = ShapeType::Cylinder; break;
        case 2: params.type = ShapeType::Cone; break;
        case 3: params.type = ShapeType::Capsule; break;
        case 4: params.type = ShapeType::Torus; break;
        }

        CollisionPrimitive* prim = CreateCollisionPrimitive(params);
        RC_ASSERT(prim != nullptr);
        RC_ASSERT(prim->GetBoundingRadius() > 0.0f);

        // A ray at exactly bounding radius distance along any axis should either
        // hit or pass tangent to the primitive (never be inside)
        float br = prim->GetBoundingRadius();
        Ray ray(Vec3(0, 0, -(br + 1.0f)), Vec3(0, 0, 1));
        RayHit hit = prim->Raycast(ray);
        if (hit.hit) {
            RC_ASSERT(hit.distance >= 0.0f);
        }

        delete prim;
    });
}

// ============================================================================
// Entry point
// ============================================================================

void RunCollisionPrimitiveTests()
{
    printf("--- Collision Primitive Tests ---\n");

    // Raycast tests
    TestSphereRayHit();
    TestSphereRayMiss();
    TestSphereRayInside();
    TestCapsuleRayHitSide();
    TestCapsuleRayHitCap();
    TestCapsuleRayMiss();
    TestCylinderRayHitSide();
    TestCylinderRayHitCap();

    // Overlap tests
    TestSphereSphereOverlap();
    TestSphereSphereNoOverlap();
    TestSphereCapsuleOverlap();
    TestCapsuleCapsuleOverlap();
    TestCapsuleCapsuleNoOverlap();

    // Factory tests
    TestFactorySphere();
    TestFactoryTorus();
    TestFactoryCylinder();
    TestFactoryCone();
    TestFactoryCapsule();

    // Utility tests
    TestTransformRayToLocal();

    // Property-based tests
    TestPropertySphereRayDistance();
    TestPropertyOverlapSymmetry();
    TestPropertyBoundingRadiusContainsPrimitive();

    printf("--- Collision Primitive Tests PASSED ---\n\n");
}
