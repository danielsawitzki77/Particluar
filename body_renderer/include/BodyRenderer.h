#pragma once

#include "BodyTypes.h"
#include "FaceGenerator.h"
#include "Triangulator.h"
#include "LightManager.h"
#include <vector>

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

class BodyRendererGL {
public:
    void Render(const Body& body, const RenderParams& params) const;

private:
    void RenderNode(const BodyNode* node, const Mat4& parent_world) const;
    void SubmitTriangles(const std::vector<Triangle>& tris, const Vec3& color, const Mat4& world) const;
    void SetupLighting(const RenderParams& params) const;

    FaceGenerator m_faceGen;
    Triangulator m_triangulator;
};

} // namespace BodyRenderer
