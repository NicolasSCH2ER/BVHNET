#pragma once
#include "vec3.h"
#include <algorithm>

struct AABB {
    Vec3 min, max;

    AABB() = default;
    AABB(Vec3 min, Vec3 max) : min(min), max(max) {}

    bool hit(const Ray& r, float t_min, float t_max) const {
        for (int a = 0; a < 3; ++a) {
            float inv_d = 1.0f / r.dir[a];
            float t0 = (min[a] - r.origin[a]) * inv_d;
            float t1 = (max[a] - r.origin[a]) * inv_d;
            if (inv_d < 0.0f) std::swap(t0, t1);
            t_min = t0 > t_min ? t0 : t_min;
            t_max = t1 < t_max ? t1 : t_max;
            if (t_max <= t_min) return false;
        }
        return true;
    }
};

inline float surface_area(const AABB& box) {
    Vec3 d = box.max - box.min;
    return 2.0f * (d.x*d.y + d.y*d.z + d.z*d.x);
}

inline AABB surrounding_box(const AABB& a, const AABB& b) {
    return {
        { std::min(a.min.x, b.min.x), std::min(a.min.y, b.min.y), std::min(a.min.z, b.min.z) },
        { std::max(a.max.x, b.max.x), std::max(a.max.y, b.max.y), std::max(a.max.z, b.max.z) }
    };
}
