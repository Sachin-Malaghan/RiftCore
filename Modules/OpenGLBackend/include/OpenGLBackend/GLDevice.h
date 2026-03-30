#pragma once

#pragma warning(push)
#pragma warning(disable: 4251 4275)

#include <RiftCore/Common/Platform.h>
#include <RiftCore/Common/Types.h>
#include <RiftCore/Common/Result.h>
#include <RiftCore/RHI/IRHIDevice.h>

#include <memory>
#include <string>

struct GLFWwindow;

#ifdef OPENGLBACKEND_EXPORTS
    #define GLBACKEND_API RIFTCORE_EXPORT
#else
    #define GLBACKEND_API RIFTCORE_IMPORT
#endif

namespace RiftCore {

    class GLBACKEND_API GLBuffer : public IRHIBuffer {
    public:
        explicit GLBuffer(const BufferDesc& desc);
        ~GLBuffer();

        void*       Map()                                override;
        void        Unmap()                              override;
        void        Upload(const void* data, usize size) override;
        usize       GetSize()                      const override;
        BufferUsage GetUsage()                     const override;

        u32 GetGLID()   const { return glID_;    }
        u32 GetTarget() const { return glTarget_; }

    private:
        u32         glID_     = 0;
        usize       size_     = 0;
        BufferUsage usage_    = BufferUsage::Vertex;
        u32         glTarget_ = 0;
    };

    class GLBACKEND_API GLTexture : public IRHITexture {
    public:
        explicit GLTexture(const TextureDesc& desc);
        ~GLTexture();

        u32           GetWidth()        const override { return width_;     }
        u32           GetHeight()       const override { return height_;    }
        u32           GetMipLevels()    const override { return mipLevels_; }
        TextureFormat GetFormat()       const override { return format_;    }
        u32           GetNativeHandle() const override { return glID_;      }

    private:
        u32           glID_      = 0;
        u32           width_     = 0;
        u32           height_    = 0;
        u32           mipLevels_ = 1;
        TextureFormat format_    = TextureFormat::RGBA8;
    };

    class GLBACKEND_API GLPipeline : public IRHIPipeline {
    public:
        explicit GLPipeline(const PipelineDesc& desc);
        ~GLPipeline();

        void        Bind()               override;
        const char* GetDebugName() const override;

        u32  GetProgramID() const { return programID_; }
        bool IsValid()      const { return programID_ != 0; }

        // Uniform setters
        void SetUniformMat4 (const char* name, const f32* data);
        void SetUniformVec3 (const char* name, f32 x, f32 y, f32 z);
        void SetUniformFloat(const char* name, f32 value);
        void SetUniformInt  (const char* name, i32 value);

    private:
        u32    programID_ = 0;
        String debugName_;

        u32  CompileShader(u32 type, const char* source);
        bool LinkProgram  (u32 vert, u32 frag);
    };

    class GLBACKEND_API GLCommandList : public IRHICommandList {
    public:
        GLCommandList();
        ~GLCommandList();

        void Begin()                                    override;
        void End()                                      override;
        void SetPipeline(IRHIPipeline* pipeline)        override;
        void SetVertexBuffer(IRHIBuffer* buffer)        override;
        void SetIndexBuffer(IRHIBuffer* buffer)         override;
        void SetViewport(const Viewport& viewport)      override;
        void Draw(u32 vertexCount,
                  u32 startVertex = 0)                  override;
        void DrawIndexed(u32 indexCount,
                         u32 startIndex = 0,
                         i32 baseVertex = 0)            override;
        void ClearColor(f32 r, f32 g,
                        f32 b, f32 a = 1.0f)            override;
        void ClearDepth(f32 depth = 1.0f)               override;

    private:
        GLPipeline* currentPipeline_ = nullptr;
        GLBuffer*   currentVBO_      = nullptr;
        GLBuffer*   currentIBO_      = nullptr;
        u32         vao_             = 0;
    };

    class GLBACKEND_API GLDevice : public IRHIDevice {
    public:
        GLDevice();
        ~GLDevice();
        RIFTCORE_NOCOPY_NOMOVE(GLDevice);

        VoidResult Initialize(void* windowHandle)       override;
        void       Shutdown()                           override;

        Result<IRHIBuffer*>
            CreateBuffer(const BufferDesc& desc)        override;
        Result<IRHITexture*>
            CreateTexture(const TextureDesc& desc)      override;
        Result<IRHIPipeline*>
            CreatePipeline(const PipelineDesc& desc)    override;
        Result<IRHICommandList*>
            CreateCommandList()                         override;

        void DestroyBuffer     (IRHIBuffer*      b)     override;
        void DestroyTexture    (IRHITexture*     t)     override;
        void DestroyPipeline   (IRHIPipeline*    p)     override;
        void DestroyCommandList(IRHICommandList* c)     override;

        void BeginFrame()                               override;
        void EndFrame()                                 override;
        void Present()                                  override;
        void WaitIdle()                                 override;

        const char* GetBackendName() const override { return "OpenGL 4.6";      }
        const char* GetGPUName()     const override { return gpuName_.c_str();  }
        u64         GetVRAMBudget()  const override { return vramBudget_;       }
        u64         GetVRAMUsed()    const override { return 0;                 }

        GLFWwindow* GetWindow()   const { return window_; }
        bool        ShouldClose() const;
        void        PollEvents();

    private:
        GLFWwindow* window_     = nullptr;
        String      gpuName_;
        u64         vramBudget_ = 0;
        u32         frameIndex_ = 0;
    };

    class GLBACKEND_API OpenGLRHI : public IRHI {
    public:
        OpenGLRHI();
        ~OpenGLRHI();

        Result<IRHIDevice*> CreateDevice(void* windowHandle) override;
        void                DestroyDevice(IRHIDevice* device) override;
        const char*         GetAPIName() const override { return "OpenGL"; }

    private:
        std::unique_ptr<GLDevice> device_;
    };

} // namespace RiftCore

#pragma warning(pop)
