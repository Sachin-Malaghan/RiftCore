#pragma once

#include <RiftCore/Common/Platform.h>
#include <RiftCore/Common/Types.h>
#include <RiftCore/Common/Result.h>









namespace RiftCore {

    // ── Light ─────────────────────────────────────────────────
    enum class LightType : u8 {
        Directional = 0,
        Point       = 1,
        Spot        = 2
    };

    struct LightDesc {
        LightType type      = LightType::Directional;
        Vec3      position  = { 0.0f, 10.0f, 0.0f };
        Vec3      direction = { 0.0f, -1.0f, 0.0f };
        Vec3      color     = { 1.0f,  1.0f, 1.0f };
        f32       intensity = 1.0f;
        f32       range     = 20.0f;
    };

    // ── Camera data ───────────────────────────────────────────
    struct CameraData {
        Vec3 position    = Vec3::Zero();
        Vec3 forward     = Vec3::Forward();
        Vec3 up          = Vec3::Up();
        f32  fovY        = 60.0f;
        f32  nearPlane   = 0.1f;
        f32  farPlane    = 1000.0f;
        f32  aspectRatio = 16.0f / 9.0f;
        Mat4 viewMatrix  = Mat4::Identity();
        Mat4 projMatrix  = Mat4::Identity();
    };

    // ── Draw call ─────────────────────────────────────────────
    struct DrawCallDesc {
        MeshHandle     mesh;
        MaterialHandle material;
        Mat4           transform    = Mat4::Identity();
        u32            instanceCount = 1;
    };

    // ── Render stats ──────────────────────────────────────────
    struct RenderStats {
        u32 drawCalls   = 0;
        u32 triangles   = 0;
        u32 lights      = 0;
        f32 frameTimeMs = 0.0f;
        u64 frameIndex  = 0;
    };

    // ── IRenderer interface ───────────────────────────────────
    class IRenderer {
    public:
        virtual ~IRenderer() = default;

        virtual VoidResult Initialize(
            void* device,
            u32   width,
            u32   height
        ) = 0;

        virtual void Shutdown()                            = 0;
        virtual void OnResize(u32 width, u32 height)      = 0;
        virtual RenderStats GetStats()               const = 0;
    };

} // namespace RiftCore
