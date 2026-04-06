#pragma once
#include <string>
#include <vector>

namespace RiftCore {
    struct UIDrawCommand {
        uint32_t VertexOffset;
        uint32_t IndexOffset;
        uint32_t TextureID;
    };
    struct UIEvent {
        enum class Type { MouseClick, KeyPress, Resize } EventType;
        bool Handled = false;
    };
    class UIManager {
    public:
        UIManager() = default;
        ~UIManager() = default;
        void LoadWorkspaceConfig(const std::string& workspaceName) { (void)workspaceName;}
        [[nodiscard]] bool RouteEvent(UIEvent& e) { (void)e; return false; }
        [[nodiscard]] const std::vector<UIDrawCommand>& GenerateDrawList() { return m_CurrentDrawList; }
    private:
        std::vector<UIDrawCommand> m_CurrentDrawList;
        std::string m_ActiveWorkspace;
    };
}
