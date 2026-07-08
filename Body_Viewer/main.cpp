#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#ifdef _WIN32
#include <windows.h>
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
#include "JointAnimator.h"
#include "ShapeScaleAnimator.h"
#include "ModelSwitcher.h"

#include <string>
#include <cstdio>
#include <cmath>
#include <algorithm>

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
// Subdivision adjustment
// ============================================================================

static void ApplySubdivision(BodyRenderer::Body& body, int level)
{
    // Adjust all shape segments based on subdivision level
    struct Adjuster {
        int level;
        void Apply(BodyRenderer::BodyNode& node) {
            BodyRenderer::ShapeParams& s = node.shape;
            // Apply multiplier
            switch (s.type) {
            case BodyRenderer::ShapeType::Cone:
            case BodyRenderer::ShapeType::Cylinder:
            case BodyRenderer::ShapeType::Capsule: {
                int base = 8;
                s.segments = base * level;
                if (s.segments < 3) s.segments = 3;
                if (s.segments > 128) s.segments = 128;
                // Height segments scale with level for tapering support
                s.height_segments = (std::max)(1, level * 2);
                if (s.height_segments > 16) s.height_segments = 16;
                break;
            }
            case BodyRenderer::ShapeType::Sphere: {
                int base_lon = 10;
                int base_lat = 8;
                s.lon_segments = base_lon * level;
                s.lat_segments = base_lat * level;
                if (s.lon_segments < 4) s.lon_segments = 4;
                if (s.lon_segments > 128) s.lon_segments = 128;
                if (s.lat_segments < 3) s.lat_segments = 3;
                if (s.lat_segments > 64) s.lat_segments = 64;
                break;
            }
            case BodyRenderer::ShapeType::Torus: {
                int base_ring = 10;
                int base_side = 8;
                s.ring_segments = base_ring * level;
                s.side_segments = base_side * level;
                if (s.ring_segments < 3) s.ring_segments = 3;
                if (s.ring_segments > 128) s.ring_segments = 128;
                if (s.side_segments < 3) s.side_segments = 3;
                if (s.side_segments > 64) s.side_segments = 64;
                break;
            }
            }

            for (auto& child : node.children) {
                Apply(child);
            }
        }
    };

    Adjuster adj;
    adj.level = level;
    adj.Apply(body.root);
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

    // Apply current subdivision level
    ApplySubdivision(state.current_body, state.subdivision_level);

    // Resolve connections
    BodyRenderer::ConnectionSolver solver;
    solver.ResolveTree(&state.current_body.root);

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

    // Apply current subdivision level
    ApplySubdivision(state.current_body, state.subdivision_level);

    // Resolve connections
    BodyRenderer::ConnectionSolver solver;
    solver.ResolveTree(&state.current_body.root);

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

    ApplySubdivision(state.current_body, state.subdivision_level);

    BodyRenderer::ConnectionSolver solver;
    solver.ResolveTree(&state.current_body.root);

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

static const char* RegionName(BodyRenderer::AttachRegion r)
{
    switch (r) {
    case BodyRenderer::AttachRegion::Surface: return "surface";
    case BodyRenderer::AttachRegion::Top: return "top";
    case BodyRenderer::AttachRegion::Bottom: return "bottom";
    case BodyRenderer::AttachRegion::Side: return "side";
    case BodyRenderer::AttachRegion::Base: return "base";
    case BodyRenderer::AttachRegion::TopCap: return "top_cap";
    case BodyRenderer::AttachRegion::BottomCap: return "bottom_cap";
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

    BodyRenderer::Mat4 world = parent_world * node->local_transform;

    // Extract translation from local_transform
    float tx = node->local_transform.m[12];
    float ty = node->local_transform.m[13];
    float tz = node->local_transform.m[14];

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
    if (parent && !node->connection.is_legacy) {
        float matched_radius = faceMatcher.ComputeMatchedRadius(
            parent->shape, node->connection.parent_attach,
            node->shape, node->connection.child_attach
        );
        int matched_segments = faceMatcher.ComputeMatchedSegments(
            parent->shape, node->connection.parent_attach,
            node->shape, node->connection.child_attach
        );
        BodyRenderer::SurfacePoint child_pt = resolver.Resolve(node->shape, node->connection.child_attach);
        BodyRenderer::ConnectionRing child_ring;
        child_ring.center = child_pt.position;
        child_ring.normal = child_pt.normal;
        child_ring.radius = matched_radius;
        child_ring.segments = matched_segments;
        child_ring.child_index = -1;
        child_ring.attach = node->connection.child_attach;
        rings.push_back(child_ring);
    }

    // Add rings for each child (parent-side attachment on this node)
    for (int i = 0; i < static_cast<int>(node->children.size()); ++i) {
        const BodyRenderer::BodyNode& child = node->children[i];
        if (child.connection.is_legacy) continue;

        float matched_radius = faceMatcher.ComputeMatchedRadius(
            node->shape, child.connection.parent_attach,
            child.shape, child.connection.child_attach
        );
        int matched_segments = faceMatcher.ComputeMatchedSegments(
            node->shape, child.connection.parent_attach,
            child.shape, child.connection.child_attach
        );

        BodyRenderer::SurfacePoint pt = resolver.Resolve(node->shape, child.connection.parent_attach);
        BodyRenderer::ConnectionRing ring;
        ring.center = pt.position;
        ring.normal = pt.normal;
        ring.radius = matched_radius;
        ring.segments = matched_segments;
        ring.child_index = i;
        ring.attach = child.connection.parent_attach;
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
    if (parent && !node->connection.is_legacy) {
        printf("\n%s  --- Connection Analysis ---\n", indent.c_str());

        // Compute parent's connection face for this child
        // Re-generate parent's faces to find its connection face
        std::vector<BodyRenderer::ConnectionRing> parent_rings;
        for (int i = 0; i < static_cast<int>(parent->children.size()); ++i) {
            const BodyRenderer::BodyNode& sibling = parent->children[i];
            if (sibling.connection.is_legacy) continue;

            float mr = faceMatcher.ComputeMatchedRadius(
                parent->shape, sibling.connection.parent_attach,
                sibling.shape, sibling.connection.child_attach
            );
            int ms = faceMatcher.ComputeMatchedSegments(
                parent->shape, sibling.connection.parent_attach,
                sibling.shape, sibling.connection.child_attach
            );

            BodyRenderer::SurfacePoint pt = resolver.Resolve(parent->shape, sibling.connection.parent_attach);
            BodyRenderer::ConnectionRing ring;
            ring.center = pt.position;
            ring.normal = pt.normal;
            ring.radius = mr;
            ring.segments = ms;
            ring.child_index = i;
            ring.attach = sibling.connection.parent_attach;
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

    printf("Body: %s (format v%d)\n", result.body.name.c_str(), result.body.format_version);

    // Resolve connection tree
    BodyRenderer::ConnectionSolver solver;
    solver.ResolveTree(&result.body.root);

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
// Main
// ============================================================================

int main(int argc, char* argv[])
{
    // Check for --dump mode (runs without SDL/window)
    if (argc >= 3 && std::string(argv[1]) == "--dump") {
        return RunDumpMode(argv[2]);
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
