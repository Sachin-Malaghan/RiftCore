#include <UI/Panels/VisualScriptingPanel.h>
#include <UI/Styling/ImGuiTheme.h>

namespace RiftCore::UI {

void VisualScriptingPanel::Initialize() {
    ed::Config config;
    config.SettingsFile = "Blueprints.json";
    m_EditorContext = ed::CreateEditor(&config);
}

void VisualScriptingPanel::Shutdown() {
    if (m_EditorContext) {
        ed::DestroyEditor(m_EditorContext);
        m_EditorContext = nullptr;
    }
}

void VisualScriptingPanel::OnUIRender() {
    ImGui::Begin("Visual Scripting");

    if (!m_EditorContext) {
        ImGui::TextColored(ImVec4(1,0,0,1), "Node Editor Context not initialized!");
        ImGui::End();
        return;
    }

    ed::SetCurrentEditor(m_EditorContext);
    ed::Begin("Scripton Node Editor");

    // --- Mockup Node 1: Event Update ---
    ed::BeginNode(1);
        ImGui::TextColored(Colors::AccentCyan, "Event OnUpdate");
        ImGui::Dummy(ImVec2(0, 10)); // Spacing
        
        ed::BeginPin(101, ed::PinKind::Output);
            ImGui::Text("Execute ->");
        ed::EndPin();
    ed::EndNode();

    // --- Mockup Node 2: Move Actor ---
    ed::BeginNode(2);
        ImGui::TextColored(Colors::AccentPurple, "Add Local Offset");
        ImGui::Dummy(ImVec2(0, 10));
        
        ed::BeginPin(201, ed::PinKind::Input);
            ImGui::Text("-> Execute");
        ed::EndPin();
        
        ImGui::SameLine(100); // Space between input and output pins
        
        ed::BeginPin(202, ed::PinKind::Output);
            ImGui::Text("Out ->");
        ed::EndPin();

        // Parameter input
        ed::BeginPin(203, ed::PinKind::Input);
            ImGui::Text("-> Delta X");
        ed::EndPin();
    ed::EndNode();

    // --- Draw the Link (Bezier Curve) ---
    ed::Link(1001, 101, 201); // Link ID, Start Pin ID, End Pin ID

    ed::End();
    ed::SetCurrentEditor(nullptr);

    ImGui::End();
}

} // namespace
