#pragma once
#include "hittable.h"
#include "material.h"
#include <memory>
#include <cmath>
#include <algorithm>

struct Triangle : Hittable {
    Vec3 v0, v1, v2;
    Vec3 n0, n1, n2;  // per-vertex normals for smooth shading
    std::shared_ptr<Material> mat;

    Triangle(Vec3 v0, Vec3 v1, Vec3 v2,
             Vec3 n0, Vec3 n1, Vec3 n2,
             std::shared_ptr<Material> mat)
        : v0(v0), v1(v1), v2(v2)
        , n0(n0), n1(n1), n2(n2)
        , mat(std::move(mat)) {}

    bool hit(const Ray& r, float t_min, float t_max, HitRecord& rec) const override {
        // Möller–Trumbore algorithm
        Vec3  e1  = v1 - v0;
        Vec3  e2  = v2 - v0;
        Vec3  h   = r.dir.cross(e2);
        float det = e1.dot(h);

        if (std::abs(det) < 1e-8f) return false;  // ray parallel to triangle

        float inv_det = 1.0f / det;
        Vec3  s = r.origin - v0;
        float u = inv_det * s.dot(h);
        if (u < 0.0f || u > 1.0f) return false;

        Vec3  q = s.cross(e1);
        float v = inv_det * r.dir.dot(q);
        if (v < 0.0f || u + v > 1.0f) return false;

        float t = inv_det * e2.dot(q);
        if (t < t_min || t > t_max) return false;

        rec.t     = t;
        rec.point = r.at(t);

        // Interpolate normal using barycentric coordinates (u, v, w)
        float w      = 1.0f - u - v;
        Vec3  normal = (w * n0 + u * n1 + v * n2).normalized();
        rec.set_face_normal(r, normal);
        rec.mat = mat.get();
        return true;
    }

    bool bounding_box(AABB& box) const override {
        Vec3 mn = {
            std::min({v0.x, v1.x, v2.x}),
            std::min({v0.y, v1.y, v2.y}),
            std::min({v0.z, v1.z, v2.z})
        };
        Vec3 mx = {
            std::max({v0.x, v1.x, v2.x}),
            std::max({v0.y, v1.y, v2.y}),
            std::max({v0.z, v1.z, v2.z})
        };
        // Small padding to avoid zero-thickness boxes on axis-aligned triangles
        constexpr float pad = 1e-4f;
        box = AABB(mn - Vec3{pad, pad, pad}, mx + Vec3{pad, pad, pad});
        return true;
    }
};
