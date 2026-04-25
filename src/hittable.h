#pragma once
#include "vec3.h"
#include "aabb.h"

struct Material;  // forward declaration

struct HitRecord {
    Vec3      point;
    Vec3      normal;      // always points against the incoming ray
    float     t;
    float     u = 0, v = 0;  // surface UV coordinates
    bool      front_face;
    Material* mat = nullptr;

    void set_face_normal(const Ray& r, const Vec3& outward_normal) {
        front_face = r.dir.dot(outward_normal) < 0.0f;
        normal     = front_face ? outward_normal : -outward_normal;
    }
};

struct Hittable {
    virtual ~Hittable() = default;
    virtual bool hit(const Ray& r, float t_min, float t_max, HitRecord& rec) const = 0;
    virtual bool bounding_box(AABB& box) const = 0;
};
