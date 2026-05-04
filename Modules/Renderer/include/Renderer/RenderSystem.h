#pragma once

#pragma warning(push)
#pragma warning(disable: 4251 4275)

#include <RiftCore/Common/Platform.h>
#include <RiftCore/Common/Types.h>
#include <RiftCore/Common/Result.h>
#include <RiftCore/Core/IModule.h>
#include <RiftCore/RHI/IRHIDevice.h>
#include <RiftCore/Renderer/IRenderer.h>

#include <Renderer/Camera.h>
#include <Renderer/RenderTypes.h>
#include <Renderer/TextureLoader.h>

#include <vector>
#include <memory>

#ifdef RENDERER_EXPORTS
    #define RENDERER_API RIFTCORE_EXPORT
#else
    #define RENDERER_API RIFTCORE_IMPORT
#endif




namespace RiftCore {

    class GLPipeline;

    class RENDERER_API RenderSystem {
    public:
        RenderSystem();
        ~RenderSystem();
        RIFTCORE_NOCOPY_NOMOVE(RenderSystem);

        VoidResult Initialize(IRHIDevice* device,
                              u32 width, u32 height);
        void       Shutdown();

        void BeginFrame (const Camera& camera);
        void Submit     (const DrawCall& drawCall);
        void SubmitLight(const Light& light);
        void EndFrame   ();
        void Present    ();

        Result<GPUMesh*> UploadMesh (const MeshData& data);
        void             DestroyMesh(GPUMesh* mesh);

        void OnResize   (u32 width, u32 height);
        void SetWireframe(bool enabled);

        RenderStats    GetStats()         const { return stats_;   }
        TextureLoader* GetTextureLoader()       { return textures_.get(); }

    private:
        VoidResult CreateShaders();
        void       RenderDrawCall(const DrawCall& dc);

        IRHIDevice*              device_       = nullptr;
        IRHIPipeline*            pipeline_     = nullptr;
        IRHIPipeline*            wirePipeline_ = nullptr;
        IRHICommandList*         cmdList_      = nullptr;

        u32 width_  = 1280;
        u32 height_ = 720;

        Camera                 camera_;
        Mat4                   viewProj_  = Mat4::Identity();
        std::vector<DrawCall>  drawQueue_;
        std::vector<Light>     lights_;

        std::unique_ptr<TextureLoader> textures_;

        RenderStats stats_;
        bool        wireframe_  = false;
        u64         frameIndex_ = 0;
    };

    class RENDERER_API RendererModule : public IModule {
    public:
        RendererModule();
        ~RendererModule();

        VoidResult       Initialize(const ModuleInitParams& params) override;
        void             OnUpdate  (f32 deltaTime)                  override;
        void             OnRender  ()                               override;
        void             Shutdown  ()                               override;
        ModuleDescriptor GetDescriptor()                      const override;

        RenderSystem* GetRenderSystem() { return renderer_.get(); }

    private:
        std::unique_ptr<RenderSystem> renderer_;
    };

} // namespace RiftCore

#pragma warning(pop)
