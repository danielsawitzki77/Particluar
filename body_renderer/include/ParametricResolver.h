#pragma once

#include "BodyTypes.h"

namespace BodyRenderer {

struct SurfacePoint {
    Vec3 position;
    Vec3 normal;
};

// Resolves parametric AttachmentPoint coordinates into 3D surface positions
class ParametricResolver {
public:
    // Given shape params and attachment point, compute the 3D surface position and normal
    SurfacePoint Resolve(const ShapeParams& shape, const AttachmentPoint& attach) const;

private:
    SurfacePoint ResolveSphere(const ShapeParams& s, const AttachmentPoint& a) const;
    SurfacePoint ResolveCylinder(const ShapeParams& s, const AttachmentPoint& a) const;
    SurfacePoint ResolveCone(const ShapeParams& s, const AttachmentPoint& a) const;
    SurfacePoint ResolveTorus(const ShapeParams& s, const AttachmentPoint& a) const;
    SurfacePoint ResolveCapsule(const ShapeParams& s, const AttachmentPoint& a) const;
};

} // namespace BodyRenderer
