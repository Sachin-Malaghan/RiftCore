#pragma once

#pragma warning(push)
#pragma warning(disable: 4251 4275)

#include <RiftCore/Common/Platform.h>
#include <RiftCore/Common/Types.h>
#include <RiftCore/Scene/ISceneSystem.h>

#include <vector>
#include <string>
#include <memory>

#ifdef SCENE_EXPORTS
    #define SCENE_API RIFTCORE_EXPORT
#else
    #define SCENE_API RIFTCORE_IMPORT
#endif

namespace RiftCore {

    class SCENE_API SceneNode : public ISceneNode {
    public:
        SceneNode(SceneNodeID id, const String& name);
        ~SceneNode() = default;

        // ISceneNode
        SceneNodeID GetID()       const override { return id_;   }
        String      GetName()     const override { return name_; }
        EntityID    GetEntityID() const override {
            return entityID_;
        }
        SceneNodeID GetParentID() const override {
            return parentID_;
        }

        Vec3 GetLocalPosition() const override {
            return localPosition_;
        }
        Vec3 GetLocalRotation() const override {
            return localRotation_;
        }
        Vec3 GetLocalScale()    const override {
            return localScale_;
        }
        Vec3 GetWorldPosition() const override;

        void SetLocalPosition(const Vec3& p) override {
            localPosition_ = p;
            dirty_ = true;
        }
        void SetLocalRotation(const Vec3& r) override {
            localRotation_ = r;
            dirty_ = true;
        }
        void SetLocalScale(const Vec3& s) override {
            localScale_ = s;
            dirty_ = true;
        }
        void SetName(const String& n) override {
            name_ = n;
        }

        std::vector<SceneNodeID> GetChildren() const override {
            return children_;
        }
        bool IsActive() const override { return active_; }
        void SetActive(bool a) override { active_ = a;  }

        // Extended access for SceneSystem
        void SetEntityID(EntityID id)     { entityID_ = id;  }
        void SetParentID(SceneNodeID pid) { parentID_ = pid; }
        void SetPhysicsBodyID(u32 id)     { physicsBodyID_=id;}
        void SetAudioSourceID(u32 id)     { audioSourceID_=id;}
        u32  GetPhysicsBodyID() const     { return physicsBodyID_; }
        u32  GetAudioSourceID() const     { return audioSourceID_; }

        void AddChild   (SceneNodeID childID);
        void RemoveChild(SceneNodeID childID);

        void SetParentNode(SceneNode* parent) {
            parent_ = parent;
        }

        bool  IsDirty() const { return dirty_; }
        void  ClearDirty()    { dirty_ = false; }

        // Component flags
        bool hasMesh    = false;
        bool hasPhysics = false;
        bool hasAudio   = false;
        bool hasLight   = false;

        SceneMeshDesc    meshDesc;
        ScenePhysicsDesc physicsDesc;
        SceneAudioDesc   audioDesc;
        SceneLightDesc   lightDesc;

    private:
        SceneNodeID              id_            = INVALID_NODE;
        String                   name_;
        EntityID                 entityID_      = 0;
        SceneNodeID              parentID_      = INVALID_NODE;
        SceneNode*               parent_        = nullptr;

        Vec3                     localPosition_ = Vec3::Zero();
        Vec3                     localRotation_ = Vec3::Zero();
        Vec3                     localScale_    = Vec3::One();

        std::vector<SceneNodeID> children_;

        u32  physicsBodyID_  = 0;
        u32  audioSourceID_  = 0;
        bool active_         = true;
        bool dirty_          = true;
    };

} // namespace RiftCore

#pragma warning(pop)
