#pragma once

#include <RiftCore/Common/Platform.h>
#include <RiftCore/Common/Types.h>
#include <RiftCore/Common/Result.h>

namespace RiftCore {

    // ── Buffer ────────────────────────────────────────────────
    enum class BufferUsage : u32 {
        Vertex  = 1 << 0,
        Index   = 1 << 1,
        Uniform = 1 << 2,
        Storage = 1 << 3,
        Staging = 1 << 4
    };

    struct BufferDesc {
        usize       size      = 0;
        BufferUsage usage     = BufferUsage::Vertex;
        bool        dynamic   = false;
        const void* initData  = nullptr;
        const char* debugName = nullptr;
    };

    class IRHIBuffer {
    public:
        virtual ~IRHIBuffer() = default;
        virtual void*       Map()                                = 0;
        virtual void        Unmap()                              = 0;
        virtual void        Upload(const void* data, usize size) = 0;
        virtual usize       GetSize()                      const = 0;
        virtual BufferUsage GetUsage()                     const = 0;
    };

    // ── Texture ───────────────────────────────────────────────
    enum class TextureFormat : u32 {
        Unknown   = 0,
        RGBA8     = 1,
        RGBA16F   = 2,
        RGBA32F   = 3,
        R32F      = 4,
        Depth32F  = 5,
        Depth24S8 = 6
    };

    enum class TextureUsage : u32 {
        Sampled      = 1 << 0,
        RenderTarget = 1 << 1,
        DepthStencil = 1 << 2,
        Storage      = 1 << 3
    };

    struct TextureDesc {
        u32           width      = 1;
        u32           height     = 1;
        u32           depth      = 1;
        u32           mipLevels  = 1;
        u32           arraySize  = 1;
        TextureFormat format     = TextureFormat::RGBA8;
        TextureUsage  usage      = TextureUsage::Sampled;
        const void*   initData   = nullptr;
        const char*   debugName  = nullptr;
    };

    class IRHITexture {
    public:
        virtual ~IRHITexture() = default;
        virtual u32           GetWidth()        const = 0;
        virtual u32           GetHeight()       const = 0;
        virtual u32           GetMipLevels()    const = 0;
        virtual TextureFormat GetFormat()       const = 0;
        virtual u32           GetNativeHandle() const = 0;
    };

    // ── Pipeline ──────────────────────────────────────────────
    enum class PrimitiveTopology : u8 {
        TriangleList  = 0,
        TriangleStrip = 1,
        LineList      = 2,
        PointList     = 3
    };

    struct ShaderDesc {
        const char* vertexSource   = nullptr;
        const char* fragmentSource = nullptr;
        const char* computeSource  = nullptr;
    };

    struct PipelineDesc {
        ShaderDesc        shaders;
        PrimitiveTopology topology  = PrimitiveTopology::TriangleList;
        bool              depthTest  = true;
        bool              depthWrite = true;
        bool              blending   = false;
        const char*       debugName  = nullptr;
    };

    class IRHIPipeline {
    public:
        virtual ~IRHIPipeline() = default;
        virtual void        Bind()               = 0;
        virtual const char* GetDebugName() const = 0;
    };

    // ── CommandList ───────────────────────────────────────────
    class IRHICommandList {
    public:
        virtual ~IRHICommandList() = default;
        virtual void Begin()                                      = 0;
        virtual void End()                                        = 0;
        virtual void SetPipeline(IRHIPipeline* pipeline)          = 0;
        virtual void SetVertexBuffer(IRHIBuffer* buffer)          = 0;
        virtual void SetIndexBuffer(IRHIBuffer* buffer)           = 0;
        virtual void SetViewport(const Viewport& viewport)        = 0;
        virtual void Draw(u32 vertexCount,
                          u32 startVertex = 0)                    = 0;
        virtual void DrawIndexed(u32 indexCount,
                                 u32 startIndex = 0,
                                 i32 baseVertex = 0)              = 0;
        virtual void ClearColor(f32 r, f32 g,
                                f32 b, f32 a = 1.0f)              = 0;
        virtual void ClearDepth(f32 depth = 1.0f)                 = 0;
    };

    // ── Device ────────────────────────────────────────────────
    class IRHIDevice {
    public:
        virtual ~IRHIDevice() = default;

        virtual VoidResult Initialize(void* windowHandle)          = 0;
        virtual void       Shutdown()                              = 0;

        virtual Result<IRHIBuffer*>
            CreateBuffer(const BufferDesc& desc)                   = 0;
        virtual Result<IRHITexture*>
            CreateTexture(const TextureDesc& desc)                 = 0;
        virtual Result<IRHIPipeline*>
            CreatePipeline(const PipelineDesc& desc)               = 0;
        virtual Result<IRHICommandList*>
            CreateCommandList()                                     = 0;

        virtual void DestroyBuffer(IRHIBuffer* b)                  = 0;
        virtual void DestroyTexture(IRHITexture* t)                = 0;
        virtual void DestroyPipeline(IRHIPipeline* p)              = 0;
        virtual void DestroyCommandList(IRHICommandList* c)        = 0;

        virtual void BeginFrame()                                  = 0;
        virtual void EndFrame()                                    = 0;
        virtual void Present()                                     = 0;
        virtual void WaitIdle()                                    = 0;

        virtual const char* GetBackendName() const                 = 0;
        virtual const char* GetGPUName()     const                 = 0;
        virtual u64         GetVRAMBudget()  const                 = 0;
        virtual u64         GetVRAMUsed()    const                 = 0;
    };

    // ── RHI Module ────────────────────────────────────────────
    class IRHI {
    public:
        virtual ~IRHI() = default;
        virtual Result<IRHIDevice*>
            CreateDevice(void* windowHandle)                       = 0;
        virtual void DestroyDevice(IRHIDevice* device)             = 0;
        virtual const char* GetAPIName() const                     = 0;
    };

} // namespace RiftCore
