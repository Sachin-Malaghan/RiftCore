#include "RiftCore/Core/PluginLoader.h"
#include <iostream>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif








namespace RiftCore {
    PluginLoader::~PluginLoader() {
        for (auto const& [name, handle] : m_LoadedLibraries) {
            #ifdef _WIN32
                FreeLibrary(static_cast<HMODULE>(handle));
            #else
                dlclose(handle);
            #endif
        }
        m_LoadedLibraries.clear();
    }

    bool PluginLoader::LoadPlugin(const std::string& pluginName) {
        if (m_LoadedLibraries.find(pluginName) != m_LoadedLibraries.end()) return true;
        void* handle = nullptr;
        std::string fullPath = pluginName;
        #ifdef _WIN32
            fullPath += ".dll";
            handle = LoadLibraryA(fullPath.c_str());
        #else
            fullPath = "lib" + pluginName + ".so";
            handle = dlopen(fullPath.c_str(), RTLD_LAZY);
        #endif

        if (!handle) {
            std::cerr << "[PluginLoader] Failed to load: " << fullPath << "\n";
            return false;
        }
        m_LoadedLibraries[pluginName] = handle;
        return true;
    }

    void* PluginLoader::GetSymbol(const std::string& pluginName, const std::string& symbolName) const {
        auto it = m_LoadedLibraries.find(pluginName);
        if (it == m_LoadedLibraries.end()) return nullptr;
        #ifdef _WIN32
            return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(it->second), symbolName.c_str()));
        #else
            return dlsym(it->second, symbolName.c_str());
        #endif
    }
}
