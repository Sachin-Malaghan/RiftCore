#pragma once
// Internal to the Renderer module: named materials for the Realistic mode.

#include <Renderer/TextureLoader.h>
#include <RiftCore/Common/Types.h>

#include <unordered_map>

namespace RiftCore {

    struct MaterialDef {
        Texture2D* albedo = nullptr;
        Texture2D* normal = nullptr;
        Texture2D* rough  = nullptr;
        f32  scale     = 0.0f;     // metres per texture repeat; 0 = untextured
        f32  roughness = 0.5f;
        f32  metallic  = 0.0f;
        f32  opacity   = 1.0f;
        bool tint      = true;     // multiply by the node colour
        bool fromFile  = false;    // maps came from Assets/Textures
        bool valid     = false;
    };

    class MaterialLibrary {
    public:
        explicit MaterialLibrary(TextureLoader* loader);
        // Null for an unknown name (the node then renders with its plain colour).
        const MaterialDef* Get(const String& name);

    private:
        TextureLoader* loader_ = nullptr;
        std::unordered_map<String, MaterialDef> materials_;
    };

} // namespace RiftCore
