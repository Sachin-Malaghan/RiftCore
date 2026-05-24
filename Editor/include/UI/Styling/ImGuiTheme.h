#pragma once
#include <imgui.h>


namespace RiftCore::UI {
    struct Colors {
        static constexpr ImVec4 Background      = { 0.05f, 0.05f, 0.07f, 1.00f }; 
        static constexpr ImVec4 PanelBg         = { 0.10f, 0.10f, 0.14f, 0.70f }; // 70% opacity for glass effect
        static constexpr ImVec4 AccentCyan      = { 0.00f, 1.00f, 1.00f, 1.00f }; 
        static constexpr ImVec4 AccentPurple    = { 0.75f, 0.25f, 0.75f, 1.00f }; 
        static constexpr ImVec4 SuccessGreen    = { 0.20f, 0.80f, 0.20f, 1.00f }; 
    };

    class ImGuiTheme {
    public:
        static void ApplyStyle();
    };
}
