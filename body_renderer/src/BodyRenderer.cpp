#include "BodyRenderer.h"
#include "ConnectionSolver.h"
#include "ParametricResolver.h"

#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>

#include <SDL3/SDL.h>

namespace BodyRenderer {

void BodyRendererGL::Render(const Body& body, const RenderParams& params) const
{
    // Verify GL context
    if (!SDL_GL_GetCurrentContext()) {
        SDL_Log("[BodyRenderer] Error: No current OpenGL context");
        return;
    }

    // Setup lighting
    SetupLighting(params);

    // Set material
    float mat_ambient[4] = { params.ambient.x, params.ambient.y, params.ambient.z, 1.0f };
    float shininess = params.shininess;
    if (shininess < 1.0f) shininess = 1.0f;
    if (shininess > 128.0f) shininess = 128.0f;
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shininess);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, mat_ambient);

    // Render tree recursively from root
    Mat4 identity;
    RenderNode(&body.root, identity);
}

std::vector<ConnectionRing> BodyRendererGL::BuildParentRings(const BodyNode* node) const
{
    std::vector<ConnectionRing> rings;
    if (!node) return rings;

    ParametricResolver resolver;

    for (int i = 0; i < static_cast<int>(node->children.size()); ++i) {
        const BodyNode& child = node->children[i];
        if (child.connection.is_legacy) continue; // skip legacy connections

        // Compute matched radius and segments
        float matched_radius = m_faceMatcher.ComputeMatchedRadius(
            node->shape, child.connection.parent_attach,
            child.shape, child.connection.child_attach
        );
        int matched_segments = m_faceMatcher.ComputeMatchedSegments(
            node->shape, child.connection.parent_attach,
            child.shape, child.connection.child_attach
        );

        // Resolve the parent attachment surface point
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

ConnectionRing BodyRendererGL::BuildChildRing(const BodyNode* child, const BodyNode* parent) const
{
    ParametricResolver resolver;

    // Compute matched radius and segments
    float matched_radius = m_faceMatcher.ComputeMatchedRadius(
        parent->shape, child->connection.parent_attach,
        child->shape, child->connection.child_attach
    );
    int matched_segments = m_faceMatcher.ComputeMatchedSegments(
        parent->shape, child->connection.parent_attach,
        child->shape, child->connection.child_attach
    );

    // Resolve the child attachment surface point (in child local space)
    SurfacePoint child_pt = resolver.Resolve(child->shape, child->connection.child_attach);

    ConnectionRing ring;
    ring.center = child_pt.position;
    ring.normal = child_pt.normal;
    ring.radius = matched_radius;
    ring.segments = matched_segments;
    ring.child_index = -1; // marks this as the child-side ring
    return ring;
}

void BodyRendererGL::RenderNode(const BodyNode* node, const Mat4& parent_world) const
{
    if (!node) return;

    // Compute world transform for this node
    Mat4 world = parent_world * node->local_transform;

    // Build connection rings for the parent side (where children attach to this node)
    std::vector<ConnectionRing> parent_rings = BuildParentRings(node);

    // Generate faces with connection ring modifications
    MatchedFaces matched = m_faceMatcher.GenerateWithConnections(*node, parent_rings);

    // Triangulate and submit
    std::vector<Triangle> tris = m_triangulator.Triangulate(matched.faces);
    SubmitTriangles(tris, node->color, world);

    // Render children
    for (int i = 0; i < static_cast<int>(node->children.size()); ++i) {
        const BodyNode& child = node->children[i];
        RenderChild(&child, node, world);
    }
}

void BodyRendererGL::RenderChild(const BodyNode* child, const BodyNode* parent, const Mat4& parent_world) const
{
    if (!child) return;

    Mat4 child_world = parent_world * child->local_transform;

    if (!child->connection.is_legacy) {
        // Build child-side ring (the face on the child that connects to its parent)
        ConnectionRing child_ring = BuildChildRing(child, parent);

        // Also build parent-side rings for this child's own children
        std::vector<ConnectionRing> all_rings;
        all_rings.push_back(child_ring);

        // Add rings for where this child's children connect
        ParametricResolver resolver;
        for (int i = 0; i < static_cast<int>(child->children.size()); ++i) {
            const BodyNode& grandchild = child->children[i];
            if (grandchild.connection.is_legacy) continue;

            float matched_radius = m_faceMatcher.ComputeMatchedRadius(
                child->shape, grandchild.connection.parent_attach,
                grandchild.shape, grandchild.connection.child_attach
            );
            int matched_segments = m_faceMatcher.ComputeMatchedSegments(
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

        // Generate child faces with all connection rings
        MatchedFaces child_matched = m_faceMatcher.GenerateWithConnections(*child, all_rings);
        std::vector<Triangle> child_tris = m_triangulator.Triangulate(child_matched.faces);
        SubmitTriangles(child_tris, child->color, child_world);
    } else {
        // Legacy connection — generate normal faces
        std::vector<Face> faces = m_faceGen.Generate(child->shape);
        std::vector<Triangle> tris = m_triangulator.Triangulate(faces);
        SubmitTriangles(tris, child->color, child_world);
    }

    // Render grandchildren recursively
    for (int i = 0; i < static_cast<int>(child->children.size()); ++i) {
        RenderChild(&child->children[i], child, child_world);
    }
}

void BodyRendererGL::SubmitTriangles(const std::vector<Triangle>& tris, const Vec3& color, const Mat4& world) const
{
    glPushMatrix();
    glMultMatrixf(world.m);

    // Set material color
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
