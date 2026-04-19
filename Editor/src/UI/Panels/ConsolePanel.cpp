#include <UI/Panels/ConsolePanel.h>
#include <imgui.h>

namespace RiftCore::UI {
void ConsolePanel::AddLog(const std::string& msg) { m_Logs.push_back(msg); }
void ConsolePanel::OnUIRender() {
    ImGui::Begin("Console");
    if (ImGui::Button("Clear")) m_Logs.clear();
    ImGui::Separator();
    for (auto& log : m_Logs) {
        ImGui::TextUnformatted(log.c_str());
    }
    ImGui::End();
}
}
