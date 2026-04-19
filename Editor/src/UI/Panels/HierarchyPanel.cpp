#include <UI/Panels/HierarchyPanel.h>
#include <imgui.h>
#include <string>

namespace RiftCore::UI {

void HierarchyPanel::OnUIRender(ISceneSystem* scene, CommandBuffer& cb) {
    ImGui::Begin("Scene Hierarchy");

    static char searchBuffer[128] = "";
    ImGui::InputText("##Search", searchBuffer, IM_ARRAYSIZE(searchBuffer));
    ImGui::SameLine();
    if (ImGui::Button("+")) { /* Add logic */ }

    ImGui::Separator();

    if (scene) {
        scene->ForEachNode([&](ISceneNode* node) {
            ImGuiTreeNodeFlags flags = ((m_SelectedNode == node->GetID()) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
            flags |= ImGuiTreeNodeFlags_SpanAvailWidth;

            bool opened = ImGui::TreeNodeEx((void*)(intptr_t)node->GetID(), flags, node->GetName().c_str());
            
            if (ImGui::IsItemClicked()) {
                m_SelectedNode = node->GetID();
            }

            if (opened) {
                // If child nodes exist, recurse here
                ImGui::TreePop();
            }
        });
    }

    // Right-click context menu
    if (ImGui::BeginPopupContextWindow("HierarchyContext", ImGuiPopupFlags_MouseButtonRight))
    {
        if (ImGui::MenuItem("Create Empty")) {
            cb.Push({EditorCommandType::SpawnEntity, "Empty"});
        }
        ImGui::EndPopup();
    }

    ImGui::End();
}

} // namespace
