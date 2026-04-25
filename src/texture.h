#pragma once
#include "vec3.h"
#include <memory>
#include <string>
#include <cmath>
#include <algorithm>
#include <cstdio>

#define STB_IMAGE_IMPLEMENTATION
#include "../third_party/stb_image.h"

// ---- Interface ---------------------------------------------------------

struct Texture {
    virtual ~Texture() = default;
    virtual Vec3 value(float u, float v, const Vec3& p) const = 0;
};

// ---- Solid color -------------------------------------------------------

struct SolidColor : Texture {
    Vec3 color;
    explicit SolidColor(Vec3 color) : color(color) {}
    Vec3 value(float, float, const Vec3&) const override { return color; }
};

// ---- Checker (spatial) -------------------------------------------------

struct CheckerTexture : Texture {
    std::shared_ptr<Texture> even, odd;
    float scale;

    CheckerTexture(std::shared_ptr<Texture> even,
                   std::shared_ptr<Texture> odd, float scale = 10.0f)
        : even(std::move(even)), odd(std::move(odd)), scale(scale) {}

    CheckerTexture(Vec3 c0, Vec3 c1, float scale = 10.0f)
        : even(std::make_shared<SolidColor>(c0))
        , odd (std::make_shared<SolidColor>(c1))
        , scale(scale) {}

    Vec3 value(float u, float v, const Vec3& p) const override {
        int ix = int(std::floor(scale * p.x));
        int iy = int(std::floor(scale * p.y));
        int iz = int(std::floor(scale * p.z));
        return ((ix + iy + iz) % 2 == 0 ? even : odd)->value(u, v, p);
    }
};

// ---- Image texture -----------------------------------------------------

struct ImageTexture : Texture {
    unsigned char* data     = nullptr;
    int            width    = 0;
    int            height   = 0;

    explicit ImageTexture(const std::string& path) {
        int channels;
        data = stbi_load(path.c_str(), &width, &height, &channels, 3);
        if (!data)
            std::fprintf(stderr, "ImageTexture: could not load \"%s\"\n", path.c_str());
    }

    ~ImageTexture() { if (data) stbi_image_free(data); }

    Vec3 value(float u, float v, const Vec3&) const override {
        if (!data) return {1, 0, 1};  // magenta = texture manquante

        u = std::max(0.0f, std::min(1.0f, u));
        v = 1.0f - std::max(0.0f, std::min(1.0f, v));  // flip V (stb = Y vers le bas)

        int i = std::min(int(u * width),  width  - 1);
        int j = std::min(int(v * height), height - 1);

        const unsigned char* px = data + (j * width + i) * 3;
        return { px[0] / 255.0f, px[1] / 255.0f, px[2] / 255.0f };
    }
};

// ---- Planar (XZ) texture -----------------------------------------------
// Maps world-space XZ position → UV, tiling every `scale` world units.
// Use this on large floor spheres where spherical UV is heavily distorted.

struct PlanarTexture : Texture {
    std::shared_ptr<Texture> tex;
    float scale;

    PlanarTexture(std::shared_ptr<Texture> tex, float scale = 1.0f)
        : tex(std::move(tex)), scale(scale) {}

    Vec3 value(float, float, const Vec3& p) const override {
        float u = p.x / scale - std::floor(p.x / scale);
        float v = p.z / scale - std::floor(p.z / scale);
        return tex->value(u, v, p);
    }
};
