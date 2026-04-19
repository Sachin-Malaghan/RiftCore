#include <UI/Panels/AssetBrowserPanel.h>
#include <imgui.h>

namespace RiftCore::UI {

void AssetBrowserPanel::OnUIRender() {
    ImGui::Begin("Project Browser");

    static float padding = 16.0f;
    static float thumbnailSize = 64.0f;
    float cellSize = thumbnailSize + padding;

    float panelWidth = ImGui::GetContentRegionAvail().x;
    int columnCount = (int)(panelWidth / cellSize);
    if (columnCount < 1) columnCount = 1;

    ImGui::Columns(columnCount, 0, false);

    for (int i = 0; i < 10; i++) { // Mock loop for files
        ImGui::PushID(i);
        
        // Thumbnail button (Purple/Cyan accent on interaction)
        ImGui::Button("##File", ImVec2(thumbnailSize, thumbnailSize));
        
        ImGui::TextWrapped("Asset_%d", i);
        ImGui::NextColumn();
        ImGui::PopID();
    }

    ImGui::Columns(1);
    ImGui::End();
}

} // namespace
