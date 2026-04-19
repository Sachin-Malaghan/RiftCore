#include <UI/Panels/ViewportPanel.h>
#include <imgui.h>
#include <ImGuizmo.h>

namespace RiftCore::UI {

void ViewportPanel::OnUIRender(uint32_t sceneTextureID, const ImVec2& viewportSize) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoScrollbar);
    
    ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
    
    // The 3D Engine Texture
    ImGui::Image((void*)(intptr_t)sceneTextureID, viewportPanelSize, {0, 1}, {1, 0});

    // --- Telemetry Overlay (Top Left) ---
    ImGui::SetCursorPos(ImVec2(10, 30));
    ImGui::BeginChild("Telemetry", ImVec2(150, 60), false, ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground);
    ImGui::TextColored(Colors::AccentCyan, "FPS: %.0f", ImGui::GetIO().Framerate);
    ImGui::TextColored(Colors::AccentCyan, "VRAM: 8.2 GB / 16 GB"); // Placeholder for GPU query
    ImGui::EndChild();

    // --- Orientation Cube (Top Right) ---
    // Note: This requires the view matrix from your camera
    // ImGuizmo::ViewManipulate(...) would go here

    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace
