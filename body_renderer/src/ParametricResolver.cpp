#include "ParametricResolver.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace BodyRenderer {

SurfacePoint ParametricResolver::Resolve(const ShapeParams& shape, const AttachmentPoint& attach) const
{
    switch (shape.type) {
    case ShapeType::Sphere:   return ResolveSphere(shape, attach);
    case ShapeType::Cylinder: return ResolveCylinder(shape, attach);
    case ShapeType::Cone:     return ResolveCone(shape, attach);
    case ShapeType::Torus:    return ResolveTorus(shape, attach);
    case ShapeType::Capsule:  return ResolveCapsule(shape, attach);
    }
    return {{0, 0, 0}, {0, 1, 0}};
}

SurfacePoint ParametricResolver::ResolveSphere(const ShapeParams& s, const AttachmentPoint& a) const
{
    // Sphere surface: u = longitude (0-1 maps to 0-2*PI), v = latitude (0=top pole, 1=bottom pole)
    float theta = a.u * 2.0f * static_cast<float>(M_PI);
    float phi = a.v * static_cast<float>(M_PI);

    float sp = std::sin(phi);
    float cp = std::cos(phi);
    float st = std::sin(theta);
    float ct = std::cos(theta);

    Vec3 normal(sp * ct, cp, sp * st);
    Vec3 position = normal * s.radius;

    return {position, normal};
}

SurfacePoint ParametricResolver::ResolveCylinder(const ShapeParams& s, const AttachmentPoint& a) const
{
    float hh = s.height * 0.5f;

    switch (a.region) {
    case AttachRegion::Top: {
        // Top cap center: position on the cap disc
        // u,v = position within the cap (0.5, 0.5 = center)
        // We use center for face connections
        Vec3 position(0, hh, 0);
        Vec3 normal(0, 1, 0);
        return {position, normal};
    }
    case AttachRegion::Bottom: {
        Vec3 position(0, -hh, 0);
        Vec3 normal(0, -1, 0);
        return {position, normal};
    }
    case AttachRegion::Side: {
        // u = angular (0-1 = 0-2*PI), v = height (0=bottom, 1=top)
        float angle = a.u * 2.0f * static_cast<float>(M_PI);
        float y = -hh + a.v * s.height;
        float cx = std::cos(angle);
        float cz = std::sin(angle);
        Vec3 position(s.radius * cx, y, s.radius * cz);
        Vec3 normal(cx, 0, cz);
        return {position, normal};
    }
    default:
        // Default to top
        return {{0, hh, 0}, {0, 1, 0}};
    }
}

SurfacePoint ParametricResolver::ResolveCone(const ShapeParams& s, const AttachmentPoint& a) const
{
    float hh = s.height * 0.5f;

    switch (a.region) {
    case AttachRegion::Base: {
        // Base center
        Vec3 position(0, -hh, 0);
        Vec3 normal(0, -1, 0);
        return {position, normal};
    }
    case AttachRegion::Side: {
        // u = angular (0-1 = 0-2*PI), v = height (0=base, 1=tip)
        float angle = a.u * 2.0f * static_cast<float>(M_PI);
        float r = s.radius * (1.0f - a.v); // radius decreases toward tip
        float y = -hh + a.v * s.height;
        float cx = std::cos(angle);
        float cz = std::sin(angle);
        Vec3 position(r * cx, y, r * cz);

        // Compute normal for cone surface
        float slope_angle = std::atan2(s.radius, s.height);
        float ny = std::sin(slope_angle);
        float nr = std::cos(slope_angle);
        Vec3 normal(nr * cx, ny, nr * cz);
        return {position, normal.Normalized()};
    }
    default:
        // Default to base
        return {{0, -hh, 0}, {0, -1, 0}};
    }
}

SurfacePoint ParametricResolver::ResolveTorus(const ShapeParams& s, const AttachmentPoint& a) const
{
    // u = ring angle (0-1 = 0-2*PI), v = tube angle (0-1 = 0-2*PI)
    float theta = a.u * 2.0f * static_cast<float>(M_PI);
    float phi = a.v * 2.0f * static_cast<float>(M_PI);

    float R = s.majorRadius;
    float r = s.minorRadius;

    float x = (R + r * std::cos(phi)) * std::cos(theta);
    float y = r * std::sin(phi);
    float z = (R + r * std::cos(phi)) * std::sin(theta);

    // Normal points outward from tube center
    float nx = std::cos(phi) * std::cos(theta);
    float ny = std::sin(phi);
    float nz = std::cos(phi) * std::sin(theta);

    Vec3 position(x, y, z);
    Vec3 normal(nx, ny, nz);
    return {position, normal.Normalized()};
}

SurfacePoint ParametricResolver::ResolveCapsule(const ShapeParams& s, const AttachmentPoint& a) const
{
    // Capsule: cylinder of height (height - 2*radius) in the middle,
    // with hemisphere caps of the given radius on top and bottom
    float cylinder_height = s.height - 2.0f * s.radius;
    if (cylinder_height < 0.0f) cylinder_height = 0.0f;
    float hh = cylinder_height * 0.5f;

    switch (a.region) {
    case AttachRegion::TopCap:
    case AttachRegion::Top: {
        // Top hemisphere: u = longitude (0-1), v = latitude (0=pole, 1=equator)
        float theta = a.u * 2.0f * static_cast<float>(M_PI);
        float phi = a.v * 0.5f * static_cast<float>(M_PI); // 0 to PI/2

        float sp = std::sin(phi);
        float cp = std::cos(phi);
        Vec3 normal(sp * std::cos(theta), cp, sp * std::sin(theta));
        Vec3 position = normal * s.radius + Vec3(0, hh, 0);
        return {position, normal};
    }
    case AttachRegion::BottomCap:
    case AttachRegion::Bottom: {
        // Bottom hemisphere: u = longitude (0-1), v = latitude (0=pole, 1=equator)
        float theta = a.u * 2.0f * static_cast<float>(M_PI);
        float phi = a.v * 0.5f * static_cast<float>(M_PI);

        float sp = std::sin(phi);
        float cp = std::cos(phi);
        Vec3 normal(sp * std::cos(theta), -cp, sp * std::sin(theta));
        Vec3 position = normal * s.radius + Vec3(0, -hh, 0);
        return {position, normal};
    }
    case AttachRegion::Side: {
        // Cylinder portion: u = angular (0-1), v = height (0=bottom, 1=top)
        float angle = a.u * 2.0f * static_cast<float>(M_PI);
        float y = -hh + a.v * cylinder_height;
        float cx = std::cos(angle);
        float cz = std::sin(angle);
        Vec3 position(s.radius * cx, y, s.radius * cz);
        Vec3 normal(cx, 0, cz);
        return {position, normal};
    }
    default:
        return {{0, hh + s.radius, 0}, {0, 1, 0}};
    }
}

} // namespace BodyRenderer
