// Property 1: JSON Round-Trip Preservation
// Property 2: Invalid JSON Graceful Rejection

#include <rapidcheck.h>
#include "BodyLoader.h"
#include <cstdio>
#include <cmath>

using namespace BodyRenderer;

// Generator for valid ShapeParams
static ShapeParams GenValidShape(ShapeType type)
{
    ShapeParams s;
    s.type = type;
    switch (type) {
    case ShapeType::Cone:
        s.radius = *rc::gen::inRange(1, 50) / 10.0f;
        s.height = *rc::gen::inRange(1, 50) / 10.0f;
        s.segments = *rc::gen::inRange(3, 32);
        break;
    case ShapeType::Cylinder:
        s.radius = *rc::gen::inRange(1, 50) / 10.0f;
        s.height = *rc::gen::inRange(1, 50) / 10.0f;
        s.segments = *rc::gen::inRange(3, 32);
        break;
    case ShapeType::Sphere:
        s.radius = *rc::gen::inRange(1, 50) / 10.0f;
        s.lon_segments = *rc::gen::inRange(4, 32);
        s.lat_segments = *rc::gen::inRange(3, 16);
        break;
    case ShapeType::Torus:
        s.major_radius = *rc::gen::inRange(20, 100) / 10.0f;
        s.minor_radius = *rc::gen::inRange(1, 19) / 10.0f;
        s.ring_segments = *rc::gen::inRange(3, 32);
        s.side_segments = *rc::gen::inRange(3, 16);
        break;
    }
    return s;
}

void RunBodyLoaderTests()
{
    printf("--- BodyLoader Tests ---\n");

    // Property 1: JSON Round-Trip
    rc::check("Property 1: JSON round-trip preserves geometric parameters", []() {
        ShapeType type = static_cast<ShapeType>(*rc::gen::inRange(0, 4));
        ShapeParams shape = GenValidShape(type);

        Body body;
        body.name = "test_body";
        body.root.name = "root";
        body.root.shape = shape;
        body.root.color = Vec3(0.5f, 0.6f, 0.7f);
        body.material.shininess = 32.0f;

        BodyLoader loader;
        std::string json = loader.Serialize(body);

        LoadResult result = loader.LoadFromString(json);
        RC_ASSERT(result.success);
        RC_ASSERT(result.body.name == body.name);
        RC_ASSERT(result.body.root.shape.type == body.root.shape.type);

        // Check geometric parameters (not subdivision — those are runtime-determined)
        float eps = 1e-4f;
        switch (type) {
        case ShapeType::Cone:
        case ShapeType::Cylinder:
        case ShapeType::Capsule:
            RC_ASSERT(std::fabs(result.body.root.shape.radius - body.root.shape.radius) < eps);
            RC_ASSERT(std::fabs(result.body.root.shape.height - body.root.shape.height) < eps);
            break;
        case ShapeType::Sphere:
            RC_ASSERT(std::fabs(result.body.root.shape.radius - body.root.shape.radius) < eps);
            break;
        case ShapeType::Torus:
            RC_ASSERT(std::fabs(result.body.root.shape.major_radius - body.root.shape.major_radius) < eps);
            RC_ASSERT(std::fabs(result.body.root.shape.minor_radius - body.root.shape.minor_radius) < eps);
            break;
        }
    });

    // Property 2: Invalid JSON graceful rejection
    rc::check("Property 2: Invalid JSON rejected gracefully", []() {
        BodyLoader loader;

        // Malformed JSON
        LoadResult r1 = loader.LoadFromString("{invalid json");
        RC_ASSERT(!r1.success);
        RC_ASSERT(!r1.error.empty());

        // Missing required fields
        LoadResult r2 = loader.LoadFromString("{\"name\": \"test\"}");
        RC_ASSERT(!r2.success);

        // Unknown shape type (box is now invalid)
        LoadResult r3 = loader.LoadFromString(
            "{\"name\":\"x\",\"root\":{\"name\":\"n\",\"shape\":{\"type\":\"box\",\"width\":1,\"height\":1,\"depth\":1},\"color\":{\"r\":1,\"g\":1,\"b\":1}}}");
        RC_ASSERT(!r3.success);

        // Frustum is now invalid
        LoadResult r4 = loader.LoadFromString(
            "{\"name\":\"x\",\"root\":{\"name\":\"n\",\"shape\":{\"type\":\"frustum\",\"top_radius\":0.1,\"bottom_radius\":0.5,\"height\":1,\"sides\":8},\"color\":{\"r\":1,\"g\":1,\"b\":1}}}");
        RC_ASSERT(!r4.success);

        // Missing dimension field for cylinder — sides is now optional (runtime-determined)
        // but radius and height are still required
        LoadResult r5 = loader.LoadFromString(
            "{\"name\":\"x\",\"root\":{\"name\":\"n\",\"shape\":{\"type\":\"cylinder\",\"radius\":1},\"color\":{\"r\":1,\"g\":1,\"b\":1}}}");
        RC_ASSERT(!r5.success);  // missing height (required geometric param)
    });

    printf("  PASS\n");
}
