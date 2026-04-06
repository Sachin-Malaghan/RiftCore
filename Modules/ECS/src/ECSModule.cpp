#pragma warning(disable: 4190)
#include <ECS/ECSModule.h>
#include <RiftCore/Common/EngineContext.h>
#include <RiftCore/Core/ILogger.h>
#include <iostream>

namespace RiftCore {

    ECSModule::ECSModule()  = default;
    ECSModule::~ECSModule() = default;

    VoidResult ECSModule::Initialize(const ModuleInitParams& params) {
        ILogger* logger = nullptr;
        if (params.context) {
            logger = params.context->Logger();
        }

        if (logger) logger->Info("ECS", "Initializing ECS...");

        world_ = std::make_unique<World>();

        if (params.context) {
            params.context->Register<IECS>(world_.get());
        }

        if (logger) {
            logger->Info("ECS", "ECS World ready.");
        } else {
            std::cout << "[ECS] World ready.\n";
        }

        return VoidResult::Ok();
    }

    void ECSModule::OnUpdate(f32 deltaTime) {
        if (world_) {
            world_->UpdateSystems(deltaTime);
        }
    }

    void ECSModule::Shutdown() {
        std::cout << "[ECS] Shutting down...\n";
        world_.reset();
        std::cout << "[ECS] Shutdown complete.\n";
    }

    ModuleDescriptor ECSModule::GetDescriptor() const {
        ModuleDescriptor desc;
        desc.name        = "ECS";
        desc.version     = "0.1.0";
        desc.apiVersion  = RIFTCORE_API_VERSION;
        desc.description = "Entity Component System";
        return desc;
    }

    RIFTCORE_IMPLEMENT_MODULE(ECSModule)

} // namespace RiftCore
