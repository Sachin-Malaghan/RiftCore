#pragma once



#pragma warning(push)
#pragma warning(disable: 4251 4275)

#include <RiftCore/Common/Platform.h>
#include <RiftCore/Common/Types.h>
#include <RiftCore/Common/Result.h>
#include <RiftCore/Common/EngineContext.h>
#include <RiftCore/Core/IModule.h>
#include <RiftCore/Scene/ISceneSystem.h>

#include <Scene/SceneNode.h>

#include <unordered_map>
#include <vector>
#include <memory>
#include <string>
#include <atomic>
#include <mutex>
#include <functional>

#ifdef SCENE_EXPORTS
    #define SCENE_API RIFTCORE_EXPORT
#else
    #define SCENE_API RIFTCORE_IMPORT
#endif

namespace RiftCore {

    class ILogger;
    class IECS;
    class IPhysics;
    class IAudio;
    class IRenderer;

    class SCENE_API SceneSystem : public ISceneSystem {
    public:
        SceneSystem();
        ~SceneSystem();

        RIFTCORE_NOCOPY_NOMOVE(SceneSystem);

        // ── Setup ─────────────────────────────────────────────
        void SetContext(EngineContext* ctx) {
            context_ = ctx;
        }

        // ISceneSystem
        VoidResult Initialize()               override;
        void       Shutdown()                 override;
        void       Update(f32 deltaTime)      override;

        VoidResult NewScene (const String& name) override;
        VoidResult SaveScene(const String& path) override;
        VoidResult LoadScene(const String& path) override;
        void       ClearScene()               override;
        SceneInfo  GetSceneInfo()       const override;

        Result<SceneNodeID> CreateNode(
            const SceneNodeDesc& desc)        override;
        void       DestroyNode(SceneNodeID id)override;
        ISceneNode* GetNode(SceneNodeID id)   override;

        void ForEachNode(
            std::function<void(ISceneNode*)> fn) override;
        void ForEachRootNode(
            std::function<void(ISceneNode*)> fn) override;

        ISceneNode* FindNode(const String& name) override;
        u32         GetNodeCount() const override {
            return static_cast<u32>(nodes_.size());
        }

    private:
        // ── Node creation helpers ─────────────────────────────
        void CreateECSEntity (SceneNode& node,
                              const SceneNodeDesc& desc);
        void CreatePhysicsBody(SceneNode& node,
                               const SceneNodeDesc& desc);
        void CreateAudioSource(SceneNode& node,
                               const SceneNodeDesc& desc);

        // ── JSON serialization ────────────────────────────────
        void SerializeNode  (const SceneNode& node,
                             void* jsonObj) const;
        void DeserializeNode(const void* jsonObj);

        // ── Access helpers ────────────────────────────────────
        SceneNode* GetNodeRaw(SceneNodeID id);

        EngineContext* context_ = nullptr;
        ILogger*       logger_  = nullptr;

        std::unordered_map<SceneNodeID,
            std::unique_ptr<SceneNode>> nodes_;
        std::vector<SceneNodeID>        rootNodes_;

        std::atomic<SceneNodeID> nextNodeID_{ 1 };
        mutable std::mutex       mutex_;

        String sceneName_;
        String sceneFilePath_;
    };

    // ── IModule wrapper ───────────────────────────────────────
    class SCENE_API SceneModule : public IModule {
    public:
        SceneModule();
        ~SceneModule();

        VoidResult       Initialize(
            const ModuleInitParams& params) override;
        void             OnUpdate(f32 dt)   override;
        void             Shutdown()         override;
        ModuleDescriptor GetDescriptor()
                                      const override;

        SceneSystem* GetSceneSystem() {
            return scene_.get();
        }

    private:
        std::unique_ptr<SceneSystem> scene_;
    };

} // namespace RiftCore

#pragma warning(pop)
