#pragma once
#include <UI/Styling/ImGuiTheme.h>
#include <GizmoSystem.h>

namespace RiftCore::UI {
    class ViewportPanel {
    public:
        void OnUIRender(uint32_t sceneTextureID, const ImVec2& viewportSize);
    private:
        GizmoSystem m_Gizmo;
    };
}
