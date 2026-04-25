#pragma once
#include "vec3.h"
#include "hittable.h"
#include "texture.h"
#include <cstdint>
#include <cmath>

// ---- RNG : PCG32, thread-safe ------------------------------------------
// Chaque thread possède son propre état → pas de contention OpenMP.
// seed_rand() appelé par pixel donne des séquences indépendantes.

inline thread_local uint64_t rng_state = 0x853c49e6748fea9bULL;

inline void seed_rand(uint64_t seed) {
    rng_state = seed + 1442695040888963407ULL;
    for (int i = 0; i < 8; ++i)          // warm-up
        rng_state = rng_state * 6364136223846793005ULL + 1442695040888963407ULL;
}

inline float rand_float() {
    uint64_t old = rng_state;
    rng_state    = old * 6364136223846793005ULL + 1442695040888963407ULL;
    uint32_t xorshifted = uint32_t(((old >> 18u) ^ old) >> 27u);
    uint32_t rot        = uint32_t(old >> 59u);
    uint32_t r = (xorshifted >> rot) | (xorshifted << ((~rot + 1u) & 31u));
    return float(r) * (1.0f / 4294967296.0f);
}

inline Vec3 rand_unit_sphere() {
    while (true) {
        Vec3 p = { rand_float()*2-1, rand_float()*2-1, rand_float()*2-1 };
        if (p.length2() < 1.0f) return p;
    }
}

inline Vec3 rand_unit_vector() {
    return rand_unit_sphere().normalized();
}

inline Vec3 reflect(const Vec3& v, const Vec3& n) {
    return v - 2.0f * v.dot(n) * n;
}

inline Vec3 refract(const Vec3& uv, const Vec3& n, float eta_ratio) {
    float cos_theta  = std::min((-uv).dot(n), 1.0f);
    Vec3  r_perp     = eta_ratio * (uv + cos_theta * n);
    Vec3  r_parallel = -std::sqrt(std::abs(1.0f - r_perp.length2())) * n;
    return r_perp + r_parallel;
}

inline float schlick(float cosine, float eta_ratio) {
    float r0 = (1.0f - eta_ratio) / (1.0f + eta_ratio);
    r0 = r0 * r0;
    return r0 + (1.0f - r0) * std::pow(1.0f - cosine, 5.0f);
}

// ---- GGX / Cook-Torrance helpers ----------------------------------------

// Repère orthonormé autour de n
inline void build_onb(const Vec3& n, Vec3& t, Vec3& b) {
    Vec3 up = std::abs(n.z) < 0.999f ? Vec3{0, 0, 1} : Vec3{1, 0, 0};
    t = up.cross(n).normalized();
    b = n.cross(t);
}

// GGX NDF   (a = roughness²)
inline float ggx_d(float NdotH, float a) {
    float a2 = a * a;
    float d  = NdotH * NdotH * (a2 - 1.0f) + 1.0f;
    return a2 / (3.14159265f * d * d + 1e-7f);
}

// Smith G1
inline float smith_g1(float NdotX, float a2) {
    return 2.0f * NdotX
         / (NdotX + std::sqrt(a2 + (1.0f - a2) * NdotX * NdotX) + 1e-6f);
}

// Smith G2 height-correlated   (a2 = roughness⁴)
inline float smith_g2(float NdotV, float NdotL, float a2) {
    return smith_g1(NdotV, a2) * smith_g1(NdotL, a2);
}

// Schlick Fresnel vectoriel pour F0 coloré (métaux)
inline Vec3 schlick_f(const Vec3& F0, float cos_theta) {
    float t = std::pow(1.0f - std::max(0.0f, cos_theta), 5.0f);
    return F0 + (Vec3{1, 1, 1} - F0) * t;
}

// Échantillonne une normale de microfacette depuis la distribution GGX
inline Vec3 sample_ggx(const Vec3& N, float roughness) {
    float a  = roughness * roughness;          // α = roughness²
    float r1 = rand_float(), r2 = rand_float();
    float phi       = 2.0f * 3.14159265f * r1;
    float cos_theta = std::sqrt((1.0f - r2) / (1.0f + (a * a - 1.0f) * r2 + 1e-7f));
    float sin_theta = std::sqrt(std::max(0.0f, 1.0f - cos_theta * cos_theta));
    Vec3 t, b;
    build_onb(N, t, b);
    return (sin_theta * std::cos(phi) * t
          + sin_theta * std::sin(phi) * b
          + cos_theta * N).normalized();
}

// ---- Base class --------------------------------------------------------

struct Material {
    virtual ~Material() = default;

    virtual bool scatter(const Ray& r_in, const HitRecord& rec,
                         Vec3& attenuation, Ray& scattered) const = 0;

    virtual Vec3 emitted() const { return {0, 0, 0}; }

    // Delta materials (mirror, glass) don't benefit from NEE
    virtual bool is_delta() const { return false; }

    // Évaluation du BRDF pour la NEE (wi = vers lumière, wo = vers caméra)
    virtual Vec3 brdf_eval(const Vec3& wi, const Vec3& wo,
                            const HitRecord& rec) const { return {0, 0, 0}; }
};

// ---- Lambertian (diffuse) ----------------------------------------------

struct Lambertian : Material {
    std::shared_ptr<Texture> albedo;

    explicit Lambertian(Vec3 color)
        : albedo(std::make_shared<SolidColor>(color)) {}
    explicit Lambertian(std::shared_ptr<Texture> tex)
        : albedo(std::move(tex)) {}

    bool scatter(const Ray&, const HitRecord& rec,
                 Vec3& attenuation, Ray& scattered) const override {
        Vec3 dir = rec.normal + rand_unit_vector();
        if (dir.near_zero()) dir = rec.normal;
        scattered   = { rec.point, dir.normalized() };
        attenuation = albedo->value(rec.u, rec.v, rec.point);
        return true;
    }

    Vec3 brdf_eval(const Vec3& wi, const Vec3& wo,
                   const HitRecord& rec) const override {
        if (rec.normal.dot(wi) <= 0.0f) return {0, 0, 0};
        return albedo->value(rec.u, rec.v, rec.point) / 3.14159265f;
    }
};

// ---- Mirror (perfect specular) -----------------------------------------

struct Mirror : Material {
    Vec3  albedo;
    float fuzz;
    Mirror(Vec3 albedo, float fuzz = 0.0f) : albedo(albedo), fuzz(fuzz) {}
    bool is_delta() const override { return true; }

    bool scatter(const Ray& r_in, const HitRecord& rec,
                 Vec3& attenuation, Ray& scattered) const override {
        Vec3 reflected = reflect(r_in.dir, rec.normal);
        reflected = reflected + fuzz * rand_unit_sphere();
        scattered   = { rec.point, reflected.normalized() };
        attenuation = albedo;
        return scattered.dir.dot(rec.normal) > 0.0f;
    }
};

// ---- Dielectric (glass, water, diamond) --------------------------------

struct Dielectric : Material {
    float ior;
    explicit Dielectric(float ior) : ior(ior) {}
    bool is_delta() const override { return true; }

    bool scatter(const Ray& r_in, const HitRecord& rec,
                 Vec3& attenuation, Ray& scattered) const override {
        attenuation     = {1.0f, 1.0f, 1.0f};  // glass absorbs nothing
        float eta_ratio = rec.front_face ? (1.0f / ior) : ior;

        float cos_theta = std::min((-r_in.dir).dot(rec.normal), 1.0f);
        float sin_theta = std::sqrt(1.0f - cos_theta * cos_theta);

        bool cannot_refract = eta_ratio * sin_theta > 1.0f;
        Vec3 dir;

        if (cannot_refract || schlick(cos_theta, eta_ratio) > rand_float())
            dir = reflect(r_in.dir, rec.normal);
        else
            dir = refract(r_in.dir, rec.normal, eta_ratio);

        scattered = {rec.point, dir};
        return true;
    }
};

// ---- Emissive (light source) -------------------------------------------

struct Emissive : Material {
    Vec3  color;
    float intensity;
    Emissive(Vec3 color, float intensity = 1.0f) : color(color), intensity(intensity) {}

    bool scatter(const Ray&, const HitRecord&, Vec3&, Ray&) const override {
        return false;  // light sources don't scatter
    }

    Vec3 emitted() const override { return color * intensity; }
};

// ---- PBR (Cook-Torrance microfacettes, workflow metallic/roughness) -----

struct PBR : Material {
    Vec3  base_color;
    float roughness;  // [0,1] → 0 = miroir, 1 = très rugueux
    float metallic;   // 0 = diélectrique, 1 = métal pur

    PBR(Vec3 base_color, float roughness, float metallic = 0.0f)
        : base_color(base_color)
        , roughness(std::max(0.04f, roughness))
        , metallic(std::min(1.0f, std::max(0.0f, metallic))) {}

    // Évaluation complète Cook-Torrance (pour NEE)
    Vec3 brdf_eval(const Vec3& wi, const Vec3& wo,
                   const HitRecord& rec) const override {
        Vec3  N = rec.normal;
        Vec3  H = (wi + wo).normalized();
        float NdotL = std::max(0.0f, N.dot(wi));
        float NdotV = std::max(0.0f, N.dot(wo));
        float NdotH = std::max(0.0f, N.dot(H));
        float HdotV = std::max(0.0f, H.dot(wo));
        if (NdotL <= 0.0f || NdotV <= 0.0f) return {0, 0, 0};

        float a2  = roughness * roughness * roughness * roughness;
        Vec3  F0  = Vec3{0.04f, 0.04f, 0.04f} * (1.0f - metallic)
                  + base_color * metallic;
        Vec3  F   = schlick_f(F0, HdotV);
        float D   = ggx_d(NdotH, roughness * roughness);
        float G   = smith_g2(NdotV, NdotL, a2);

        Vec3 specular = F * (D * G / std::max(1e-6f, 4.0f * NdotV * NdotL));
        Vec3 kd       = (Vec3{1, 1, 1} - F) * (1.0f - metallic);
        Vec3 diffuse  = kd * base_color / 3.14159265f;
        return diffuse + specular;
    }

    bool scatter(const Ray& r_in, const HitRecord& rec,
                 Vec3& attenuation, Ray& scattered) const override {
        Vec3  N   = rec.normal;
        Vec3  V   = -r_in.dir;
        Vec3  F0  = Vec3{0.04f, 0.04f, 0.04f} * (1.0f - metallic)
                  + base_color * metallic;
        float cosV   = std::max(0.0f, N.dot(V));
        Vec3  Fapprox = schlick_f(F0, cosV);
        float p_spec  = std::min(0.95f, std::max(0.05f,
                            std::max({Fapprox.x, Fapprox.y, Fapprox.z})));

        if (rand_float() < p_spec) {
            // Voie spéculaire : importance sampling GGX
            Vec3  H = sample_ggx(N, roughness);
            Vec3  L = reflect(-V, H);
            if (L.dot(N) <= 0.0f) return false;

            float NdotV = std::max(1e-6f, N.dot(V));
            float NdotL = std::max(1e-6f, N.dot(L));
            float NdotH = std::max(1e-6f, N.dot(H));
            float HdotV = std::max(1e-6f, H.dot(V));
            float a2    = roughness * roughness * roughness * roughness;
            Vec3  F     = schlick_f(F0, HdotV);
            float G     = smith_g2(NdotV, NdotL, a2);

            // Poids = F·G·(H·V) / (N·H · N·V) / p_spec
            attenuation = F * G * HdotV / (NdotH * NdotV + 1e-6f) / p_spec;
            scattered   = { rec.point, L.normalized() };
        } else {
            // Voie diffuse : cosine-weighted sampling
            Vec3 dir = N + rand_unit_vector();
            if (dir.near_zero()) dir = N;
            Vec3 F   = schlick_f(F0, cosV);
            Vec3 kd  = (Vec3{1, 1, 1} - F) * (1.0f - metallic);
            attenuation = kd * base_color / (1.0f - p_spec + 1e-6f);
            scattered   = { rec.point, dir.normalized() };
        }
        return true;
    }
};
