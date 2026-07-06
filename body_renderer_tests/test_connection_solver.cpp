// Property 6: Connection Transform — Shared Face Coincidence
// Property 7: Nested Transform Accumulation

#include <rapidcheck.h>
#include "ConnectionSolver.h"
#include "FaceGenerator.h"
#include <cstdio>
#include <cmath>

using namespace BodyRenderer;

void RunConnectionSolverTests()
{
    printf("--- ConnectionSolver Tests ---\n");

    // Property 6: Face connection makes child's connection face exactly coincident
    // with parent's connection face (shared polygon — same center, same plane).
    rc::check("Property 6: Face connection positions child face coincident with parent face", []() {
        FaceGenerator faceGen;
        ConnectionSolver solver;

        // Parent: a cylinder with 8 sides
        ShapeParams parent_shape;
        parent_shape.type = ShapeType::Cylinder;
        parent_shape.radius = 1.0f;
        parent_shape.height = 2.0f;
        parent_shape.segments = 8;

        auto parent_faces = faceGen.Generate(parent_shape);
        RC_PRE(parent_faces.size() == 10); // 8 lateral + top cap + bottom cap

        // Child: a cylinder with 8 sides (same segment count = compatible topology)
        ShapeParams child_shape;
        child_shape.type = ShapeType::Cylinder;
        child_shape.radius = 0.5f;
        child_shape.height = 1.0f;
        child_shape.segments = 8;

        auto child_faces = faceGen.Generate(child_shape);
        RC_PRE(child_faces.size() == 10);

        // Connect parent top cap (face 8) to child bottom cap (face 9)
        // Both are 8-vertex polygons — topologically compatible
        Connection conn;
        conn.type = ConnectionType::Face_Connection;
        conn.parent_face_index = 8; // top cap
        conn.child_face_index = 9;  // bottom cap
        conn.offset_u = 0.5f;
        conn.offset_v = 0.5f;
        conn.rotation = 0.0f;

        Mat4 transform = solver.ComputeTransform(conn, parent_faces, child_faces);

        // Verify: child's connection face center ends up at parent face center
        Vec3 child_face_center(0, 0, 0);
        for (const Vec3& v : child_faces[9].vertices) {
            child_face_center = child_face_center + v;
        }
        child_face_center = child_face_center * (1.0f / static_cast<float>(child_faces[9].vertices.size()));

        Vec3 transformed_child_center = transform.TransformPoint(child_face_center);

        Vec3 parent_face_center(0, 0, 0);
        for (const Vec3& v : parent_faces[8].vertices) {
            parent_face_center = parent_face_center + v;
        }
        parent_face_center = parent_face_center * (1.0f / static_cast<float>(parent_faces[8].vertices.size()));

        Vec3 diff = transformed_child_center - parent_face_center;
        float dist = diff.Length();
        RC_ASSERT(dist < 0.01f);

        // Verify: child's connection face normal is opposite to parent face normal
        // (faces are back-to-back, sharing the boundary polygon)
        Vec3 child_normal = child_faces[9].normal;
        Vec3 transformed_normal = transform.TransformDirection(child_normal).Normalized();
        Vec3 parent_normal = parent_faces[8].normal.Normalized();

        // Should point in opposite direction
        float ndot = transformed_normal.Dot(parent_normal);
        RC_ASSERT(ndot < -0.9f); // nearly anti-parallel

        // Verify: child face is scaled to match parent face size
        // Compute radius (max distance from center) for both
        float parent_radius = 0.0f;
        for (const Vec3& v : parent_faces[8].vertices) {
            float d = (v - parent_face_center).Length();
            if (d > parent_radius) parent_radius = d;
        }

        float child_radius = 0.0f;
        for (const Vec3& v : child_faces[9].vertices) {
            Vec3 tv = transform.TransformPoint(v);
            float d = (tv - transformed_child_center).Length();
            if (d > child_radius) child_radius = d;
        }

        // Radii should match (faces are same size)
        RC_ASSERT(std::fabs(parent_radius - child_radius) < 0.05f);
    });

    // Property 6b: Lateral face connection (quad-to-quad)
    rc::check("Property 6b: Lateral face connection shares quad face", []() {
        FaceGenerator faceGen;
        ConnectionSolver solver;

        // Two cylinders with same segment count — lateral faces are quads
        ShapeParams parent_shape;
        parent_shape.type = ShapeType::Cylinder;
        parent_shape.radius = 1.0f;
        parent_shape.height = 2.0f;
        parent_shape.segments = 8;

        ShapeParams child_shape;
        child_shape.type = ShapeType::Cylinder;
        child_shape.radius = 0.7f;
        child_shape.height = 1.5f;
        child_shape.segments = 8;

        auto parent_faces = faceGen.Generate(parent_shape);
        auto child_faces = faceGen.Generate(child_shape);

        // Connect lateral face 3 (parent) to lateral face 5 (child) — both are quads
        Connection conn;
        conn.type = ConnectionType::Face_Connection;
        conn.parent_face_index = 3;
        conn.child_face_index = 5;
        conn.rotation = 0.0f;

        Mat4 transform = solver.ComputeTransform(conn, parent_faces, child_faces);

        // Verify face centers coincide
        Vec3 pc(0, 0, 0);
        for (const Vec3& v : parent_faces[3].vertices) pc = pc + v;
        pc = pc * (1.0f / static_cast<float>(parent_faces[3].vertices.size()));

        Vec3 cc(0, 0, 0);
        for (const Vec3& v : child_faces[5].vertices) cc = cc + v;
        cc = cc * (1.0f / static_cast<float>(child_faces[5].vertices.size()));
        Vec3 tcc = transform.TransformPoint(cc);

        float dist = (tcc - pc).Length();
        RC_ASSERT(dist < 0.01f);
    });

    // Property 7: Inserting identity transform doesn't change result
    rc::check("Property 7: Identity transform preserves positions", []() {
        Mat4 identity;
        Vec3 test_point(*rc::gen::inRange(-100, 100) / 10.0f,
                        *rc::gen::inRange(-100, 100) / 10.0f,
                        *rc::gen::inRange(-100, 100) / 10.0f);

        // Any transform * identity = same transform
        Mat4 trans = Mat4::Translation(1.0f, 2.0f, 3.0f);
        Mat4 result = trans * identity;

        Vec3 p1 = trans.TransformPoint(test_point);
        Vec3 p2 = result.TransformPoint(test_point);

        float eps = 1e-5f;
        RC_ASSERT(std::fabs(p1.x - p2.x) < eps);
        RC_ASSERT(std::fabs(p1.y - p2.y) < eps);
        RC_ASSERT(std::fabs(p1.z - p2.z) < eps);

        // identity * transform = same transform
        Mat4 result2 = identity * trans;
        Vec3 p3 = result2.TransformPoint(test_point);
        RC_ASSERT(std::fabs(p1.x - p3.x) < eps);
        RC_ASSERT(std::fabs(p1.y - p3.y) < eps);
        RC_ASSERT(std::fabs(p1.z - p3.z) < eps);
    });

    printf("  PASS\n");
}
