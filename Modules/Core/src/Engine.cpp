#include <Core/Engine.h>
#include <Core/Logger.h>
#include <Core/EventBus.h>
#include <Core/MemoryAllocator.h>
#include <Core/PluginManager.h>
#include <RiftCore/Core/IEventBus.h>
#include <thread>
#include <iostream>










namespace RiftCore {

    Engine* Engine::instance_ = nullptr;

    Engine::Engine() {
        instance_ = this;
    }

    Engine::~Engine() {
        if (state_ != EngineState::Shutdown) {
            Shutdown();
        }
        instance_ = nullptr;
    }

    Engine& Engine::Get() {
        assert(instance_ && "Engine not created");
        return *instance_;
    }

    VoidResult Engine::Initialize(const EngineConfig& config) {
        config_ = config;
        state_  = EngineState::Initializing;

        targetFrameTime_ = 1.0f / static_cast<f32>(config_.targetFPS);

        // Create logger first - everything else needs it
        logger_ = std::make_unique<Logger>();
        logger_->SetLevel(config_.logLevel);
        logger_->AddSink(
            std::make_shared<FileLogSink>(config_.logFilePath)
        );

        logger_->Info("Engine", "==============================");
        logger_->Info("Engine", "  RiftCore Engine Starting    ");
        logger_->Info("Engine", "==============================");
        logger_->Info("Engine", "App: " + config_.appName);

        // Create memory system
        memory_ = std::make_unique<EngineMemoryAllocator>();
        logger_->Info("Engine", "Memory system initialized");

        // Create event bus
        eventBus_ = std::make_unique<EventBus>();
        logger_->Info("Engine", "Event bus initialized");

        // Create plugin manager
        pluginManager_ = std::make_unique<PluginManager>();
        logger_->Info("Engine", "Plugin manager initialized");

        // Create DI context and register core services
        context_ = std::make_unique<EngineContext>();
        context_->Register<ILogger>(logger_.get());
        context_->Register<IEventBus>(eventBus_.get());
        context_->Register<IMemoryAllocator>(memory_.get());

        logger_->Info("Engine", "Engine context created");

        state_ = EngineState::Running;
        lastFrameTime_ = std::chrono::high_resolution_clock::now();

        logger_->Info("Engine", "Engine initialized successfully");

        // Fire start event
        eventBus_->Publish(EngineStartEvent{});

        return VoidResult::Ok();
    }

    void Engine::Run() {
        assert(state_ == EngineState::Running);
        running_ = true;

        logger_->Info("Engine", "Entering main loop");

        while (running_) {
            Tick();
        }

        logger_->Info("Engine", "Exiting main loop");
        Shutdown();
    }

    void Engine::Tick() {
        CalculateDeltaTime();
        BeginFrame();
        UpdateFrame();
        EndFrame();
    }

    void Engine::CalculateDeltaTime() {
        auto now  = std::chrono::high_resolution_clock::now();
        auto diff = std::chrono::duration<f32>(
            now - lastFrameTime_
        );
        deltaTime_     = diff.count();
        lastFrameTime_ = now;

        // Cap delta time to prevent spiral of death
        if (deltaTime_ > 0.1f) deltaTime_ = 0.1f;
    }

    void Engine::BeginFrame() {
        frameIndex_++;
        memory_->BeginFrame();
        eventBus_->ProcessQueue();
        eventBus_->Publish(FrameBeginEvent{ deltaTime_, frameIndex_ });
    }

    void Engine::UpdateFrame() {
        pluginManager_->UpdateAll(deltaTime_);
    }

    void Engine::EndFrame() {
        pluginManager_->RenderAll();
        memory_->EndFrame();
        eventBus_->Publish(FrameEndEvent{ frameIndex_ });
    }

    void Engine::Quit() {
        logger_->Info("Engine", "Quit requested");
        running_ = false;
    }

    void Engine::Shutdown() {
        if (state_ == EngineState::Shutdown) return;
        state_ = EngineState::ShuttingDown;

        logger_->Info("Engine", "Shutting down...");

        eventBus_->Publish(EngineShutdownEvent{});

        pluginManager_->UnloadAll();
        pluginManager_.reset();

        context_.reset();
        eventBus_.reset();
        memory_.reset();

        logger_->Info("Engine", "Shutdown complete.");
        logger_->Flush();
        logger_.reset();

        state_ = EngineState::Shutdown;
    }

    EngineContext*         Engine::GetContext()       const { return context_.get();       }
    PluginManager*         Engine::GetPluginManager() const { return pluginManager_.get(); }
    Logger*                Engine::GetLogger()        const { return logger_.get();         }
    EventBus*              Engine::GetEventBus()      const { return eventBus_.get();       }
    EngineMemoryAllocator* Engine::GetMemory()        const { return memory_.get();         }

} // namespace RiftCore
