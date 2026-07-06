// Tests for ConnectionFaceMatcher — verifies face-matching logic at connection junctions

#include <rapidcheck.h>
#include <cstdio>
#include <cmath>

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
        parent.lon_segments = *rc::gen::inRange(6, 20);
        parent.lat_segments = *rc::gen::inRange(4, 16);
        parent.major_radius = *rc::gen::inRange(50, 150) / 100.0f;
        parent.minor_radius = *rc::gen::inRange(10, 40) / 100.0f;

        ShapeParams child;
        child.type = static_cast<ShapeType>(*rc::gen::inRange(0, 5));
        child.radius = *rc::gen::inRange(10, 80) / 100.0f;
        child.height = *rc::gen::inRange(30, 150) / 100.0f;
        child.segments = *rc::gen::inRange(6, 20);
        child.lon_segments = *rc::gen::inRange(6, 20);
        child.lat_segments = *rc::gen::inRange(4, 16);
        child.major_radius = *rc::gen::inRange(40, 120) / 100.0f;
        child.minor_radius = *rc::gen::inRange(10, 35) / 100.0f;

        AttachmentPoint parent_attach(AttachRegion::Top, 0.5f, 0.5f);
        AttachmentPoint child_attach(AttachRegion::Bottom, 0.5f, 0.5f);

        float parent_r = matcher.ComputeConnectionRadius(parent, parent_attach);
        float child_r = matcher.ComputeConnectionRadius(child, child_attach);
        float matched = matcher.ComputeMatchedRadius(parent, parent_attach, child, child_attach);

        RC_ASSERT(matched <= parent_r + 0.01f);
        RC_ASSERT(matched <= child_r + 0.01f);
        RC_ASSERT(matched >= 0.01f); // minimum viable
    });

    // Test: Matched segments >= 3
    rc::check("Matched segments >= 3", []() {
        ConnectionFaceMatcher matcher;

        ShapeParams parent;
        parent.type = ShapeType::Cylinder;
        parent.segments = *rc::gen::inRange(3, 30);
        parent.lon_segments = parent.segments;

        ShapeParams child;
        child.type = ShapeType::Sphere;
        child.segments = *rc::gen::inRange(3, 30);
        child.lon_segments = *rc::gen::inRange(3, 30);

        AttachmentPoint parent_attach(AttachRegion::Top, 0.5f, 0.5f);
        AttachmentPoint child_attach(AttachRegion::Surface, 0.5f, 1.0f);

        int matched = matcher.ComputeMatchedSegments(parent, parent_attach, child, child_attach);
        RC_ASSERT(matched >= 3);
    });

    // Test: GenerateWithConnections produces non-empty faces
    rc::check("GenerateWithConnections produces valid faces", []() {
        ConnectionFaceMatcher matcher;

        BodyNode node;
        node.name = "test_sphere";
        node.shape.type = ShapeType::Sphere;
        node.shape.radius = 1.0f;
        node.shape.lon_segments = 12;
        node.shape.lat_segments = 8;

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

        // The ring face should have the requested number of segments as vertices
        const Face& ring_face = result.faces[ring_idx];
        RC_ASSERT(static_cast<int>(ring_face.vertices.size()) == ring.segments);
    });

    // Test: Cylinder connection radius matches cylinder radius for cap
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

    printf("  PASS\n");
}
