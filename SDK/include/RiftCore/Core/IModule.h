#pragma once

#include <RiftCore/Common/Platform.h>
#include <RiftCore/Common/Types.h>
#include <RiftCore/Common/Result.h>
#include <RiftCore/Common/Version.h>

namespace RiftCore {

    class EngineContext;

    struct ModuleInitParams {
        EngineContext* context    = nullptr;
        String         configPath;
        bool           isEditor   = false;
    };

    struct ModuleDescriptor {
        String name;
        String version;
        u32    apiVersion   = 0;
        String description;
        String author       = "RiftCore Team";
    };

    class IModule {
    public:
        virtual ~IModule() = default;

        virtual VoidResult Initialize(const ModuleInitParams& params) = 0;

        virtual void OnUpdate(f32 deltaTime) = 0;

        virtual void OnRender() {}

        // fixedDeltaTime parameter suppressed deliberately
        virtual void OnFixedUpdate(f32 /*fixedDeltaTime*/) {}

        virtual void Shutdown() = 0;

        virtual ModuleDescriptor GetDescriptor() const = 0;

        String GetName() const { return GetDescriptor().name; }
    };

    using FnCreateModule  = IModule*(*)();
    using FnDestroyModule = void(*)(IModule*);
    using FnGetModuleInfo = ModuleDescriptor(*)();

    #define RIFTCORE_IMPLEMENT_MODULE(ModuleClass)                   \
        extern "C" RIFTCORE_EXPORT                                   \
        RiftCore::IModule* CreateModule() {                          \
            return new ModuleClass();                                \
        }                                                            \
        extern "C" RIFTCORE_EXPORT                                   \
        void DestroyModule(RiftCore::IModule* module) {              \
            delete module;                                           \
        }                                                            \
        extern "C" RIFTCORE_EXPORT                                   \
        RiftCore::ModuleDescriptor GetModuleInfo() {                 \
            ModuleClass temp;                                        \
            return temp.GetDescriptor();                             \
        }

} // namespace RiftCore
