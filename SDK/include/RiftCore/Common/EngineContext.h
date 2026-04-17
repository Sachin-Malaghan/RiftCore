#pragma once

#pragma warning(push)
#pragma warning(disable: 4251 4275)

#include <RiftCore/Common/Platform.h>
#include <RiftCore/Common/Types.h>
#include <RiftCore/Common/Result.h>

// Include ALL interface headers so types are fully defined
#include <RiftCore/Core/ILogger.h>
#include <RiftCore/Core/IEventBus.h>
#include <RiftCore/Core/IMemoryAllocator.h>
#include <RiftCore/Job/IJobSystem.h>
#include <RiftCore/ECS/IECS.h>
#include <RiftCore/Renderer/IRenderer.h>
#include <RiftCore/RHI/IRHIDevice.h>
#include <RiftCore/Physics/IPhysics.h>
#include <RiftCore/Input/IInput.h>
#include <RiftCore/Audio/IAudio.h>
#include <RiftCore/Asset/IAssetSystem.h>
#include <RiftCore/Scene/ISceneSystem.h>
#include <RiftCore/Scripting/IScripting.h>
#include <RiftCore/VR/IVRModule.h>

#include <unordered_map>
#include <typeindex>
#include <mutex>

namespace RiftCore {
    class IScripting;
    class EngineContext {
    public:
        EngineContext()  = default;
        ~EngineContext() = default;

        RIFTCORE_NOCOPY_NOMOVE(EngineContext);

        // Register a service by interface type
        template<typename Interface>
        void Register(Interface* service) {
            std::lock_guard<std::mutex> lock(mutex_);
            auto key       = std::type_index(typeid(Interface));
            services_[key] = static_cast<void*>(service);
        }

        // Get a service by interface type - returns nullptr if missing
        template<typename Interface>
        Interface* Get() const {
            std::lock_guard<std::mutex> lock(mutex_);
            auto key = std::type_index(typeid(Interface));
            auto it  = services_.find(key);
            if (it == services_.end()) return nullptr;
            return static_cast<Interface*>(it->second);
        }

        // Get - asserts if not found
        template<typename Interface>
        Interface& Require() const {
            auto* ptr = Get<Interface>();
            RIFTCORE_ASSERT_MSG(
                ptr != nullptr,
                "Required service not registered in EngineContext"
            );
            return *ptr;
        }

        // Check if registered
        template<typename Interface>
        bool Has() const {
            std::lock_guard<std::mutex> lock(mutex_);
            auto key = std::type_index(typeid(Interface));
            return services_.find(key) != services_.end();
        }

        // Unregister
        template<typename Interface>
        void Unregister() {
            std::lock_guard<std::mutex> lock(mutex_);
            auto key = std::type_index(typeid(Interface));
            services_.erase(key);
        }

        void Clear() {
            std::lock_guard<std::mutex> lock(mutex_);
            services_.clear();
        }

        // Typed convenience accessors
        ILogger*          Logger()    const { return Get<ILogger>();          }
        IEventBus*        EventBus()  const { return Get<IEventBus>();        }
        IMemoryAllocator* Memory()    const { return Get<IMemoryAllocator>(); }
        IJobSystem*       JobSystem() const { return Get<IJobSystem>();       }
        IECS*             ECS()       const { return Get<IECS>();             }
        IRenderer*        Renderer()  const { return Get<IRenderer>();        }
        IRHI*             RHI()       const { return Get<IRHI>();             }
        IPhysics*         Physics()   const { return Get<IPhysics>();         }
        IInput*           Input()     const { return Get<IInput>();           }
        IAudio*           Audio()     const { return Get<IAudio>();           }
        IAssetSystem*     Assets()    const { return Get<IAssetSystem>();     }
        ISceneSystem*     Scene()     const { return Get<ISceneSystem>();     }
        IScripting*       Scripting() const { return Get<IScripting>();       }
        IVRModule*        VR()        const { return Get<IVRModule>();        }

    private:
        mutable std::mutex                             mutex_;
        std::unordered_map<std::type_index, void*>    services_;
    };

} // namespace RiftCore

#pragma warning(pop)
