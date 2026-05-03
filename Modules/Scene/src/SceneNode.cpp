#include <Scene/SceneNode.h>
#include <algorithm>






namespace RiftCore {

    SceneNode::SceneNode(SceneNodeID id, const String& name)
        : id_(id)
        , name_(name)
    {}

    Vec3 SceneNode::GetWorldPosition() const {
        if (!parent_) {
            return localPosition_;
        }

        // Accumulate parent world position
        Vec3 parentWorld = parent_->GetWorldPosition();
        Vec3 parentScale = parent_->GetWorldPosition();

        // Simple world position (no rotation inheritance yet)
        return {
            parentWorld.x + localPosition_.x *
                parent_->GetLocalScale().x,
            parentWorld.y + localPosition_.y *
                parent_->GetLocalScale().y,
            parentWorld.z + localPosition_.z *
                parent_->GetLocalScale().z
        };
    }

    void SceneNode::AddChild(SceneNodeID childID) {
        auto it = std::find(
            children_.begin(), children_.end(), childID);
        if (it == children_.end()) {
            children_.push_back(childID);
        }
    }

    void SceneNode::RemoveChild(SceneNodeID childID) {
        children_.erase(
            std::remove(children_.begin(),
                        children_.end(), childID),
            children_.end());
    }

} // namespace RiftCore
