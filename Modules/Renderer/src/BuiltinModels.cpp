// Built-in model library: basic shapes and ready-made props, generated
// procedurally so the engine ships useful content without asset files.
// Every model fits a unit cube centred on the origin (Y up), so a node's
// scale is its size in metres. Names are listed in Scene/ModelCatalog.h.

#include <Renderer/RenderTypes.h>
#include <Renderer/Camera.h>
#include <RiftCore/Scene/ModelCatalog.h>

#include <cmath>
#include <functional>
#include <unordered_map>
#include <vector>

namespace RiftCore {

    namespace {

        const f32 kPi = 3.14159265358979f;

        struct P2 { f32 r, y; };          // lathe profile point

        Vec3 Sub(const Vec3& a, const Vec3& b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }

        // Counter-clockwise triangle / quad with a flat normal.
        void Tri(MeshData& m, Vec3 a, Vec3 b, Vec3 c, Vec3 col) {
            Vec3 n = Math::Normalize(Math::Cross(Sub(b, a), Sub(c, a)));
            u32 base = static_cast<u32>(m.vertices.size());
            m.vertices.push_back({a, n, {0, 0},       col});
            m.vertices.push_back({b, n, {1, 0},       col});
            m.vertices.push_back({c, n, {0.5f, 1.0f}, col});
            m.indices.insert(m.indices.end(), {base, base + 1, base + 2});
        }

        void Quad(MeshData& m, Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec3 col) {
            Vec3 n = Math::Normalize(Math::Cross(Sub(b, a), Sub(c, a)));
            u32 base = static_cast<u32>(m.vertices.size());
            m.vertices.push_back({a, n, {0, 0}, col});
            m.vertices.push_back({b, n, {1, 0}, col});
            m.vertices.push_back({c, n, {1, 1}, col});
            m.vertices.push_back({d, n, {0, 1}, col});
            m.indices.insert(m.indices.end(),
                {base, base + 1, base + 2, base, base + 2, base + 3});
        }

        void Box(MeshData& m, Vec3 c, Vec3 size, Vec3 col) {
            f32 x = size.x * 0.5f, y = size.y * 0.5f, z = size.z * 0.5f;
            auto P = [&](f32 sx, f32 sy, f32 sz) {
                return Vec3{c.x + sx * x, c.y + sy * y, c.z + sz * z};
            };
            Quad(m, P(-1,-1, 1), P( 1,-1, 1), P( 1, 1, 1), P(-1, 1, 1), col);   // +Z
            Quad(m, P( 1,-1,-1), P(-1,-1,-1), P(-1, 1,-1), P( 1, 1,-1), col);   // -Z
            Quad(m, P(-1,-1,-1), P(-1,-1, 1), P(-1, 1, 1), P(-1, 1,-1), col);   // -X
            Quad(m, P( 1,-1, 1), P( 1,-1,-1), P( 1, 1,-1), P( 1, 1, 1), col);   // +X
            Quad(m, P(-1, 1, 1), P( 1, 1, 1), P( 1, 1,-1), P(-1, 1,-1), col);   // +Y
            Quad(m, P(-1,-1,-1), P( 1,-1,-1), P( 1,-1, 1), P(-1,-1, 1), col);   // -Y
        }

        // Maps geometry built around the Y axis onto the X or Z axis
        // (proper rotations, so winding is preserved).
        Vec3 AxisMap(const Vec3& v, int axis) {
            if (axis == 0) return { v.y, -v.x, v.z };
            if (axis == 2) return { v.x, -v.z, v.y };
            return v;
        }

        // Surface of revolution around an axis. The profile runs so that
        // "outside" is on its right: bottom centre -> rim -> top centre
        // gives a closed solid. smooth=false keeps hard edges between
        // profile segments (cylinders, cones); true blends them (spheres).
        void Lathe(MeshData& m, const std::vector<P2>& pts, u32 slices, Vec3 col,
                   bool smooth = false, Vec3 off = {0, 0, 0}, int axis = 1)
        {
            const size_t n = pts.size();
            if (n < 2) return;

            std::vector<P2> segN(n - 1);
            for (size_t k = 0; k + 1 < n; k++) {
                f32 dr = pts[k + 1].r - pts[k].r, dy = pts[k + 1].y - pts[k].y;
                f32 len = std::sqrt(dr * dr + dy * dy);
                if (len < 1e-6f) len = 1.0f;
                segN[k] = { dy / len, -dr / len };
            }
            auto pointNormal = [&](size_t k) {
                P2 a = segN[k > 0 ? k - 1 : 0];
                P2 b = segN[k < n - 1 ? k : n - 2];
                f32 r = a.r + b.r, y = a.y + b.y;
                f32 len = std::sqrt(r * r + y * y);
                if (len < 1e-6f) return b;
                return P2{ r / len, y / len };
            };

            for (size_t k = 0; k + 1 < n; k++) {
                P2 n0 = smooth ? pointNormal(k)     : segN[k];
                P2 n1 = smooth ? pointNormal(k + 1) : segN[k];
                u32 base = static_cast<u32>(m.vertices.size());
                for (u32 j = 0; j <= slices; j++) {
                    f32 t = 2.0f * kPi * j / slices;
                    f32 ct = std::cos(t), st = std::sin(t);
                    f32 u = static_cast<f32>(j) / slices;
                    for (int e = 0; e < 2; e++) {
                        const P2& p  = pts[k + e];
                        const P2& nn = e ? n1 : n0;
                        Vertex3D v;
                        Vec3 pos = AxisMap({ p.r * ct, p.y, p.r * st }, axis);
                        v.position = { pos.x + off.x, pos.y + off.y, pos.z + off.z };
                        v.normal   = AxisMap({ nn.r * ct, nn.y, nn.r * st }, axis);
                        v.texCoord = { u, static_cast<f32>(k + e) / (n - 1) };
                        v.color    = col;
                        m.vertices.push_back(v);
                    }
                }
                for (u32 j = 0; j < slices; j++) {
                    u32 a = base + j * 2, c = a + 1, b = a + 2, d = a + 3;
                    m.indices.insert(m.indices.end(), {a, c, b, b, c, d});
                }
            }
        }

        // Ellipsoid.
        void Ball(MeshData& m, Vec3 c, Vec3 radii, Vec3 col,
                  u32 stacks = 16, u32 slices = 24)
        {
            u32 base = static_cast<u32>(m.vertices.size());
            for (u32 i = 0; i <= stacks; i++) {
                f32 phi = -0.5f * kPi + kPi * i / stacks;
                for (u32 j = 0; j <= slices; j++) {
                    f32 t = 2.0f * kPi * j / slices;
                    Vec3 d = { std::cos(phi) * std::cos(t), std::sin(phi),
                               std::cos(phi) * std::sin(t) };
                    Vertex3D v;
                    v.position = { c.x + d.x * radii.x, c.y + d.y * radii.y,
                                   c.z + d.z * radii.z };
                    v.normal   = Math::Normalize(
                        { d.x / radii.x, d.y / radii.y, d.z / radii.z });
                    v.texCoord = { static_cast<f32>(j) / slices,
                                   static_cast<f32>(i) / stacks };
                    v.color    = col;
                    m.vertices.push_back(v);
                }
            }
            u32 w = slices + 1;
            for (u32 i = 0; i < stacks; i++) {
                for (u32 j = 0; j < slices; j++) {
                    u32 a = base + i * w + j, b = a + 1, cc = a + w, d = cc + 1;
                    m.indices.insert(m.indices.end(), {a, cc, b, b, cc, d});
                }
            }
        }

        void Torus(MeshData& m, f32 R, f32 r, Vec3 col, u32 rings = 32, u32 sides = 16) {
            u32 base = static_cast<u32>(m.vertices.size());
            for (u32 i = 0; i <= rings; i++) {
                f32 t = 2.0f * kPi * i / rings;
                for (u32 j = 0; j <= sides; j++) {
                    f32 p = 2.0f * kPi * j / sides;
                    Vertex3D v;
                    v.normal   = { std::cos(p) * std::cos(t), std::sin(p),
                                   std::cos(p) * std::sin(t) };
                    v.position = { (R + r * std::cos(p)) * std::cos(t), r * std::sin(p),
                                   (R + r * std::cos(p)) * std::sin(t) };
                    v.texCoord = { static_cast<f32>(i) / rings,
                                   static_cast<f32>(j) / sides };
                    v.color    = col;
                    m.vertices.push_back(v);
                }
            }
            u32 w = sides + 1;
            for (u32 i = 0; i < rings; i++) {
                for (u32 j = 0; j < sides; j++) {
                    u32 a = base + i * w + j, c = a + 1, b = a + w, d = b + 1;
                    m.indices.insert(m.indices.end(), {a, c, b, b, c, d});
                }
            }
        }

        // Gable roof / triangular prism, ridge along X.
        void Gable(MeshData& m, Vec3 c, Vec3 size, Vec3 col) {
            f32 x = size.x * 0.5f, y = size.y * 0.5f, z = size.z * 0.5f;
            Vec3 A{c.x - x, c.y - y, c.z + z}, B{c.x + x, c.y - y, c.z + z};
            Vec3 C{c.x + x, c.y - y, c.z - z}, D{c.x - x, c.y - y, c.z - z};
            Vec3 E{c.x - x, c.y + y, c.z},     F{c.x + x, c.y + y, c.z};
            Quad(m, A, B, F, E, col);
            Quad(m, C, D, E, F, col);
            Tri (m, D, A, E, col);
            Tri (m, B, C, F, col);
            Quad(m, D, C, B, A, col);
        }

        void Wedge(MeshData& m, Vec3 c, Vec3 size, Vec3 col) {
            f32 x = size.x * 0.5f, y = size.y * 0.5f, z = size.z * 0.5f;
            Vec3 A{c.x - x, c.y - y, c.z + z}, B{c.x + x, c.y - y, c.z + z};
            Vec3 C{c.x + x, c.y - y, c.z - z}, D{c.x - x, c.y - y, c.z - z};
            Vec3 E{c.x - x, c.y + y, c.z - z}, F{c.x + x, c.y + y, c.z - z};
            Quad(m, A, B, F, E, col);
            Quad(m, C, D, E, F, col);
            Tri (m, D, A, E, col);
            Tri (m, B, C, F, col);
            Quad(m, D, C, B, A, col);
        }

        void Pyramid(MeshData& m, Vec3 c, Vec3 size, Vec3 col) {
            f32 x = size.x * 0.5f, y = size.y * 0.5f, z = size.z * 0.5f;
            Vec3 A{c.x - x, c.y - y, c.z + z}, B{c.x + x, c.y - y, c.z + z};
            Vec3 C{c.x + x, c.y - y, c.z - z}, D{c.x - x, c.y - y, c.z - z};
            Vec3 P{c.x, c.y + y, c.z};
            Tri (m, A, B, P, col);
            Tri (m, B, C, P, col);
            Tri (m, C, D, P, col);
            Tri (m, D, A, P, col);
            Quad(m, D, C, B, A, col);
        }

        std::vector<P2> Shift(std::vector<P2> p, f32 dy) {
            for (auto& q : p) q.y += dy;
            return p;
        }

        std::vector<P2> CylinderProfile(f32 r, f32 y0, f32 y1) {
            return { {0, y0}, {r, y0}, {r, y1}, {0, y1} };
        }

        std::vector<P2> ConeProfile(f32 r, f32 y0, f32 y1) {
            return { {0, y0}, {r, y0}, {0, y1} };
        }

        // Palette for the props.
        const Vec3 kWhite  {1.00f, 1.00f, 1.00f};
        const Vec3 kWood   {0.55f, 0.36f, 0.20f};
        const Vec3 kDarkWd {0.36f, 0.23f, 0.13f};
        const Vec3 kLeaf   {0.20f, 0.52f, 0.24f};
        const Vec3 kLeaf2  {0.28f, 0.62f, 0.26f};
        const Vec3 kStone  {0.58f, 0.58f, 0.60f};
        const Vec3 kDark   {0.14f, 0.14f, 0.16f};
        const Vec3 kMetal  {0.70f, 0.72f, 0.76f};
        const Vec3 kRed    {0.80f, 0.16f, 0.14f};
        const Vec3 kBlue   {0.18f, 0.36f, 0.78f};
        const Vec3 kGlass  {0.55f, 0.78f, 0.92f};
        const Vec3 kCream  {0.93f, 0.88f, 0.76f};
        const Vec3 kGold   {0.90f, 0.72f, 0.22f};
        const Vec3 kGlow   {1.00f, 0.95f, 0.65f};

        using Builder = std::function<void(MeshData&)>;

        const std::unordered_map<std::string, Builder>& Builders() {
            static const std::unordered_map<std::string, Builder> k = {
                // ---- Shapes --------------------------------------------
                { "primitive:cube", [](MeshData& m) { Box(m, {0,0,0}, {1,1,1}, kWhite); } },
                { "primitive:sphere", [](MeshData& m) {
                    Ball(m, {0,0,0}, {0.5f,0.5f,0.5f}, kWhite, 24, 32); } },
                { "primitive:plane", [](MeshData& m) {
                    Quad(m, {-0.5f,0,0.5f}, {0.5f,0,0.5f}, {0.5f,0,-0.5f}, {-0.5f,0,-0.5f}, kWhite);
                    Quad(m, {-0.5f,0,-0.5f}, {0.5f,0,-0.5f}, {0.5f,0,0.5f}, {-0.5f,0,0.5f}, kWhite); } },
                { "primitive:cylinder", [](MeshData& m) {
                    Lathe(m, CylinderProfile(0.5f, -0.5f, 0.5f), 32, kWhite); } },
                { "primitive:cone", [](MeshData& m) {
                    Lathe(m, ConeProfile(0.5f, -0.5f, 0.5f), 32, kWhite); } },
                { "primitive:capsule", [](MeshData& m) {
                    std::vector<P2> p;
                    const f32 r = 0.25f, h = 0.25f;
                    for (int i = 0; i <= 8; i++) {
                        f32 a = -0.5f * kPi + 0.5f * kPi * i / 8;
                        p.push_back({ r * std::cos(a), -h + r * std::sin(a) });
                    }
                    for (int i = 0; i <= 8; i++) {
                        f32 a = 0.5f * kPi * i / 8;
                        p.push_back({ r * std::cos(a), h + r * std::sin(a) });
                    }
                    Lathe(m, p, 24, kWhite, true); } },
                { "primitive:torus", [](MeshData& m) { Torus(m, 0.375f, 0.125f, kWhite); } },
                { "primitive:pyramid", [](MeshData& m) { Pyramid(m, {0,0,0}, {1,1,1}, kWhite); } },
                { "primitive:wedge", [](MeshData& m) { Wedge(m, {0,0,0}, {1,1,1}, kWhite); } },
                { "primitive:tube", [](MeshData& m) {
                    Lathe(m, { {0.35f,-0.5f}, {0.5f,-0.5f}, {0.5f,0.5f},
                               {0.35f,0.5f}, {0.35f,-0.5f} }, 32, kWhite); } },
                { "primitive:hemisphere", [](MeshData& m) {
                    std::vector<P2> p = { {0, -0.25f} };
                    for (int i = 0; i <= 12; i++) {
                        f32 a = 0.5f * kPi * i / 12;
                        p.push_back({ 0.5f * std::cos(a), -0.25f + 0.5f * std::sin(a) });
                    }
                    Lathe(m, { p[0], p[1] }, 32, kWhite);
                    Lathe(m, std::vector<P2>(p.begin() + 1, p.end()), 32, kWhite, true); } },
                { "primitive:disc", [](MeshData& m) {
                    Lathe(m, CylinderProfile(0.5f, -0.05f, 0.05f), 40, kWhite); } },
                { "primitive:stairs", [](MeshData& m) {
                    for (int k = 0; k < 4; k++) {
                        f32 h = 0.25f * (k + 1);
                        Box(m, {0, -0.5f + h * 0.5f, 0.375f - 0.25f * k}, {1, h, 0.25f}, kWhite);
                    } } },
                { "primitive:gem", [](MeshData& m) {
                    Lathe(m, { {0,-0.5f}, {0.4f,0.1f}, {0.22f,0.5f}, {0,0.5f} }, 8, kWhite); } },

                // ---- Props ---------------------------------------------
                { "model:tree_pine", [](MeshData& m) {
                    Lathe(m, CylinderProfile(0.06f, -0.5f, -0.22f), 10, kDarkWd);
                    Lathe(m, ConeProfile(0.34f, -0.28f, 0.08f), 14, kLeaf);
                    Lathe(m, ConeProfile(0.27f, -0.06f, 0.30f), 14, kLeaf2);
                    Lathe(m, ConeProfile(0.19f,  0.16f, 0.50f), 14, kLeaf); } },
                { "model:tree_round", [](MeshData& m) {
                    Lathe(m, CylinderProfile(0.07f, -0.5f, -0.05f), 10, kDarkWd);
                    Ball(m, {0, 0.15f, 0},        {0.34f, 0.34f, 0.34f}, kLeaf,  10, 14);
                    Ball(m, {0.16f, 0.0f, 0.08f}, {0.20f, 0.18f, 0.20f}, kLeaf2, 8, 12);
                    Ball(m, {-0.15f, 0.02f, -0.1f}, {0.20f, 0.18f, 0.20f}, kLeaf2, 8, 12); } },
                { "model:rock", [](MeshData& m) {
                    Ball(m, {0, -0.06f, 0},         {0.48f, 0.36f, 0.40f}, kStone, 5, 7);
                    Ball(m, {0.2f, -0.2f, 0.18f},   {0.26f, 0.22f, 0.24f}, {0.5f,0.5f,0.52f}, 4, 6);
                    Ball(m, {-0.22f, -0.22f, -0.12f}, {0.24f, 0.2f, 0.26f}, {0.64f,0.64f,0.66f}, 4, 6); } },
                { "model:table", [](MeshData& m) {
                    Box(m, {0, 0.45f, 0}, {1, 0.1f, 0.6f}, kWood);
                    for (int sx = -1; sx <= 1; sx += 2)
                        for (int sz = -1; sz <= 1; sz += 2)
                            Box(m, {0.44f * sx, -0.05f, 0.24f * sz}, {0.08f, 0.9f, 0.08f}, kDarkWd); } },
                { "model:chair", [](MeshData& m) {
                    Box(m, {0, -0.05f, 0}, {0.5f, 0.07f, 0.5f}, kWood);
                    for (int sx = -1; sx <= 1; sx += 2)
                        for (int sz = -1; sz <= 1; sz += 2)
                            Box(m, {0.21f * sx, -0.29f, 0.21f * sz}, {0.06f, 0.42f, 0.06f}, kDarkWd);
                    Box(m, {0, 0.24f, -0.22f}, {0.5f, 0.52f, 0.06f}, kWood); } },
                { "model:barrel", [](MeshData& m) {
                    Lathe(m, { {0,-0.5f}, {0.30f,-0.5f}, {0.38f,-0.2f}, {0.38f,0.2f},
                               {0.30f,0.5f}, {0,0.5f} }, 20, kWood);
                    Lathe(m, { {0.385f,-0.30f}, {0.40f,-0.30f}, {0.40f,-0.24f}, {0.385f,-0.24f} }, 20, kDark);
                    Lathe(m, { {0.385f, 0.24f}, {0.40f, 0.24f}, {0.40f, 0.30f}, {0.385f, 0.30f} }, 20, kDark); } },
                { "model:crate", [](MeshData& m) {
                    Box(m, {0,0,0}, {0.94f, 0.94f, 0.94f}, kWood);
                    for (int a = -1; a <= 1; a += 2)
                        for (int b = -1; b <= 1; b += 2) {
                            Box(m, {0, 0.45f * a, 0.45f * b}, {1, 0.1f, 0.1f}, kDarkWd);
                            Box(m, {0.45f * a, 0, 0.45f * b}, {0.1f, 1, 0.1f}, kDarkWd);
                            Box(m, {0.45f * a, 0.45f * b, 0}, {0.1f, 0.1f, 1}, kDarkWd);
                        } } },
                { "model:lamp_post", [](MeshData& m) {
                    Lathe(m, CylinderProfile(0.09f, -0.5f, -0.44f), 12, kDark);
                    Lathe(m, CylinderProfile(0.03f, -0.44f, 0.42f), 10, kDark);
                    Box(m, {0.12f, 0.42f, 0}, {0.3f, 0.04f, 0.04f}, kDark);
                    Ball(m, {0.25f, 0.36f, 0}, {0.07f, 0.07f, 0.07f}, kGlow, 8, 12); } },
                { "model:fence", [](MeshData& m) {
                    Box(m, {0,  0.15f, 0}, {1, 0.06f, 0.04f}, kWood);
                    Box(m, {0, -0.20f, 0}, {1, 0.06f, 0.04f}, kWood);
                    for (int i = -2; i <= 2; i++)
                        Box(m, {0.22f * i, -0.05f, 0.03f}, {0.09f, 0.9f, 0.03f}, kCream); } },
                { "model:rocket", [](MeshData& m) {
                    Lathe(m, CylinderProfile(0.15f, -0.3f, 0.2f), 20, kWhite);
                    Lathe(m, ConeProfile(0.15f, 0.2f, 0.5f), 20, kRed);
                    Lathe(m, { {0,-0.5f}, {0.11f,-0.5f}, {0.06f,-0.3f}, {0,-0.3f} }, 16, kDark);
                    Lathe(m, { {0.151f,-0.02f}, {0.156f,-0.02f}, {0.156f,0.04f}, {0.151f,0.04f} }, 20, kBlue);
                    for (int s = -1; s <= 1; s += 2) {
                        Wedge(m, {0.21f * s, -0.3f, 0}, {0.02f, 0.26f, 0.02f}, kRed);
                        Box  (m, {0.21f * s, -0.33f, 0}, {0.13f, 0.2f, 0.02f}, kRed);
                        Box  (m, {0, -0.33f, 0.21f * s}, {0.02f, 0.2f, 0.13f}, kRed);
                    } } },
                { "model:car", [](MeshData& m) {
                    Box(m, {0, -0.07f, 0}, {1, 0.2f, 0.44f}, kRed);
                    Box(m, {-0.04f, 0.11f, 0}, {0.5f, 0.17f, 0.4f}, kGlass);
                    Box(m, {-0.04f, 0.2f, 0}, {0.52f, 0.02f, 0.42f}, kRed);
                    Box(m, {0.5f, -0.05f, 0.15f},  {0.02f, 0.06f, 0.08f}, kGlow);
                    Box(m, {0.5f, -0.05f, -0.15f}, {0.02f, 0.06f, 0.08f}, kGlow);
                    for (int sx = -1; sx <= 1; sx += 2)
                        for (int sz = -1; sz <= 1; sz += 2)
                            Lathe(m, CylinderProfile(0.11f, -0.04f, 0.04f), 16, kDark, false,
                                  {0.3f * sx, -0.19f, 0.22f * sz}, 2); } },
                { "model:house", [](MeshData& m) {
                    Box(m, {0, -0.2f, 0}, {0.9f, 0.6f, 0.7f}, kCream);
                    Gable(m, {0, 0.3f, 0}, {1.0f, 0.4f, 0.8f}, kRed);
                    Box(m, {0, -0.33f, 0.355f}, {0.16f, 0.34f, 0.02f}, kDarkWd);
                    Box(m, {-0.28f, -0.12f, 0.355f}, {0.16f, 0.16f, 0.02f}, kGlass);
                    Box(m, { 0.28f, -0.12f, 0.355f}, {0.16f, 0.16f, 0.02f}, kGlass);
                    Box(m, {0.27f, 0.36f, -0.15f}, {0.1f, 0.26f, 0.1f}, kStone); } },
                { "model:tower", [](MeshData& m) {
                    Lathe(m, { {0,-0.5f}, {0.30f,-0.5f}, {0.25f,0.2f}, {0.33f,0.2f},
                               {0.33f,0.33f}, {0,0.33f} }, 16, kStone);
                    Lathe(m, ConeProfile(0.3f, 0.33f, 0.5f), 16, kRed);
                    Box(m, {0, -0.37f, 0.29f}, {0.12f, 0.26f, 0.04f}, kDarkWd); } },
                { "model:bridge", [](MeshData& m) {
                    Box(m, {0, 0.02f, 0}, {1, 0.06f, 0.36f}, kWood);
                    for (int s = -1; s <= 1; s += 2) {
                        Box(m, {0.36f * s, -0.25f, 0}, {0.1f, 0.5f, 0.32f}, kStone);
                        Box(m, {0, 0.2f, 0.17f * s}, {1, 0.03f, 0.03f}, kDarkWd);
                        for (int i = -2; i <= 2; i++)
                            Box(m, {0.24f * i, 0.12f, 0.17f * s}, {0.03f, 0.16f, 0.03f}, kDarkWd);
                    } } },
                { "model:satellite", [](MeshData& m) {
                    Box(m, {0,0,0}, {0.2f, 0.2f, 0.3f}, kGold);
                    for (int s = -1; s <= 1; s += 2) {
                        Box(m, {0.13f * s, 0, 0}, {0.06f, 0.03f, 0.03f}, kMetal);
                        Box(m, {0.33f * s, 0, 0}, {0.34f, 0.02f, 0.22f}, kBlue);
                    }
                    Lathe(m, { {0,0.15f}, {0.02f,0.15f}, {0.15f,0.27f}, {0,0.24f} }, 16, kMetal,
                          false, {0,0,0}, 2);
                    Lathe(m, CylinderProfile(0.008f, 0.1f, 0.4f), 6, kDark); } },
                { "model:robot", [](MeshData& m) {
                    Box(m, {0, 0, 0}, {0.4f, 0.4f, 0.25f}, kMetal);
                    Box(m, {0, 0.03f, 0.13f}, {0.2f, 0.14f, 0.02f}, kBlue);
                    Box(m, {0, 0.33f, 0}, {0.26f, 0.22f, 0.22f}, kStone);
                    Box(m, { 0.06f, 0.35f, 0.115f}, {0.05f, 0.05f, 0.02f}, kGlow);
                    Box(m, {-0.06f, 0.35f, 0.115f}, {0.05f, 0.05f, 0.02f}, kGlow);
                    Box(m, {0, 0.47f, 0}, {0.02f, 0.06f, 0.02f}, kRed);
                    for (int s = -1; s <= 1; s += 2) {
                        Box(m, {0.25f * s, -0.01f, 0}, {0.08f, 0.36f, 0.1f}, kDark);
                        Box(m, {0.1f * s, -0.35f, 0}, {0.12f, 0.3f, 0.14f}, kDark);
                    } } },
            };
            return k;
        }

    } // namespace

    bool MeshFactory::CreateBuiltin(const String& path, MeshData& out) {
        const ModelCatalogEntry* entry = FindModel(path.c_str());
        if (!entry) return false;
        auto& builders = Builders();
        auto it = builders.find(entry->path);
        if (it == builders.end()) return false;
        out = MeshData{};
        out.name = entry->path;
        it->second(out);
        return !out.vertices.empty();
    }

} // namespace RiftCore
