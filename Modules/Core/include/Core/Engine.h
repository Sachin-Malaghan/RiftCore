#pragma once

#pragma warning(push)
#pragma warning(disable: 4251 4275)

#include <RiftCore/Common/Platform.h>
#include <RiftCore/Common/Types.h>
#include <RiftCore/Common/Result.h>
#include <RiftCore/Common/EngineContext.h>
#include <RiftCore/Core/ILogger.h>

#include <memory>
#include <chrono>

#ifdef CORE_EXPORTS
    #define CORE_API RIFTCORE_EXPORT
#else
    #define CORE_API RIFTCORE_IMPORT
#endif

namespace RiftCore {

    class Logger;
    class EventBus;
    class EngineMemoryAllocator;
    class PluginManager;

    struct EngineConfig {
        String   appName      = "RiftCore App";
        String   logFilePath  = "RiftCore.log";
        String   modulesPath  = "./";
        u32      targetFPS    = 60;
        bool     enableEditor = false;
        LogLevel logLevel     = LogLevel::Debug;
    };

    enum class EngineState : u8 {
        Uninitialized = 0,
        Initializing,
        Running,
        Paused,
        ShuttingDown,
        Shutdown
    };

    class CORE_API Engine {
    public:
        Engine();
        ~Engine();

        RIFTCORE_NOCOPY_NOMOVE(Engine);

        VoidResult Initialize(const EngineConfig& config);
        void       Run();
        void       Quit();
        void       Tick();
        void       Shutdown();

        EngineContext*         GetContext()       const;
        PluginManager*         GetPluginManager() const;
        Logger*                GetLogger()        const;
        EventBus*              GetEventBus()      const;
        EngineMemoryAllocator* GetMemory()        const;

        EngineState GetState()      const { return state_;      }
        bool        IsRunning()     const { return running_;    }
        u64         GetFrameIndex() const { return frameIndex_; }
        f32         GetDeltaTime()  const { return deltaTime_;  }

        static Engine& Get();

    private:
        void BeginFrame();
        void UpdateFrame();
        void EndFrame();
        void CalculateDeltaTime();

        EngineConfig config_;
        EngineState  state_           = EngineState::Uninitialized;
        bool         running_         = false;
        u64          frameIndex_      = 0;
        f32          deltaTime_       = 0.0f;
        f32          targetFrameTime_ = 0.0f;

        std::unique_ptr<Logger>                logger_;
        std::unique_ptr<EventBus>              eventBus_;
        std::unique_ptr<EngineMemoryAllocator> memory_;
        std::unique_ptr<PluginManager>         pluginManager_;
        std::unique_ptr<EngineContext>         context_;

        std::chrono::high_resolution_clock::time_point lastFrameTime_;

        static Engine* instance_;
    };

} // namespace RiftCore

#pragma warning(pop)
