#include "BodyRenderer.h"
#include "ConnectionSolver.h"

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

void BodyRendererGL::RenderNode(const BodyNode* node, const Mat4& parent_world) const
{
    if (!node) return;

    // Compute world transform for this node
    Mat4 world = parent_world * node->local_transform;

    // Generate faces and triangulate
    std::vector<Face> faces = m_faceGen.Generate(node->shape);
    std::vector<Triangle> tris = m_triangulator.Triangulate(faces);

    // Submit triangles
    SubmitTriangles(tris, node->color, world);

    // Render children
    for (const auto& child : node->children) {
        RenderNode(&child, world);
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
