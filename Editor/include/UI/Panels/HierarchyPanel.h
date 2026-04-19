#pragma once
#include <RiftCore/Scene/ISceneSystem.h>
#include <UI/Commands/CommandBuffer.h>

namespace RiftCore::UI {
    class HierarchyPanel {
    public:
        void OnUIRender(ISceneSystem* scene, CommandBuffer& cb);
    private:
        SceneNodeID m_SelectedNode = 0;
    };
}
