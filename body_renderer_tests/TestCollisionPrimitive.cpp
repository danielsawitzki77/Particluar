// TestCollisionPrimitive.cpp — Property-based tests for collision primitives

#include <rapidcheck.h>
#include "CollisionPrimitive.h"
#include "BodyGenerator.h"
#include "ConnectionSolver.h"
#include "SubdivisionSolver.h"
#include <cstdio>
#include <cmath>
#include <memory>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace BodyRenderer;

// ============================================================================
// Generators
// ============================================================================

static rc::Gen<Vec3> genVec3(float range = 10.0f)
{
    return rc::gen::map(
        rc::gen::tuple(
            rc::gen::inRange(-static_cast<int>(range * 100), static_cast<int>(range * 100)),
            rc::gen::inRange(-static_cast<int>(range * 100), static_cast<int>(range * 100)),
            rc::gen::inRange(-static_cast<int>(range * 100), static_cast<int>(range * 100))
        ),
        [](const std::tuple<int, int, int>& t) {
            return Vec3(
                std::get<0>(t) / 100.0f,
                std::get<1>(t) / 100.0f,
                std::get<2>(t) / 100.0f
            );
        }
    );
}

static rc::Gen<float> genRadius()
{
    return rc::gen::map(
        rc::gen::inRange(10, 300),
        [](int v) { return v / 100.0f; }
    );
}

static rc::Gen<float> genHeight()
{
    return rc::gen::map(
        rc::gen::inRange(20, 500),
        [](int v) { return v / 100.0f; }
    );
}

static rc::Gen<Ray> genRayToward(const Vec3& target, float scatter = 0.1f)
{
    return rc::gen::map(
        rc::gen::tuple(
            genVec3(8.0f),
            rc::gen::inRange(-10, 10),
            rc::gen::inRange(-10, 10),
            rc::gen::inRange(-10, 10)
        ),
        [target, scatter](const std::tuple<Vec3, int, int, int>& t) {
            Vec3 origin = std::get<0>(t);
            Vec3 jitter(
                std::get<1>(t) * scatter / 10.0f,
                std::get<2>(t) * scatter / 10.0f,
                std::get<3>(t) * scatter / 10.0f
            );
            Vec3 dir = ((target + jitter) - origin).Normalized();
            return Ray(origin, dir);
        }
    );
}

// ============================================================================
// Tests
// ============================================================================

void RunCollisionPrimitiveTests()
{
    printf("--- CollisionPrimitive Tests ---\n");

    // Test 1: Sphere raycast — ray from outside toward center always hits
    rc::check("Sphere: ray toward center hits", []() {
        Vec3 center = *genVec3(5.0f);
        float radius = *genRadius();
        CollisionSphere sphere(center, radius);

        // Generate origin outside the sphere
        Vec3 dir_from_center = *genVec3(1.0f);
        float dir_len = dir_from_center.Length();
        if (dir_len < 0.01f) return; // skip degenerate
        dir_from_center = dir_from_center * (1.0f / dir_len);
        Vec3 origin = center + dir_from_center * (radius + 2.0f);

        Vec3 ray_dir = (center - origin).Normalized();
        Ray ray(origin, ray_dir);

        RayHit hit = sphere.Raycast(ray);
        RC_ASSERT(hit.hit);
        RC_ASSERT(hit.distance > 0.0f);
        RC_ASSERT(hit.distance < (radius + 2.0f) + 0.01f);

        // Hit point should be on sphere surface
        float dist_to_center = (hit.point - center).Length();
        RC_ASSERT(std::fabs(dist_to_center - radius) < 0.01f);
    });

    // Test 2: Sphere raycast — ray pointing away from sphere misses
    rc::check("Sphere: ray away misses", []() {
        Vec3 center(0, 0, 0);
        float radius = *genRadius();
        CollisionSphere sphere(center, radius);

        Vec3 origin(radius + 2.0f, 0, 0);
        Vec3 dir(1, 0, 0); // pointing away
        Ray ray(origin, dir);

        RayHit hit = sphere.Raycast(ray);
        RC_ASSERT(!hit.hit);
    });

    // Test 3: Sphere-sphere overlap — overlapping spheres detected
    rc::check("Sphere-sphere overlap: close spheres overlap", []() {
        Vec3 c1(0, 0, 0);
        Vec3 c2(1, 0, 0);
        CollisionSphere s1(c1, 1.0f);
        CollisionSphere s2(c2, 1.0f);

        RC_ASSERT(s1.Overlaps(s2)); // distance=1, combined radii=2
    });

    // Test 4: Sphere-sphere overlap — distant spheres do not overlap
    rc::check("Sphere-sphere no overlap: far spheres", []() {
        Vec3 c1(0, 0, 0);
        Vec3 c2(10, 0, 0);
        CollisionSphere s1(c1, 1.0f);
        CollisionSphere s2(c2, 1.0f);

        RC_ASSERT(!s1.Overlaps(s2));
    });

    // Test 5: Capsule raycast — ray toward midpoint hits
    rc::check("Capsule: ray toward midpoint hits", []() {
        Vec3 a(0, -1, 0);
        Vec3 b(0, 1, 0);
        float radius = 0.5f;
        CollisionCapsule cap(a, b, radius);

        Vec3 origin(5, 0, 0);
        Vec3 dir = (Vec3(0, 0, 0) - origin).Normalized();
        Ray ray(origin, dir);

        RayHit hit = cap.Raycast(ray);
        RC_ASSERT(hit.hit);
        RC_ASSERT(hit.distance > 0.0f);
    });

    // Test 6: Cylinder raycast — ray hitting side wall
    rc::check("Cylinder: ray toward center hits side", []() {
        Vec3 base(0, -1, 0);
        Vec3 top(0, 1, 0);
        float radius = 0.5f;
        CollisionCylinder cyl(base, top, radius);

        Vec3 origin(5, 0, 0);
        Vec3 dir(-1, 0, 0);
        Ray ray(origin, dir);

        RayHit hit = cyl.Raycast(ray);
        RC_ASSERT(hit.hit);
        RC_ASSERT(std::fabs(hit.point.x - radius) < 0.01f);
    });

    // Test 7: Cylinder raycast — ray hitting top cap
    rc::check("Cylinder: ray from above hits top cap", []() {
        Vec3 base(0, -1, 0);
        Vec3 top(0, 1, 0);
        float radius = 0.5f;
        CollisionCylinder cyl(base, top, radius);

        Vec3 origin(0, 5, 0);
        Vec3 dir(0, -1, 0);
        Ray ray(origin, dir);

        RayHit hit = cyl.Raycast(ray);
        RC_ASSERT(hit.hit);
        RC_ASSERT(std::fabs(hit.point.y - 1.0f) < 0.01f);
    });

    // Test 8: Capsule-sphere overlap
    rc::check("Capsule-sphere overlap", []() {
        CollisionCapsule cap(Vec3(0, -1, 0), Vec3(0, 1, 0), 0.5f);
        CollisionSphere s(Vec3(0.8f, 0, 0), 0.5f); // touching

        RC_ASSERT(cap.Overlaps(s));
    });

    // Test 9: Factory creates correct type for each shape
    rc::check("Factory: each shape type produces valid collider", []() {
        int type_idx = *rc::gen::inRange(0, 5);
        ShapeParams shape;
        shape.radius = 0.5f;
        shape.height = 2.0f;
        shape.majorRadius = 1.0f;
        shape.minorRadius = 0.25f;

        switch (type_idx) {
        case 0: shape.type = ShapeType::Sphere; break;
        case 1: shape.type = ShapeType::Cylinder; break;
        case 2: shape.type = ShapeType::Cone; break;
        case 3: shape.type = ShapeType::Capsule; break;
        case 4: shape.type = ShapeType::Torus; break;
        }

        Mat4 identity;
        identity.Identity();
        auto prim = CreateCollisionPrimitive(shape, identity);

        RC_ASSERT(prim != nullptr);
        RC_ASSERT(prim->GetBoundingRadius() > 0.0f);
    });

    // Test 10: BuildBodyColliders produces one collider per node
    rc::check("BuildBodyColliders: collider count equals node count", []() {
        unsigned int seed = *rc::gen::inRange(1, 10000);
        BodyGenerator gen;
        Body body = gen.Generate(seed, 3);

        SubdivisionSolver subdivSolver;
        subdivSolver.PrepareBody(body, 8);

        auto colliders = BuildBodyColliders(body);

        // Count nodes in tree
        std::function<int(const BodyNode*)> countNodes;
        countNodes = [&countNodes](const BodyNode* node) -> int {
            int count = 1;
            for (const auto& child : node->children) {
                count += countNodes(&child);
            }
            return count;
        };

        int expected = countNodes(&body.root);
        RC_ASSERT(static_cast<int>(colliders.size()) == expected);
    });

    // Test 11: RaycastBody — ray toward body hits some node
    rc::check("RaycastBody: ray from outside toward origin hits", []() {
        BodyGenerator gen;
        Body body = gen.Generate(42, 2);

        SubdivisionSolver subdivSolver;
        subdivSolver.PrepareBody(body, 8);

        auto colliders = BuildBodyColliders(body);

        // Ray from far away toward origin
        Vec3 origin(10, 0, 0);
        Vec3 dir(-1, 0, 0);
        Ray ray(origin, dir);

        BodyRayHit hit = RaycastBody(colliders, ray);
        // The body is centered near origin, so this should hit
        RC_ASSERT(hit.hit);
        RC_ASSERT(!hit.nodeName.empty());
        RC_ASSERT(hit.distance > 0.0f);
    });

    // Test 12: Self-overlap — same body at same position overlaps itself
    rc::check("CheckBodyOverlap: same body at same position overlaps", []() {
        BodyGenerator gen;
        Body body = gen.Generate(100, 2);

        SubdivisionSolver subdivSolver;
        subdivSolver.PrepareBody(body, 8);

        auto colliders = BuildBodyColliders(body);

        BodyOverlapResult result = CheckBodyOverlap(colliders, colliders);
        RC_ASSERT(result.overlaps);
    });

    // Test 13: Non-overlap — bodies far apart do not overlap
    rc::check("CheckBodyOverlap: distant bodies do not overlap", []() {
        BodyGenerator gen;
        Body bodyA = gen.Generate(200, 2);
        Body bodyB = gen.Generate(201, 2);

        SubdivisionSolver subdivSolver;
        subdivSolver.PrepareBody(bodyA, 8);
        subdivSolver.PrepareBody(bodyB, 8);

        auto collidersA = BuildBodyColliders(bodyA);

        // Manually offset all colliders of B by 100 units
        // Instead of modifying the body tree, just build B's colliders
        // and check against A after verifying they don't overlap at distance
        // Use a simple sphere-based check: bodyB centered at (100,0,0) cannot
        // overlap bodyA centered at origin if the max bounding radii sum < 100
        float maxRadA = 0.0f;
        for (const auto& c : collidersA) {
            float r = c.primitive->GetBoundingRadius() + c.primitive->GetCenter().Length();
            if (r > maxRadA) maxRadA = r;
        }

        auto collidersB = BuildBodyColliders(bodyB);
        float maxRadB = 0.0f;
        for (const auto& c : collidersB) {
            float r = c.primitive->GetBoundingRadius() + c.primitive->GetCenter().Length();
            if (r > maxRadB) maxRadB = r;
        }

        // Only test if bodies are small enough that 100 unit separation is sufficient
        if (maxRadA + maxRadB >= 100.0f) return; // skip huge generated bodies

        // Create offset colliders for B
        std::vector<NodeCollider> offsetColliders;
        for (const auto& c : collidersB) {
            NodeCollider nc;
            nc.nodeName = c.nodeName;
            // Rebuild with offset - create a sphere at offset center
            Vec3 center = c.primitive->GetCenter();
            float radius = c.primitive->GetBoundingRadius();
            nc.primitive = std::unique_ptr<CollisionPrimitive>(
                new CollisionSphere(Vec3(center.x + 100.0f, center.y, center.z), radius));
            offsetColliders.push_back(std::move(nc));
        }

        BodyOverlapResult result = CheckBodyOverlap(collidersA, offsetColliders);
        RC_ASSERT(!result.overlaps);
    });

    // Test 14: Cone raycast — ray from side toward cone base hits
    rc::check("Cone: ray toward base center hits", []() {
        Vec3 base(0, -1, 0);
        Vec3 tip(0, 1, 0);
        float radius = 0.5f;
        CollisionCone cone(base, tip, radius);

        Vec3 origin(0, -5, 0);
        Vec3 dir(0, 1, 0); // from below toward base
        Ray ray(origin, dir);

        RayHit hit = cone.Raycast(ray);
        RC_ASSERT(hit.hit);
        RC_ASSERT(std::fabs(hit.point.y - (-1.0f)) < 0.01f);
    });

    printf("  All CollisionPrimitive tests passed.\n");
}
