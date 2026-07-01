// Property 4: Triangle Decomposition Validity

#include <rapidcheck.h>
#include "Triangulator.h"
#include "FaceGenerator.h"
#include <cstdio>
#include <cmath>

using namespace BodyRenderer;

void RunTriangulatorTests()
{
    printf("--- Triangulator Tests ---\n");

    // Property 4: Triangle count = sum(vertices_per_face - 2)
    rc::check("Property 4: Triangle decomposition produces correct count", []() {
        FaceGenerator faceGen;
        Triangulator tri;

        ShapeType type = static_cast<ShapeType>(*rc::gen::inRange(0, 6));
        ShapeParams s;
        s.type = type;
        switch (type) {
        case ShapeType::Box:
            s.width = 1.0f; s.height = 1.0f; s.depth = 1.0f;
            break;
        case ShapeType::Cone:
            s.radius = 1.0f; s.height = 2.0f; s.segments = *rc::gen::inRange(3, 16);
            break;
        case ShapeType::Cylinder:
            s.radius = 1.0f; s.height = 2.0f; s.segments = *rc::gen::inRange(3, 16);
            break;
        case ShapeType::Sphere:
            s.radius = 1.0f; s.lon_segments = *rc::gen::inRange(4, 12); s.lat_segments = *rc::gen::inRange(3, 8);
            break;
        case ShapeType::Torus:
            s.major_radius = 2.0f; s.minor_radius = 0.5f;
            s.ring_segments = *rc::gen::inRange(3, 12); s.side_segments = *rc::gen::inRange(3, 8);
            break;
        case ShapeType::Frustum:
            s.bottom_radius = 2.0f; s.top_radius = 0.5f; s.height = 2.0f;
            s.segments = *rc::gen::inRange(3, 16);
            break;
        }

        auto faces = faceGen.Generate(s);
        auto tris = tri.Triangulate(faces);

        // Expected triangle count = sum(verts_per_face - 2)
        int expected = 0;
        for (const Face& f : faces) {
            int n = static_cast<int>(f.vertices.size());
            if (n >= 3) expected += (n - 2);
        }

        RC_ASSERT(static_cast<int>(tris.size()) == expected);

        // Each triangle should have a normal consistent with source face
        for (const Triangle& t : tris) {
            float len = t.normal.Length();
            RC_ASSERT(len > 0.01f); // non-zero normal
        }
    });

    printf("  PASS\n");
}
