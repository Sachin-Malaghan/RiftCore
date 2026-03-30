#pragma once

#pragma warning(push)
#pragma warning(disable: 4251 4275)

#include <RiftCore/Common/Platform.h>
#include <RiftCore/Common/Types.h>
#include <RiftCore/Common/Result.h>
#include <RiftCore/RHI/IRHIDevice.h>
#include <RiftCore/Renderer/IRenderer.h>

#include <vector>
#include <string>

#ifdef RENDERER_EXPORTS
    #define RENDERER_API RIFTCORE_EXPORT
#else
    #define RENDERER_API RIFTCORE_IMPORT
#endif

namespace RiftCore {

    // Forward declare
    struct Texture2D;

    // ── Light ─────────────────────────────────────────────────
    struct Light {
        LightType type       = LightType::Directional;
        Vec3      position   = { 0.0f, 10.0f, 0.0f };
        Vec3      direction  = { 0.0f, -1.0f, 0.0f };
        Vec3      color      = { 1.0f,  1.0f, 1.0f };
        f32       intensity  = 1.0f;
        f32       range      = 20.0f;
        bool      castShadow = false;
    };

    // ── Vertex format ─────────────────────────────────────────
    struct Vertex3D {
        Vec3 position;
        Vec3 normal;
        Vec2 texCoord;
        Vec3 color = { 1,1,1 };
    };

    // ── Mesh data ─────────────────────────────────────────────
    struct MeshData {
        std::vector<Vertex3D> vertices;
        std::vector<u32>      indices;
        String                name;
    };

    // ── Material ──────────────────────────────────────────────
    struct MaterialData {
        Vec3      albedo       = { 1.0f, 1.0f, 1.0f };
        f32       metallic     = 0.0f;
        f32       roughness    = 0.5f;
        f32       ao           = 1.0f;
        bool      wireframe    = false;
        String    name;

        // Texture slots
        // Set to nullptr to use vertex color / solid albedo
        Texture2D* albedoTex   = nullptr;
        Texture2D* normalTex   = nullptr;
        Texture2D* metallicTex = nullptr;

        // Tiling
        f32 texTileX = 1.0f;
        f32 texTileY = 1.0f;
    };

    // ── GPU Mesh ──────────────────────────────────────────────
    struct GPUMesh {
        IRHIBuffer* vertexBuffer = nullptr;
        IRHIBuffer* indexBuffer  = nullptr;
        u32         indexCount   = 0;
        u32         vertexCount  = 0;
        String      name;

        bool isValid() const { return vertexBuffer != nullptr; }
    };

    // ── Draw call ─────────────────────────────────────────────
    struct DrawCall {
        GPUMesh*     mesh      = nullptr;
        MaterialData material;
        Mat4         transform = Mat4::Identity();
    };

    // ── Mesh factory ──────────────────────────────────────────
    struct RENDERER_API MeshFactory {
        static MeshData CreateCube   (f32 size   = 1.0f);
        static MeshData CreatePlane  (f32 size   = 10.0f,
                                      u32 divs   = 1);
        static MeshData CreateSphere (f32 radius = 0.5f,
                                      u32 stacks = 16,
                                      u32 slices = 16);
        static MeshData CreatePyramid(f32 base   = 1.0f,
                                      f32 height = 1.5f);
    };

} // namespace RiftCore

#pragma warning(pop)
