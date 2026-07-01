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
    case ShapeType::Box:
        s.width = *rc::gen::inRange(1, 100) / 10.0f;
        s.height = *rc::gen::inRange(1, 100) / 10.0f;
        s.depth = *rc::gen::inRange(1, 100) / 10.0f;
        break;
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
    case ShapeType::Frustum:
        s.bottom_radius = *rc::gen::inRange(10, 50) / 10.0f;
        s.top_radius = *rc::gen::inRange(1, 9) / 10.0f;
        s.height = *rc::gen::inRange(1, 50) / 10.0f;
        s.segments = *rc::gen::inRange(3, 32);
        break;
    }
    return s;
}

void RunBodyLoaderTests()
{
    printf("--- BodyLoader Tests ---\n");

    // Property 1: JSON Round-Trip
    rc::check("Property 1: JSON round-trip preserves structure", []() {
        ShapeType type = static_cast<ShapeType>(*rc::gen::inRange(0, 6));
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

        // Check float values within epsilon
        float eps = 1e-4f;
        switch (type) {
        case ShapeType::Box:
            RC_ASSERT(std::fabs(result.body.root.shape.width - body.root.shape.width) < eps);
            RC_ASSERT(std::fabs(result.body.root.shape.height - body.root.shape.height) < eps);
            RC_ASSERT(std::fabs(result.body.root.shape.depth - body.root.shape.depth) < eps);
            break;
        case ShapeType::Cone:
        case ShapeType::Cylinder:
            RC_ASSERT(std::fabs(result.body.root.shape.radius - body.root.shape.radius) < eps);
            RC_ASSERT(std::fabs(result.body.root.shape.height - body.root.shape.height) < eps);
            RC_ASSERT(result.body.root.shape.segments == body.root.shape.segments);
            break;
        case ShapeType::Sphere:
            RC_ASSERT(std::fabs(result.body.root.shape.radius - body.root.shape.radius) < eps);
            RC_ASSERT(result.body.root.shape.lon_segments == body.root.shape.lon_segments);
            RC_ASSERT(result.body.root.shape.lat_segments == body.root.shape.lat_segments);
            break;
        case ShapeType::Torus:
            RC_ASSERT(std::fabs(result.body.root.shape.major_radius - body.root.shape.major_radius) < eps);
            RC_ASSERT(std::fabs(result.body.root.shape.minor_radius - body.root.shape.minor_radius) < eps);
            RC_ASSERT(result.body.root.shape.ring_segments == body.root.shape.ring_segments);
            RC_ASSERT(result.body.root.shape.side_segments == body.root.shape.side_segments);
            break;
        case ShapeType::Frustum:
            RC_ASSERT(std::fabs(result.body.root.shape.top_radius - body.root.shape.top_radius) < eps);
            RC_ASSERT(std::fabs(result.body.root.shape.bottom_radius - body.root.shape.bottom_radius) < eps);
            RC_ASSERT(std::fabs(result.body.root.shape.height - body.root.shape.height) < eps);
            RC_ASSERT(result.body.root.shape.segments == body.root.shape.segments);
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

        // Unknown shape type
        LoadResult r3 = loader.LoadFromString(
            "{\"name\":\"x\",\"shape\":{\"type\":\"hexagon\",\"width\":1},\"color\":{\"r\":1,\"g\":1,\"b\":1}}");
        RC_ASSERT(!r3.success);

        // Missing dimension field
        LoadResult r4 = loader.LoadFromString(
            "{\"name\":\"x\",\"shape\":{\"type\":\"box\",\"width\":1,\"height\":1},\"color\":{\"r\":1,\"g\":1,\"b\":1}}");
        RC_ASSERT(!r4.success);  // missing depth
    });

    printf("  PASS\n");
}
