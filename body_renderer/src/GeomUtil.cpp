#include "GeomUtil.h"

namespace BodyRenderer {

float GeomUtil::ClosestPointOnSegment(const Vec3& a, const Vec3& b,
                                      const Vec3& point, Vec3& outClosest)
{
    Vec3 ab = b - a;
    float abLenSq = ab.Dot(ab);
    if (abLenSq < 1e-12f) {
        outClosest = a;
        return 0.0f;
    }
    float t = (point - a).Dot(ab) / abLenSq;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    outClosest = a + ab * t;
    return t;
}

float GeomUtil::ClosestDistSegmentSegment(const Vec3& p1, const Vec3& q1,
                                          const Vec3& p2, const Vec3& q2,
                                          Vec3& outClosest1, Vec3& outClosest2)
{
    Vec3 d1 = q1 - p1;
    Vec3 d2 = q2 - p2;
    Vec3 r = p1 - p2;

    float a = d1.Dot(d1);
    float e = d2.Dot(d2);
    float f = d2.Dot(r);

    float s, t;

    if (a <= 1e-12f && e <= 1e-12f) {
        outClosest1 = p1;
        outClosest2 = p2;
        Vec3 diff = outClosest1 - outClosest2;
        return diff.Dot(diff);
    }

    if (a <= 1e-12f) {
        s = 0.0f;
        t = f / e;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
    } else {
        float c = d1.Dot(r);
        if (e <= 1e-12f) {
            t = 0.0f;
            s = -c / a;
            if (s < 0.0f) s = 0.0f;
            if (s > 1.0f) s = 1.0f;
        } else {
            float b = d1.Dot(d2);
            float denom = a * e - b * b;

            if (denom != 0.0f) {
                s = (b * f - c * e) / denom;
                if (s < 0.0f) s = 0.0f;
                if (s > 1.0f) s = 1.0f;
            } else {
                s = 0.0f;
            }

            t = (b * s + f) / e;

            if (t < 0.0f) {
                t = 0.0f;
                s = -c / a;
                if (s < 0.0f) s = 0.0f;
                if (s > 1.0f) s = 1.0f;
            } else if (t > 1.0f) {
                t = 1.0f;
                s = (b - c) / a;
                if (s < 0.0f) s = 0.0f;
                if (s > 1.0f) s = 1.0f;
            }
        }
    }

    outClosest1 = p1 + d1 * s;
    outClosest2 = p2 + d2 * t;
    Vec3 diff = outClosest1 - outClosest2;
    return diff.Dot(diff);
}

Vec3 GeomUtil::GetWorldPosition(const Mat4& world)
{
    return Vec3(world.m[12], world.m[13], world.m[14]);
}

} // namespace BodyRenderer
