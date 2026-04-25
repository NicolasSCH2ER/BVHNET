#pragma once
#include "hittable.h"
#include "material.h"
#include <cmath>
#include <memory>

// Maps a unit-sphere surface point to UV in [0,1]²
inline void sphere_uv(const Vec3& p, float& u, float& v) {
    float theta = std::acos(-p.y);
    float phi   = std::atan2(-p.z, p.x) + 3.14159265f;
    u = phi   / (2.0f * 3.14159265f);
    v = theta /         3.14159265f;
}

struct Sphere : Hittable {
    Vec3   center;
    float  radius;
    std::shared_ptr<Material> mat;

    Sphere(Vec3 center, float radius, std::shared_ptr<Material> mat)
        : center(center), radius(radius), mat(std::move(mat)) {}

    bool hit(const Ray& r, float t_min, float t_max, HitRecord& rec) const override {
        Vec3  oc = r.origin - center;
        float a  = r.dir.length2();
        float hb = oc.dot(r.dir);          // half b
        float c  = oc.length2() - radius * radius;
        float disc = hb*hb - a*c;

        if (disc < 0.0f) return false;

        float sqrt_d = std::sqrt(disc);
        float root   = (-hb - sqrt_d) / a;
        if (root <= t_min || root >= t_max) {
            root = (-hb + sqrt_d) / a;
            if (root <= t_min || root >= t_max) return false;
        }

        rec.t     = root;
        rec.point = r.at(root);
        Vec3 outward = (rec.point - center) / radius;
        rec.set_face_normal(r, outward);
        rec.mat   = mat.get();
        sphere_uv(outward, rec.u, rec.v);
        return true;
    }

    bool bounding_box(AABB& box) const override {
        Vec3 r{radius, radius, radius};
        box = AABB(center - r, center + r);
        return true;
    }
};
