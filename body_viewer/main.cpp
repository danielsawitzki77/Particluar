#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#endif
#include <GL/gl.h>
#include <GL/glu.h>

#include "BodyLoader.h"
#include "BodyRenderer.h"
#include "BodyGenerator.h"
#include "ConnectionSolver.h"
#include "ConnectionValidator.h"
#include "ConnectionFaceMatcher.h"
#include "FaceGenerator.h"
#include "ParametricResolver.h"
#include "SubdivisionSolver.h"
#include "JointAnimator.h"
#include "ShapeScaleAnimator.h"
#include "ModelSwitcher.h"
#include "JointFaceAnalyzer.h"
#include "CollisionPrimitive.h"
#include "picojson.h"

#include <string>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <ctime>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// Viewer state
// ============================================================================

struct ViewerState {
    float yaw = 30.0f;
    float pitch = 20.0f;
    float distance = 5.0f;
    BodyRenderer::Body current_body;
    bool has_model = false;

    // Subdivision resolution multiplier (applied to all shapes)
    int subdivision_level = 1; // 1 = default, 2 = doubled, etc.

    // Joint animation
    BodyRenderer::JointAnimator animator;

    // Shape scale animation
    BodyRenderer::ShapeScaleAnimator scale_animator;

    // Random generation state
    unsigned int next_gen_seed = 1000;

    // Collision visualization toggle
    bool show_colliders = false;
};

// ============================================================================
// Multi-light setup (key, fill, back, overhead)
// ============================================================================

static void BuildLightSetup(std::vector<BodyRenderer::PointLight>& lights)
{
    lights.clear();

    // Key light — main illumination (warm white, upper right)
    BodyRenderer::PointLight key;
    key.position = BodyRenderer::Vec3(4.0f, 5.0f, 4.0f);
    key.diffuse = BodyRenderer::Vec3(1.0f, 0.95f, 0.9f);
    key.specular = BodyRenderer::Vec3(1.0f, 1.0f, 1.0f);
    key.constant_atten = 1.0f;
    key.linear_atten = 0.02f;
    key.quadratic_atten = 0.005f;
    lights.push_back(key);

    // Fill light — softer, opposite side (cool blue)
    BodyRenderer::PointLight fill;
    fill.position = BodyRenderer::Vec3(-3.0f, 2.0f, 3.0f);
    fill.diffuse = BodyRenderer::Vec3(0.4f, 0.5f, 0.7f);
    fill.specular = BodyRenderer::Vec3(0.3f, 0.3f, 0.5f);
    fill.constant_atten = 1.0f;
    fill.linear_atten = 0.03f;
    fill.quadratic_atten = 0.01f;
    lights.push_back(fill);

    // Back/rim light — behind and above (highlights silhouette)
    BodyRenderer::PointLight rim;
    rim.position = BodyRenderer::Vec3(-1.0f, 4.0f, -5.0f);
    rim.diffuse = BodyRenderer::Vec3(0.6f, 0.6f, 0.8f);
    rim.specular = BodyRenderer::Vec3(0.8f, 0.8f, 1.0f);
    rim.constant_atten = 1.0f;
    rim.linear_atten = 0.02f;
    rim.quadratic_atten = 0.008f;
    lights.push_back(rim);

    // Overhead fill — top-down ambient boost
    BodyRenderer::PointLight overhead;
    overhead.position = BodyRenderer::Vec3(0.0f, 7.0f, 0.0f);
    overhead.diffuse = BodyRenderer::Vec3(0.3f, 0.3f, 0.3f);
    overhead.specular = BodyRenderer::Vec3(0.2f, 0.2f, 0.2f);
    overhead.constant_atten = 1.0f;
    overhead.linear_atten = 0.05f;
    overhead.quadratic_atten = 0.01f;
    lights.push_back(overhead);
}

// ============================================================================
// Projection
// ============================================================================

static void SetupProjection(int width, int height)
{
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float aspect = static_cast<float>(width) / static_cast<float>(height);
    gluPerspective(45.0, aspect, 0.1, 100.0);
    glMatrixMode(GL_MODELVIEW);
}

// ============================================================================
// Render
// ============================================================================

static void RenderFrame(const ViewerState& state, const BodyRenderer::BodyRendererGL& renderer)
{
    glClearColor(0.08f, 0.08f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (!state.has_model) return;

    glLoadIdentity();

    // Camera: orbit around origin
    float cam_x = state.distance * std::sin(state.yaw * static_cast<float>(M_PI) / 180.0f) * std::cos(state.pitch * static_cast<float>(M_PI) / 180.0f);
    float cam_y = state.distance * std::sin(state.pitch * static_cast<float>(M_PI) / 180.0f);
    float cam_z = state.distance * std::cos(state.yaw * static_cast<float>(M_PI) / 180.0f) * std::cos(state.pitch * static_cast<float>(M_PI) / 180.0f);

    gluLookAt(cam_x, cam_y, cam_z,
              0.0, 0.0, 0.0,
              0.0, 1.0, 0.0);

    // Build render params with multi-light setup
    BodyRenderer::RenderParams params;
    params.ambient = state.current_body.material.ambient;
    params.shininess = state.current_body.material.shininess;
    params.model_color = state.current_body.root.color;
    BuildLightSetup(params.lights);

    renderer.Render(state.current_body, params);
}

// ============================================================================
// Collision wireframe rendering
// ============================================================================

static void DrawWireSphere(float radius, int segments)
{
    // Draw three circles (XY, XZ, YZ planes)
    float step = 2.0f * static_cast<float>(M_PI) / segments;

    // XY circle
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < segments; ++i) {
        float angle = i * step;
        glVertex3f(radius * std::cos(angle), radius * std::sin(angle), 0.0f);
    }
    glEnd();

    // XZ circle
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < segments; ++i) {
        float angle = i * step;
        glVertex3f(radius * std::cos(angle), 0.0f, radius * std::sin(angle));
    }
    glEnd();

    // YZ circle
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < segments; ++i) {
        float angle = i * step;
        glVertex3f(0.0f, radius * std::cos(angle), radius * std::sin(angle));
    }
    glEnd();

    // Additional latitude rings
    for (int lat = 1; lat < segments / 2; ++lat) {
        float phi = static_cast<float>(M_PI) * lat / (segments / 2);
        float ring_r = radius * std::sin(phi);
        float ring_y = radius * std::cos(phi);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < segments; ++i) {
            float angle = i * step;
            glVertex3f(ring_r * std::cos(angle), ring_y, ring_r * std::sin(angle));
        }
        glEnd();
    }
}

static void DrawWireCapsule(float radius, float halfHeight, int segments)
{
    float step = 2.0f * static_cast<float>(M_PI) / segments;

    // Top hemisphere arcs
    for (int arc = 0; arc < 4; ++arc) {
        float base_angle = arc * static_cast<float>(M_PI) / 2.0f;
        glBegin(GL_LINE_STRIP);
        for (int i = 0; i <= segments / 4; ++i) {
            float phi = static_cast<float>(M_PI) / 2.0f * i / (segments / 4);
            float y = halfHeight + radius * std::sin(phi);
            float r = radius * std::cos(phi);
            glVertex3f(r * std::cos(base_angle), y, r * std::sin(base_angle));
        }
        glEnd();
    }

    // Bottom hemisphere arcs
    for (int arc = 0; arc < 4; ++arc) {
        float base_angle = arc * static_cast<float>(M_PI) / 2.0f;
        glBegin(GL_LINE_STRIP);
        for (int i = 0; i <= segments / 4; ++i) {
            float phi = static_cast<float>(M_PI) / 2.0f * i / (segments / 4);
            float y = -halfHeight - radius * std::sin(phi);
            float r = radius * std::cos(phi);
            glVertex3f(r * std::cos(base_angle), y, r * std::sin(base_angle));
        }
        glEnd();
    }

    // Top ring
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < segments; ++i) {
        float angle = i * step;
        glVertex3f(radius * std::cos(angle), halfHeight, radius * std::sin(angle));
    }
    glEnd();

    // Bottom ring
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < segments; ++i) {
        float angle = i * step;
        glVertex3f(radius * std::cos(angle), -halfHeight, radius * std::sin(angle));
    }
    glEnd();

    // Middle ring
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < segments; ++i) {
        float angle = i * step;
        glVertex3f(radius * std::cos(angle), 0.0f, radius * std::sin(angle));
    }
    glEnd();

    // Vertical lines connecting top and bottom rings
    for (int i = 0; i < 4; ++i) {
        float angle = i * static_cast<float>(M_PI) / 2.0f;
        float x = radius * std::cos(angle);
        float z = radius * std::sin(angle);
        glBegin(GL_LINES);
        glVertex3f(x, halfHeight, z);
        glVertex3f(x, -halfHeight, z);
        glEnd();
    }
}

static void DrawWireCylinder(float radius, float halfHeight, int segments)
{
    float step = 2.0f * static_cast<float>(M_PI) / segments;

    // Top cap ring
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < segments; ++i) {
        float angle = i * step;
        glVertex3f(radius * std::cos(angle), halfHeight, radius * std::sin(angle));
    }
    glEnd();

    // Bottom cap ring
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < segments; ++i) {
        float angle = i * step;
        glVertex3f(radius * std::cos(angle), -halfHeight, radius * std::sin(angle));
    }
    glEnd();

    // Middle ring
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < segments; ++i) {
        float angle = i * step;
        glVertex3f(radius * std::cos(angle), 0.0f, radius * std::sin(angle));
    }
    glEnd();

    // Vertical lines
    for (int i = 0; i < segments; i += segments / 4) {
        float angle = i * step;
        float x = radius * std::cos(angle);
        float z = radius * std::sin(angle);
        glBegin(GL_LINES);
        glVertex3f(x, halfHeight, z);
        glVertex3f(x, -halfHeight, z);
        glEnd();
    }

    // Top cap cross
    glBegin(GL_LINES);
    glVertex3f(-radius, halfHeight, 0.0f);
    glVertex3f(radius, halfHeight, 0.0f);
    glVertex3f(0.0f, halfHeight, -radius);
    glVertex3f(0.0f, halfHeight, radius);
    glEnd();

    // Bottom cap cross
    glBegin(GL_LINES);
    glVertex3f(-radius, -halfHeight, 0.0f);
    glVertex3f(radius, -halfHeight, 0.0f);
    glVertex3f(0.0f, -halfHeight, -radius);
    glVertex3f(0.0f, -halfHeight, radius);
    glEnd();
}

static void RenderColliderNode(const BodyRenderer::BodyNode* node,
                                const BodyRenderer::Mat4& parentWorld,
                                int subdivisionLevel)
{
    if (!node) return;

    BodyRenderer::Mat4 world = parentWorld * node->localTransform;

    // Create collision primitive for this node
    BodyRenderer::CollisionPrimitive* prim = BodyRenderer::CreateCollisionPrimitive(node->shape);

    // Apply the world transform via OpenGL
    glPushMatrix();
    float glMat[16];
    for (int i = 0; i < 16; ++i) glMat[i] = world.m[i];
    glMultMatrixf(glMat);

    int segs = subdivisionLevel * 8; // same subdivision level as the body mesh

    // Draw the appropriate wireframe shape
    switch (prim->GetType()) {
    case BodyRenderer::CollisionPrimitive::PRIM_SPHERE: {
        const BodyRenderer::CollisionSphere* sphere =
            static_cast<const BodyRenderer::CollisionSphere*>(prim);
        DrawWireSphere(sphere->radius, segs);
        break;
    }
    case BodyRenderer::CollisionPrimitive::PRIM_CAPSULE: {
        const BodyRenderer::CollisionCapsule* capsule =
            static_cast<const BodyRenderer::CollisionCapsule*>(prim);
        DrawWireCapsule(capsule->radius, capsule->halfHeight, segs);
        break;
    }
    case BodyRenderer::CollisionPrimitive::PRIM_CYLINDER: {
        const BodyRenderer::CollisionCylinder* cyl =
            static_cast<const BodyRenderer::CollisionCylinder*>(prim);
        DrawWireCylinder(cyl->radius, cyl->halfHeight, segs);
        break;
    }
    }

    glPopMatrix();
    delete prim;

    // Recurse into children
    for (size_t i = 0; i < node->children.size(); ++i) {
        RenderColliderNode(&node->children[i], world, subdivisionLevel);
    }
}

static void RenderColliders(const ViewerState& state)
{
    if (!state.has_model || !state.show_colliders) return;

    // Disable lighting, enable wireframe overlay
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST); // Always render on top to avoid z-fighting
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glLineWidth(1.5f);

    // Green wireframe for collision shapes
    glColor3f(0.0f, 1.0f, 0.3f);

    BodyRenderer::Mat4 identity;
    identity.Identity();
    RenderColliderNode(&state.current_body.root, identity, state.subdivision_level);

    // Restore state
    glEnable(GL_DEPTH_TEST);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glEnable(GL_LIGHTING);
    glLineWidth(1.0f);
}

// ============================================================================
// Model loading and preparation
// ============================================================================

static bool LoadModel(const std::string& path, ViewerState& state)
{
    BodyRenderer::BodyLoader loader;
    BodyRenderer::LoadResult result = loader.LoadFromFile(path);
    if (!result.success) {
        SDL_Log("[Body_Viewer] Failed to load '%s': %s", path.c_str(), result.error.c_str());
        return false;
    }

    // Validate connections before accepting the model
    BodyRenderer::ConnectionValidator validator;
    auto vr = validator.ValidateBody(result.body);
    if (!vr.valid) {
        SDL_Log("[Body_Viewer] Validation failed for '%s': %s", path.c_str(), vr.error.c_str());
        return false;
    }

    state.current_body = result.body;
    state.has_model = true;

    // Prepare body: derive subdivision + resolve connections
    BodyRenderer::SubdivisionSolver subdivSolver;
    subdivSolver.PrepareBody(state.current_body, state.subdivision_level * 8);

    // Setup animators for this body
    state.animator.SetBody(state.current_body);
    state.scale_animator.SetBody(state.current_body);

    return true;
}

static void LoadGeneratedBody(ViewerState& state)
{
    BodyRenderer::BodyGenerator generator;
    BodyRenderer::Body body = generator.Generate(state.next_gen_seed, 4);
    state.next_gen_seed++;

    state.current_body = body;
    state.has_model = true;

    // Prepare body: derive subdivision + resolve connections
    BodyRenderer::SubdivisionSolver subdivSolver;
    subdivSolver.PrepareBody(state.current_body, state.subdivision_level * 8);

    // Setup animators
    state.animator.SetBody(state.current_body);
    state.scale_animator.SetBody(state.current_body);
}

static void ReloadCurrentBody(ViewerState& state, BodyRenderer::ModelSwitcher& switcher,
                               bool is_generated)
{
    if (is_generated) {
        // Re-generate with same seed (next_gen_seed - 1 was last used)
        BodyRenderer::BodyGenerator generator;
        BodyRenderer::Body body = generator.Generate(state.next_gen_seed - 1, 4);
        state.current_body = body;
        state.has_model = true;
    } else {
        // Reload from file
        BodyRenderer::BodyLoader loader;
        BodyRenderer::LoadResult result = loader.LoadFromFile(switcher.GetCurrentPath());
        if (!result.success) return;
        state.current_body = result.body;
        state.has_model = true;
    }

    BodyRenderer::SubdivisionSolver subdivSolver;
    subdivSolver.PrepareBody(state.current_body, state.subdivision_level * 8);

    state.animator.SetBody(state.current_body);
    state.scale_animator.SetBody(state.current_body);
}

// ============================================================================
// HUD text (simple OpenGL bitmap — use window title as fallback)
// ============================================================================

static void UpdateWindowTitle(SDL_Window* window, const ViewerState& state,
                               const std::string& model_name, int model_index, int model_count)
{
    std::string title = "Body Viewer - " + model_name;
    title += " [" + std::to_string(model_index + 1) + "/" + std::to_string(model_count) + "]";
    title += " | Subdiv: " + std::to_string(state.subdivision_level);
    title += " | Joints: " + std::to_string(state.animator.GetJointCount());
    if (state.animator.IsEnabled()) {
        title += " | JointAnim: " + state.animator.GetCurrentJointName();
    } else {
        title += " | JointAnim: OFF (Space)";
    }
    if (state.scale_animator.IsEnabled()) {
        title += " | Scale: " + state.scale_animator.GetCurrentShapeName() + "." + state.scale_animator.GetCurrentDimensionName();
    } else {
        title += " | Scale: OFF (T)";
    }
    title += " | Colliders: ";
    title += state.show_colliders ? "ON (C)" : "OFF (C)";
    SDL_SetWindowTitle(window, title.c_str());
}

// ============================================================================
// Geometry Dump Mode (--dump <model_path>)
// ============================================================================

static const char* ShapeTypeName(BodyRenderer::ShapeType t)
{
    switch (t) {
    case BodyRenderer::ShapeType::Cone: return "cone";
    case BodyRenderer::ShapeType::Cylinder: return "cylinder";
    case BodyRenderer::ShapeType::Sphere: return "sphere";
    case BodyRenderer::ShapeType::Torus: return "torus";
    case BodyRenderer::ShapeType::Capsule: return "capsule";
    }
    return "unknown";
}

static BodyRenderer::Vec3 ComputeFaceCenterUtil(const BodyRenderer::Face& face)
{
    BodyRenderer::Vec3 center(0, 0, 0);
    if (face.vertices.empty()) return center;
    for (const auto& v : face.vertices) {
        center = center + v;
    }
    return center * (1.0f / static_cast<float>(face.vertices.size()));
}

static void DumpNodeRecursive(
    const BodyRenderer::BodyNode* node,
    const BodyRenderer::BodyNode* parent,
    const BodyRenderer::Mat4& parent_world,
    const BodyRenderer::ConnectionFaceMatcher& faceMatcher,
    int depth)
{
    if (!node) return;

    BodyRenderer::Mat4 world = parent_world * node->localTransform;

    // Extract translation from localTransform
    float tx = node->localTransform.m[12];
    float ty = node->localTransform.m[13];
    float tz = node->localTransform.m[14];

    // Print header
    std::string indent(depth * 2, ' ');
    if (!parent) {
        printf("\n%s=== Node: %s (root) ===\n", indent.c_str(), node->name.c_str());
    } else {
        printf("\n%s=== Node: %s (child of %s) ===\n", indent.c_str(), node->name.c_str(), parent->name.c_str());
    }
    printf("%s  Shape: %s\n", indent.c_str(), ShapeTypeName(node->shape.type));

    if (!parent) {
        printf("%s  Local transform: identity\n", indent.c_str());
    } else {
        printf("%s  Local transform: T=(%.4f, %.4f, %.4f)\n", indent.c_str(), tx, ty, tz);
    }

    // Build rings for this node (as parent of its children)
    BodyRenderer::ParametricResolver resolver;
    std::vector<BodyRenderer::ConnectionRing> rings;

    // If this node IS a child, include its own child-side ring first
    if (parent && !node->connection.isLegacy) {
        float matched_radius = faceMatcher.ComputeMatchedRadius(
            parent->shape, node->connection.parentAttach,
            node->shape, node->connection.childAttach
        );
        int matched_segments = faceMatcher.ComputeMatchedSegments(
            parent->shape, node->connection.parentAttach,
            node->shape, node->connection.childAttach
        );
        BodyRenderer::SurfacePoint child_pt = resolver.Resolve(node->shape, node->connection.childAttach);
        BodyRenderer::ConnectionRing child_ring;
        child_ring.center = child_pt.position;
        child_ring.normal = child_pt.normal;
        child_ring.radius = matched_radius;
        child_ring.segments = matched_segments;
        child_ring.child_index = -1;
        child_ring.attach = node->connection.childAttach;
        rings.push_back(child_ring);
    }

    // Add rings for each child (parent-side attachment on this node)
    for (int i = 0; i < static_cast<int>(node->children.size()); ++i) {
        const BodyRenderer::BodyNode& child = node->children[i];
        if (child.connection.isLegacy) continue;

        float matched_radius = faceMatcher.ComputeMatchedRadius(
            node->shape, child.connection.parentAttach,
            child.shape, child.connection.childAttach
        );
        int matched_segments = faceMatcher.ComputeMatchedSegments(
            node->shape, child.connection.parentAttach,
            child.shape, child.connection.childAttach
        );

        BodyRenderer::SurfacePoint pt = resolver.Resolve(node->shape, child.connection.parentAttach);
        BodyRenderer::ConnectionRing ring;
        ring.center = pt.position;
        ring.normal = pt.normal;
        ring.radius = matched_radius;
        ring.segments = matched_segments;
        ring.child_index = i;
        ring.attach = child.connection.parentAttach;
        rings.push_back(ring);
    }

    // Generate faces with connection matching
    BodyRenderer::MatchedFaces matched = faceMatcher.GenerateWithConnections(*node, rings);
    int num_faces = static_cast<int>(matched.faces.size());
    printf("%s  Total faces: %d\n", indent.c_str(), num_faces);

    // Print connection face indices
    if (!matched.connection_face_indices.empty()) {
        printf("%s  Connection face indices: [", indent.c_str());
        for (size_t i = 0; i < matched.connection_face_indices.size(); ++i) {
            if (i > 0) printf(", ");
            printf("%d", matched.connection_face_indices[i]);
        }
        printf("]\n");

        // Print details for each connection face
        for (size_t i = 0; i < matched.connection_face_indices.size(); ++i) {
            int fi = matched.connection_face_indices[i];
            if (fi < 0 || fi >= num_faces) continue;

            BodyRenderer::Vec3 local_center = ComputeFaceCenterUtil(matched.faces[fi]);
            BodyRenderer::Vec3 world_center = world.TransformPoint(local_center);
            BodyRenderer::Vec3 local_normal = matched.faces[fi].normal;
            BodyRenderer::Vec3 world_normal = world.TransformDirection(local_normal).Normalized();

            const char* role = (rings[i].child_index == -1) ? "own/child-attach" : "parent-attach";
            printf("%s  Face %d (%s):\n", indent.c_str(), fi, role);
            printf("%s    center (local): (%.4f, %.4f, %.4f)\n", indent.c_str(), local_center.x, local_center.y, local_center.z);
            printf("%s    center (world): (%.4f, %.4f, %.4f)\n", indent.c_str(), world_center.x, world_center.y, world_center.z);
            printf("%s    normal (local): (%.4f, %.4f, %.4f)\n", indent.c_str(), local_normal.x, local_normal.y, local_normal.z);
            printf("%s    normal (world): (%.4f, %.4f, %.4f)\n", indent.c_str(), world_normal.x, world_normal.y, world_normal.z);
            printf("%s    vertices: %d\n", indent.c_str(), static_cast<int>(matched.faces[fi].vertices.size()));
        }
    }

    // Connection analysis: if this is a child node, compare parent/child face contact
    if (parent && !node->connection.isLegacy) {
        printf("\n%s  --- Connection Analysis ---\n", indent.c_str());

        // Compute parent's connection face for this child
        // Re-generate parent's faces to find its connection face
        std::vector<BodyRenderer::ConnectionRing> parent_rings;
        for (int i = 0; i < static_cast<int>(parent->children.size()); ++i) {
            const BodyRenderer::BodyNode& sibling = parent->children[i];
            if (sibling.connection.isLegacy) continue;

            float mr = faceMatcher.ComputeMatchedRadius(
                parent->shape, sibling.connection.parentAttach,
                sibling.shape, sibling.connection.childAttach
            );
            int ms = faceMatcher.ComputeMatchedSegments(
                parent->shape, sibling.connection.parentAttach,
                sibling.shape, sibling.connection.childAttach
            );

            BodyRenderer::SurfacePoint pt = resolver.Resolve(parent->shape, sibling.connection.parentAttach);
            BodyRenderer::ConnectionRing ring;
            ring.center = pt.position;
            ring.normal = pt.normal;
            ring.radius = mr;
            ring.segments = ms;
            ring.child_index = i;
            ring.attach = sibling.connection.parentAttach;
            parent_rings.push_back(ring);
        }

        BodyRenderer::MatchedFaces parent_matched = faceMatcher.GenerateWithConnections(*parent, parent_rings);

        // Find which parent ring corresponds to this node
        int my_index = -1;
        for (int i = 0; i < static_cast<int>(parent->children.size()); ++i) {
            if (&parent->children[i] == node) { my_index = i; break; }
        }

        int parent_ring_idx = -1;
        for (int i = 0; i < static_cast<int>(parent_rings.size()); ++i) {
            if (parent_rings[i].child_index == my_index) {
                parent_ring_idx = i;
                break;
            }
        }

        if (parent_ring_idx >= 0 && parent_ring_idx < static_cast<int>(parent_matched.connection_face_indices.size())) {
            int parent_fi = parent_matched.connection_face_indices[parent_ring_idx];
            if (parent_fi >= 0 && parent_fi < static_cast<int>(parent_matched.faces.size())) {
                BodyRenderer::Vec3 parent_face_center_local = ComputeFaceCenterUtil(parent_matched.faces[parent_fi]);
                BodyRenderer::Vec3 parent_face_center_world = parent_world.TransformPoint(parent_face_center_local);
                BodyRenderer::Vec3 parent_face_normal_local = parent_matched.faces[parent_fi].normal;
                BodyRenderer::Vec3 parent_face_normal_world = parent_world.TransformDirection(parent_face_normal_local).Normalized();

                printf("%s  Parent face %d center (world): (%.4f, %.4f, %.4f)\n", indent.c_str(),
                       parent_fi, parent_face_center_world.x, parent_face_center_world.y, parent_face_center_world.z);
                printf("%s  Parent face normal (world):     (%.4f, %.4f, %.4f)\n", indent.c_str(),
                       parent_face_normal_world.x, parent_face_normal_world.y, parent_face_normal_world.z);

                // Child's own connection face (first ring, child_index == -1)
                if (!matched.connection_face_indices.empty()) {
                    int child_fi = matched.connection_face_indices[0]; // first ring is own attach
                    if (child_fi >= 0 && child_fi < num_faces) {
                        BodyRenderer::Vec3 child_face_center_local = ComputeFaceCenterUtil(matched.faces[child_fi]);
                        BodyRenderer::Vec3 child_face_center_world = world.TransformPoint(child_face_center_local);
                        BodyRenderer::Vec3 child_face_normal_local = matched.faces[child_fi].normal;
                        BodyRenderer::Vec3 child_face_normal_world = world.TransformDirection(child_face_normal_local).Normalized();

                        printf("%s  Child face %d center (world):  (%.4f, %.4f, %.4f)\n", indent.c_str(),
                               child_fi, child_face_center_world.x, child_face_center_world.y, child_face_center_world.z);
                        printf("%s  Child face normal (world):      (%.4f, %.4f, %.4f)\n", indent.c_str(),
                               child_face_normal_world.x, child_face_normal_world.y, child_face_normal_world.z);

                        float dx = parent_face_center_world.x - child_face_center_world.x;
                        float dy = parent_face_center_world.y - child_face_center_world.y;
                        float dz = parent_face_center_world.z - child_face_center_world.z;
                        float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
                        printf("%s  Distance: %.6f (should be ~0 if touching)\n", indent.c_str(), distance);

                        float dot = parent_face_normal_world.Dot(child_face_normal_world);
                        printf("%s  Dot product of normals: %.6f (should be -1.0 if flush)\n", indent.c_str(), dot);
                    }
                }
            }
        } else {
            printf("%s  (Could not find parent connection face for analysis)\n", indent.c_str());
        }
    }

    // Recurse into children
    for (int i = 0; i < static_cast<int>(node->children.size()); ++i) {
        DumpNodeRecursive(&node->children[i], node, world, faceMatcher, depth + 1);
    }
}

static int RunDumpMode(const std::string& model_path)
{
    printf("=== Geometry Dump: %s ===\n", model_path.c_str());

    // Load the body
    BodyRenderer::BodyLoader loader;
    BodyRenderer::LoadResult result = loader.LoadFromFile(model_path);
    if (!result.success) {
        fprintf(stderr, "ERROR: Failed to load '%s': %s\n", model_path.c_str(), result.error.c_str());
        return 1;
    }

    printf("Body: %s (format v%d)\n", result.body.name.c_str(), result.body.formatVersion);

    // Prepare body: derive subdivision + resolve connections
    BodyRenderer::SubdivisionSolver subdivSolver;
    subdivSolver.PrepareBody(result.body, 1 * 8);

    // Setup face matcher (same as renderer uses)
    BodyRenderer::ConnectionFaceMatcher faceMatcher;

    // Start recursive dump from root
    BodyRenderer::Mat4 identity;
    identity.Identity();
    DumpNodeRecursive(&result.body.root, nullptr, identity, faceMatcher, 0);

    printf("\n=== End Dump ===\n");
    return 0;
}

// ============================================================================
// Analyzer Pair Testing Config
// ============================================================================

struct AnalyzerConfig {
    int num_random_models = 64;
    int bodies_per_model = 2;
    std::vector<int> subdivision_range = {1, 2, 3, 4, 5, 6, 7, 8};
    int random_seed = 0;
};

static AnalyzerConfig LoadAnalyzerConfig()
{
    AnalyzerConfig cfg;
    std::ifstream f("body_viewer_config.json");
    if (!f.is_open()) {
        printf("[Config] body_viewer_config.json not found, using defaults.\n");
        return cfg;
    }

    picojson::value root;
    std::string err = picojson::parse(root, f);
    if (!err.empty()) {
        printf("[Config] Parse error: %s\n", err.c_str());
        return cfg;
    }

    if (!root.is<picojson::object>()) return cfg;
    const picojson::object& obj = root.get<picojson::object>();

    if (obj.count("analyze") && obj.at("analyze").is<picojson::object>()) {
        const picojson::object& analyze = obj.at("analyze").get<picojson::object>();

        if (analyze.count("num_random_models") && analyze.at("num_random_models").is<double>()) {
            cfg.num_random_models = static_cast<int>(analyze.at("num_random_models").get<double>());
        }
        if (analyze.count("bodies_per_model") && analyze.at("bodies_per_model").is<double>()) {
            cfg.bodies_per_model = static_cast<int>(analyze.at("bodies_per_model").get<double>());
        }
        if (analyze.count("random_seed") && analyze.at("random_seed").is<double>()) {
            cfg.random_seed = static_cast<int>(analyze.at("random_seed").get<double>());
        }
        if (analyze.count("subdivision_range") && analyze.at("subdivision_range").is<picojson::array>()) {
            const picojson::array& arr = analyze.at("subdivision_range").get<picojson::array>();
            cfg.subdivision_range.clear();
            for (const auto& item : arr) {
                if (item.is<double>()) {
                    cfg.subdivision_range.push_back(static_cast<int>(item.get<double>()));
                }
            }
        }
    }

    printf("[Config] Loaded: models=%d, bodies_per_model=%d, seed=%d, subdivisions=%d levels\n",
           cfg.num_random_models, cfg.bodies_per_model, cfg.random_seed,
           static_cast<int>(cfg.subdivision_range.size()));
    return cfg;
}

// ============================================================================
// BMP Screenshot Writer
// ============================================================================

static bool WriteBMP(const std::string& filepath, const std::vector<unsigned char>& pixels, int width, int height)
{
    // BMP file: 14-byte file header + 40-byte DIB header + pixel data (BGR, bottom-up)
    int row_stride = width * 3;
    int padding = (4 - (row_stride % 4)) % 4;
    int padded_row = row_stride + padding;
    int pixel_data_size = padded_row * height;
    int file_size = 14 + 40 + pixel_data_size;

    std::ofstream out(filepath, std::ios::binary);
    if (!out.is_open()) return false;

    // File header (14 bytes)
    unsigned char file_header[14] = {};
    file_header[0] = 'B'; file_header[1] = 'M';
    file_header[2] = (file_size) & 0xFF;
    file_header[3] = (file_size >> 8) & 0xFF;
    file_header[4] = (file_size >> 16) & 0xFF;
    file_header[5] = (file_size >> 24) & 0xFF;
    file_header[10] = 54; // offset to pixel data
    out.write(reinterpret_cast<char*>(file_header), 14);

    // DIB header (40 bytes)
    unsigned char dib_header[40] = {};
    dib_header[0] = 40; // header size
    dib_header[4] = (width) & 0xFF;
    dib_header[5] = (width >> 8) & 0xFF;
    dib_header[6] = (width >> 16) & 0xFF;
    dib_header[7] = (width >> 24) & 0xFF;
    dib_header[8] = (height) & 0xFF;
    dib_header[9] = (height >> 8) & 0xFF;
    dib_header[10] = (height >> 16) & 0xFF;
    dib_header[11] = (height >> 24) & 0xFF;
    dib_header[12] = 1; // planes
    dib_header[14] = 24; // bits per pixel
    dib_header[20] = (pixel_data_size) & 0xFF;
    dib_header[21] = (pixel_data_size >> 8) & 0xFF;
    dib_header[22] = (pixel_data_size >> 16) & 0xFF;
    dib_header[23] = (pixel_data_size >> 24) & 0xFF;
    out.write(reinterpret_cast<char*>(dib_header), 40);

    // Pixel data: glReadPixels gives RGB bottom-up, BMP expects BGR bottom-up
    unsigned char pad_bytes[3] = {0, 0, 0};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int src_idx = (y * width + x) * 3;
            unsigned char bgr[3] = { pixels[src_idx + 2], pixels[src_idx + 1], pixels[src_idx] };
            out.write(reinterpret_cast<char*>(bgr), 3);
        }
        if (padding > 0) out.write(reinterpret_cast<char*>(pad_bytes), padding);
    }

    out.close();
    return true;
}

static bool CaptureScreenshot(const std::string& filepath, int width, int height)
{
    std::vector<unsigned char> pixels(width * height * 3);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    return WriteBMP(filepath, pixels, width, height);
}

// ============================================================================
// Directory creation utility
// ============================================================================

static bool CreateDirectoryRecursive(const std::string& path)
{
#ifdef _WIN32
    // Try to create each component
    std::string current;
    for (size_t i = 0; i < path.size(); ++i) {
        char c = path[i];
        if (c == '/' || c == '\\') {
            if (!current.empty()) {
                _mkdir(current.c_str());
            }
            current += '\\';
        } else {
            current += c;
        }
    }
    if (!current.empty()) {
        _mkdir(current.c_str());
    }
    // Check if final directory exists
    DWORD attr = GetFileAttributesA(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY));
#else
    // POSIX fallback
    std::string cmd = "mkdir -p \"" + path + "\"";
    return system(cmd.c_str()) == 0;
#endif
}

// ============================================================================
// Analyze Pairs Mode (--analyze-pairs [output_path])
// ============================================================================

static const char* PairGradeString(BodyRenderer::JointFaceReport::Grade g)
{
    switch (g) {
    case BodyRenderer::JointFaceReport::PERFECT:    return "PERFECT";
    case BodyRenderer::JointFaceReport::GOOD:       return "GOOD";
    case BodyRenderer::JointFaceReport::ACCEPTABLE: return "ACCEPTABLE";
    case BodyRenderer::JointFaceReport::POOR:       return "POOR";
    case BodyRenderer::JointFaceReport::FAILING:    return "FAILING";
    }
    return "UNKNOWN";
}

static int RunAnalyzePairsMode(const std::string& base_output_path)
{
    printf("=== Analyzer Pair Testing ===\n");

    // Task 2: Load config
    AnalyzerConfig cfg = LoadAnalyzerConfig();

    // Task 3: Determine seed
    unsigned int seed = static_cast<unsigned int>(cfg.random_seed);
    if (seed == 0) {
        seed = static_cast<unsigned int>(time(nullptr));
    }
    printf("Using seed: %u\n", seed);
    printf("Generating %d random 2-body models...\n", cfg.num_random_models);
    printf("Subdivision levels: ");
    for (size_t i = 0; i < cfg.subdivision_range.size(); ++i) {
        if (i > 0) printf(", ");
        printf("%d", cfg.subdivision_range[i]);
    }
    printf("\n\n");

    // Task 6: Create timestamped output folder
    std::string output_dir;
    if (!base_output_path.empty()) {
        output_dir = base_output_path;
    } else {
        time_t now = time(nullptr);
        struct tm t_buf;
#ifdef _WIN32
        localtime_s(&t_buf, &now);
#else
        t_buf = *localtime(&now);
#endif
        char dir_name[128];
        snprintf(dir_name, sizeof(dir_name), "test_output/run_%04d%02d%02d_%02d%02d%02d",
                 t_buf.tm_year + 1900, t_buf.tm_mon + 1, t_buf.tm_mday,
                 t_buf.tm_hour, t_buf.tm_min, t_buf.tm_sec);
        output_dir = dir_name;
    }

    std::string screenshots_dir = output_dir + "/screenshots";
    if (!CreateDirectoryRecursive(screenshots_dir)) {
        fprintf(stderr, "ERROR: Failed to create output directory: %s\n", screenshots_dir.c_str());
        return 1;
    }
    printf("Output directory: %s\n", output_dir.c_str());

    // Task 5: Initialize SDL for offscreen rendering
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "ERROR: SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    const int CAPTURE_WIDTH = 512;
    const int CAPTURE_HEIGHT = 512;

    SDL_Window* window = SDL_CreateWindow("Analyzer Pairs", CAPTURE_WIDTH, CAPTURE_HEIGHT,
                                          SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL);
    if (!window) {
        fprintf(stderr, "ERROR: Window creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        fprintf(stderr, "ERROR: OpenGL context creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glClearDepth(1.0);
    SetupProjection(CAPTURE_WIDTH, CAPTURE_HEIGHT);

    // Setup renderer and lights
    BodyRenderer::BodyRendererGL renderer;
    std::vector<BodyRenderer::PointLight> lights;
    BuildLightSetup(lights);

    // Task 4: Generate 2-body models and capture screenshots
    BodyRenderer::BodyGenerator generator;
    BodyRenderer::BodyLoader loader;
    BodyRenderer::SubdivisionSolver subdivSolver;

    // Collect generated bodies and their JSON for the report
    struct ModelRecord {
        BodyRenderer::Body body;
        std::string json;
        unsigned int model_seed;
    };
    std::vector<ModelRecord> models;
    models.reserve(cfg.num_random_models);

    for (int i = 0; i < cfg.num_random_models; ++i) {
        unsigned int model_seed = seed + static_cast<unsigned int>(i);
        // Generate with depth_limit=2 (allows root + children)
        BodyRenderer::Body body = generator.Generate(model_seed, 2);
        
        // Force exactly bodies_per_model nodes: trim to root + (bodies_per_model-1) children
        if (cfg.bodies_per_model == 2) {
            // Keep only the first child, remove the rest
            if (body.root.children.empty()) {
                // Root has no children — regenerate with offset seed until we get one
                unsigned int retry_seed = model_seed + 1000;
                for (int attempt = 0; attempt < 100 && body.root.children.empty(); ++attempt) {
                    body = generator.Generate(retry_seed + attempt, 2);
                }
            }
            if (body.root.children.size() > 1) {
                body.root.children.resize(1);
            }
            // Also remove grandchildren to ensure exactly 2 total nodes
            if (!body.root.children.empty()) {
                body.root.children[0].children.clear();
            }
        }

        // Task 7: Serialize the body to JSON
        std::string body_json = loader.Serialize(body);

        models.push_back({body, body_json, model_seed});
    }

    printf("Generated %d models. Capturing screenshots...\n", static_cast<int>(models.size()));

    // Quality analysis results per model/subdiv
    BodyRenderer::JointFaceAnalyzer analyzer;
    struct AnalysisRecord {
        int model_index;
        int subdiv;
        std::vector<BodyRenderer::JointFaceReport> reports;
    };
    std::vector<AnalysisRecord> analysis_results;

    int total_screenshots = 0;
    for (int m = 0; m < static_cast<int>(models.size()); ++m) {
        for (int s = 0; s < static_cast<int>(cfg.subdivision_range.size()); ++s) {
            int subdiv = cfg.subdivision_range[s];

            // Prepare body at this subdivision level
            BodyRenderer::Body body_copy = models[m].body;
            subdivSolver.PrepareBody(body_copy, subdiv * 8);

            // Render
            glClearColor(0.08f, 0.08f, 0.12f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glLoadIdentity();

            // Camera: fixed distance looking at origin
            float cam_distance = 5.0f;
            float cam_yaw = 30.0f;
            float cam_pitch = 20.0f;
            float cam_x = cam_distance * std::sin(cam_yaw * static_cast<float>(M_PI) / 180.0f) * std::cos(cam_pitch * static_cast<float>(M_PI) / 180.0f);
            float cam_y = cam_distance * std::sin(cam_pitch * static_cast<float>(M_PI) / 180.0f);
            float cam_z = cam_distance * std::cos(cam_yaw * static_cast<float>(M_PI) / 180.0f) * std::cos(cam_pitch * static_cast<float>(M_PI) / 180.0f);
            gluLookAt(cam_x, cam_y, cam_z, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0);

            BodyRenderer::RenderParams params;
            params.ambient = body_copy.material.ambient;
            params.shininess = body_copy.material.shininess;
            params.model_color = body_copy.root.color;
            params.lights = lights;

            renderer.Render(body_copy, params);
            renderer.InvalidateCache();
            glFinish();

            // Capture screenshot
            char filename[256];
            snprintf(filename, sizeof(filename), "model_%02d_subdiv_%d.bmp", m, subdiv);
            std::string filepath = screenshots_dir + "/" + filename;
            if (!CaptureScreenshot(filepath, CAPTURE_WIDTH, CAPTURE_HEIGHT)) {
                fprintf(stderr, "WARNING: Failed to capture screenshot: %s\n", filepath.c_str());
            }

            // Run quality analysis on this model at this subdivision level
            auto joint_reports = analyzer.AnalyzeBody(models[m].body, subdiv);
            analysis_results.push_back({m, subdiv, joint_reports});

            total_screenshots++;
        }

        if ((m + 1) % 10 == 0 || m == static_cast<int>(models.size()) - 1) {
            printf("  Progress: %d/%d models captured\n", m + 1, static_cast<int>(models.size()));
        }
    }

    printf("\nCaptured %d screenshots.\n", total_screenshots);

    // Task 7: Write markdown report with embedded JSON
    std::string report_path = output_dir + "/report.md";
    std::ofstream report(report_path);
    if (!report.is_open()) {
        fprintf(stderr, "ERROR: Failed to write report: %s\n", report_path.c_str());
        SDL_GL_DestroyContext(gl_context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Report header
    {
        time_t now = time(nullptr);
        struct tm t_buf;
#ifdef _WIN32
        localtime_s(&t_buf, &now);
#else
        t_buf = *localtime(&now);
#endif
        char time_str[64];
        snprintf(time_str, sizeof(time_str), "%04d-%02d-%02d %02d:%02d:%02d",
                 t_buf.tm_year + 1900, t_buf.tm_mon + 1, t_buf.tm_mday,
                 t_buf.tm_hour, t_buf.tm_min, t_buf.tm_sec);

        report << "# Analyzer Pair Test Report\n\n";
        report << "Generated: " << time_str << "\n";
        report << "Seed: " << seed << "\n";
        report << "Models: " << cfg.num_random_models << "\n";
        report << "Bodies per model: " << cfg.bodies_per_model << "\n";
        report << "Subdivision range: [";
        for (size_t i = 0; i < cfg.subdivision_range.size(); ++i) {
            if (i > 0) report << ", ";
            report << cfg.subdivision_range[i];
        }
        report << "]\n\n";
    }

    for (int m = 0; m < static_cast<int>(models.size()); ++m) {
        char model_header[64];
        snprintf(model_header, sizeof(model_header), "## Model %02d", m);
        report << model_header << "\n\n";
        report << "Seed: " << models[m].model_seed << "\n\n";

        // Embed body JSON
        report << "```json\n";
        report << models[m].json;
        report << "```\n\n";

        // Screenshots and quality analysis for each subdivision level
        for (int s = 0; s < static_cast<int>(cfg.subdivision_range.size()); ++s) {
            int subdiv = cfg.subdivision_range[s];
            char filename[256];
            snprintf(filename, sizeof(filename), "model_%02d_subdiv_%d.bmp", m, subdiv);

            report << "### Subdivision " << subdiv << "\n\n";
            report << "![" << filename << "](screenshots/" << filename << ")\n\n";

            // Find matching analysis for this model/subdiv
            for (const auto& ar : analysis_results) {
                if (ar.model_index == m && ar.subdiv == subdiv) {
                    if (ar.reports.empty()) {
                        report << "- Grade: N/A (no joints)\n";
                    } else {
                        // Use worst grade across all joints at this subdiv
                        BodyRenderer::JointFaceReport::Grade worst = BodyRenderer::JointFaceReport::PERFECT;
                        float worst_area_ratio = 1.0f;
                        float worst_distance = 0.0f;
                        bool all_match = true;
                        int total_parent_verts = 0;
                        int total_child_verts = 0;
                        for (const auto& jr : ar.reports) {
                            if (jr.grade > worst) worst = jr.grade;
                            if (jr.area_ratio < worst_area_ratio) worst_area_ratio = jr.area_ratio;
                            if (jr.face_center_distance > worst_distance) worst_distance = jr.face_center_distance;
                            if (!jr.vertex_count_match) all_match = false;
                            total_parent_verts += jr.parent_face_vertex_count;
                            total_child_verts += jr.child_face_vertex_count;
                        }
                        report << "- Grade: " << PairGradeString(worst) << "\n";
                        report << "- Area ratio: " << std::fixed << std::setprecision(3) << worst_area_ratio << "\n";
                        report << "- Vertex match: " << (all_match ? "YES" : "NO")
                               << " (" << total_parent_verts << "/" << total_child_verts << ")\n";
                        report << "- Distance: " << std::fixed << std::setprecision(3) << worst_distance << "\n";
                    }
                    break;
                }
            }
            report << "\n";
        }
    }

    report.close();
    printf("Report written to: %s\n", report_path.c_str());

    // Cleanup
    SDL_GL_DestroyContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    printf("\n=== Pair Testing Complete ===\n");
    printf("Total models: %d\n", static_cast<int>(models.size()));
    printf("Total screenshots: %d\n", total_screenshots);
    printf("Report: %s\n", report_path.c_str());
    return 0;
}

// ============================================================================
// Analyze Mode (--analyze [output_path])
// ============================================================================

static int RunAnalyzeMode(const std::string& output_path)
{
    printf("=== Joint Face Matching Analysis ===\n");
    printf("Output: %s\n\n", output_path.c_str());

    BodyRenderer::JointFaceAnalyzer analyzer;

    // Test at multiple subdivision levels: low (1), default (2), medium (4), high (8)
    std::vector<int> levels = {1, 2, 4, 8};

    auto reports = analyzer.AnalyzeDirectory("assets/bodies/", levels);

    if (reports.empty()) {
        fprintf(stderr, "ERROR: No models found or all failed to load.\n");
        return 1;
    }

    printf("\nAnalyzed %d models. Writing report...\n", static_cast<int>(reports.size()));

    if (!analyzer.WriteReport(reports, output_path)) {
        fprintf(stderr, "ERROR: Failed to write report to '%s'\n", output_path.c_str());
        return 1;
    }

    // Print summary to stdout as well
    int total = 0, passing = 0;
    for (const auto& model : reports) {
        for (const auto& jr : model.joints) {
            total++;
            if (jr.grade <= BodyRenderer::JointFaceReport::ACCEPTABLE) {
                passing++;
            }
        }
    }

    printf("\n=== Results ===\n");
    printf("Total joint/level combinations: %d\n", total);
    printf("Passing (PERFECT/GOOD/ACCEPTABLE): %d\n", passing);
    printf("Failing (POOR/FAILING): %d\n", total - passing);
    if (total > 0) {
        printf("Pass rate: %.1f%%\n", 100.0f * passing / total);
    }
    printf("\nFull report written to: %s\n", output_path.c_str());
    return (total - passing > 0) ? 2 : 0; // exit code 2 if any failures
}

// ============================================================================
// Collision Test Mode (--collision-test)
// ============================================================================

static const char* CollisionPrimTypeName(BodyRenderer::CollisionPrimitive::PrimitiveType t)
{
    switch (t) {
    case BodyRenderer::CollisionPrimitive::PRIM_SPHERE: return "Sphere";
    case BodyRenderer::CollisionPrimitive::PRIM_CAPSULE: return "Capsule";
    case BodyRenderer::CollisionPrimitive::PRIM_CYLINDER: return "Cylinder";
    }
    return "Unknown";
}

static void LogCollisionNodeRecursive(
    const BodyRenderer::BodyNode* node,
    const BodyRenderer::Mat4& parentWorld,
    std::vector<std::pair<BodyRenderer::CollisionPrimitive*, BodyRenderer::Mat4>>& outPrimitives,
    int depth)
{
    if (!node) return;

    BodyRenderer::Mat4 world = parentWorld * node->localTransform;

    std::string indent(depth * 2, ' ');
    printf("%s[%s] shape=%s", indent.c_str(), node->name.c_str(), ShapeTypeName(node->shape.type));

    // Create collision primitive for this node
    BodyRenderer::CollisionPrimitive* prim = BodyRenderer::CreateCollisionPrimitive(node->shape);
    printf(" -> collision: %s (boundingR=%.3f)\n",
           CollisionPrimTypeName(prim->GetType()), prim->GetBoundingRadius());

    outPrimitives.push_back({prim, world});

    // Raycast test: shoot rays from 6 cardinal directions
    printf("%s  Raycast tests (6 directions):\n", indent.c_str());
    BodyRenderer::Vec3 directions[] = {
        BodyRenderer::Vec3(1, 0, 0), BodyRenderer::Vec3(-1, 0, 0),
        BodyRenderer::Vec3(0, 1, 0), BodyRenderer::Vec3(0, -1, 0),
        BodyRenderer::Vec3(0, 0, 1), BodyRenderer::Vec3(0, 0, -1)
    };
    const char* dirNames[] = { "+X", "-X", "+Y", "-Y", "+Z", "-Z" };

    float br = prim->GetBoundingRadius();
    int hits = 0;
    for (int i = 0; i < 6; ++i) {
        // Ray starts outside bounding radius, pointing inward
        BodyRenderer::Vec3 origin = directions[i] * (-(br + 2.0f));
        BodyRenderer::Vec3 dir = directions[i];
        BodyRenderer::Ray worldRay(world.TransformPoint(origin), world.TransformDirection(dir).Normalized());
        BodyRenderer::Ray localRay = BodyRenderer::TransformRayToLocal(worldRay, world);
        BodyRenderer::RayHit hit = prim->Raycast(localRay);

        if (hit.hit) {
            hits++;
            printf("%s    %s: HIT at dist=%.4f normal=(%.2f, %.2f, %.2f)\n",
                   indent.c_str(), dirNames[i], hit.distance,
                   hit.normal.x, hit.normal.y, hit.normal.z);
        } else {
            printf("%s    %s: MISS\n", indent.c_str(), dirNames[i]);
        }
    }
    printf("%s  Raycast summary: %d/6 hits\n", indent.c_str(), hits);

    // Recurse into children
    for (size_t i = 0; i < node->children.size(); ++i) {
        LogCollisionNodeRecursive(&node->children[i], world, outPrimitives, depth + 1);
    }
}

static int RunCollisionTestMode()
{
    printf("=== Collision Primitive Test ===\n\n");

    // Generate several random bodies and test collision on each
    BodyRenderer::BodyGenerator generator;
    BodyRenderer::SubdivisionSolver subdivSolver;

    int totalTests = 0;
    int totalOverlaps = 0;
    int totalOverlapTests = 0;

    // Test with 10 random bodies + all body files in assets/bodies/
    printf("--- Testing generated bodies ---\n\n");
    for (unsigned int seed = 1000; seed < 1010; ++seed) {
        BodyRenderer::Body body = generator.Generate(seed, 3);
        subdivSolver.PrepareBody(body, 8);

        printf("=== Generated body (seed=%u, name=%s) ===\n", seed, body.name.c_str());

        std::vector<std::pair<BodyRenderer::CollisionPrimitive*, BodyRenderer::Mat4>> primitives;
        BodyRenderer::Mat4 identity;
        identity.Identity();
        LogCollisionNodeRecursive(&body.root, identity, primitives, 0);

        // Overlap tests between all pairs of primitives in this body
        printf("\n  Overlap tests (all pairs):\n");
        for (size_t i = 0; i < primitives.size(); ++i) {
            for (size_t j = i + 1; j < primitives.size(); ++j) {
                totalOverlapTests++;
                BodyRenderer::OverlapResult r = primitives[i].first->TestOverlap(
                    *primitives[j].first,
                    primitives[i].second,
                    primitives[j].second
                );
                if (r.overlapping) {
                    totalOverlaps++;
                    printf("    [%d vs %d] OVERLAP depth=%.4f axis=(%.2f, %.2f, %.2f)\n",
                           static_cast<int>(i), static_cast<int>(j),
                           r.penetrationDepth,
                           r.separationAxis.x, r.separationAxis.y, r.separationAxis.z);
                }
            }
        }

        totalTests += static_cast<int>(primitives.size());

        // Clean up
        for (auto& p : primitives) {
            delete p.first;
        }
        printf("\n");
    }

    // Try loading bodies from assets/bodies/
    printf("\n--- Testing asset bodies ---\n\n");
    BodyRenderer::ModelSwitcher switcher;
    if (switcher.LoadDirectory("assets/bodies/")) {
        for (int i = 0; i < switcher.GetCount(); ++i) {
            BodyRenderer::BodyLoader loader;
            BodyRenderer::LoadResult result = loader.LoadFromFile(switcher.GetCurrentPath());
            if (result.success) {
                subdivSolver.PrepareBody(result.body, 8);
                printf("=== %s ===\n", result.body.name.c_str());

                std::vector<std::pair<BodyRenderer::CollisionPrimitive*, BodyRenderer::Mat4>> primitives;
                BodyRenderer::Mat4 identity;
                identity.Identity();
                LogCollisionNodeRecursive(&result.body.root, identity, primitives, 0);

                printf("\n  Overlap tests (all pairs):\n");
                for (size_t a = 0; a < primitives.size(); ++a) {
                    for (size_t bIdx = a + 1; bIdx < primitives.size(); ++bIdx) {
                        totalOverlapTests++;
                        BodyRenderer::OverlapResult r = primitives[a].first->TestOverlap(
                            *primitives[bIdx].first,
                            primitives[a].second,
                            primitives[bIdx].second
                        );
                        if (r.overlapping) {
                            totalOverlaps++;
                            printf("    [%d vs %d] OVERLAP depth=%.4f\n",
                                   static_cast<int>(a), static_cast<int>(bIdx),
                                   r.penetrationDepth);
                        }
                    }
                }

                totalTests += static_cast<int>(primitives.size());
                for (auto& p : primitives) {
                    delete p.first;
                }
                printf("\n");
            }
            switcher.Next();
        }
    } else {
        printf("  (No asset bodies found in assets/bodies/)\n");
    }

    printf("\n=== Collision Test Summary ===\n");
    printf("Total primitives tested: %d\n", totalTests);
    printf("Total overlap tests: %d\n", totalOverlapTests);
    printf("Overlaps detected: %d\n", totalOverlaps);
    printf("=== Done ===\n");
    return 0;
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[])
{
    // Check for --analyze-pairs mode (offscreen pair testing)
    if (argc >= 2 && std::string(argv[1]) == "--analyze-pairs") {
        std::string output = "";
        if (argc >= 3) output = argv[2];
        return RunAnalyzePairsMode(output);
    }

    // Check for --analyze mode (runs without SDL/window)
    if (argc >= 2 && std::string(argv[1]) == "--analyze") {
        std::string output = "joint_face_analysis.md";
        if (argc >= 3) output = argv[2];
        return RunAnalyzeMode(output);
    }

    // Check for --dump mode (runs without SDL/window)
    if (argc >= 3 && std::string(argv[1]) == "--dump") {
        return RunDumpMode(argv[2]);
    }

    // Check for --collision-test mode (runs without SDL/window)
    if (argc >= 2 && std::string(argv[1]) == "--collision-test") {
        return RunCollisionTestMode();
    }

    // Determine body directory
    std::string body_dir = "assets/bodies/";
    if (argc > 1) {
        body_dir = argv[1];
    }

    // Initialize SDL
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("[Body_Viewer] SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    // Set OpenGL attributes
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    // Create window
    SDL_Window* window = SDL_CreateWindow("Particluar Body Viewer", 1024, 768,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) {
        SDL_Log("[Body_Viewer] Window creation failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Create GL context
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        SDL_Log("[Body_Viewer] OpenGL context creation failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glClearDepth(1.0);

    // Setup initial projection
    SetupProjection(1024, 768);

    // Load models from directory
    BodyRenderer::ModelSwitcher switcher;
    ViewerState state;
    BodyRenderer::BodyRendererGL renderer;

    bool viewing_generated = false;
    int total_models = 0; // file models + generated
    int current_index = 0;

    if (switcher.LoadDirectory(body_dir)) {
        LoadModel(switcher.GetCurrentPath(), state);
        renderer.InvalidateCache();
        total_models = switcher.GetCount();
        SDL_Log("[Body_Viewer] Loaded %d models from '%s'", total_models, body_dir.c_str());
    } else {
        SDL_Log("[Body_Viewer] No models found in '%s' — starting with generated bodies", body_dir.c_str());
        LoadGeneratedBody(state);
        renderer.InvalidateCache();
        viewing_generated = true;
        total_models = 1;
    }

    UpdateWindowTitle(window, state,
                      state.has_model ? state.current_body.name : "None",
                      current_index, total_models);

    // Main loop
    bool running = true;
    Uint64 last_time = SDL_GetTicks();
    const float ROTATION_SPEED = 90.0f; // degrees per second

    SDL_Log("[Body_Viewer] Controls:");
    SDL_Log("  Left/Right arrows  - Cycle models (generates random when past end)");
    SDL_Log("  W/A/S/D            - Orbit camera");
    SDL_Log("  +/-                - Increase/decrease subdivision");
    SDL_Log("  Space              - Toggle joint animation");
    SDL_Log("  T                  - Toggle shape scale animation");
    SDL_Log("  G                  - Generate new random body");
    SDL_Log("  C                  - Toggle collision shape wireframe overlay");
    SDL_Log("  Scroll wheel       - Zoom");
    SDL_Log("  Escape             - Quit");

    while (running) {
        Uint64 now = SDL_GetTicks();
        float dt = static_cast<float>(now - last_time) / 1000.0f;
        last_time = now;

        // Process events
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_EVENT_QUIT:
                running = false;
                break;

            case SDL_EVENT_KEY_DOWN:
                if (event.key.key == SDLK_LEFT) {
                    if (viewing_generated || current_index == 0) {
                        // If we're viewing generated or at start, go to last file model
                        if (switcher.GetCount() > 0) {
                            viewing_generated = false;
                            switcher.Previous();
                            current_index = switcher.GetCurrentIndex();
                            LoadModel(switcher.GetCurrentPath(), state);
                        }
                    } else {
                        switcher.Previous();
                        current_index = switcher.GetCurrentIndex();
                        LoadModel(switcher.GetCurrentPath(), state);
                        viewing_generated = false;
                    }
                    // Reset animation state on model switch
                    state.animator.SetEnabled(false);
                    state.scale_animator.SetEnabled(false);
                    renderer.InvalidateCache();
                    total_models = switcher.GetCount() + 1; // +1 for potential generated
                    UpdateWindowTitle(window, state, state.current_body.name,
                                      current_index, total_models);
                } else if (event.key.key == SDLK_RIGHT) {
                    if (!viewing_generated && switcher.GetCount() > 0) {
                        switcher.Next();
                        current_index = switcher.GetCurrentIndex();
                        // If we wrapped around, generate instead
                        if (current_index == 0) {
                            LoadGeneratedBody(state);
                            viewing_generated = true;
                            current_index = switcher.GetCount();
                        } else {
                            LoadModel(switcher.GetCurrentPath(), state);
                        }
                    } else {
                        // Already viewing generated — generate next
                        LoadGeneratedBody(state);
                        viewing_generated = true;
                        current_index = switcher.GetCount();
                    }
                    // Reset animation state on model switch
                    state.animator.SetEnabled(false);
                    state.scale_animator.SetEnabled(false);
                    renderer.InvalidateCache();
                    total_models = switcher.GetCount() + 1;
                    UpdateWindowTitle(window, state, state.current_body.name,
                                      current_index, total_models);
                } else if (event.key.key == SDLK_ESCAPE) {
                    running = false;
                } else if (event.key.key == SDLK_SPACE) {
                    // Toggle joint animation
                    state.animator.SetEnabled(!state.animator.IsEnabled());
                    UpdateWindowTitle(window, state, state.current_body.name,
                                      current_index, total_models);
                } else if (event.key.key == SDLK_T) {
                    // Toggle shape scale animation
                    state.scale_animator.SetEnabled(!state.scale_animator.IsEnabled());
                    if (!state.scale_animator.IsEnabled()) {
                        // Reset shapes to base values when disabling
                        state.scale_animator.ResetTo(state.current_body);
                        BodyRenderer::ConnectionSolver solver;
                        solver.ResolveTree(&state.current_body.root);
                        renderer.InvalidateCache();
                    }
                    UpdateWindowTitle(window, state, state.current_body.name,
                                      current_index, total_models);
                } else if (event.key.key == SDLK_EQUALS || event.key.key == SDLK_KP_PLUS) {
                    // Increase subdivision
                    if (state.subdivision_level < 8) {
                        state.subdivision_level++;
                        ReloadCurrentBody(state, switcher, viewing_generated);
                        renderer.InvalidateCache();
                        UpdateWindowTitle(window, state, state.current_body.name,
                                          current_index, total_models);
                    }
                } else if (event.key.key == SDLK_MINUS || event.key.key == SDLK_KP_MINUS) {
                    // Decrease subdivision
                    if (state.subdivision_level > 1) {
                        state.subdivision_level--;
                        ReloadCurrentBody(state, switcher, viewing_generated);
                        renderer.InvalidateCache();
                        UpdateWindowTitle(window, state, state.current_body.name,
                                          current_index, total_models);
                    }
                } else if (event.key.key == SDLK_G) {
                    // Generate a new random body
                    LoadGeneratedBody(state);
                    viewing_generated = true;
                    current_index = switcher.GetCount();
                    // Reset animation state on model switch
                    state.animator.SetEnabled(false);
                    state.scale_animator.SetEnabled(false);
                    renderer.InvalidateCache();
                    total_models = switcher.GetCount() + 1;
                    UpdateWindowTitle(window, state, state.current_body.name,
                                      current_index, total_models);
                } else if (event.key.key == SDLK_C) {
                    // Toggle collision primitive visualization
                    state.show_colliders = !state.show_colliders;
                    if (state.has_model) {
                        if (state.show_colliders) {
                            // Log collision info on enable
                            printf("\n=== Collision Primitives: %s ===\n", state.current_body.name.c_str());
                            std::vector<std::pair<BodyRenderer::CollisionPrimitive*, BodyRenderer::Mat4>> primitives;
                            BodyRenderer::Mat4 identity;
                            identity.Identity();
                            LogCollisionNodeRecursive(&state.current_body.root, identity, primitives, 0);

                            printf("\n  Overlap tests (all pairs):\n");
                            int overlaps = 0;
                            for (size_t ci = 0; ci < primitives.size(); ++ci) {
                                for (size_t cj = ci + 1; cj < primitives.size(); ++cj) {
                                    BodyRenderer::OverlapResult r = primitives[ci].first->TestOverlap(
                                        *primitives[cj].first,
                                        primitives[ci].second,
                                        primitives[cj].second
                                    );
                                    if (r.overlapping) {
                                        overlaps++;
                                        printf("    [%d vs %d] OVERLAP depth=%.4f axis=(%.2f, %.2f, %.2f)\n",
                                               static_cast<int>(ci), static_cast<int>(cj),
                                               r.penetrationDepth,
                                               r.separationAxis.x, r.separationAxis.y, r.separationAxis.z);
                                    }
                                }
                            }
                            if (overlaps == 0) {
                                printf("    No overlaps detected.\n");
                            }
                            printf("=== End Collision Info ===\n\n");

                            for (auto& p : primitives) {
                                delete p.first;
                            }
                        } else {
                            printf("[Colliders] Visualization OFF\n");
                        }
                    }
                    UpdateWindowTitle(window, state, state.current_body.name,
                                      current_index, total_models);
                }
                break;

            case SDL_EVENT_MOUSE_WHEEL:
                state.distance -= event.wheel.y * 0.5f;
                if (state.distance < 1.0f) state.distance = 1.0f;
                if (state.distance > 20.0f) state.distance = 20.0f;
                break;

            case SDL_EVENT_WINDOW_RESIZED: {
                int w = event.window.data1;
                int h = event.window.data2;
                SetupProjection(w, h);
                break;
            }
            }
        }

        // Continuous rotation via held keys
        const bool* keys = SDL_GetKeyboardState(NULL);
        if (keys[SDL_SCANCODE_W]) state.pitch += ROTATION_SPEED * dt;
        if (keys[SDL_SCANCODE_S]) state.pitch -= ROTATION_SPEED * dt;
        if (keys[SDL_SCANCODE_A]) state.yaw -= ROTATION_SPEED * dt;
        if (keys[SDL_SCANCODE_D]) state.yaw += ROTATION_SPEED * dt;

        // Clamp pitch
        if (state.pitch > 89.0f) state.pitch = 89.0f;
        if (state.pitch < -89.0f) state.pitch = -89.0f;

        // Update joint animation
        if (state.animator.IsEnabled() && state.has_model) {
            bool changed = state.animator.Update(dt);
            if (changed) {
                // Re-apply animation to body (modifies connection rotations)
                state.animator.ApplyTo(state.current_body);

                // Re-resolve transforms after animation
                BodyRenderer::ConnectionSolver solver;
                solver.ResolveTree(&state.current_body.root);

                // Invalidate geometry cache since transforms changed
                renderer.InvalidateCache();

                // Update title to show current joint
                UpdateWindowTitle(window, state, state.current_body.name,
                                  current_index, total_models);
            }
        }

        // Update shape scale animation
        if (state.scale_animator.IsEnabled() && state.has_model) {
            bool changed = state.scale_animator.Update(dt);
            if (changed) {
                state.scale_animator.ApplyTo(state.current_body);

                // Re-resolve transforms after scale change
                BodyRenderer::ConnectionSolver solver;
                solver.ResolveTree(&state.current_body.root);

                // Invalidate geometry cache since shapes changed
                renderer.InvalidateCache();

                UpdateWindowTitle(window, state, state.current_body.name,
                                  current_index, total_models);
            }
        }

        // Render
        RenderFrame(state, renderer);
        RenderColliders(state);
        SDL_GL_SwapWindow(window);

        // Cap frame rate
        SDL_Delay(16); // ~60fps
    }

    // Cleanup
    SDL_GL_DestroyContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
