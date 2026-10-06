#pragma once
// Internal to the Renderer module: the Realistic render mode.

#include <Renderer/Camera.h>
#include <Renderer/RenderTypes.h>
#include <RiftCore/Renderer/IRenderer.h>

#include <unordered_map>
#include <vector>

namespace RiftCore {

    class RealisticPass {
    public:
        RealisticPass();
        ~RealisticPass();

        // Draws the frame into the currently bound framebuffer.
        void Render(const std::vector<DrawCall>& draws, const std::vector<Light>& lights,
                    const Camera& camera, u32 width, u32 height, RenderStats& stats);

        void SetShadows(bool on)   { shadows_ = on; }
        void SetExposure(f32 e)    { exposure_ = e; }
        void ForgetMesh(const GPUMesh* mesh);

    private:
        bool Init();
        unsigned int VaoFor(const GPUMesh* mesh);

        static constexpr int kShadowSize = 4096;
        bool ready_   = false;
        bool shadows_ = true;
        f32  exposure_ = 1.0f;
        unsigned int meshProgram_ = 0, depthProgram_ = 0, skyProgram_ = 0;
        unsigned int shadowFbo_ = 0, shadowTex_ = 0, skyVao_ = 0;
        std::unordered_map<const GPUMesh*, unsigned int> vaos_;
    };

} // namespace RiftCore
