#include "BodyRenderer.h"
#include "ConnectionSolver.h"
#include "ParametricResolver.h"

#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>

#include <SDL3/SDL.h>

namespace BodyRenderer {

// ============================================================================
// Cache building: traverses the body tree and generates all triangulated geometry
// ============================================================================

static void BuildCacheForNode(
    const BodyNode* node,
    const Mat4& parent_world,
    const FaceGenerator& faceGen,
    const ConnectionFaceMatcher& faceMatcher,
    const Triangulator& triangulator,
    std::vector<std::pair<std::vector<Triangle>, Vec3>>& cache);

static void BuildCacheForChild(
    const BodyNode* child,
    const BodyNode* parent,
    const Mat4& parent_world,
    const FaceGenerator& faceGen,
    const ConnectionFaceMatcher& faceMatcher,
    const Triangulator& triangulator,
    std::vector<std::pair<std::vector<Triangle>, Vec3>>& cache);

static std::vector<ConnectionRing> BuildParentRingsStatic(
    const BodyNode* node,
    const ConnectionFaceMatcher& faceMatcher)
{
    std::vector<ConnectionRing> rings;
    if (!node) return rings;

    ParametricResolver resolver;

    for (int i = 0; i < static_cast<int>(node->children.size()); ++i) {
        const BodyNode& child = node->children[i];
        if (child.connection.is_legacy) continue;

        float matched_radius = faceMatcher.ComputeMatchedRadius(
            node->shape, child.connection.parent_attach,
            child.shape, child.connection.child_attach
        );
        int matched_segments = faceMatcher.ComputeMatchedSegments(
            node->shape, child.connection.parent_attach,
            child.shape, child.connection.child_attach
        );

        SurfacePoint parent_pt = resolver.Resolve(node->shape, child.connection.parent_attach);

        ConnectionRing ring;
        ring.center = parent_pt.position;
        ring.normal = parent_pt.normal;
        ring.radius = matched_radius;
        ring.segments = matched_segments;
        ring.child_index = i;
        rings.push_back(ring);
    }

    return rings;
}

static ConnectionRing BuildChildRingStatic(
    const BodyNode* child,
    const BodyNode* parent,
    const ConnectionFaceMatcher& faceMatcher)
{
    ParametricResolver resolver;

    float matched_radius = faceMatcher.ComputeMatchedRadius(
        parent->shape, child->connection.parent_attach,
        child->shape, child->connection.child_attach
    );
    int matched_segments = faceMatcher.ComputeMatchedSegments(
        parent->shape, child->connection.parent_attach,
        child->shape, child->connection.child_attach
    );

    SurfacePoint child_pt = resolver.Resolve(child->shape, child->connection.child_attach);

    ConnectionRing ring;
    ring.center = child_pt.position;
    ring.normal = child_pt.normal;
    ring.radius = matched_radius;
    ring.segments = matched_segments;
    ring.child_index = -1;
    return ring;
}

static void BuildCacheForNode(
    const BodyNode* node,
    const Mat4& parent_world,
    const FaceGenerator& faceGen,
    const ConnectionFaceMatcher& faceMatcher,
    const Triangulator& triangulator,
    std::vector<std::pair<std::vector<Triangle>, Vec3>>& cache)
{
    if (!node) return;

    Mat4 world = parent_world * node->local_transform;

    std::vector<ConnectionRing> parent_rings = BuildParentRingsStatic(node, faceMatcher);
    MatchedFaces matched = faceMatcher.GenerateWithConnections(*node, parent_rings);
    std::vector<Triangle> tris = triangulator.Triangulate(matched.faces);

    // Transform triangles to world space
    for (auto& tri : tris) {
        tri.v0 = world.TransformPoint(tri.v0);
        tri.v1 = world.TransformPoint(tri.v1);
        tri.v2 = world.TransformPoint(tri.v2);
        tri.normal = world.TransformDirection(tri.normal).Normalized();
    }

    cache.push_back({std::move(tris), node->color});

    for (int i = 0; i < static_cast<int>(node->children.size()); ++i) {
        BuildCacheForChild(&node->children[i], node, world, faceGen, faceMatcher, triangulator, cache);
    }
}

static void BuildCacheForChild(
    const BodyNode* child,
    const BodyNode* parent,
    const Mat4& parent_world,
    const FaceGenerator& faceGen,
    const ConnectionFaceMatcher& faceMatcher,
    const Triangulator& triangulator,
    std::vector<std::pair<std::vector<Triangle>, Vec3>>& cache)
{
    if (!child) return;

    Mat4 child_world = parent_world * child->local_transform;

    if (!child->connection.is_legacy) {
        ConnectionRing child_ring = BuildChildRingStatic(child, parent, faceMatcher);

        std::vector<ConnectionRing> all_rings;
        all_rings.push_back(child_ring);

        ParametricResolver resolver;
        for (int i = 0; i < static_cast<int>(child->children.size()); ++i) {
            const BodyNode& grandchild = child->children[i];
            if (grandchild.connection.is_legacy) continue;

            float matched_radius = faceMatcher.ComputeMatchedRadius(
                child->shape, grandchild.connection.parent_attach,
                grandchild.shape, grandchild.connection.child_attach
            );
            int matched_segments = faceMatcher.ComputeMatchedSegments(
                child->shape, grandchild.connection.parent_attach,
                grandchild.shape, grandchild.connection.child_attach
            );

            SurfacePoint pt = resolver.Resolve(child->shape, grandchild.connection.parent_attach);
            ConnectionRing ring;
            ring.center = pt.position;
            ring.normal = pt.normal;
            ring.radius = matched_radius;
            ring.segments = matched_segments;
            ring.child_index = i;
            all_rings.push_back(ring);
        }

        MatchedFaces child_matched = faceMatcher.GenerateWithConnections(*child, all_rings);

        int shared_face_index = -1;
        if (!child_matched.connection_face_indices.empty()) {
            shared_face_index = child_matched.connection_face_indices[0];
        }

        std::vector<Face> render_faces;
        render_faces.reserve(child_matched.faces.size());
        for (int fi = 0; fi < static_cast<int>(child_matched.faces.size()); ++fi) {
            if (fi == shared_face_index) continue;
            render_faces.push_back(child_matched.faces[fi]);
        }

        std::vector<Triangle> child_tris = triangulator.Triangulate(render_faces);

        for (auto& tri : child_tris) {
            tri.v0 = child_world.TransformPoint(tri.v0);
            tri.v1 = child_world.TransformPoint(tri.v1);
            tri.v2 = child_world.TransformPoint(tri.v2);
            tri.normal = child_world.TransformDirection(tri.normal).Normalized();
        }

        cache.push_back({std::move(child_tris), child->color});
    } else {
        std::vector<Face> faces = faceGen.Generate(child->shape);
        std::vector<Triangle> tris = triangulator.Triangulate(faces);

        for (auto& tri : tris) {
            tri.v0 = child_world.TransformPoint(tri.v0);
            tri.v1 = child_world.TransformPoint(tri.v1);
            tri.v2 = child_world.TransformPoint(tri.v2);
            tri.normal = child_world.TransformDirection(tri.normal).Normalized();
        }

        cache.push_back({std::move(tris), child->color});
    }

    for (int i = 0; i < static_cast<int>(child->children.size()); ++i) {
        BuildCacheForChild(&child->children[i], child, child_world, faceGen, faceMatcher, triangulator, cache);
    }
}

// ============================================================================
// Public API
// ============================================================================

void BodyRendererGL::Render(const Body& body, const RenderParams& params) const
{
    if (!SDL_GL_GetCurrentContext()) {
        SDL_Log("[BodyRenderer] Error: No current OpenGL context");
        return;
    }

    SetupLighting(params);

    float mat_ambient[4] = { params.ambient.x, params.ambient.y, params.ambient.z, 1.0f };
    float shininess = params.shininess;
    if (shininess < 1.0f) shininess = 1.0f;
    if (shininess > 128.0f) shininess = 128.0f;
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shininess);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, mat_ambient);

    // Rebuild geometry cache if invalid
    if (!m_cacheValid) {
        m_cachedNodes.clear();
        Mat4 identity;
        BuildCacheForNode(&body.root, identity, m_faceGen, m_faceMatcher, m_triangulator, m_cachedNodes);
        m_cacheValid = true;
    }

    // Submit cached geometry — triangles are in world (model) space.
    // The caller has already set up the view matrix (gluLookAt) on the MODELVIEW stack.
    // We don't call glLoadIdentity here because the view transform must be preserved.
    for (const auto& entry : m_cachedNodes) {
        const auto& tris = entry.first;
        const auto& color = entry.second;

        float diffuse[4] = { color.x, color.y, color.z, 1.0f };
        float specular[4] = { 0.5f, 0.5f, 0.5f, 1.0f };
        glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse);
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular);

        glBegin(GL_TRIANGLES);
        for (const Triangle& tri : tris) {
            glNormal3f(tri.normal.x, tri.normal.y, tri.normal.z);
            glVertex3f(tri.v0.x, tri.v0.y, tri.v0.z);
            glVertex3f(tri.v1.x, tri.v1.y, tri.v1.z);
            glVertex3f(tri.v2.x, tri.v2.y, tri.v2.z);
        }
        glEnd();
    }
}

// Legacy methods still needed for old callers — delegate to static versions
std::vector<ConnectionRing> BodyRendererGL::BuildParentRings(const BodyNode* node) const
{
    return BuildParentRingsStatic(node, m_faceMatcher);
}

ConnectionRing BodyRendererGL::BuildChildRing(const BodyNode* child, const BodyNode* parent) const
{
    return BuildChildRingStatic(child, parent, m_faceMatcher);
}

void BodyRendererGL::RenderNode(const BodyNode* /*node*/, const Mat4& /*parent_world*/) const
{
    // No longer used — geometry is pre-cached
}

void BodyRendererGL::RenderChild(const BodyNode* /*child*/, const BodyNode* /*parent*/, const Mat4& /*parent_world*/) const
{
    // No longer used — geometry is pre-cached
}

void BodyRendererGL::SubmitTriangles(const std::vector<Triangle>& tris, const Vec3& color, const Mat4& world) const
{
    glPushMatrix();
    glMultMatrixf(world.m);

    float diffuse[4] = { color.x, color.y, color.z, 1.0f };
    float specular[4] = { 0.5f, 0.5f, 0.5f, 1.0f };
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular);

    glBegin(GL_TRIANGLES);
    for (const Triangle& tri : tris) {
        glNormal3f(tri.normal.x, tri.normal.y, tri.normal.z);
        glVertex3f(tri.v0.x, tri.v0.y, tri.v0.z);
        glVertex3f(tri.v1.x, tri.v1.y, tri.v1.z);
        glVertex3f(tri.v2.x, tri.v2.y, tri.v2.z);
    }
    glEnd();

    glPopMatrix();
}

void BodyRendererGL::SetupLighting(const RenderParams& params) const
{
    LightManager lightMgr;
    lightMgr.Apply(params.lights, params.ambient);
}

} // namespace BodyRenderer
