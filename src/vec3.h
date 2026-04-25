#pragma once
#include <cmath>
#include <ostream>

struct Vec3 {
    float x, y, z;

    Vec3() : x(0), y(0), z(0) {}
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}

    Vec3 operator+(const Vec3& v) const { return {x+v.x, y+v.y, z+v.z}; }
    Vec3 operator-(const Vec3& v) const { return {x-v.x, y-v.y, z-v.z}; }
    Vec3 operator*(float t)        const { return {x*t,   y*t,   z*t  }; }
    Vec3 operator*(const Vec3& v)  const { return {x*v.x, y*v.y, z*v.z}; }
    Vec3 operator/(float t)        const { return *this * (1.0f / t); }
    Vec3 operator-()               const { return {-x, -y, -z}; }

    Vec3& operator+=(const Vec3& v) { x+=v.x; y+=v.y; z+=v.z; return *this; }
    Vec3& operator*=(float t)       { x*=t;   y*=t;   z*=t;   return *this; }

    float dot(const Vec3& v)  const { return x*v.x + y*v.y + z*v.z; }
    Vec3  cross(const Vec3& v) const {
        return { y*v.z - z*v.y,
                 z*v.x - x*v.z,
                 x*v.y - y*v.x };
    }

    float length2() const { return dot(*this); }
    float length()  const { return std::sqrt(length2()); }
    Vec3  normalized() const { return *this / length(); }

    bool near_zero() const {
        constexpr float eps = 1e-8f;
        return std::abs(x) < eps && std::abs(y) < eps && std::abs(z) < eps;
    }

    float  operator[](int i) const { if (i == 0) return x; if (i == 1) return y; return z; }
    float& operator[](int i)       { if (i == 0) return x; if (i == 1) return y; return z; }
};

inline Vec3 operator*(float t, const Vec3& v) { return v * t; }

// ---- Ray ---------------------------------------------------------------

struct Ray {
    Vec3 origin, dir;
    Vec3 at(float t) const { return origin + dir * t; }
};
