// Property 8: Connection Validator rejects mismatched face topology
// Property 9: Connection Validator accepts compatible N-gon faces

#include <rapidcheck.h>
#include "ConnectionValidator.h"
#include "BodyLoader.h"
#include <cstdio>
#include <cmath>

using namespace BodyRenderer;

void RunConnectionValidatorTests()
{
    printf("--- ConnectionValidator Tests ---\n");

    // Property 8: Mismatched face vertex counts are rejected
    rc::check("Property 8: Mismatched face topology rejected", []() {
        ConnectionValidator validator;
        FaceGenerator faceGen;

        // Parent: cylinder with 8 sides — top cap (face 8) is 8-gon
        ShapeParams parent_shape;
        parent_shape.type = ShapeType::Cylinder;
        parent_shape.radius = 1.0f;
        parent_shape.height = 1.0f;
        parent_shape.segments = 8;
        auto parent_faces = faceGen.Generate(parent_shape);

        // Child: cylinder with 12 sides — top cap (face 12) is 12-gon
        ShapeParams child_shape;
        child_shape.type = ShapeType::Cylinder;
        child_shape.radius = 0.5f;
        child_shape.height = 0.5f;
        child_shape.segments = 12;
        auto child_faces = faceGen.Generate(child_shape);

        // Try to connect 8-gon cap to 12-gon cap — should fail
        Connection conn;
        conn.type = ConnectionType::Face_Connection;
        conn.parent_face_index = 8;   // parent top cap (8 vertices)
        conn.child_face_index = 12;   // child top cap (12 vertices)

        auto result = validator.ValidateConnection(conn, parent_faces, child_faces, "parent", "child");
        RC_ASSERT(!result.valid);
        RC_ASSERT(result.error.find("topology mismatch") != std::string::npos);
    });

    // Property 9: Compatible N-gon faces accepted
    rc::check("Property 9: Compatible N-gon connections accepted", []() {
        ConnectionValidator validator;
        FaceGenerator faceGen;

        int n = *rc::gen::inRange(3, 32);

        // Parent: cylinder with N sides — top cap is N-gon (face N)
        ShapeParams parent_shape;
        parent_shape.type = ShapeType::Cylinder;
        parent_shape.radius = 1.0f;
        parent_shape.height = 1.0f;
        parent_shape.segments = n;
        auto parent_faces = faceGen.Generate(parent_shape);

        // Child: cone with N sides — base is N-gon (face N)
        ShapeParams child_shape;
        child_shape.type = ShapeType::Cone;
        child_shape.radius = 0.5f;
        child_shape.height = 1.0f;
        child_shape.segments = n;
        auto child_faces = faceGen.Generate(child_shape);

        // Connect cylinder top cap (face N, N vertices) to cone base (face N, N vertices)
        Connection conn;
        conn.type = ConnectionType::Face_Connection;
        conn.parent_face_index = n;   // cylinder top cap
        conn.child_face_index = n;    // cone base

        auto result = validator.ValidateConnection(conn, parent_faces, child_faces, "parent", "child");
        RC_ASSERT(result.valid);
    });

    // Property: Out-of-range face indices rejected
    rc::check("Property: Out-of-range face indices rejected", []() {
        ConnectionValidator validator;
        FaceGenerator faceGen;

        ShapeParams shape;
        shape.type = ShapeType::Cylinder;
        shape.radius = 1.0f;
        shape.height = 1.0f;
        shape.segments = 8;
        auto faces = faceGen.Generate(shape);

        Connection conn;
        conn.type = ConnectionType::Face_Connection;
        conn.parent_face_index = 99;  // out of range
        conn.child_face_index = 0;

        auto result = validator.ValidateConnection(conn, faces, faces, "parent", "child");
        RC_ASSERT(!result.valid);
        RC_ASSERT(result.error.find("out of range") != std::string::npos);
    });

    // Property: Edge and Point connections skip topology check
    rc::check("Property: Point_Connection bypasses topology check", []() {
        ConnectionValidator validator;
        FaceGenerator faceGen;

        // Parent: cylinder 8 sides
        ShapeParams parent_shape;
        parent_shape.type = ShapeType::Cylinder;
        parent_shape.radius = 1.0f;
        parent_shape.height = 1.0f;
        parent_shape.segments = 8;
        auto parent_faces = faceGen.Generate(parent_shape);

        // Child: sphere (has triangle faces, different from cylinder quads/N-gons)
        ShapeParams child_shape;
        child_shape.type = ShapeType::Sphere;
        child_shape.radius = 0.5f;
        child_shape.lon_segments = 8;
        child_shape.lat_segments = 6;
        auto child_faces = faceGen.Generate(child_shape);

        // Point connection should not check topology
        Connection conn;
        conn.type = ConnectionType::Point_Connection;
        conn.parent_face_index = 8;
        conn.child_face_index = 0;

        auto result = validator.ValidateConnection(conn, parent_faces, child_faces, "parent", "child");
        RC_ASSERT(result.valid);
    });

    // Property: Body-level validation catches incompatible children
    rc::check("Property: ValidateBody catches incompatible child connections", []() {
        ConnectionValidator validator;

        Body body;
        body.name = "test";
        body.root.name = "parent";
        body.root.shape.type = ShapeType::Cylinder;
        body.root.shape.radius = 1.0f;
        body.root.shape.height = 1.0f;
        body.root.shape.segments = 8;

        BodyNode child;
        child.name = "child";
        child.shape.type = ShapeType::Cylinder;
        child.shape.radius = 0.5f;
        child.shape.height = 0.5f;
        child.shape.segments = 12; // different segment count

        // Connect parent top cap (8-gon) to child top cap (12-gon) — incompatible
        child.connection.type = ConnectionType::Face_Connection;
        child.connection.parent_face_index = 8;
        child.connection.child_face_index = 12;

        body.root.children.push_back(child);

        auto result = validator.ValidateBody(body);
        RC_ASSERT(!result.valid);
    });

    printf("  PASS\n");
}
