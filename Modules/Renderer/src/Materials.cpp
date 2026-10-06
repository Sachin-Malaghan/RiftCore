// Material library for the Realistic render mode.
//
// A scene node names its material ("paint", "wood_floor", "tile", ...). For
// each name the library first looks for real texture maps on disk,
//   Assets/Textures/<name>/albedo.jpg|png  normal.jpg|png  rough.jpg|png
// (Tools/fetch_assets.py downloads CC0 sets into that layout), and otherwise
// generates tileable albedo + normal maps procedurally, so the engine never
// depends on downloaded files.

#include "Materials.h"

#include <glad/glad.h>

#include <cmath>
#include <filesystem>
#include <functional>
#include <vector>

namespace RiftCore {

    namespace {

        const int N = 512;

        struct Canvas {
            std::vector<Vec3> color = std::vector<Vec3>(N * N, Vec3{ 1, 1, 1 });
            std::vector<f32>  height = std::vector<f32>(N * N, 0.5f);
        };

        f32 Hash(int x, int y, int seed) {
            u32 h = static_cast<u32>(x) * 374761393u + static_cast<u32>(y) * 668265263u +
                    static_cast<u32>(seed) * 2246822519u;
            h = (h ^ (h >> 13)) * 1274126177u;
            return static_cast<f32>((h ^ (h >> 16)) & 0xFFFFFFu) / 16777215.0f;
        }

        // Value noise that wraps every `period` cells, so textures tile.
        f32 Noise(f32 x, f32 y, int period, int seed) {
            int xi = static_cast<int>(std::floor(x)), yi = static_cast<int>(std::floor(y));
            f32 fx = x - xi, fy = y - yi;
            fx = fx * fx * (3 - 2 * fx);
            fy = fy * fy * (3 - 2 * fy);
            auto W = [&](int v) { return ((v % period) + period) % period; };
            f32 a = Hash(W(xi), W(yi), seed), b = Hash(W(xi + 1), W(yi), seed);
            f32 c = Hash(W(xi), W(yi + 1), seed), d = Hash(W(xi + 1), W(yi + 1), seed);
            return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
        }

        f32 Fbm(f32 u, f32 v, int period, int octaves, int seed, f32 stretchU = 1.0f) {
            f32 sum = 0, amp = 0.5f, norm = 0;
            for (int o = 0; o < octaves; o++) {
                int p = period << o;
                int pu = std::max(1, static_cast<int>(p * stretchU));
                // Separate periods per axis keep stretched (wood grain) noise tileable.
                int xi = static_cast<int>(std::floor(u * pu)), yi = static_cast<int>(std::floor(v * p));
                f32 fx = u * pu - xi, fy = v * p - yi;
                fx = fx * fx * (3 - 2 * fx);
                fy = fy * fy * (3 - 2 * fy);
                auto WU = [&](int k) { return ((k % pu) + pu) % pu; };
                auto WV = [&](int k) { return ((k % p) + p) % p; };
                f32 a = Hash(WU(xi), WV(yi), seed + o), b = Hash(WU(xi + 1), WV(yi), seed + o);
                f32 c = Hash(WU(xi), WV(yi + 1), seed + o), d = Hash(WU(xi + 1), WV(yi + 1), seed + o);
                f32 top = a + (b - a) * fx, bot = c + (d - c) * fx;
                sum += (top + (bot - top) * fy) * amp;
                norm += amp;
                amp *= 0.5f;
            }
            return sum / norm;
        }

        Vec3 Mix(const Vec3& a, const Vec3& b, f32 t) {
            return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t };
        }

        Vec3 Scale(const Vec3& a, f32 s) { return { a.x * s, a.y * s, a.z * s }; }

        using Painter = std::function<void(f32 u, f32 v, Vec3& color, f32& height)>;

        // Rectangular units (tiles, bricks, planks) with joints. Returns the
        // distance to the nearest joint in 0..1 and the cell id.
        f32 Cell(f32 u, f32 v, int cols, int rows, f32 rowShift, int& id) {
            f32 y = v * rows;
            int r = static_cast<int>(std::floor(y));
            f32 x = u * cols + (r % 2 ? rowShift : 0.0f);
            int c = static_cast<int>(std::floor(x));
            f32 fx = x - c, fy = y - r;
            id = (((c % cols) + cols) % cols) * 131 + r * 17;
            f32 ex = std::min(fx, 1.0f - fx) * (static_cast<f32>(N) / cols);
            f32 ey = std::min(fy, 1.0f - fy) * (static_cast<f32>(N) / rows);
            return std::min(ex, ey);               // pixels from the joint
        }

        Painter PainterFor(const std::string& name) {
            if (name == "paint") return [](f32 u, f32 v, Vec3& c, f32& h) {
                f32 n = Fbm(u, v, 8, 5, 11), fine = Fbm(u, v, 64, 3, 12);
                c = Scale({ 1, 1, 1 }, 0.90f + n * 0.07f + fine * 0.03f);
                h = 0.5f + (fine - 0.5f) * 0.35f + (n - 0.5f) * 0.15f;
            };
            if (name == "concrete") return [](f32 u, f32 v, Vec3& c, f32& h) {
                f32 n = Fbm(u, v, 4, 6, 21), sp = Fbm(u, v, 128, 2, 22);
                c = Scale({ 0.62f, 0.61f, 0.59f }, 0.78f + n * 0.36f - (sp > 0.72f ? 0.10f : 0.0f));
                h = 0.5f + (n - 0.5f) * 0.4f - (sp > 0.72f ? 0.25f : 0.0f);
            };
            if (name == "wood_floor" || name == "wood" || name == "wood_dark") {
                bool planks = name == "wood_floor";
                Vec3 a = name == "wood_dark" ? Vec3{ 0.20f, 0.12f, 0.07f } : planks ? Vec3{ 0.50f, 0.32f, 0.17f }
                                                                                      : Vec3{ 0.44f, 0.27f, 0.14f };
                Vec3 b = name == "wood_dark" ? Vec3{ 0.34f, 0.21f, 0.12f } : planks ? Vec3{ 0.74f, 0.53f, 0.32f }
                                                                                      : Vec3{ 0.63f, 0.43f, 0.25f };
                return [=](f32 u, f32 v, Vec3& c, f32& h) {
                    int id = 0;
                    f32 edge = planks ? Cell(u, v, 2, 8, 0.5f, id) : 99.0f;
                    f32 shift = planks ? Hash(id, 3, 5) : 0.0f;
                    f32 grain = Fbm(u + shift, v, 48, 4, 31 + (planks ? id % 7 : 0), 1.0f / 12.0f);
                    f32 rings = 0.5f + 0.5f * std::sin((v * 40.0f + grain * 9.0f + shift * 20.0f) * 6.2831f * 0.25f);
                    f32 t = grain * 0.7f + rings * 0.3f;
                    c = Scale(Mix(a, b, t), planks ? 0.85f + Hash(id, 9, 7) * 0.3f : 1.0f);
                    h = 0.55f + (grain - 0.5f) * 0.12f;
                    if (edge < 1.5f) { c = Scale(c, 0.45f); h = 0.2f; }
                };
            }
            if (name == "tile" || name == "bath_tile" || name == "marble") {
                bool marble = name == "marble";
                Vec3 base = name == "bath_tile" ? Vec3{ 0.62f, 0.76f, 0.82f } : marble ? Vec3{ 0.93f, 0.92f, 0.90f }
                                                                               : Vec3{ 0.84f, 0.80f, 0.72f };
                return [=](f32 u, f32 v, Vec3& c, f32& h) {
                    int id = 0;
                    f32 edge = Cell(u, v, 2, 2, 0.0f, id);
                    f32 n = Fbm(u, v, 6, 5, 41 + id);
                    c = Scale(base, 0.94f + Hash(id, 1, 3) * 0.06f + (n - 0.5f) * 0.05f);
                    if (marble) {
                        f32 w = Fbm(u, v, 3, 5, 44);
                        f32 vein = std::pow(1.0f - std::fabs(std::sin((u * 3.0f + v * 2.0f + w * 3.5f) * 3.14159f)), 14.0f);
                        c = Mix(c, { 0.52f, 0.52f, 0.55f }, vein * 0.75f);
                        c = Scale(c, 0.96f + (w - 0.5f) * 0.10f);
                    }
                    h = 0.6f;
                    if (edge < 2.0f) { c = { 0.55f, 0.54f, 0.52f }; h = 0.25f; }
                    else if (edge < 4.0f) h = 0.45f;
                };
            }
            if (name == "brick") return [](f32 u, f32 v, Vec3& c, f32& h) {
                int id = 0;
                f32 edge = Cell(u, v, 4, 12, 0.5f, id);
                f32 n = Fbm(u, v, 32, 4, 51);
                Vec3 brick = Mix({ 0.52f, 0.20f, 0.13f }, { 0.72f, 0.36f, 0.22f }, Hash(id, 2, 9));
                c = Scale(brick, 0.82f + n * 0.36f);
                h = 0.65f + (n - 0.5f) * 0.25f;
                if (edge < 3.0f) { c = Scale({ 0.72f, 0.70f, 0.66f }, 0.85f + n * 0.25f); h = 0.2f + n * 0.1f; }
            };
            if (name == "roof_tile") return [](f32 u, f32 v, Vec3& c, f32& h) {
                f32 y = v * 5.0f;
                int r = static_cast<int>(std::floor(y));
                f32 fy = y - r;
                f32 x = u * 8.0f + (r % 2 ? 0.5f : 0.0f);
                int ci = static_cast<int>(std::floor(x));
                f32 fx = x - ci;
                f32 curve = std::sin(fx * 3.14159f);                  // barrel tile profile
                f32 n = Fbm(u, v, 24, 4, 61);
                Vec3 clay = Mix({ 0.50f, 0.19f, 0.12f }, { 0.70f, 0.33f, 0.20f }, Hash(ci, r, 4));
                c = Scale(clay, (0.70f + 0.30f * curve) * (0.80f + 0.25f * (1.0f - fy)) * (0.9f + n * 0.2f));
                h = 0.25f + 0.45f * curve + 0.25f * (1.0f - fy);
                if (fy > 0.94f) { c = Scale(c, 0.45f); h = 0.1f; }
            };
            if (name == "grass") return [](f32 u, f32 v, Vec3& c, f32& h) {
                f32 a = Fbm(u, v, 6, 5, 71), b = Fbm(u, v, 96, 3, 72);
                c = Mix({ 0.13f, 0.30f, 0.09f }, { 0.36f, 0.55f, 0.18f }, a * 0.6f + b * 0.4f);
                h = b;
            };
            if (name == "paving") return [](f32 u, f32 v, Vec3& c, f32& h) {
                int id = 0;
                f32 edge = Cell(u, v, 3, 6, 0.5f, id);
                f32 n = Fbm(u, v, 48, 4, 81);
                c = Scale({ 0.62f, 0.60f, 0.57f }, 0.75f + Hash(id, 5, 2) * 0.2f + n * 0.2f);
                h = 0.6f + (n - 0.5f) * 0.3f;
                if (edge < 2.5f) { c = Scale(c, 0.5f); h = 0.15f; }
            };
            if (name == "granite") return [](f32 u, f32 v, Vec3& c, f32& h) {
                f32 a = Fbm(u, v, 128, 2, 91), b = Fbm(u, v, 24, 4, 92);
                f32 g = 0.10f + (a > 0.62f ? 0.42f : a < 0.36f ? -0.05f : 0.08f) + b * 0.10f;
                c = { g, g, g * 1.05f };
                h = 0.5f;
            };
            if (name == "fabric") return [](f32 u, f32 v, Vec3& c, f32& h) {
                f32 wx = 0.5f + 0.5f * std::sin(u * 64.0f * 6.2831f), wy = 0.5f + 0.5f * std::sin(v * 64.0f * 6.2831f);
                f32 weave = (static_cast<int>(u * 64) + static_cast<int>(v * 64)) % 2 ? wx : wy;
                f32 n = Fbm(u, v, 16, 4, 95);
                c = Scale({ 1, 1, 1 }, 0.72f + weave * 0.22f + n * 0.08f);
                h = weave;
            };
            return nullptr;
        }

        struct Preset { const char* name; f32 scale, roughness, metallic, opacity; bool tint; f32 bump; };

        // scale: metres per texture repeat. tint: multiply by the node colour
        // (paint, fabric) instead of using the texture's own colour.
        const Preset kPresets[] = {
            { "paint",      1.6f, 0.88f, 0.0f, 1.0f, true,  1.2f },
            { "concrete",   2.0f, 0.85f, 0.0f, 1.0f, false, 2.0f },
            { "wood_floor", 1.6f, 0.42f, 0.0f, 1.0f, false, 2.5f },
            { "wood",       0.9f, 0.48f, 0.0f, 1.0f, false, 1.2f },
            { "wood_dark",  0.9f, 0.45f, 0.0f, 1.0f, false, 1.2f },
            { "tile",       1.2f, 0.22f, 0.0f, 1.0f, false, 3.0f },
            { "marble",     1.6f, 0.12f, 0.0f, 1.0f, false, 2.0f },
            { "bath_tile",  0.6f, 0.18f, 0.0f, 1.0f, false, 3.0f },
            { "brick",      1.0f, 0.90f, 0.0f, 1.0f, false, 5.0f },
            { "roof_tile",  1.4f, 0.75f, 0.0f, 1.0f, false, 6.0f },
            { "grass",      2.5f, 1.00f, 0.0f, 1.0f, false, 3.0f },
            { "paving",     1.2f, 0.80f, 0.0f, 1.0f, false, 4.0f },
            { "granite",    1.0f, 0.16f, 0.0f, 1.0f, false, 0.0f },
            { "fabric",     0.3f, 1.00f, 0.0f, 1.0f, true,  2.0f },
            { "glass",      0.0f, 0.04f, 0.0f, 0.22f, true, 0.0f },
            { "mirror",     0.0f, 0.03f, 1.0f, 1.0f, true,  0.0f },
            { "metal",      0.0f, 0.30f, 1.0f, 1.0f, true,  0.0f },
            { "ceramic",    0.0f, 0.10f, 0.0f, 1.0f, true,  0.0f },
        };

        void SetupSampler(Texture2D* tex) {
            if (!tex || !tex->isValid()) return;
            glBindTexture(GL_TEXTURE_2D, tex->glID);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameterf(GL_TEXTURE_2D, 0x84FE /* GL_TEXTURE_MAX_ANISOTROPY */, 8.0f);
            glGenerateMipmap(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        Texture2D* LoadFile(TextureLoader* loader, const std::string& dir, const char* base) {
            std::error_code ec;
            for (const char* ext : { ".jpg", ".png", ".jpeg" }) {
                std::string path = dir + "/" + base + ext;
                if (!std::filesystem::exists(path, ec)) continue;
                auto r = loader->Load(path);
                if (r.IsOk()) { SetupSampler(r.Value()); return r.Value(); }
            }
            return nullptr;
        }

    } // namespace

    MaterialLibrary::MaterialLibrary(TextureLoader* loader) : loader_(loader) {}

    const MaterialDef* MaterialLibrary::Get(const String& name) {
        if (name.empty()) return nullptr;
        auto it = materials_.find(name);
        if (it != materials_.end()) return it->second.valid ? &it->second : nullptr;

        MaterialDef def;
        const Preset* preset = nullptr;
        for (auto& p : kPresets) if (name == p.name) preset = &p;
        if (preset) {
            def.valid     = true;
            def.scale     = preset->scale;
            def.roughness = preset->roughness;
            def.metallic  = preset->metallic;
            def.opacity   = preset->opacity;
            def.tint      = preset->tint;
        }

        // 1. Real texture maps on disk win.
        std::string dir = "Assets/Textures/" + name;
        std::error_code ec;
        if (loader_ && std::filesystem::is_directory(dir, ec)) {
            def.albedo = LoadFile(loader_, dir, "albedo");
            def.normal = LoadFile(loader_, dir, "normal");
            def.rough  = LoadFile(loader_, dir, "rough");
            if (def.albedo) {
                def.valid    = true;
                def.fromFile = true;
                if (def.scale <= 0.0f) def.scale = 1.5f;
                if (!preset) { def.roughness = 0.6f; def.tint = false; }
            }
        }

        // 2. Otherwise generate the maps.
        Painter paint = (!def.albedo && preset && preset->scale > 0.0f) ? PainterFor(name) : nullptr;
        if (paint && loader_) {
            Canvas cv;
            for (int y = 0; y < N; y++)
                for (int x = 0; x < N; x++)
                    paint((x + 0.5f) / N, (y + 0.5f) / N, cv.color[y * N + x], cv.height[y * N + x]);

            std::vector<u8> albedo(N * N * 4), normal(N * N * 4);
            for (int y = 0; y < N; y++) {
                for (int x = 0; x < N; x++) {
                    int i = y * N + x;
                    auto B = [](f32 v) { return static_cast<u8>(std::fmin(std::fmax(v, 0.0f), 1.0f) * 255.0f + 0.5f); };
                    albedo[i * 4 + 0] = B(cv.color[i].x);
                    albedo[i * 4 + 1] = B(cv.color[i].y);
                    albedo[i * 4 + 2] = B(cv.color[i].z);
                    albedo[i * 4 + 3] = 255;
                    auto H = [&](int px, int py) { return cv.height[((py + N) % N) * N + ((px + N) % N)]; };
                    f32 dx = (H(x + 1, y) - H(x - 1, y)) * preset->bump;
                    f32 dy = (H(x, y + 1) - H(x, y - 1)) * preset->bump;
                    f32 len = std::sqrt(dx * dx + dy * dy + 1.0f);
                    normal[i * 4 + 0] = B(-dx / len * 0.5f + 0.5f);
                    normal[i * 4 + 1] = B(-dy / len * 0.5f + 0.5f);
                    normal[i * 4 + 2] = B(1.0f / len * 0.5f + 0.5f);
                    normal[i * 4 + 3] = 255;
                }
            }
            auto a = loader_->CreateFromPixels("mat:" + name + ":albedo", albedo.data(), N, N, 4);
            auto n = loader_->CreateFromPixels("mat:" + name + ":normal", normal.data(), N, N, 4);
            if (a.IsOk()) { def.albedo = a.Value(); SetupSampler(def.albedo); }
            if (n.IsOk()) { def.normal = n.Value(); SetupSampler(def.normal); }
        }

        materials_[name] = def;
        return def.valid ? &materials_[name] : nullptr;
    }

} // namespace RiftCore
