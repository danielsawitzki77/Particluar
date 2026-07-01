#pragma once

#include "Vec3.h"
#include <cmath>
#include <cstring>

namespace Particluar {

// Column-major 4x4 matrix (OpenGL layout)
struct Mat4 {
    float m[16];

    Mat4() { Identity(); }

    void Identity() {
        std::memset(m, 0, sizeof(m));
        m[0] = m[5] = m[10] = m[15] = 1.0f;
    }

    // Translation in elements [12], [13], [14]
    static Mat4 Translation(float x, float y, float z) {
        Mat4 r;
        r.m[12] = x; r.m[13] = y; r.m[14] = z;
        return r;
    }

    static Mat4 RotationX(float radians) {
        Mat4 r;
        float c = std::cos(radians), s = std::sin(radians);
        r.m[5] = c;  r.m[6] = s;
        r.m[9] = -s; r.m[10] = c;
        return r;
    }

    static Mat4 RotationY(float radians) {
        Mat4 r;
        float c = std::cos(radians), s = std::sin(radians);
        r.m[0] = c;  r.m[2] = -s;
        r.m[8] = s;  r.m[10] = c;
        return r;
    }

    static Mat4 RotationZ(float radians) {
        Mat4 r;
        float c = std::cos(radians), s = std::sin(radians);
        r.m[0] = c;  r.m[1] = s;
        r.m[4] = -s; r.m[5] = c;
        return r;
    }

    // Rotation around arbitrary axis (radians)
    static Mat4 RotationAxis(const Vec3& axis, float radians) {
        Vec3 a = axis.Normalized();
        float c = std::cos(radians), s = std::sin(radians);
        float t = 1.0f - c;
        Mat4 r;
        r.m[0]  = t * a.x * a.x + c;
        r.m[1]  = t * a.x * a.y + s * a.z;
        r.m[2]  = t * a.x * a.z - s * a.y;
        r.m[4]  = t * a.x * a.y - s * a.z;
        r.m[5]  = t * a.y * a.y + c;
        r.m[6]  = t * a.y * a.z + s * a.x;
        r.m[8]  = t * a.x * a.z + s * a.y;
        r.m[9]  = t * a.y * a.z - s * a.x;
        r.m[10] = t * a.z * a.z + c;
        return r;
    }

    Mat4 operator*(const Mat4& b) const {
        Mat4 result;
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                float sum = 0.0f;
                for (int k = 0; k < 4; ++k) {
                    sum += m[k * 4 + row] * b.m[col * 4 + k];
                }
                result.m[col * 4 + row] = sum;
            }
        }
        return result;
    }

    Vec3 TransformPoint(const Vec3& p) const {
        float w = m[3] * p.x + m[7] * p.y + m[11] * p.z + m[15];
        if (std::fabs(w) < 1e-9f) w = 1.0f;
        return Vec3(
            (m[0] * p.x + m[4] * p.y + m[8]  * p.z + m[12]) / w,
            (m[1] * p.x + m[5] * p.y + m[9]  * p.z + m[13]) / w,
            (m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14]) / w
        );
    }

    Vec3 TransformDirection(const Vec3& d) const {
        return Vec3(
            m[0] * d.x + m[4] * d.y + m[8]  * d.z,
            m[1] * d.x + m[5] * d.y + m[9]  * d.z,
            m[2] * d.x + m[6] * d.y + m[10] * d.z
        );
    }
};

} // namespace Particluar
