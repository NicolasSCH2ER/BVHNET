#pragma once
#include "hittable.h"
#include "aabb.h"
#include <vector>
#include <memory>
#include <algorithm>
#include <limits>

struct BVHNode : Hittable {
    std::shared_ptr<Hittable> left, right;
    AABB box;

    static constexpr int N_BINS = 8;

    BVHNode(std::vector<std::shared_ptr<Hittable>>& src, size_t start, size_t end) {
        size_t span = end - start;

        // Base cases
        if (span == 1) {
            src[start]->bounding_box(box);
            left = right = src[start];
            return;
        }
        if (span == 2) {
            AABB b0, b1;
            src[start  ]->bounding_box(b0);
            src[start+1]->bounding_box(b1);
            box   = surrounding_box(b0, b1);
            left  = src[start];
            right = src[start + 1];
            return;
        }

        // Bounding box of all primitives in range
        AABB total;
        src[start]->bounding_box(total);
        for (size_t i = start + 1; i < end; ++i) {
            AABB b; src[i]->bounding_box(b);
            total = surrounding_box(total, b);
        }
        box = total;

        // Centroid bounding box (used to place bins)
        Vec3 c_min{ 1e30f,  1e30f,  1e30f};
        Vec3 c_max{-1e30f, -1e30f, -1e30f};
        for (size_t i = start; i < end; ++i) {
            AABB b; src[i]->bounding_box(b);
            Vec3 c = (b.min + b.max) * 0.5f;
            for (int a = 0; a < 3; ++a) {
                c_min[a] = std::min(c_min[a], c[a]);
                c_max[a] = std::max(c_max[a], c[a]);
            }
        }

        // SAH binning: try all axes, pick split with minimum cost
        float best_cost  = std::numeric_limits<float>::infinity();
        int   best_axis  = -1;
        float best_split = 0.0f;

        struct Bin { AABB box; int count = 0; bool valid = false; };

        for (int axis = 0; axis < 3; ++axis) {
            float lo     = c_min[axis];
            float extent = c_max[axis] - lo;
            if (extent < 1e-6f) continue;

            // Fill bins
            Bin bins[N_BINS];
            for (size_t i = start; i < end; ++i) {
                AABB b; src[i]->bounding_box(b);
                Vec3 c = (b.min + b.max) * 0.5f;
                int k  = std::min(int(N_BINS * (c[axis] - lo) / extent), N_BINS - 1);
                bins[k].count++;
                bins[k].box   = bins[k].valid ? surrounding_box(bins[k].box, b) : b;
                bins[k].valid = true;
            }

            // Left prefix: cumulative SA and count
            AABB  left_box;
            bool  left_valid = false;
            int   left_count = 0;
            float left_sa [N_BINS - 1];
            int   left_cnt[N_BINS - 1];
            for (int k = 0; k < N_BINS - 1; ++k) {
                if (bins[k].valid) {
                    left_box   = left_valid ? surrounding_box(left_box, bins[k].box) : bins[k].box;
                    left_valid = true;
                }
                left_count  += bins[k].count;
                left_sa [k]  = left_valid ? surface_area(left_box) : 0.0f;
                left_cnt[k]  = left_count;
            }

            // Right suffix: cumulative SA and count — evaluate SAH cost
            AABB  right_box;
            bool  right_valid = false;
            int   right_count = 0;
            for (int k = N_BINS - 1; k > 0; --k) {
                if (bins[k].valid) {
                    right_box   = right_valid ? surrounding_box(right_box, bins[k].box) : bins[k].box;
                    right_valid = true;
                }
                right_count += bins[k].count;
                if (left_cnt[k-1] > 0 && right_count > 0) {
                    float cost = left_sa[k-1] * float(left_cnt[k-1])
                               + (right_valid ? surface_area(right_box) : 0.0f) * float(right_count);
                    if (cost < best_cost) {
                        best_cost  = cost;
                        best_axis  = axis;
                        best_split = lo + float(k) / float(N_BINS) * extent;
                    }
                }
            }
        }

        // Partition primitives at best split
        size_t mid;
        if (best_axis < 0) {
            mid = start + span / 2;  // fallback: all centroids identical
        } else {
            auto it = std::partition(src.begin() + start, src.begin() + end,
                [&](const std::shared_ptr<Hittable>& obj) {
                    AABB b; obj->bounding_box(b);
                    return (b.min[best_axis] + b.max[best_axis]) * 0.5f < best_split;
                });
            mid = size_t(it - src.begin());
            if (mid == start || mid == end)
                mid = start + span / 2;  // degenerate partition → fallback
        }

        left  = std::make_shared<BVHNode>(src, start, mid);
        right = std::make_shared<BVHNode>(src, mid,   end);
    }

    bool hit(const Ray& r, float t_min, float t_max, HitRecord& rec) const override {
        if (!box.hit(r, t_min, t_max)) return false;
        bool hit_left  = left->hit(r, t_min, t_max, rec);
        bool hit_right = right->hit(r, t_min, hit_left ? rec.t : t_max, rec);
        return hit_left || hit_right;
    }

    bool bounding_box(AABB& out) const override {
        out = box;
        return true;
    }
};
