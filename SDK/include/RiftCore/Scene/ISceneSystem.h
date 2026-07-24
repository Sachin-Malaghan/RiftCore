#pragma once

#include <RiftCore/Common/Platform.h>
#include <RiftCore/Common/Types.h>
#include <RiftCore/Common/Result.h>
#include <RiftCore/ECS/IECS.h>









#include <vector>
#include <string>
#include <functional>

namespace RiftCore {

    // ── Scene node handle ─────────────────────────────────────
    using SceneNodeID = u32;
    constexpr SceneNodeID INVALID_NODE = 0;

    // ── Component type flags ──────────────────────────────────
    enum class SceneComponentType : u32 {
        None      = 0,
        Transform = 1 << 0,
        Mesh      = 1 << 1,
        Physics   = 1 << 2,
        Audio     = 1 << 3,
        Light     = 1 << 4,
        Camera    = 1 << 5,
        Script    = 1 << 6
    };

    // ── Component descriptors ─────────────────────────────────
    struct SceneMeshDesc {
        String meshPath;
        String materialName;
        Vec3   albedo    = {1,1,1};
        f32    metallic  = 0.0f;
        f32    roughness = 0.5f;
    };

    struct ScenePhysicsDesc {
        bool  isStatic    = false;
        f32   mass        = 1.0f;
        f32   restitution = 0.4f;
        f32   friction    = 0.5f;
        String colliderShape = "box";  // "box","sphere","plane"
        Vec3   halfExtents   = {0.5f,0.5f,0.5f};
        f32    radius        = 0.5f;
    };

    struct SceneAudioDesc {
        String clipPath;
        f32    volume  = 1.0f;
        bool   looping = false;
        bool   is3D    = true;
        bool   playOnStart = false;
    };

    struct SceneLightDesc {
        Vec3  color     = {1,1,1};
        f32   intensity = 1.0f;
        String type     = "directional";
    };

    // ── Scene node descriptor (for creating nodes) ────────────
    struct SceneNodeDesc {
        String          name;
        Vec3            position    = Vec3::Zero();
        Vec3            rotation    = Vec3::Zero();
        Vec3            scale       = Vec3::One();
        SceneNodeID     parentID    = INVALID_NODE;

        // Optional components
        bool            hasMesh     = false;
        SceneMeshDesc   mesh;
        bool            hasPhysics  = false;
        ScenePhysicsDesc physics;
        bool            hasAudio    = false;
        SceneAudioDesc  audio;
        bool            hasLight    = false;
        SceneLightDesc  light;
    };

    // ── Scene info ────────────────────────────────────────────
    struct SceneInfo {
        String      name;
        String      filePath;
        u32         nodeCount   = 0;
        u32         entityCount = 0;
        bool        isLoaded    = false;
    };

    // ── ISceneNode ────────────────────────────────────────────
    class ISceneNode {
    public:
        virtual ~ISceneNode() = default;

        virtual SceneNodeID GetID()          const = 0;
        virtual String      GetName()        const = 0;
        virtual EntityID    GetEntityID()    const = 0;
        virtual SceneNodeID GetParentID()    const = 0;

        virtual Vec3 GetLocalPosition()      const = 0;
        virtual Vec3 GetLocalRotation()      const = 0;
        virtual Vec3 GetLocalScale()         const = 0;
        virtual Vec3 GetWorldPosition()      const = 0;

        virtual void SetLocalPosition(const Vec3& p)  = 0;
        virtual void SetLocalRotation(const Vec3& r)  = 0;
        virtual void SetLocalScale   (const Vec3& s)  = 0;
        virtual void SetName         (const String& n)= 0;

        virtual std::vector<SceneNodeID>
                     GetChildren()           const = 0;
        virtual bool IsActive()              const = 0;
        virtual void SetActive(bool active)        = 0;
    };

    // ── ISceneSystem ──────────────────────────────────────────
    class ISceneSystem {
    public:
        virtual ~ISceneSystem() = default;

        // ── Lifecycle ─────────────────────────────────────────
        virtual VoidResult Initialize()                  = 0;
        virtual void       Shutdown()                    = 0;
        virtual void       Update(f32 deltaTime)         = 0;

        // ── Scene management ──────────────────────────────────
        virtual VoidResult NewScene(const String& name)  = 0;
        virtual VoidResult SaveScene(const String& path) = 0;
        virtual VoidResult LoadScene(const String& path) = 0;
        virtual void       ClearScene()                  = 0;
        virtual SceneInfo  GetSceneInfo()          const = 0;

        // ── Node management ───────────────────────────────────
        virtual Result<SceneNodeID> CreateNode(
            const SceneNodeDesc& desc)                   = 0;
        virtual void DestroyNode(SceneNodeID id)         = 0;
        virtual ISceneNode* GetNode(SceneNodeID id)      = 0;

        // ── Iteration ─────────────────────────────────────────
        virtual void ForEachNode(
            std::function<void(ISceneNode*)> fn)         = 0;
        virtual void ForEachRootNode(
            std::function<void(ISceneNode*)> fn)         = 0;

        // ── Query ─────────────────────────────────────────────
        virtual ISceneNode* FindNode(const String& name) = 0;
        virtual u32         GetNodeCount()         const = 0;
    };

} // namespace RiftCore
