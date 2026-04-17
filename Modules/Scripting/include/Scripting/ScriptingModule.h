#pragma once

#include <RiftCore/Scripting/IScripting.h>
#include <RiftCore/Scene/ISceneSystem.h>
#include <RiftCore/Core/ILogger.h>
#include <mutex>
#include <queue>
#include <atomic>
#include <string> // We can safely use std::string INTERNALLY

namespace RiftCore {

    struct AutomationCommand {
        enum class Type { SpawnNode, InjectFlux, ClearScene };
        Type type;
        SceneNodeDesc nodeDesc;
        f32 fluxAmount;
    };

    class ScriptingModule : public IScripting {
    public:
        ScriptingModule();
        virtual ~ScriptingModule() override;

        // Core lifecycle methods
        // --- IModule Interface Overrides ---
        virtual VoidResult  Initialize(const ModuleInitParams& params) override; // Fixed signature
        virtual void        Shutdown() override;
        virtual void        OnUpdate(f32 deltaTime) override;                    // Renamed to OnUpdate
        virtual ModuleDescriptor GetDescriptor() const override;                 // Added missing method

        // --- IScripting Interface Overrides ---
        virtual VoidResult  LoadScript(const char* filePath) override;
        virtual VoidResult  ExecuteString(const char* code) override;
        virtual void        RegisterFunction(const char* name, void(*fn)()) override;

    private:
        void ProcessCommandQueue();

        ISceneSystem* m_scene;
        ILogger* m_logger;
        std::atomic<bool> m_initialized{ false };

        std::queue<AutomationCommand> m_commandQueue;
        std::mutex m_queueMutex;
    };
}