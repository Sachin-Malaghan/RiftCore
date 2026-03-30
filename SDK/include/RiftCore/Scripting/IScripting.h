// ── SDK/include/RiftCore/Scripting/IScripting.h ───────────────
#pragma once
#include "../Common/Platform.h"
#include "../Common/Types.h"
#include "../Common/Result.h"

namespace RiftCore {

    class IScripting {
    public:
        virtual ~IScripting() = default;

        virtual VoidResult  Initialize()                                    = 0;
        virtual void        Shutdown()                                      = 0;
        virtual VoidResult  LoadScript(const String& filePath)             = 0;
        virtual VoidResult  ExecuteString(const String& code)              = 0;
        virtual void        Update(f32 deltaTime)                           = 0;

        // Call a script function by name
        virtual VoidResult  CallFunction(
            const String& name,
            const std::vector<String>& args = {}
        ) = 0;

        // Register C++ function callable from scripts
        virtual void RegisterFunction(
            const String& name,
            std::function<void()> fn
        ) = 0;
    };

} // namespace RiftCore