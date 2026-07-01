#pragma once

#include "BodyTypes.h"
#include <vector>

namespace BodyRenderer {

struct PointLight {
    Vec3 position;
    Vec3 diffuse;
    Vec3 specular;
    float constant_atten;
    float linear_atten;
    float quadratic_atten;

    PointLight()
        : position(0, 0, 0)
        , diffuse(1.0f, 1.0f, 1.0f)
        , specular(1.0f, 1.0f, 1.0f)
        , constant_atten(1.0f)
        , linear_atten(0.0f)
        , quadratic_atten(0.0f)
    {}
};

class LightManager {
public:
    void Apply(const std::vector<PointLight>& lights, const Vec3& ambient) const;
    void Disable() const;
};

} // namespace BodyRenderer
