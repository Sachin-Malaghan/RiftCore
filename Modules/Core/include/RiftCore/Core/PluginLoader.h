#pragma once

#ifdef _WIN32
    #ifdef RiftCore_Core_EXPORTS
        #define RIFT_PLUGIN_API __declspec(dllexport)
    #else
        #define RIFT_PLUGIN_API __declspec(dllimport)
    #endif
#else
    #define RIFT_PLUGIN_API
#endif




#include <string>
#include <memory>
#include <unordered_map>
#include <stdexcept>

namespace RiftCore {
    #pragma warning(push)
    #pragma warning(disable: 4251)
    class RIFT_PLUGIN_API PluginLoader {
    public:
        ~PluginLoader();
        [[nodiscard]] bool LoadPlugin(const std::string& pluginName);
        void UnloadPlugin(const std::string& pluginName);
        [[nodiscard]] void* GetSymbol(const std::string& pluginName, const std::string& symbolName) const;
    private:
        std::unordered_map<std::string, void*> m_LoadedLibraries;
    };
    #pragma warning(pop)
}
