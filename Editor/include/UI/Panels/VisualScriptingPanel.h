#pragma once
#include <imgui.h>
#include <imgui_node_editor.h>

namespace ed = ax::NodeEditor;

namespace RiftCore::UI {
    class VisualScriptingPanel {
    public:
        void Initialize();
        void Shutdown();
        void OnUIRender();
    private:
        ed::EditorContext* m_EditorContext = nullptr;
    };
}
