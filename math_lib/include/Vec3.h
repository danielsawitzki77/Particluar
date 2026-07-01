#pragma once

#include <cmath>

namespace Particluar {

struct Vec3 {
    float x, y, z;

    Vec3() : x(0.0f), y(0.0f), z(0.0f) {}
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& b) const { return Vec3(x + b.x, y + b.y, z + b.z); }
    Vec3 operator-(const Vec3& b) const { return Vec3(x - b.x, y - b.y, z - b.z); }
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }
    Vec3 operator-() const { return Vec3(-x, -y, -z); }

    float Dot(const Vec3& b) const { return x * b.x + y * b.y + z * b.z; }
    Vec3 Cross(const Vec3& b) const {
        return Vec3(y * b.z - z * b.y,
                    z * b.x - x * b.z,
                    x * b.y - y * b.x);
    }
    float Length() const { return std::sqrt(x * x + y * y + z * z); }
    Vec3 Normalized() const {
        float len = Length();
        if (len < 1e-9f) return Vec3(0, 0, 0);
        return Vec3(x / len, y / len, z / len);
    }
};

} // namespace Particluar
