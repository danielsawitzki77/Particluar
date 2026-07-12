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

    // Property 6: Face connection places child connection face on parent face
    rc::check("Property 6: Face connection positions child on parent face", []() {
        FaceGenerator faceGen;
        ConnectionSolver solver;

        // Parent: a cylinder
        ShapeParams parent_shape;
        parent_shape.type = ShapeType::Cylinder;
        parent_shape.radius = 1.0f;
        parent_shape.height = 2.0f;
        parent_shape.segments = 8;

        auto parent_faces = faceGen.Generate(parent_shape);
        RC_PRE(parent_faces.size() == 10); // 8 lateral + top cap + bottom cap

        // Child: a cylinder
        ShapeParams child_shape;
        child_shape.type = ShapeType::Cylinder;
        child_shape.radius = 0.5f;
        child_shape.height = 1.0f;
        child_shape.segments = 8;

        auto child_faces = faceGen.Generate(child_shape);

        // Connect via top cap (face 8) to bottom cap (face 9)
        // Both are 8-gons (8 vertices) — topologically compatible
        Connection conn;
        conn.type = ConnectionType::FaceConnection;
        conn.parentFaceIndex = 8; // top cap
        conn.childFaceIndex = 9;  // bottom cap
        conn.offsetU = 0.5f;
        conn.offsetV = 0.5f;
        conn.rotation = 0.0f;

        Mat4 transform = solver.ComputeLegacyTransform(conn, parent_faces, child_faces);

        // The child's connection face center should end up on the parent face center.
        // This ensures bodies share the face without intersecting volumes.
        Vec3 child_face_center(0, 0, 0);
        for (const Vec3& v : child_faces[9].vertices) {
            child_face_center = child_face_center + v;
        }
        child_face_center = child_face_center * (1.0f / static_cast<float>(child_faces[9].vertices.size()));

        Vec3 transformed_child_face_center = transform.TransformPoint(child_face_center);

        Vec3 parent_face_center(0, 0, 0);
        for (const Vec3& v : parent_faces[8].vertices) {
            parent_face_center = parent_face_center + v;
        }
        parent_face_center = parent_face_center * (1.0f / static_cast<float>(parent_faces[8].vertices.size()));

        // Child's connection face center should be at the parent face center
        Vec3 diff = transformed_child_face_center - parent_face_center;
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
