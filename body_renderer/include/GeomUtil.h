#pragma once

#include "BodyTypes.h"

namespace BodyRenderer {

// ============================================================================
// GeomUtil — Geometry utility class for collision and spatial queries.
// Provides closest-point and distance calculations for line segments.
// ============================================================================

class GeomUtil {
public:
    // Finds the closest point on a line segment (a to b) to a given point.
    // Returns the parameter t in [0,1] and writes the closest point to 'outClosest'.
    static float ClosestPointOnSegment(const Vec3& a, const Vec3& b,
                                       const Vec3& point, Vec3& outClosest);

    // Computes the squared distance between two line segments (p1->q1 and p2->q2).
    // Writes the closest points on each segment to outClosest1 and outClosest2.
    static float ClosestDistSegmentSegment(const Vec3& p1, const Vec3& q1,
                                           const Vec3& p2, const Vec3& q2,
                                           Vec3& outClosest1, Vec3& outClosest2);

    // Extracts the translation (position) from a 4x4 world transform matrix.
    static Vec3 GetWorldPosition(const Mat4& world);
};

} // namespace BodyRenderer
