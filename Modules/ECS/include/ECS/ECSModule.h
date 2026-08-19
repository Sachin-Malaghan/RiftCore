#pragma once

#pragma warning(push)
#pragma warning(disable: 4251 4275)

#include <RiftCore/Common/Platform.h>
#include <RiftCore/Common/Types.h>
#include <RiftCore/Common/Result.h>
#include <RiftCore/Core/IModule.h>
#include <RiftCore/ECS/IECS.h>

#include <ECS/World.h>

#include <memory>

#ifdef ECS_EXPORTS
    #define ECS_API RIFTCORE_EXPORT
#else
    #define ECS_API RIFTCORE_IMPORT
#endif

namespace RiftCore {

    class ECS_API ECSModule : public IModule {
    public:
        ECSModule();
        ~ECSModule();

        VoidResult       Initialize(const ModuleInitParams& params) override;
        void             OnUpdate(f32 deltaTime)                    override;
        void             Shutdown()                                 override;
        ModuleDescriptor GetDescriptor()                      const override;

        World* GetWorld() { return world_.get(); }

    private:
        std::unique_ptr<World> world_;
    };

} // namespace RiftCore

#pragma warning(pop)
