#pragma once
#define TINYOBJLOADER_IMPLEMENTATION
#include "../third_party/tiny_obj_loader.h"

#include "triangle.h"
#include "bvh.h"

#include <string>
#include <memory>
#include <vector>
#include <cstdio>

// Loads an OBJ file and returns a BVH over its triangles.
// All triangles share the same material.
inline std::shared_ptr<BVHNode> load_obj(const std::string& path,
                                          std::shared_ptr<Material> mat,
                                          Vec3  offset = {0, 0, 0},
                                          float scale  = 1.0f) {
    tinyobj::attrib_t                attrib;
    std::vector<tinyobj::shape_t>    shapes;
    std::vector<tinyobj::material_t> materials;
    std::string err;

    bool ok = tinyobj::LoadObj(&attrib, &shapes, &materials, &err,
                               path.c_str(), nullptr, /*triangulate=*/true);
    if (!err.empty())
        std::fprintf(stderr, "OBJ: %s\n", err.c_str());
    if (!ok) return nullptr;

    std::vector<std::shared_ptr<Hittable>> tris;

    for (const auto& shape : shapes) {
        size_t idx_offset = 0;

        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f) {
            Vec3 verts[3], normals[3];
            bool has_normals = true;

            for (int i = 0; i < 3; ++i) {
                tinyobj::index_t idx = shape.mesh.indices[idx_offset + i];

                verts[i] = Vec3{
                    attrib.vertices[3 * idx.vertex_index + 0],
                    attrib.vertices[3 * idx.vertex_index + 1],
                    attrib.vertices[3 * idx.vertex_index + 2]
                } * scale + offset;

                if (idx.normal_index >= 0) {
                    normals[i] = {
                        attrib.normals[3 * idx.normal_index + 0],
                        attrib.normals[3 * idx.normal_index + 1],
                        attrib.normals[3 * idx.normal_index + 2]
                    };
                } else {
                    has_normals = false;
                }
            }

            // Fallback: flat normal from edge vectors
            if (!has_normals) {
                Vec3 n = (verts[1] - verts[0]).cross(verts[2] - verts[0]).normalized();
                normals[0] = normals[1] = normals[2] = n;
            }

            tris.push_back(std::make_shared<Triangle>(
                verts[0], verts[1], verts[2],
                normals[0], normals[1], normals[2],
                mat
            ));

            idx_offset += 3;
        }
    }

    std::printf("Loaded %zu triangles from \"%s\"\n", tris.size(), path.c_str());
    return std::make_shared<BVHNode>(tris, 0, tris.size());
}
