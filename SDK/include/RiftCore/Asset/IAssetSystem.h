// ── SDK/include/RiftCore/Asset/IAssetSystem.h ─────────────────
#pragma once
#include "../Common/Platform.h"
#include "../Common/Types.h"
#include "../Common/Result.h"

#include <memory>
#include <functional>






namespace RiftCore {

    // ── Asset metadata ────────────────────────────────────────
    enum class AssetType : u8 {
        Unknown = 0,
        Texture, Mesh, Material,
        Audio, Shader, Scene,
        Script, Font, Animation,
        Prefab
    };

    struct AssetMetadata {
        GUID        guid;
        String      virtualPath;    // "Assets/Textures/grass.tex"
        String      physicalPath;   // "C:/project/assets/grass.tex"
        AssetType   type     = AssetType::Unknown;
        usize       fileSize = 0;
        u64         lastModified = 0;
    };

    // ── Asset base ────────────────────────────────────────────
    class IAsset {
    public:
        virtual ~IAsset() = default;
        virtual GUID        GetGUID()     const = 0;
        virtual AssetType   GetType()     const = 0;
        virtual String      GetPath()     const = 0;
        virtual bool        IsLoaded()    const = 0;
        virtual usize       GetMemoryUsage() const = 0;
    };

    // ── Async load callback ───────────────────────────────────
    using AssetLoadCallback = std::function<void(
        AssetHandle handle,
        IAsset* asset,
        bool success
    )>;

    // ── IAssetSystem interface ────────────────────────────────
    class IAssetSystem {
    public:
        virtual ~IAssetSystem() = default;

        virtual VoidResult Initialize(const String& assetRootPath)   = 0;
        virtual void       Shutdown()                                 = 0;
        virtual void       Update()                                   = 0;  // pump async loads

        // ── Sync loading ──────────────────────────────────────
        virtual Result<AssetHandle> LoadSync (const String& path)    = 0;

        // ── Async loading ─────────────────────────────────────
        virtual AssetHandle         LoadAsync(
            const String&      path,
            AssetLoadCallback  callback = nullptr
        ) = 0;

        // ── Access ────────────────────────────────────────────
        virtual IAsset*    GetAsset    (AssetHandle handle) const    = 0;
        virtual bool       IsLoaded    (AssetHandle handle) const    = 0;
        virtual void       Unload      (AssetHandle handle)          = 0;
        virtual void       UnloadAll   ()                            = 0;

        // ── Cache control ─────────────────────────────────────
        virtual void       SetCacheLimit(usize bytes)                = 0;
        virtual usize      GetCacheUsage()                     const  = 0;
        virtual void       PurgeCacheUntil(usize targetBytes)        = 0;
    };

} // namespace RiftCore
