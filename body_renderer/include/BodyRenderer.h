#pragma once

#include "BodyTypes.h"
#include "FaceGenerator.h"
#include "ConnectionFaceMatcher.h"
#include "Triangulator.h"
#include "LightManager.h"
#include <vector>
#include <utility>

namespace BodyRenderer {

struct RenderParams {
    Vec3 ambient;
    std::vector<PointLight> lights; // max 8
    float shininess;
    Vec3 model_color;

    RenderParams()
        : ambient(0.2f, 0.2f, 0.2f)
        , shininess(32.0f)
        , model_color(0.7f, 0.7f, 0.7f)
    {}
};

// Cached triangulated geometry for a single node
struct CachedNodeGeometry {
    std::vector<Triangle> triangles;
    Vec3 color;
};

class BodyRendererGL {
public:
    void Render(const Body& body, const RenderParams& params) const;

    // Invalidate geometry cache (call when body geometry changes —
    // model switch, subdivision change, scale animation, etc.)
    void InvalidateCache() { m_cacheValid = false; }

private:
    void RenderNode(const BodyNode* node, const Mat4& parent_world) const;
    void RenderChild(const BodyNode* child, const BodyNode* parent, const Mat4& parent_world) const;
    void SubmitTriangles(const std::vector<Triangle>& tris, const Vec3& color, const Mat4& world) const;
    void SetupLighting(const RenderParams& params) const;

    // Build connection rings for a node (describing where its children connect to it)
    std::vector<ConnectionRing> BuildParentRings(const BodyNode* node) const;

    // Build the child-side ring (the face on the child that connects to its parent)
    ConnectionRing BuildChildRing(const BodyNode* child, const BodyNode* parent) const;

    FaceGenerator m_faceGen;
    ConnectionFaceMatcher m_faceMatcher;
    Triangulator m_triangulator;

    // Geometry cache — mutable because Render() is const but caching is an implementation detail
    mutable bool m_cacheValid = false;
    mutable std::vector<std::pair<std::vector<Triangle>, Vec3>> m_cachedNodes;
};

} // namespace BodyRenderer
