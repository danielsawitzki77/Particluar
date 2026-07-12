// Property: Volume Overlap Validation
// Tests that bodies connect via shared faces without overlapping volumes

#include <rapidcheck.h>
#include "ConnectionValidator.h"
#include "ConnectionSolver.h"
#include "FaceGenerator.h"
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
        conn.type = ConnectionType::FaceConnection;
        conn.parentFaceIndex = 8;   // parent top cap (8 vertices)
        conn.childFaceIndex = 12;   // child top cap (12 vertices)

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
        conn.type = ConnectionType::FaceConnection;
        conn.parentFaceIndex = n;   // cylinder top cap
        conn.childFaceIndex = n;    // cone base

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
        conn.type = ConnectionType::FaceConnection;
        conn.parentFaceIndex = 99;  // out of range
        conn.childFaceIndex = 0;

        auto result = validator.ValidateConnection(conn, faces, faces, "parent", "child");
        RC_ASSERT(!result.valid);
        RC_ASSERT(result.error.find("out of range") != std::string::npos);
    });

    // Property: Edge and Point connections skip topology check
    rc::check("Property: PointConnection bypasses topology check", []() {
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
        child_shape.lonSegments = 8;
        child_shape.latSegments = 6;
        auto child_faces = faceGen.Generate(child_shape);

        // Point connection should not check topology
        Connection conn;
        conn.type = ConnectionType::PointConnection;
        conn.parentFaceIndex = 8;
        conn.childFaceIndex = 0;

        auto result = validator.ValidateConnection(conn, parent_faces, child_faces, "parent", "child");
        RC_ASSERT(result.valid);
    });

    // Property: Body-level validation catches incompatible children
    rc::check("Property: ValidateBody catches incompatible child connections", []() {
        ConnectionValidator validator;

        Body body;
        body.name = "test";
        body.formatVersion = 1;
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
        child.connection.type = ConnectionType::FaceConnection;
        child.connection.parentFaceIndex = 8;
        child.connection.childFaceIndex = 12;

        body.root.children.push_back(child);

        auto result = validator.ValidateBody(body);
        RC_ASSERT(!result.valid);
    });

    // =========================================================================
    // Volume Overlap Tests
    // =========================================================================

    // Property: Valid face connection (cap-to-cap) has no volume overlap
    rc::check("Property: Cap-to-cap cylinder connection has no volume overlap", []() {
        ConnectionValidator validator;
        ConnectionSolver solver;
        FaceGenerator faceGen;

        int n = *rc::gen::inRange(4, 24);

        // Parent: cylinder
        ShapeParams parent_shape;
        parent_shape.type = ShapeType::Cylinder;
        parent_shape.radius = 1.0f;
        parent_shape.height = 2.0f;
        parent_shape.segments = n;
        auto parent_faces = faceGen.Generate(parent_shape);

        // Child: smaller cylinder connecting bottom cap to parent's top cap
        ShapeParams child_shape;
        child_shape.type = ShapeType::Cylinder;
        child_shape.radius = 0.5f;
        child_shape.height = 1.0f;
        child_shape.segments = n;
        auto child_faces = faceGen.Generate(child_shape);

        // Connect parent top cap (face N) to child bottom cap (face N+1)
        Connection conn;
        conn.type = ConnectionType::FaceConnection;
        conn.parentFaceIndex = n;     // parent top cap
        conn.childFaceIndex = n + 1;  // child bottom cap
        conn.rotation = 0.0f;

        Mat4 transform = solver.ComputeLegacyTransform(conn, parent_faces, child_faces);

        auto result = validator.ValidateNoVolumeOverlap(
            conn, parent_faces, child_faces, transform, "parent", "child");
        RC_ASSERT(result.valid);
    });

    // Property: Connecting top-cap-to-top-cap also works (solver flips child)
    rc::check("Property: Top-cap-to-top-cap also valid (solver flips child correctly)", []() {
        ConnectionValidator validator;
        ConnectionSolver solver;
        FaceGenerator faceGen;

        int n = *rc::gen::inRange(4, 24);

        ShapeParams parent_shape;
        parent_shape.type = ShapeType::Cylinder;
        parent_shape.radius = 1.0f;
        parent_shape.height = 2.0f;
        parent_shape.segments = n;
        auto parent_faces = faceGen.Generate(parent_shape);

        ShapeParams child_shape;
        child_shape.type = ShapeType::Cylinder;
        child_shape.radius = 0.5f;
        child_shape.height = 1.0f;
        child_shape.segments = n;
        auto child_faces = faceGen.Generate(child_shape);

        // Connect parent top cap to child top cap — solver flips child upside down
        // so it extends outward (upward). This should be valid.
        Connection conn;
        conn.type = ConnectionType::FaceConnection;
        conn.parentFaceIndex = n;  // parent top cap
        conn.childFaceIndex = n;   // child top cap
        conn.rotation = 0.0f;

        Mat4 transform = solver.ComputeLegacyTransform(conn, parent_faces, child_faces);

        auto result = validator.ValidateNoVolumeOverlap(
            conn, parent_faces, child_faces, transform, "parent", "child");
        RC_ASSERT(result.valid);
    });

    // Property: Torus connected via a lateral face causes overlap (wraps around both sides)
    rc::check("Property: Torus lateral face connection causes volume overlap", []() {
        ConnectionValidator validator;
        ConnectionSolver solver;
        FaceGenerator faceGen;

        // Parent: cylinder
        ShapeParams parent_shape;
        parent_shape.type = ShapeType::Cylinder;
        parent_shape.radius = 1.0f;
        parent_shape.height = 2.0f;
        parent_shape.segments = 8;
        auto parent_faces = faceGen.Generate(parent_shape);

        // Child: torus — geometry wraps around in a ring. When connecting via
        // one of its lateral quad faces, the torus extends on BOTH sides of that face.
        ShapeParams child_shape;
        child_shape.type = ShapeType::Torus;
        child_shape.majorRadius = 1.0f;
        child_shape.minorRadius = 0.4f;
        child_shape.ringSegments = 8;
        child_shape.sideSegments = 8;
        auto child_faces = faceGen.Generate(child_shape);

        // Connect parent top cap (face 8, N=8-gon) to torus face 0 (a quad)
        // The torus face is a quad but the parent top cap is an 8-gon — topology mismatch.
        // Use a lateral parent face (quad) to a torus face (quad) instead.
        Connection conn;
        conn.type = ConnectionType::FaceConnection;
        conn.parentFaceIndex = 0;  // parent lateral quad face
        conn.childFaceIndex = 0;   // torus quad face
        conn.rotation = 0.0f;

        Mat4 transform = solver.ComputeLegacyTransform(conn, parent_faces, child_faces);

        auto result = validator.ValidateNoVolumeOverlap(
            conn, parent_faces, child_faces, transform, "parent", "child");
        // Torus wraps around — some geometry will be behind the connection face
        RC_ASSERT(!result.valid);
        RC_ASSERT(result.error.find("volume overlap") != std::string::npos);
    });

    // Property: Cone on cylinder top cap is valid (cone base matches cylinder cap)
    rc::check("Property: Cone on cylinder top cap has no overlap", []() {
        ConnectionValidator validator;
        ConnectionSolver solver;
        FaceGenerator faceGen;

        int n = *rc::gen::inRange(4, 24);

        ShapeParams parent_shape;
        parent_shape.type = ShapeType::Cylinder;
        parent_shape.radius = 1.0f;
        parent_shape.height = 2.0f;
        parent_shape.segments = n;
        auto parent_faces = faceGen.Generate(parent_shape);

        ShapeParams child_shape;
        child_shape.type = ShapeType::Cone;
        child_shape.radius = 0.8f;
        child_shape.height = 1.5f;
        child_shape.segments = n;
        auto child_faces = faceGen.Generate(child_shape);

        // Cone base (face N, last face) connects to cylinder top cap (face N)
        Connection conn;
        conn.type = ConnectionType::FaceConnection;
        conn.parentFaceIndex = n;  // cylinder top cap
        conn.childFaceIndex = n;   // cone base
        conn.rotation = 0.0f;

        Mat4 transform = solver.ComputeLegacyTransform(conn, parent_faces, child_faces);

        auto result = validator.ValidateNoVolumeOverlap(
            conn, parent_faces, child_faces, transform, "parent", "child");
        RC_ASSERT(result.valid);
    });

    // Property: PointConnection skips volume overlap check
    rc::check("Property: PointConnection skips volume overlap check", []() {
        ConnectionValidator validator;
        ConnectionSolver solver;
        FaceGenerator faceGen;

        ShapeParams parent_shape;
        parent_shape.type = ShapeType::Cylinder;
        parent_shape.radius = 1.0f;
        parent_shape.height = 2.0f;
        parent_shape.segments = 8;
        auto parent_faces = faceGen.Generate(parent_shape);

        ShapeParams child_shape;
        child_shape.type = ShapeType::Sphere;
        child_shape.radius = 0.5f;
        child_shape.lonSegments = 8;
        child_shape.latSegments = 6;
        auto child_faces = faceGen.Generate(child_shape);

        Connection conn;
        conn.type = ConnectionType::PointConnection;
        conn.parentFaceIndex = 8;
        conn.childFaceIndex = 0;

        Mat4 transform = solver.ComputeLegacyTransform(conn, parent_faces, child_faces);

        auto result = validator.ValidateNoVolumeOverlap(
            conn, parent_faces, child_faces, transform, "parent", "child");
        RC_ASSERT(result.valid); // skipped, always valid for PointConnection
    });

    // Property: ValidateBody passes for correctly defined bodies
    rc::check("Property: Correctly defined body passes full validation", []() {
        ConnectionValidator validator;

        Body body;
        body.name = "valid_robot";
        body.root.name = "torso";
        body.root.shape.type = ShapeType::Cylinder;
        body.root.shape.radius = 0.5f;
        body.root.shape.height = 2.0f;
        body.root.shape.segments = 12;

        // Head on top (parent top cap → child bottom cap)
        BodyNode head;
        head.name = "head";
        head.shape.type = ShapeType::Cylinder;
        head.shape.radius = 0.3f;
        head.shape.height = 0.6f;
        head.shape.segments = 12;
        head.connection.type = ConnectionType::FaceConnection;
        head.connection.parentFaceIndex = 12;  // torso top cap
        head.connection.childFaceIndex = 13;   // head bottom cap
        head.connection.rotation = 0.0f;

        body.root.children.push_back(head);

        auto result = validator.ValidateBody(body);
        RC_ASSERT(result.valid);
    });

    // Property: ValidateBody catches torus overlap in nested bodies
    rc::check("Property: ValidateBody catches torus volume overlap", []() {
        ConnectionValidator validator;

        Body body;
        body.name = "test_overlap";
        body.formatVersion = 1;
        body.root.name = "base";
        body.root.shape.type = ShapeType::Cylinder;
        body.root.shape.radius = 1.0f;
        body.root.shape.height = 2.0f;
        body.root.shape.segments = 8;

        // Attach torus via lateral face connection — torus wraps and overlaps
        BodyNode child;
        child.name = "overlapping_torus";
        child.shape.type = ShapeType::Torus;
        child.shape.majorRadius = 1.0f;
        child.shape.minorRadius = 0.4f;
        child.shape.ringSegments = 8;
        child.shape.sideSegments = 8;

        // Connect cylinder lateral quad (face 0) to torus quad (face 0)
        child.connection.type = ConnectionType::FaceConnection;
        child.connection.parentFaceIndex = 0;  // lateral quad
        child.connection.childFaceIndex = 0;   // torus face
        child.connection.rotation = 0.0f;

        body.root.children.push_back(child);

        auto result = validator.ValidateBody(body);
        RC_ASSERT(!result.valid);
        RC_ASSERT(result.error.find("volume overlap") != std::string::npos);
    });

    // Property: Lateral face connections with correctly oriented children pass
    rc::check("Property: Lateral connection with correct child face passes", []() {
        ConnectionValidator validator;
        ConnectionSolver solver;
        FaceGenerator faceGen;

        int n = 12;

        // Parent: cylinder
        ShapeParams parent_shape;
        parent_shape.type = ShapeType::Cylinder;
        parent_shape.radius = 1.0f;
        parent_shape.height = 2.0f;
        parent_shape.segments = n;
        auto parent_faces = faceGen.Generate(parent_shape);

        // Child: small cylinder connecting via its top cap to a lateral face
        ShapeParams child_shape;
        child_shape.type = ShapeType::Cylinder;
        child_shape.radius = 0.15f;
        child_shape.height = 0.8f;
        child_shape.segments = n;
        auto child_faces = faceGen.Generate(child_shape);

        // Connect parent lateral face 3 (quad) to child top cap (N-gon)
        // Note: topology mismatch (quad vs N-gon) — this would fail topology check.
        // Use lateral-to-lateral instead (both are quads for same N).
        Connection conn;
        conn.type = ConnectionType::FaceConnection;
        conn.parentFaceIndex = 3;  // parent lateral quad
        conn.childFaceIndex = 3;   // child lateral quad
        conn.rotation = 0.0f;

        Mat4 transform = solver.ComputeLegacyTransform(conn, parent_faces, child_faces);

        auto result = validator.ValidateNoVolumeOverlap(
            conn, parent_faces, child_faces, transform, "parent", "child");
        RC_ASSERT(result.valid);
    });

    // Property: Valid body JSON files pass full validation including overlap check
    rc::check("Property: Rocket body file passes full validation including overlap check", []() {
        BodyLoader loader;

        // Test with the rocket body (cap-to-cap face connections throughout)
        LoadResult result = loader.LoadFromFile(
            "c:\\Users\\Daniel Sawitzki\\Desktop\\github\\Particluar\\assets\\bodies\\06_rocket.json");
        RC_ASSERT(result.success);
    });

    // Property: All body files in assets/bodies/ load and validate successfully
    rc::check("Property: All asset body files pass validation", []() {
        BodyLoader loader;

        const char* files[] = {
            "c:\\Users\\Daniel Sawitzki\\Desktop\\github\\Particluar\\assets\\bodies\\01_unit_cube.json",
            "c:\\Users\\Daniel Sawitzki\\Desktop\\github\\Particluar\\assets\\bodies\\02_snowman.json",
            "c:\\Users\\Daniel Sawitzki\\Desktop\\github\\Particluar\\assets\\bodies\\03_robot_arm.json",
            "c:\\Users\\Daniel Sawitzki\\Desktop\\github\\Particluar\\assets\\bodies\\04_space_station.json",
            "c:\\Users\\Daniel Sawitzki\\Desktop\\github\\Particluar\\assets\\bodies\\05_chess_pawn.json",
            "c:\\Users\\Daniel Sawitzki\\Desktop\\github\\Particluar\\assets\\bodies\\06_rocket.json",
            "c:\\Users\\Daniel Sawitzki\\Desktop\\github\\Particluar\\assets\\bodies\\07_dumbbell.json",
            "c:\\Users\\Daniel Sawitzki\\Desktop\\github\\Particluar\\assets\\bodies\\08_table.json",
            "c:\\Users\\Daniel Sawitzki\\Desktop\\github\\Particluar\\assets\\bodies\\09_spider_bot.json",
            "c:\\Users\\Daniel Sawitzki\\Desktop\\github\\Particluar\\assets\\bodies\\10_satellite.json",
            "c:\\Users\\Daniel Sawitzki\\Desktop\\github\\Particluar\\assets\\bodies\\11_humanoid.json",
            "c:\\Users\\Daniel Sawitzki\\Desktop\\github\\Particluar\\assets\\bodies\\12_windmill.json"
        };

        for (const char* f : files) {
            LoadResult result = loader.LoadFromFile(f);
            if (!result.success) {
                printf("\n  LOAD FAIL: %s\n  Error: %s\n", f, result.error.c_str());
            }
            RC_ASSERT(result.success);
        }
    });

    printf("  PASS\n");
}
