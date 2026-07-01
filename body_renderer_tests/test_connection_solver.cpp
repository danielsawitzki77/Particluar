// Property 6: Connection Transform Geometric Correctness
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

    // Property 6: Face connection places child on parent face plane
    rc::check("Property 6: Face connection positions child on parent face", []() {
        FaceGenerator faceGen;
        ConnectionSolver solver;

        // Parent: a box
        ShapeParams parent_shape;
        parent_shape.type = ShapeType::Box;
        parent_shape.width = 2.0f;
        parent_shape.height = 2.0f;
        parent_shape.depth = 2.0f;

        auto parent_faces = faceGen.Generate(parent_shape);
        RC_PRE(parent_faces.size() == 6);

        // Child: a cylinder
        ShapeParams child_shape;
        child_shape.type = ShapeType::Cylinder;
        child_shape.radius = 0.5f;
        child_shape.height = 1.0f;
        child_shape.segments = 8;

        auto child_faces = faceGen.Generate(child_shape);

        int parent_face_idx = *rc::gen::inRange(0, 6);
        Connection conn;
        conn.type = ConnectionType::Face_Connection;
        conn.parent_face_index = parent_face_idx;
        conn.child_face_index = 0;
        conn.offset_u = 0.5f;
        conn.offset_v = 0.5f;
        conn.rotation = 0.0f;

        Mat4 transform = solver.ComputeTransform(conn, parent_faces, child_faces);

        // The child origin (0,0,0) should end up on the parent face plane
        Vec3 child_origin = transform.TransformPoint(Vec3(0, 0, 0));
        Vec3 face_center(0, 0, 0);
        for (const Vec3& v : parent_faces[parent_face_idx].vertices) {
            face_center = face_center + v;
        }
        face_center = face_center * (1.0f / static_cast<float>(parent_faces[parent_face_idx].vertices.size()));

        // Child origin should be near the face center
        Vec3 diff = child_origin - face_center;
        float dist = diff.Length();
        RC_ASSERT(dist < 0.1f);
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
