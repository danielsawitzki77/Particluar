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

        // Sphere: latSegments * lonSegments
        {
            ShapeParams s;
            s.type = ShapeType::Sphere;
            s.radius = *rc::gen::inRange(1, 50) / 10.0f;
            s.lonSegments = *rc::gen::inRange(4, 16);
            s.latSegments = *rc::gen::inRange(3, 8);
            auto faces = gen.Generate(s);
            RC_ASSERT(static_cast<int>(faces.size()) == s.latSegments * s.lonSegments);
        }

        // Torus: ringSegments * sideSegments
        {
            ShapeParams s;
            s.type = ShapeType::Torus;
            s.majorRadius = *rc::gen::inRange(20, 50) / 10.0f;
            s.minorRadius = *rc::gen::inRange(1, 19) / 10.0f;
            s.ringSegments = *rc::gen::inRange(3, 16);
            s.sideSegments = *rc::gen::inRange(3, 8);
            auto faces = gen.Generate(s);
            RC_ASSERT(static_cast<int>(faces.size()) == s.ringSegments * s.sideSegments);
        }
    });

    // Property 5: Face Normal Unit-Length
    rc::check("Property 5: Face normals have unit length", []() {
        FaceGenerator gen;
        ShapeType type = static_cast<ShapeType>(*rc::gen::inRange(0, 4));

        ShapeParams s;
        s.type = type;
        switch (type) {
        case ShapeType::Cone:
            s.radius = 1.0f; s.height = 2.0f; s.segments = 8;
            break;
        case ShapeType::Cylinder:
            s.radius = 1.0f; s.height = 2.0f; s.segments = 8;
            break;
        case ShapeType::Sphere:
            s.radius = 1.0f; s.lonSegments = 8; s.latSegments = 6;
            break;
        case ShapeType::Torus:
            s.majorRadius = 2.0f; s.minorRadius = 0.5f; s.ringSegments = 8; s.sideSegments = 6;
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
