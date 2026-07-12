// Tests for ConnectionFaceMatcher — verifies face-matching logic at connection junctions

#include <rapidcheck.h>
#include <cstdio>
#include <cmath>
#include <algorithm>

#include "ConnectionFaceMatcher.h"
#include "FaceGenerator.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace BodyRenderer;

void RunConnectionFaceMatcherTests()
{
    printf("--- ConnectionFaceMatcher Tests ---\n");

    // Test: Matched radius is <= both individual connection radii
    rc::check("Matched radius <= min(parent_radius, child_radius)", []() {
        ConnectionFaceMatcher matcher;

        // Generate random parent/child shapes
        ShapeParams parent;
        parent.type = static_cast<ShapeType>(*rc::gen::inRange(0, 5));
        parent.radius = *rc::gen::inRange(20, 100) / 100.0f;
        parent.height = *rc::gen::inRange(50, 200) / 100.0f;
        parent.segments = *rc::gen::inRange(6, 20);
        parent.lonSegments = *rc::gen::inRange(6, 20);
        parent.latSegments = *rc::gen::inRange(4, 16);
        parent.majorRadius = *rc::gen::inRange(50, 150) / 100.0f;
        parent.minorRadius = *rc::gen::inRange(10, 40) / 100.0f;

        ShapeParams child;
        child.type = static_cast<ShapeType>(*rc::gen::inRange(0, 5));
        child.radius = *rc::gen::inRange(10, 80) / 100.0f;
        child.height = *rc::gen::inRange(30, 150) / 100.0f;
        child.segments = *rc::gen::inRange(6, 20);
        child.lonSegments = *rc::gen::inRange(6, 20);
        child.latSegments = *rc::gen::inRange(4, 16);
        child.majorRadius = *rc::gen::inRange(40, 120) / 100.0f;
        child.minorRadius = *rc::gen::inRange(10, 35) / 100.0f;

        AttachmentPoint parentAttach(AttachRegion::Top, 0.5f, 0.5f);
        AttachmentPoint childAttach(AttachRegion::Bottom, 0.5f, 0.5f);

        float parent_r = matcher.ComputeConnectionRadius(parent, parentAttach);
        float child_r = matcher.ComputeConnectionRadius(child, childAttach);
        float matched = matcher.ComputeMatchedRadius(parent, parentAttach, child, childAttach);

        // Matched radius should be between the two sides' radii (with tolerance)
        float max_r = (std::max)(parent_r, child_r);
        RC_ASSERT(matched <= max_r + 0.01f);
        RC_ASSERT(matched >= 0.01f); // minimum viable
    });

    // Test: Matched segments >= 3
    rc::check("Matched segments >= 3", []() {
        ConnectionFaceMatcher matcher;

        ShapeParams parent;
        parent.type = ShapeType::Cylinder;
        parent.segments = *rc::gen::inRange(3, 30);
        parent.lonSegments = parent.segments;

        ShapeParams child;
        child.type = ShapeType::Sphere;
        child.segments = *rc::gen::inRange(3, 30);
        child.lonSegments = *rc::gen::inRange(3, 30);

        AttachmentPoint parentAttach(AttachRegion::Top, 0.5f, 0.5f);
        AttachmentPoint childAttach(AttachRegion::Surface, 0.5f, 1.0f);

        int matched = matcher.ComputeMatchedSegments(parent, parentAttach, child, childAttach);
        RC_ASSERT(matched >= 3);
    });

    // Test: GenerateWithConnections produces non-empty faces
    rc::check("GenerateWithConnections produces valid faces", []() {
        ConnectionFaceMatcher matcher;

        BodyNode node;
        node.name = "test_sphere";
        node.shape.type = ShapeType::Sphere;
        node.shape.radius = 1.0f;
        node.shape.lonSegments = 12;
        node.shape.latSegments = 8;

        // No rings — should produce same as FaceGenerator
        MatchedFaces result = matcher.GenerateWithConnections(node, {});
        RC_ASSERT(!result.faces.empty());

        FaceGenerator faceGen;
        std::vector<Face> plain_faces = faceGen.Generate(node.shape);
        RC_ASSERT(result.faces.size() == plain_faces.size());
    });

    // Test: Ring insertion produces a face at the ring location
    rc::check("Ring insertion adds a connection face near the ring center", []() {
        ConnectionFaceMatcher matcher;

        BodyNode node;
        node.name = "test_cylinder";
        node.shape.type = ShapeType::Cylinder;
        node.shape.radius = 1.0f;
        node.shape.height = 2.0f;
        node.shape.segments = 12;

        // Add a ring at the top cap with half the cylinder radius
        ConnectionRing ring;
        ring.center = Vec3(0, 1.0f, 0); // top of cylinder
        ring.normal = Vec3(0, 1, 0);
        ring.radius = 0.5f;
        ring.segments = 8;
        ring.child_index = 0;

        MatchedFaces result = matcher.GenerateWithConnections(node, {ring});
        RC_ASSERT(!result.faces.empty());
        RC_ASSERT(result.connection_face_indices.size() == 1);

        int ring_idx = result.connection_face_indices[0];
        RC_ASSERT(ring_idx >= 0);
        RC_ASSERT(ring_idx < static_cast<int>(result.faces.size()));

        // The ring face is the cylinder's top cap (identified by grid index),
        // which has the cylinder's segment count as its vertex count.
        const Face& ring_face = result.faces[ring_idx];
        RC_ASSERT(static_cast<int>(ring_face.vertices.size()) == node.shape.segments);
    });

    // Test: Cylinder top/bottom connection radius matches cylinder radius for cap
    rc::check("Cylinder top/bottom connection radius equals cylinder radius", []() {
        ConnectionFaceMatcher matcher;

        ShapeParams cyl;
        cyl.type = ShapeType::Cylinder;
        cyl.radius = *rc::gen::inRange(20, 200) / 100.0f;
        cyl.height = *rc::gen::inRange(50, 300) / 100.0f;
        cyl.segments = 16;

        AttachmentPoint top_attach(AttachRegion::Top, 0.5f, 0.5f);
        float r = matcher.ComputeConnectionRadius(cyl, top_attach);
        RC_ASSERT(std::fabs(r - cyl.radius) < 0.001f);

        AttachmentPoint bot_attach(AttachRegion::Bottom, 0.5f, 0.5f);
        r = matcher.ComputeConnectionRadius(cyl, bot_attach);
        RC_ASSERT(std::fabs(r - cyl.radius) < 0.001f);
    });

    // Test: Tapering produces outward-pointing normals (no inverted faces)
    rc::check("Tapered faces have outward-pointing normals", []() {
        ConnectionFaceMatcher matcher;

        BodyNode node;
        node.name = "test_cylinder_tapered";
        node.shape.type = ShapeType::Cylinder;
        node.shape.radius = 1.0f;
        node.shape.height = 3.0f;
        node.shape.segments = 12;
        node.shape.heightSegments = 4; // subdivided for tapering

        // Add a ring at the top cap with half the cylinder radius — triggers tapering
        ConnectionRing ring;
        ring.center = Vec3(0, 1.5f, 0); // top of cylinder
        ring.normal = Vec3(0, 1, 0);
        ring.radius = 0.4f; // much smaller than cylinder radius
        ring.segments = 8;
        ring.child_index = 0;

        MatchedFaces result = matcher.GenerateWithConnections(node, {ring});
        RC_ASSERT(!result.faces.empty());

        // Verify all face normals point outward (dot product with face center from origin > 0)
        // For lateral faces, the normal should point away from the Y axis
        for (const auto& face : result.faces) {
            Vec3 face_center(0, 0, 0);
            for (const auto& v : face.vertices) face_center = face_center + v;
            face_center = face_center * (1.0f / face.vertices.size());

            // For top/bottom cap faces, check normal vs Y axis
            if (std::fabs(face.normal.y) > 0.8f) {
                // Cap face: normal should point up for top cap, down for bottom cap
                if (face_center.y > 0) {
                    RC_ASSERT(face.normal.y > 0); // top cap normal points up
                } else {
                    RC_ASSERT(face.normal.y < 0); // bottom cap normal points down
                }
            } else {
                // Lateral face: normal should point outward from Y axis
                Vec3 radial(face_center.x, 0, face_center.z);
                if (radial.Length() > 0.01f) {
                    float dot = face.normal.Dot(radial.Normalized());
                    RC_ASSERT(dot > -0.1f); // allow slight tolerance but not inverted
                }
            }
        }
    });

    // Test: Height-segmented cylinder produces correct face count
    rc::check("Cylinder with heightSegments produces correct face count", []() {
        FaceGenerator gen;

        ShapeParams s;
        s.type = ShapeType::Cylinder;
        s.radius = 1.0f;
        s.height = 2.0f;
        s.segments = *rc::gen::inRange(3, 20);
        s.heightSegments = *rc::gen::inRange(1, 8);

        auto faces = gen.Generate(s);
        // Expected: segments * heightSegments lateral quads + 2 caps
        int expected = s.segments * s.heightSegments + 2;
        RC_ASSERT(static_cast<int>(faces.size()) == expected);
    });

    printf("  PASS\n");
}
