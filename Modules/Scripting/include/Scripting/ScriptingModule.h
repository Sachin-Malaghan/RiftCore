#pragma once

#include <RiftCore/Scripting/IScripting.h>
#include <RiftCore/Scene/ISceneSystem.h>
#include <RiftCore/Core/ILogger.h>
#include <string>
#include <vector>

namespace RiftCore {

    class EngineContext;
    class IPhysics;

    // Embedded Python scripting. Scripts import the built-in `riftcore`
    // module to build and drive the scene (see Assets/Scripts/README.md).
    // When the engine is built without Python the module still loads and
    // reports IsAvailable() == false.
    class ScriptingModule : public IScripting {
    public:
        ScriptingModule();
        ~ScriptingModule() override;

        // --- IModule ---
        VoidResult       Initialize(const ModuleInitParams& params) override;
        void             Shutdown() override;
        void             OnUpdate(f32 deltaTime) override;
        ModuleDescriptor GetDescriptor() const override;

        // --- IScripting ---
        VoidResult  LoadScript(const char* filePath) override;
        VoidResult  ExecuteString(const char* code) override;
        void        RegisterFunction(const char* name, void(*fn)()) override;
        const char* ConsumeOutput() override;
        bool        IsAvailable() const override { return available_; }
        void        SetSimulating(bool simulating) override { simulating_ = simulating; }

        // --- Used by the Python bindings ---
        ISceneSystem* Scene()   const;
        IPhysics*     Physics() const;
        ILogger*      Logger()  const;
        void  AppendOutput(const char* text);
        void  AddUpdateCallback(void* pyCallable);
        void  ClearUpdateCallbacks();
        f64   GetTime()      const { return time_; }
        bool  IsSimulating() const { return simulating_; }

    private:
        VoidResult Run(const std::string& source, const char* fileName, bool interactive);

        EngineContext* context_     = nullptr;
        bool           initialized_ = false;
        bool           available_   = false;
        bool           simulating_  = true;
        f64            time_        = 0.0;

        std::string        output_;
        std::string        consumed_;
        std::vector<void*> updateCallbacks_;   // PyObject*
    };

} // namespace RiftCore
