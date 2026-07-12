#pragma once

#pragma warning(push)
#pragma warning(disable: 4251 4275)

#include <RiftCore/Common/Platform.h>
#include <RiftCore/Common/Types.h>
#include <RiftCore/Common/Result.h>
#include <RiftCore/Core/IModule.h>

#include <unordered_map>
#include <memory>
#include <mutex>
#include <vector>

#ifdef CORE_EXPORTS
    #define CORE_API RIFTCORE_EXPORT
#else
    #define CORE_API RIFTCORE_IMPORT
#endif

#ifdef RIFTCORE_PLATFORM_WINDOWS
    #include <Windows.h>
    using DLLHandle = HMODULE;
#else
    using DLLHandle = void*;
#endif



namespace RiftCore {

    struct PluginInfo {
        String           name;
        String           path;
        String           version;
        DLLHandle        handle     = nullptr;
        IModule*         module     = nullptr;
        bool             loaded     = false;
        ModuleDescriptor descriptor;
    };

    class CORE_API PluginManager {
    public:
        PluginManager();
        ~PluginManager();

        RIFTCORE_NOCOPY_NOMOVE(PluginManager);

        VoidResult LoadPlugin(const String& name, const String& dllPath);

        VoidResult InitializePlugin(
            const String&          name,
            const ModuleInitParams& params
        );

        VoidResult LoadAndInit(
            const String&          name,
            const String&          dllPath,
            const ModuleInitParams& params
        );

        void UnloadPlugin(const String& name);
        void UnloadAll();

        IModule* GetModule(const String& name) const;

        template<typename T>
        T* GetModuleAs(const String& name) const {
            // NEVER use dynamic_cast across DLL boundaries.
            // static_cast blindly trusts that the VTable matches, 
            // which it will, as long as both inherit from IModule.
            return static_cast<T*>(GetModule(name));
        }

        bool IsLoaded(const String& name) const;
        std::vector<PluginInfo> GetAllPlugins() const;
        void UpdateAll(f32 deltaTime);
        void RenderAll();

    private:
        VoidResult LoadDLL(const String& path, DLLHandle& outHandle);
        void       UnloadDLL(DLLHandle handle);
        void*      GetSymbol(DLLHandle handle, const char* symbolName);

        std::unordered_map<String, PluginInfo> plugins_;
        mutable std::mutex                     mutex_;
    };

} // namespace RiftCore
#pragma warning(pop)
