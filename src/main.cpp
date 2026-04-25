#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb_image_write.h"

#include "vec3.h"
#include "camera.h"
#include "hittable.h"
#include "sphere.h"
#include "material.h"
#include "scene.h"
#include "bvh.h"
#include "mesh.h"

#include <vector>
#include <memory>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <algorithm>

#ifdef _OPENMP
#include <omp.h>
#endif

// ---- Config ------------------------------------------------------------

constexpr int   WIDTH    = 1920;
constexpr int   HEIGHT   = 1080;
constexpr int   SAMPLES   = 1024;   // doit être un carré parfait (32²=1024, 16²=256…)
constexpr int   SQRT_SPP  = 32;     // √SAMPLES
constexpr int   MAX_DEPTH = 16;
constexpr float ASPECT   = float(WIDTH) / float(HEIGHT);

// 0 = scène principale (sol + globe + suzannes)
// 1 = Cornell box
constexpr int   SCENE    = 1;

// ---- Lights ------------------------------------------------------------

struct LightInfo {
    Vec3  center;
    float radius;
    Vec3  emission;  // color * intensity
};

// ---- Path tracing ------------------------------------------------------

Vec3 ray_color(const Ray& r, const Hittable& world,
               const std::vector<LightInfo>& lights,
               const Vec3& background,
               int depth, bool include_emitted = true) {
    if (depth <= 0) return {0, 0, 0};

    HitRecord rec;
    if (!world.hit(r, 1e-4f, 1e30f, rec))
        return background;

    Vec3 Lo{0, 0, 0};

    // Emitted — skipped after diffuse bounce to avoid double-counting with NEE
    if (include_emitted)
        Lo += rec.mat->emitted();

    Ray  scattered;
    Vec3 attenuation;
    if (!rec.mat->scatter(r, rec, attenuation, scattered))
        return Lo;

    bool delta = rec.mat->is_delta();

    // ---- Next Event Estimation (diffuse surfaces only) -----------------
    if (!delta && !lights.empty()) {
        // Pick one light at random
        const LightInfo& li = lights[int(rand_float() * float(lights.size())) % lights.size()];

        Vec3  lp      = li.center + li.radius * rand_unit_vector();
        Vec3  to_l    = lp - rec.point;
        float dist    = to_l.length();
        Vec3  dir_l   = to_l / dist;
        float cos_x   = rec.normal.dot(dir_l);

        if (cos_x > 0.0f) {
            HitRecord sh;
            if (!world.hit(Ray{rec.point, dir_l}, 1e-4f, dist - 1e-3f, sh)) {
                Vec3  ln    = (lp - li.center) / li.radius;
                float cos_l = std::max(0.0f, (-dir_l).dot(ln));
                float area  = 4.0f * 3.14159265f * li.radius * li.radius;
                float n_li  = float(lights.size());
                Vec3  brdf  = rec.mat->brdf_eval(dir_l, -r.dir, rec);
                Lo += brdf * li.emission * cos_x * cos_l * area / (dist * dist) * n_li;
            }
        }
    }

    // ---- Russian Roulette ----------------------------------------------
    float rr = std::min(0.95f, std::max({attenuation.x, attenuation.y, attenuation.z}));
    if (rand_float() > rr) return Lo;
    attenuation = attenuation / rr;

    // Specular bounces can still see emitted; diffuse can't (NEE counted it)
    return Lo + attenuation * ray_color(scattered, world, lights, background, depth - 1, delta);
}

// ---- Tone mapping (ACES filmic) ----------------------------------------

inline float to_srgb(float x) {
    x = std::max(0.0f, std::min(1.0f, x));
    return x < 0.0031308f ? 12.92f * x : 1.055f * std::pow(x, 1.0f / 2.2f) - 0.055f;
}

inline Vec3 aces(Vec3 x) {
    // Narkowicz 2015 ACES fitted curve — applied per channel
    constexpr float a = 2.51f, b = 0.03f, c = 2.43f, d = 0.59f, e = 0.14f;
    auto f = [&](float v) {
        return std::max(0.f, std::min(1.f, (v * (a * v + b)) / (v * (c * v + d) + e)));
    };
    return {f(x.x), f(x.y), f(x.z)};
}

// ---- Scene setup -------------------------------------------------------

Scene build_scene() {
    Scene scene;

    auto checker  = std::make_shared<CheckerTexture>(
                        Vec3{0.85f, 0.85f, 0.85f}, Vec3{0.15f, 0.15f, 0.15f}, 6.0f);
    auto ground   = std::make_shared<Lambertian>(checker);
    auto light    = std::make_shared<Emissive>(Vec3{1.0f, 0.95f, 0.8f}, 8.0f);
    auto tex      = std::make_shared<ImageTexture>("earth.jpg");
    auto earth    = std::make_shared<Lambertian>(tex);
    scene.add(std::make_shared<Sphere>(Vec3{ 0.0f, -100.5f, -1.0f}, 100.0f, ground));
    scene.add(std::make_shared<Sphere>(Vec3{ 0.0f,    2.5f, -1.2f},   0.8f, light));
    scene.add(std::make_shared<Sphere>(Vec3{ 0.0f,    0.0f, -1.2f},   0.5f, earth));

    return scene;
}

// ---- Cornell box -------------------------------------------------------
//
// Cube unitaire [0,1]³, ouvert côté caméra (z>1).
//   x=0 rouge  |  x=1 vert  |  y=0 sol  |  y=1 plafond  |  z=0 fond blanc
// Sphère lumineuse au plafond (NEE-compatible) + sphère diffuse + sphère verre.

Scene build_cornell_box() {
    Scene scene;

    auto white = std::make_shared<Lambertian>(Vec3{0.73f, 0.73f, 0.73f});
    auto red   = std::make_shared<Lambertian>(Vec3{0.65f, 0.05f, 0.05f});
    auto green = std::make_shared<Lambertian>(Vec3{0.12f, 0.45f, 0.15f});
    auto light = std::make_shared<Emissive> (Vec3{1.0f,  0.95f, 0.8f }, 15.0f);
    auto glass = std::make_shared<Dielectric>(1.5f);

    // Quad = 2 triangles, normale constante par face
    auto add_quad = [&](Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec3 n,
                        std::shared_ptr<Material> mat) {
        scene.add(std::make_shared<Triangle>(a, b, c, n, n, n, mat));
        scene.add(std::make_shared<Triangle>(a, c, d, n, n, n, mat));
    };

    add_quad({0,0,0},{1,0,0},{1,0,1},{0,0,1}, { 0, 1, 0}, white); // sol
    add_quad({0,1,1},{1,1,1},{1,1,0},{0,1,0}, { 0,-1, 0}, white); // plafond
    add_quad({0,0,0},{0,1,0},{1,1,0},{1,0,0}, { 0, 0, 1}, white); // fond
    add_quad({0,0,1},{0,1,1},{0,1,0},{0,0,0}, { 1, 0, 0}, red);   // gauche
    add_quad({1,0,0},{1,1,0},{1,1,1},{1,0,1}, {-1, 0, 0}, green); // droite

    scene.add(std::make_shared<Sphere>(Vec3{0.5f, 0.85f, 0.5f}, 0.15f, light));

    // PBR : or métallique poli + plastique rouge rugueux
    auto gold    = std::make_shared<PBR>(Vec3{1.0f, 0.76f, 0.33f}, 0.15f, 1.0f);
    auto plastic = std::make_shared<PBR>(Vec3{0.8f, 0.1f, 0.1f},  0.6f,  0.0f);
    scene.add(std::make_shared<Sphere>(Vec3{0.27f, 0.2f, 0.35f}, 0.2f, gold));
    scene.add(std::make_shared<Sphere>(Vec3{0.73f, 0.2f, 0.65f}, 0.2f, plastic));

    return scene;
}

// ---- Main --------------------------------------------------------------

int main() {

    Scene tmp;
    std::vector<LightInfo> lights;
    Vec3 background{0, 0, 0};
    Camera cam = [&]() -> Camera {
        if constexpr (SCENE == 1) {
            // ---- Cornell box -------------------------------------------
            tmp        = build_cornell_box();
            background = {0, 0, 0};  // scène fermée : pas de ciel
            lights = { { Vec3{0.5f, 0.85f, 0.5f}, 0.15f,
                         Vec3{1.0f, 0.95f, 0.8f} * 15.0f } };
            return Camera(
                Vec3{0.5f, 0.5f, 2.2f},
                Vec3{0.5f, 0.5f, 0.0f},
                Vec3{0, 1, 0},
                40.0f, ASPECT, 0.0f, 2.2f);
        } else {
            // ---- Scène principale --------------------------------------
            tmp = build_scene();
            auto mat   = std::make_shared<Mirror>(Vec3{0.9f, 0.9f, 0.9f}, 0.0f);
            Vec3  pos   = {0.0f, 0.0f, -1.2f};
            float scale = 0.5f;
            auto mesh  = load_obj("./items/Suzanne_5.obj", mat, pos - Vec3{1.5f,0,0}, scale);
            auto mesh2 = load_obj("./items/Suzanne_4.obj", mat, pos + Vec3{1.5f,0,0}, scale);
            if (mesh)  tmp.add(mesh);
            if (mesh2) tmp.add(mesh2);
            background = {0.5f, 0.7f, 1.0f};  // ciel bleu pour scène extérieure
            lights = { { Vec3{0.0f, 2.5f, -1.2f}, 0.8f,
                         Vec3{1.0f, 0.95f, 0.8f} * 8.0f } };
            return Camera(
                Vec3{0, 1.5f, 3.0f},
                Vec3{0, 0,   -1.0f},
                Vec3{0, 1,    0   },
                60.0f, ASPECT, 0.0f, 4.5f);
        }
    }();

    BVHNode world(tmp.objects, 0, tmp.objects.size());

    std::vector<uint8_t> pixels(WIDTH * HEIGHT * 3);

    std::printf("Rendering %dx%d @ %d spp...\n", WIDTH, HEIGHT, SAMPLES);

    auto t_start = std::chrono::steady_clock::now();
    std::atomic<int> remaining{HEIGHT};

    #pragma omp parallel for schedule(dynamic, 1)
    for (int j = HEIGHT - 1; j >= 0; --j) {
        for (int i = 0; i < WIDTH; ++i) {
            seed_rand(uint64_t(j) * WIDTH + uint64_t(i));
            Vec3 color{0, 0, 0};

            // Stratified sampling : grille SQRT_SPP × SQRT_SPP par pixel
            // Chaque cellule reçoit exactement un sample → distribution uniforme
            for (int sy = 0; sy < SQRT_SPP; ++sy) {
                for (int sx = 0; sx < SQRT_SPP; ++sx) {
                    float u = (i + (sx + rand_float()) / SQRT_SPP) / (WIDTH  - 1);
                    float v = (j + (sy + rand_float()) / SQRT_SPP) / (HEIGHT - 1);
                    Ray r = cam.get_ray(u, v);
                    color += ray_color(r, world, lights, background, MAX_DEPTH);
                }
            }

            color = color / float(SAMPLES);

            // ACES tone mapping + sRGB gamma
            Vec3 mapped = aces(color);
            int row = HEIGHT - 1 - j;
            int idx = (row * WIDTH + i) * 3;
            pixels[idx + 0] = uint8_t(to_srgb(mapped.x) * 255.99f);
            pixels[idx + 1] = uint8_t(to_srgb(mapped.y) * 255.99f);
            pixels[idx + 2] = uint8_t(to_srgb(mapped.z) * 255.99f);
        }

        std::printf("\rScanlines remaining: %4d", --remaining);
        std::fflush(stdout);
    }

    auto t_end = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(t_end - t_start).count();
    std::printf("\nRender time : %.2f s\n", elapsed);

    std::printf("Writing output.png...\n");
    stbi_write_png("output.png", WIDTH, HEIGHT, 3, pixels.data(), WIDTH * 3);
    std::printf("Done.\n");
    return 0;
}
