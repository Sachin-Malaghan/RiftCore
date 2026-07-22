#include <UI/Styling/ImGuiTheme.h>





void RiftCore::UI::ImGuiTheme::ApplyStyle() {
    auto& style = ImGui::GetStyle();
    auto& colors = style.Colors;

    style.WindowRounding = 8.0f;
    style.FrameRounding = 4.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.Colors[ImGuiCol_Border] = ImVec4(1, 1, 1, 0.12f); // Thin glass highlight

    colors[ImGuiCol_WindowBg]         = Colors::PanelBg;
    colors[ImGuiCol_ChildBg]          = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_PopupBg]          = Colors::Background;
    
    colors[ImGuiCol_Header]           = ImVec4(0.00f, 1.00f, 1.00f, 0.15f);
    colors[ImGuiCol_HeaderHovered]    = ImVec4(0.00f, 1.00f, 1.00f, 0.35f);
    colors[ImGuiCol_HeaderActive]     = Colors::AccentCyan;

    colors[ImGuiCol_Button]           = ImVec4(0.15f, 0.15f, 0.20f, 1.00f);
    colors[ImGuiCol_ButtonHovered]    = Colors::AccentPurple;
    colors[ImGuiCol_ButtonActive]     = Colors::AccentPurple;

    colors[ImGuiCol_Tab]              = ImVec4(0.10f, 0.10f, 0.15f, 1.00f);
    colors[ImGuiCol_TabHovered]       = Colors::AccentCyan;
    colors[ImGuiCol_TabActive]        = Colors::AccentPurple;
}
