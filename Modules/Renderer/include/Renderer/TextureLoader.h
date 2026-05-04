#pragma once

#pragma warning(push)
#pragma warning(disable: 4251 4275)




#include <RiftCore/Common/Platform.h>
#include <RiftCore/Common/Types.h>
#include <RiftCore/Common/Result.h>
#include <RiftCore/RHI/IRHIDevice.h>

#include <unordered_map>
#include <string>
#include <memory>

#ifdef RENDERER_EXPORTS
    #define RENDERER_API RIFTCORE_EXPORT
#else
    #define RENDERER_API RIFTCORE_IMPORT
#endif

namespace RiftCore {

    // ── Texture slot bindings ─────────────────────────────────
    // Which GL texture unit each texture type uses
    enum class TextureSlot : u32 {
        Albedo    = 0,
        Normal    = 1,
        Metallic  = 2,
        Roughness = 3,
        AO        = 4
    };

    // ── CPU-side image data ───────────────────────────────────
    struct ImageData {
        std::vector<u8> pixels;
        i32             width    = 0;
        i32             height   = 0;
        i32             channels = 4;
        bool            isValid  = false;
    };

    // ── GPU texture wrapper ───────────────────────────────────
    struct Texture2D {
        u32    glID     = 0;
        i32    width    = 0;
        i32    height   = 0;
        String path;
        bool   isValid() const { return glID != 0; }
    };

    // ── TextureLoader ─────────────────────────────────────────
    class RENDERER_API TextureLoader {
    public:
        TextureLoader();
        ~TextureLoader();

        RIFTCORE_NOCOPY_NOMOVE(TextureLoader);

        // Load texture from file path
        // Returns cached texture if already loaded
        Result<Texture2D*> Load(const String& filePath);

        // Create texture from raw RGBA pixel data
        Result<Texture2D*> CreateFromPixels(
            const String& name,
            const u8*     pixels,
            i32           width,
            i32           height,
            i32           channels = 4
        );

        // Create a solid color texture (useful for defaults)
        Result<Texture2D*> CreateSolidColor(
            const String& name,
            u8 r, u8 g, u8 b, u8 a = 255
        );

        // Create a checkerboard texture (for UV debugging)
        Result<Texture2D*> CreateCheckerboard(
            const String& name,
            i32  size       = 256,
            i32  checkSize  = 32,
            u8   colorA[4]  = nullptr,
            u8   colorB[4]  = nullptr
        );

        // Create a simple grid/tile texture
        Result<Texture2D*> CreateGrid(
            const String& name,
            i32  size      = 256,
            i32  gridSize  = 32,
            u8   lineWidth = 2
        );

        // Unload a specific texture
        void Unload(const String& name);

        // Unload all textures
        void UnloadAll();

        // Bind texture to a GL texture unit
        void Bind(Texture2D* tex, u32 slot = 0);
        void Unbind(u32 slot = 0);

        // Get white default texture
        Texture2D* GetWhiteTexture()       { return white_.get();       }
        Texture2D* GetBlackTexture()       { return black_.get();       }
        Texture2D* GetCheckerTexture()     { return checker_.get();     }
        Texture2D* GetNormalMapDefault()   { return normalDefault_.get();}

    private:
        u32 CreateGLTexture(
            const u8* pixels,
            i32 width, i32 height, i32 channels
        );

        void CreateDefaultTextures();

        std::unordered_map<String,
            std::unique_ptr<Texture2D>> cache_;

        std::unique_ptr<Texture2D> white_;
        std::unique_ptr<Texture2D> black_;
        std::unique_ptr<Texture2D> checker_;
        std::unique_ptr<Texture2D> normalDefault_;
    };

} // namespace RiftCore

#pragma warning(pop)
