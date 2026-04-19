#pragma once
#include <RiftCore/Scene/ISceneSystem.h>
#include <UI/Commands/CommandBuffer.h>

namespace RiftCore::UI {
    class InspectorPanel {
    public:
        void OnUIRender(ISceneNode* selectedNode, CommandBuffer& cb);
    private:
        void DrawTransform(ISceneNode* node);
        void DrawPhysics(ISceneNode* node);
        void DrawMaterial(ISceneNode* node);
    };
}
