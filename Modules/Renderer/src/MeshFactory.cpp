#include <Renderer/RenderTypes.h>
#include <Renderer/Camera.h>
#include <cmath>

namespace RiftCore {

    MeshData MeshFactory::CreateCube(f32 size) {
        f32 h = size * 0.5f;
        MeshData mesh;
        mesh.name = "Cube";

        auto addFace = [&](Vec3 p0, Vec3 p1,
                           Vec3 p2, Vec3 p3,
                           Vec3 normal, Vec3 color) {
            u32 base = static_cast<u32>(
                mesh.vertices.size());
            mesh.vertices.push_back(
                {p0, normal, {0,0}, color});
            mesh.vertices.push_back(
                {p1, normal, {1,0}, color});
            mesh.vertices.push_back(
                {p2, normal, {1,1}, color});
            mesh.vertices.push_back(
                {p3, normal, {0,1}, color});
            mesh.indices.insert(mesh.indices.end(), {
                base,   base+1, base+2,
                base,   base+2, base+3
            });
        };

        // Front +Z
        addFace({-h,-h, h},{h,-h, h},{h,h, h},{-h,h, h},
                {0,0,1},  {1.0f,0.3f,0.3f});
        // Back -Z
        addFace({h,-h,-h},{-h,-h,-h},{-h,h,-h},{h,h,-h},
                {0,0,-1}, {0.3f,1.0f,1.0f});
        // Left -X
        addFace({-h,-h,-h},{-h,-h,h},{-h,h,h},{-h,h,-h},
                {-1,0,0}, {0.3f,1.0f,0.3f});
        // Right +X
        addFace({h,-h,h},{h,-h,-h},{h,h,-h},{h,h,h},
                {1,0,0},  {1.0f,0.3f,1.0f});
        // Top +Y
        addFace({-h,h,h},{h,h,h},{h,h,-h},{-h,h,-h},
                {0,1,0},  {0.3f,0.3f,1.0f});
        // Bottom -Y
        addFace({-h,-h,-h},{h,-h,-h},{h,-h,h},{-h,-h,h},
                {0,-1,0}, {1.0f,1.0f,0.3f});

        return mesh;
    }

    MeshData MeshFactory::CreatePlane(f32 size, u32 divs) {
        MeshData mesh;
        mesh.name = "Plane";
        f32 step = size / static_cast<f32>(divs);
        f32 half = size * 0.5f;

        for (u32 z = 0; z <= divs; z++) {
            for (u32 x = 0; x <= divs; x++) {
                Vertex3D v;
                v.position = {
                    -half + x * step,
                    0.0f,
                    -half + z * step
                };
                v.normal   = {0, 1, 0};
                v.texCoord = {
                    static_cast<f32>(x) / divs,
                    static_cast<f32>(z) / divs
                };
                v.color = {0.5f, 0.7f, 0.5f};
                mesh.vertices.push_back(v);
            }
        }

        u32 w = divs + 1;
        for (u32 z = 0; z < divs; z++) {
            for (u32 x = 0; x < divs; x++) {
                u32 tl = z*w + x;
                u32 tr = z*w + x + 1;
                u32 bl = (z+1)*w + x;
                u32 br = (z+1)*w + x + 1;
                mesh.indices.insert(
                    mesh.indices.end(),
                    {tl, bl, tr, tr, bl, br});
            }
        }
        return mesh;
    }

    MeshData MeshFactory::CreateSphere(
        f32 radius, u32 stacks, u32 slices
    ) {
        MeshData mesh;
        mesh.name = "Sphere";
        const f32 PI = 3.14159265f;

        for (u32 i = 0; i <= stacks; i++) {
            f32 phi = PI * i / stacks;
            for (u32 j = 0; j <= slices; j++) {
                f32 theta = 2.0f * PI * j / slices;
                Vertex3D v;
                v.position = {
                    radius * std::sin(phi) * std::cos(theta),
                    radius * std::cos(phi),
                    radius * std::sin(phi) * std::sin(theta)
                };
                v.normal   = Math::Normalize(v.position);
                v.texCoord = {
                    static_cast<f32>(j) / slices,
                    static_cast<f32>(i) / stacks
                };
                v.color = {
                    0.5f + 0.5f * v.normal.x,
                    0.5f + 0.5f * v.normal.y,
                    0.5f + 0.5f * v.normal.z
                };
                mesh.vertices.push_back(v);
            }
        }

        for (u32 i = 0; i < stacks; i++) {
            for (u32 j = 0; j < slices; j++) {
                u32 r  = slices + 1;
                u32 tl = i*r + j;
                u32 tr = i*r + j + 1;
                u32 bl = (i+1)*r + j;
                u32 br = (i+1)*r + j + 1;
                mesh.indices.insert(
                    mesh.indices.end(),
                    {tl, bl, tr, tr, bl, br});
            }
        }
        return mesh;
    }

    MeshData MeshFactory::CreatePyramid(f32 base, f32 height) {
        MeshData mesh;
        mesh.name = "Pyramid";
        f32 h = base * 0.5f;

        Vec3 apex = {0, height, 0};
        Vec3 bl   = {-h, 0, -h};
        Vec3 br   = { h, 0, -h};
        Vec3 fl   = {-h, 0,  h};
        Vec3 fr   = { h, 0,  h};

        auto addTri = [&](Vec3 a, Vec3 b, Vec3 c, Vec3 col) {
            Vec3 ab = {b.x-a.x, b.y-a.y, b.z-a.z};
            Vec3 ac = {c.x-a.x, c.y-a.y, c.z-a.z};
            Vec3 n  = Math::Normalize(Math::Cross(ab, ac));
            u32 base2 = static_cast<u32>(
                mesh.vertices.size());
            mesh.vertices.push_back({a, n, {0,   0  }, col});
            mesh.vertices.push_back({b, n, {1,   0  }, col});
            mesh.vertices.push_back({c, n, {0.5f,1.0f}, col});
            mesh.indices.insert(mesh.indices.end(),
                {base2, base2+1, base2+2});
        };

        addTri(apex, fl, fr, {1.0f,0.3f,0.3f});
        addTri(apex, fr, br, {0.3f,1.0f,0.3f});
        addTri(apex, br, bl, {0.3f,0.3f,1.0f});
        addTri(apex, bl, fl, {1.0f,1.0f,0.3f});

        u32 b2 = static_cast<u32>(mesh.vertices.size());
        Vec3 bn = {0,-1,0};
        Vec3 bc = {0.5f, 0.5f, 0.5f};
        mesh.vertices.push_back({bl, bn, {0,0}, bc});
        mesh.vertices.push_back({br, bn, {1,0}, bc});
        mesh.vertices.push_back({fr, bn, {1,1}, bc});
        mesh.vertices.push_back({fl, bn, {0,1}, bc});
        mesh.indices.insert(mesh.indices.end(),
            {b2, b2+1, b2+2, b2, b2+2, b2+3});

        return mesh;
    }

} // namespace RiftCore
