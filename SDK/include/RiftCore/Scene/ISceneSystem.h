// ── SDK/include/RiftCore/Scene/ISceneSystem.h ─────────────────
#pragma once
#include "../Common/Platform.h"
#include "../Common/Types.h"
#include "../Common/Result.h"
#include "../ECS/IECS.h"

namespace RiftCore {

    using SceneID = u32;
    constexpr SceneID INVALID_SCENE = 0;

    struct SceneDesc {
        String name;
        String filePath;
    };

    class IScene {
    public:
        virtual ~IScene()                               = default;
        virtual SceneID     GetID()               const = 0;
        virtual String      GetName()             const = 0;
        virtual IECS*       GetECS()                    = 0;
        virtual VoidResult  Load()                      = 0;
        virtual VoidResult  Save(const String& path)    = 0;
        virtual void        Update(f32 dt)              = 0;
    };

    class ISceneSystem {
    public:
        virtual ~ISceneSystem() = default;

        virtual VoidResult         Initialize()                         = 0;
        virtual void               Shutdown()                           = 0;
        virtual Result<SceneID>    CreateScene(const SceneDesc& desc)   = 0;
        virtual void               DestroyScene(SceneID id)             = 0;
        virtual VoidResult         LoadScene(SceneID id)                = 0;
        virtual VoidResult         UnloadScene(SceneID id)              = 0;
        virtual IScene*            GetActiveScene()               const  = 0;
        virtual void               SetActiveScene(SceneID id)           = 0;
        virtual void               Update(f32 deltaTime)                = 0;
    };

} // namespace RiftCore