#pragma once
#include <RiftCore/Common/Platform.h>
#include <RiftCore/Common/Types.h>
#include <RiftCore/Common/Result.h>
#include <RiftCore/Core/IModule.h>











namespace RiftCore {

    class IScripting : public IModule {
    public:
        virtual ~IScripting() = default;

        // ONLY Scripting-specific methods belong here now.
        // Initialize, OnUpdate, Shutdown, and GetDescriptor are inherited from IModule.
        virtual VoidResult  LoadScript(const char* filePath) = 0;
        virtual VoidResult  ExecuteString(const char* code) = 0;
        virtual void        RegisterFunction(const char* name, void(*fn)()) = 0;

        // Text the scripts printed since the last call (stdout + errors).
        // The pointer stays valid until the next call.
        virtual const char* ConsumeOutput() = 0;
        // True when a real script interpreter (Python) is available.
        virtual bool        IsAvailable() const = 0;
        // While false, per-frame script callbacks are not invoked.
        virtual void        SetSimulating(bool simulating) = 0;
    };

} // namespace RiftCore
