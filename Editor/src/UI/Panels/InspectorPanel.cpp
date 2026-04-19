#include <UI/Panels/InspectorPanel.h>
#include <UI/Styling/ImGuiTheme.h>
#include <imgui.h>
#include <Scene/SceneNode.h>
#include <cstring> // For strncpy

namespace RiftCore::UI {

    void InspectorPanel::OnUIRender(ISceneNode* node, CommandBuffer& cb) {
        ImGui::Begin("Details");

        // 1. Selection Guard: If no node is selected, show a placeholder
        if (!node) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            ImGui::TextWrapped("Select an object to view properties.");
            ImGui::PopStyleColor();
            ImGui::End();
            return;
        }

        // 2. Header Information (Using Cyan accent from the design tokens)
        ImGui::TextColored(Colors::AccentCyan, "Node ID: %u", node->GetID());

        // 3. Name Field
        char nameBuf[128];
        std::strncpy(nameBuf, node->GetName().c_str(), sizeof(nameBuf) - 1);
        nameBuf[sizeof(nameBuf) - 1] = '\0'; // Ensure null-termination

        if (ImGui::InputText("Name", nameBuf, IM_ARRAYSIZE(nameBuf))) {
            node->SetName(nameBuf);
        }

        ImGui::Separator();
        ImGui::Spacing();

        // 4. Default Component: Transform (Always present)
        DrawTransform(node);

        // 5. Conditional Components (Casting to concrete SceneNode to check flags)
        auto* sNode = static_cast<SceneNode*>(node);
        if (sNode) {
            if (sNode->hasPhysics) {
                ImGui::Spacing();
                DrawPhysics(node);
            }

            if (sNode->hasMesh) {
                ImGui::Spacing();
                DrawMaterial(node);
            }
        }

        ImGui::End();
    }

    void InspectorPanel::DrawTransform(ISceneNode* node) {
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            // --- Position ---
            Vec3 pos = node->GetLocalPosition();
            float p[3] = { pos.x, pos.y, pos.z };
            if (ImGui::DragFloat3("Position", p, 0.1f)) {
                node->SetLocalPosition({ p[0], p[1], p[2] });
            }

            // --- Rotation ---
            Vec3 rot = node->GetLocalRotation();
            float r[3] = { rot.x, rot.y, rot.z };
            if (ImGui::DragFloat3("Rotation", r, 0.5f)) {
                node->SetLocalRotation({ r[0], r[1], r[2] });
            }

            // --- Scale ---
            Vec3 scl = node->GetLocalScale();
            float s[3] = { scl.x, scl.y, scl.z };
            if (ImGui::DragFloat3("Scale", s, 0.05f)) {
                node->SetLocalScale({ s[0], s[1], s[2] });
            }
        }
    }

    void InspectorPanel::DrawMaterial(ISceneNode* node) {
        if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::TextColored(Colors::AccentCyan, "Shader: PBR_Standard");

            auto* sNode = static_cast<SceneNode*>(node);

            // Mocking the "Base Color" look from your image
            float col[3] = { sNode->meshDesc.albedo.x, sNode->meshDesc.albedo.y, sNode->meshDesc.albedo.z };
            if (ImGui::ColorEdit3("Base Color", col)) {
                sNode->meshDesc.albedo = { col[0], col[1], col[2] };
            }

            ImGui::SliderFloat("Metallic", &sNode->meshDesc.metallic, 0.0f, 1.0f);
            ImGui::SliderFloat("Roughness", &sNode->meshDesc.roughness, 0.0f, 1.0f);
        }
    }

    void InspectorPanel::DrawPhysics(ISceneNode* node) {
        if (ImGui::CollapsingHeader("RigidBody Physics", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto* sNode = static_cast<SceneNode*>(node);

            ImGui::Text("Shape: %s", sNode->physicsDesc.colliderShape.c_str());

            if (ImGui::DragFloat("Mass", &sNode->physicsDesc.mass, 0.1f, 0.0f, 100.0f)) {
                // Note: If physics is running, you might need to push a command here 
                // via the CommandBuffer instead of direct modification.
            }

            ImGui::SliderFloat("Restitution", &sNode->physicsDesc.restitution, 0.0f, 1.0f);
        }
    }

} // namespace RiftCore::UI