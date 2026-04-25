#pragma once
#include "vec3.h"
#include <cmath>
#include <cstdlib>

inline Vec3 rand_in_disk() {
    while (true) {
        float x = static_cast<float>(std::rand()) / float(RAND_MAX) * 2.0f - 1.0f;
        float y = static_cast<float>(std::rand()) / float(RAND_MAX) * 2.0f - 1.0f;
        if (x*x + y*y < 1.0f) return {x, y, 0.0f};
    }
}

// Thin-lens camera with depth of field.
// aperture   : lens diameter — 0 = pinhole (no blur)
// focus_dist : distance from camera at which objects are perfectly sharp
struct Camera {
    Vec3  origin;
    Vec3  lower_left;
    Vec3  horizontal;
    Vec3  vertical;
    Vec3  u, v;        // camera right / up basis vectors
    float aperture;

    Camera(Vec3 from, Vec3 at, Vec3 up,
           float vfov, float aspect_ratio,
           float aperture = 0.0f, float focus_dist = 1.0f)
        : aperture(aperture)
    {
        float theta  = vfov * (3.14159265f / 180.0f);
        float half_h = std::tan(theta / 2.0f);
        float half_w = aspect_ratio * half_h;

        Vec3 w = (from - at).normalized();
        u      = up.cross(w).normalized();
        v      = w.cross(u);

        origin     = from;
        horizontal = 2.0f * half_w * focus_dist * u;
        vertical   = 2.0f * half_h * focus_dist * v;
        lower_left = from
                   - half_w * focus_dist * u
                   - half_h * focus_dist * v
                   - focus_dist * w;
    }

    Ray get_ray(float s, float t) const {
        Vec3 rd     = aperture * rand_in_disk();
        Vec3 offset = u * rd.x + v * rd.y;
        Vec3 target = lower_left + s * horizontal + t * vertical;
        return { origin + offset, (target - origin - offset).normalized() };
    }
};
