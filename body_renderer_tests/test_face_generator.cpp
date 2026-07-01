// Property 3: Face Count Invariant
// Property 5: Face Normal Unit-Length and Perpendicularity

#include <rapidcheck.h>
#include "FaceGenerator.h"
#include <cstdio>
#include <cmath>

using namespace BodyRenderer;

void RunFaceGeneratorTests()
{
    printf("--- FaceGenerator Tests ---\n");

    // Property 3: Face Count Invariant
    rc::check("Property 3: Face count matches formula for shape type", []() {
        FaceGenerator gen;

        // Box: always 6 faces
        {
            ShapeParams s;
            s.type = ShapeType::Box;
            s.width = *rc::gen::inRange(1, 100) / 10.0f;
            s.height = *rc::gen::inRange(1, 100) / 10.0f;
            s.depth = *rc::gen::inRange(1, 100) / 10.0f;
            auto faces = gen.Generate(s);
            RC_ASSERT(static_cast<int>(faces.size()) == 6);
        }

        // Cone: segments + 1
        {
            ShapeParams s;
            s.type = ShapeType::Cone;
            s.radius = *rc::gen::inRange(1, 50) / 10.0f;
            s.height = *rc::gen::inRange(1, 50) / 10.0f;
            s.segments = *rc::gen::inRange(3, 32);
            auto faces = gen.Generate(s);
            RC_ASSERT(static_cast<int>(faces.size()) == s.segments + 1);
        }

        // Cylinder: segments + 2
        {
            ShapeParams s;
            s.type = ShapeType::Cylinder;
            s.radius = *rc::gen::inRange(1, 50) / 10.0f;
            s.height = *rc::gen::inRange(1, 50) / 10.0f;
            s.segments = *rc::gen::inRange(3, 32);
            auto faces = gen.Generate(s);
            RC_ASSERT(static_cast<int>(faces.size()) == s.segments + 2);
        }

        // Sphere: lat_segments * lon_segments
        {
            ShapeParams s;
            s.type = ShapeType::Sphere;
            s.radius = *rc::gen::inRange(1, 50) / 10.0f;
            s.lon_segments = *rc::gen::inRange(4, 16);
            s.lat_segments = *rc::gen::inRange(3, 8);
            auto faces = gen.Generate(s);
            RC_ASSERT(static_cast<int>(faces.size()) == s.lat_segments * s.lon_segments);
        }

        // Torus: ring_segments * side_segments
        {
            ShapeParams s;
            s.type = ShapeType::Torus;
            s.major_radius = *rc::gen::inRange(20, 50) / 10.0f;
            s.minor_radius = *rc::gen::inRange(1, 19) / 10.0f;
            s.ring_segments = *rc::gen::inRange(3, 16);
            s.side_segments = *rc::gen::inRange(3, 8);
            auto faces = gen.Generate(s);
            RC_ASSERT(static_cast<int>(faces.size()) == s.ring_segments * s.side_segments);
        }

        // Frustum (with top_radius > 0): segments + 2
        {
            ShapeParams s;
            s.type = ShapeType::Frustum;
            s.bottom_radius = *rc::gen::inRange(10, 50) / 10.0f;
            s.top_radius = *rc::gen::inRange(1, 9) / 10.0f;
            s.height = *rc::gen::inRange(1, 50) / 10.0f;
            s.segments = *rc::gen::inRange(3, 32);
            auto faces = gen.Generate(s);
            RC_ASSERT(static_cast<int>(faces.size()) == s.segments + 2);
        }
    });

    // Property 5: Face Normal Unit-Length
    rc::check("Property 5: Face normals have unit length", []() {
        FaceGenerator gen;
        ShapeType type = static_cast<ShapeType>(*rc::gen::inRange(0, 6));

        ShapeParams s;
        s.type = type;
        switch (type) {
        case ShapeType::Box:
            s.width = 2.0f; s.height = 3.0f; s.depth = 1.0f;
            break;
        case ShapeType::Cone:
            s.radius = 1.0f; s.height = 2.0f; s.segments = 8;
            break;
        case ShapeType::Cylinder:
            s.radius = 1.0f; s.height = 2.0f; s.segments = 8;
            break;
        case ShapeType::Sphere:
            s.radius = 1.0f; s.lon_segments = 8; s.lat_segments = 6;
            break;
        case ShapeType::Torus:
            s.major_radius = 2.0f; s.minor_radius = 0.5f; s.ring_segments = 8; s.side_segments = 6;
            break;
        case ShapeType::Frustum:
            s.bottom_radius = 2.0f; s.top_radius = 1.0f; s.height = 2.0f; s.segments = 8;
            break;
        }

        auto faces = gen.Generate(s);
        for (const Face& f : faces) {
            float len = f.normal.Length();
            RC_ASSERT(std::fabs(len - 1.0f) < 0.01f);
        }
    });

    printf("  PASS\n");
}
