#include <Core/PluginManager.h>
#include <iostream>

#ifdef RIFTCORE_PLATFORM_WINDOWS
    #include <Windows.h>
#else
    #include <dlfcn.h>
#endif








namespace RiftCore {

    PluginManager::PluginManager()  = default;
    PluginManager::~PluginManager() { UnloadAll(); }

    VoidResult PluginManager::LoadDLL(
        const String& path,
        DLLHandle&    outHandle
    ) {
#ifdef RIFTCORE_PLATFORM_WINDOWS
        outHandle = LoadLibraryA(path.c_str());
        if (!outHandle) {
            DWORD err = GetLastError();
            return VoidResult::Err(
                "LoadLibrary failed for: " + path +
                " Error code: " + std::to_string(err)
            );
        }
#else
        outHandle = dlopen(path.c_str(), RTLD_LAZY | RTLD_LOCAL);
        if (!outHandle) {
            return VoidResult::Err(
                String("dlopen failed: ") + dlerror()
            );
        }
#endif
        return VoidResult::Ok();
    }

    void PluginManager::UnloadDLL(DLLHandle handle) {
        if (!handle) return;
#ifdef RIFTCORE_PLATFORM_WINDOWS
        FreeLibrary(handle);
#else
        dlclose(handle);
#endif
    }

    void* PluginManager::GetSymbol(
        DLLHandle   handle,
        const char* symbolName
    ) {
        if (!handle) return nullptr;
#ifdef RIFTCORE_PLATFORM_WINDOWS
        return reinterpret_cast<void*>(
            GetProcAddress(handle, symbolName)
        );
#else
        return dlsym(handle, symbolName);
#endif
    }

    VoidResult PluginManager::LoadPlugin(
        const String& name,
        const String& dllPath
    ) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (plugins_.count(name)) {
            return VoidResult::Err(
                "Plugin already loaded: " + name
            );
        }

        PluginInfo info;
        info.name = name;
        info.path = dllPath;

        auto result = LoadDLL(dllPath, info.handle);
        if (result.IsErr()) {
            return result;
        }

        // Get factory function
        auto fnCreate = reinterpret_cast<FnCreateModule>(
            GetSymbol(info.handle, "CreateModule")
        );

        if (!fnCreate) {
            UnloadDLL(info.handle);
            return VoidResult::Err(
                "CreateModule not found in: " + dllPath
            );
        }

        // Get info function (optional)
        auto fnInfo = reinterpret_cast<FnGetModuleInfo>(
            GetSymbol(info.handle, "GetModuleInfo")
        );

        if (fnInfo) {
            info.descriptor = fnInfo();
            info.version    = info.descriptor.version;
        }

        // Create module instance
        info.module = fnCreate();
        if (!info.module) {
            UnloadDLL(info.handle);
            return VoidResult::Err(
                "CreateModule returned null for: " + name
            );
        }

        info.loaded = true;
        plugins_[name] = std::move(info);

        std::cout << "[PluginManager] Loaded: "
                  << name << " from " << dllPath << "\n";

        return VoidResult::Ok();
    }

    VoidResult PluginManager::InitializePlugin(
        const String&          name,
        const ModuleInitParams& params
    ) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = plugins_.find(name);
        if (it == plugins_.end()) {
            return VoidResult::Err(
                "Plugin not loaded: " + name
            );
        }

        auto result = it->second.module->Initialize(params);
        if (result.IsErr()) {
            return VoidResult::Err(
                "Plugin init failed [" + name + "]: " +
                result.Error().message
            );
        }

        std::cout << "[PluginManager] Initialized: " << name << "\n";
        return VoidResult::Ok();
    }

    VoidResult PluginManager::LoadAndInit(
        const String&          name,
        const String&          dllPath,
        const ModuleInitParams& params
    ) {
        auto r1 = LoadPlugin(name, dllPath);
        if (r1.IsErr()) return r1;

        auto r2 = InitializePlugin(name, params);
        if (r2.IsErr()) return r2;

        return VoidResult::Ok();
    }

    void PluginManager::UnloadPlugin(const String& name) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = plugins_.find(name);
        if (it == plugins_.end()) return;

        if (it->second.module) {
            it->second.module->Shutdown();

            auto fnDestroy = reinterpret_cast<FnDestroyModule>(
                GetSymbol(it->second.handle, "DestroyModule")
            );
            if (fnDestroy) {
                fnDestroy(it->second.module);
            }
            it->second.module = nullptr;
        }

        UnloadDLL(it->second.handle);
        it->second.handle = nullptr;
        it->second.loaded = false;

        std::cout << "[PluginManager] Unloaded: " << name << "\n";
        plugins_.erase(it);
    }

    void PluginManager::UnloadAll() {
        std::vector<String> names;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& [name, _] : plugins_) {
                names.push_back(name);
            }
        }
        for (auto& name : names) {
            UnloadPlugin(name);
        }
    }

    IModule* PluginManager::GetModule(const String& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = plugins_.find(name);
        if (it == plugins_.end()) return nullptr;
        return it->second.module;
    }

    bool PluginManager::IsLoaded(const String& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return plugins_.count(name) > 0;
    }

    std::vector<PluginInfo> PluginManager::GetAllPlugins() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<PluginInfo> result;
        result.reserve(plugins_.size());
        for (auto& [_, info] : plugins_) {
            result.push_back(info);
        }
        return result;
    }

    void PluginManager::UpdateAll(f32 deltaTime) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [_, info] : plugins_) {
            if (info.module && info.loaded) {
                info.module->OnUpdate(deltaTime);
            }
        }
    }

    void PluginManager::RenderAll() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [_, info] : plugins_) {
            if (info.module && info.loaded) {
                info.module->OnRender();
            }
        }
    }

} // namespace RiftCore
