#include "RiftCore/Core/PluginLoader.h"
#include "RiftCore/UI/UIManager.h"
#include "RiftCore/RHI/IRHI.h"
#include <iostream>
#include <memory>
#include <string>
// modify and distribute or seperate all the implementation for each and everything

struct MockConfig {
    std::string RHIBackend = "RiftCore_OpenGLBackend";
    std::string Workspace = "LevelEditor";
};

typedef RiftCore::IRHI* (*CreateRHIFunc)();

int main() {
    std::cout << "--- Bootstrapping RiftCore Engine ---\n";

    MockConfig config; 
    std::cout << "[Boot] Target Workspace: " << config.Workspace << "\n";
    std::cout << "[Boot] Target RHI Backend: " << config.RHIBackend << "\n";

    RiftCore::PluginLoader pluginLoader;
    auto uiManager = std::make_unique<RiftCore::UIManager>();
    uiManager->LoadWorkspaceConfig(config.Workspace);

    std::unique_ptr<RiftCore::IRHI> rhiSystem = nullptr;

    if (pluginLoader.LoadPlugin(config.RHIBackend)) {
        auto createRHI = reinterpret_cast<CreateRHIFunc>(
            pluginLoader.GetSymbol(config.RHIBackend, "CreateRHIModule")
        );

        if (createRHI) {
            rhiSystem.reset(createRHI());
            rhiSystem->Initialize();
        } else {
            std::cerr << "[Boot] Warning: CreateRHIModule symbol not implemented yet, skipping init.\n";
        }
    } else {
        std::cerr << "[Boot] Fatal: Failed to load backend DLL.\n";
    }

    std::cout << "--- Shutdown Complete ---\n";
    return 0;
}
