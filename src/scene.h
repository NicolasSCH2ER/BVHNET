#pragma once
#include "hittable.h"
#include <vector>
#include <memory>

struct Scene : Hittable {
    std::vector<std::shared_ptr<Hittable>> objects;

    void add(std::shared_ptr<Hittable> obj) {
        objects.push_back(std::move(obj));
    }

    bool hit(const Ray& r, float t_min, float t_max, HitRecord& rec) const override {
        HitRecord tmp;
        bool hit_anything = false;
        float closest     = t_max;

        for (const auto& obj : objects) {
            if (obj->hit(r, t_min, closest, tmp)) {
                hit_anything = true;
                closest      = tmp.t;
                rec          = tmp;
            }
        }
        return hit_anything;
    }

    bool bounding_box(AABB& box) const override {
        if (objects.empty()) return false;
        AABB tmp;
        bool first = true;
        for (const auto& obj : objects) {
            if (!obj->bounding_box(tmp)) return false;
            box   = first ? tmp : surrounding_box(box, tmp);
            first = false;
        }
        return true;
    }
};
