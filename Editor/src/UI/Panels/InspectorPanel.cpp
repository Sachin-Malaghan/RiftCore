/**
 * @file InspectorPanel.cpp
 * @brief Production-grade Inspector/Details Panel for RiftCore Engine
 * 
 * This panel provides a property editor for selected scene nodes, similar to
 * Unreal Engine's Details panel. Displays and edits transform, components,
 * materials, physics properties, and custom properties.
 * 
 * @author RiftCore Team
 * @version 2.0.0
 * @date 2026-04-21
 * 
 * @note Architecture inspired by Unreal Engine's SDetailsView
 * 
 * ============================================================================
 * EXTERNAL DEPENDENCIES (TODO: Implement these interfaces)
 * ============================================================================
 * - ISceneNode: Scene node interface with component access
 * - IComponent: Base component interface
 * - IPropertyEditor: Custom property editor interface
 * - CommandBuffer: Command pattern for undo/redo support
 * - EventDispatcher: For property change notifications
 * - AssetThumbnailProvider: For asset preview thumbnails
 * ============================================================================
 */











#include <UI/Panels/InspectorPanel.h>
#include <UI/Styling/ImGuiTheme.h>
#include <imgui.h>
#include <Scene/SceneNode.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <functional>
#include <unordered_map>

// TODO: Include your engine's component system headers
// #include <Components/TransformComponent.h>
// #include <Components/MeshComponent.h>
// #include <Components/PhysicsComponent.h>
// #include <Editor/CommandBuffer.h>

namespace RiftCore::UI {

//=============================================================================
// CONFIGURATION CONSTANTS
//=============================================================================

namespace InspectorConfig {
    /** Label column width ratio */
    constexpr float LABEL_WIDTH_RATIO = 0.35f;
    
    /** Minimum label width */
    constexpr float MIN_LABEL_WIDTH = 100.0f;
    
    /** Maximum label width */
    constexpr float MAX_LABEL_WIDTH = 200.0f;
    
    /** Drag speed for float values */
    constexpr float FLOAT_DRAG_SPEED = 0.1f;
    
    /** Drag speed for rotation values (degrees) */
    constexpr float ROTATION_DRAG_SPEED = 0.5f;
    
    /** Drag speed for scale values */
    constexpr float SCALE_DRAG_SPEED = 0.01f;
    
    /** Default collapsed header state */
    constexpr bool DEFAULT_HEADER_OPEN = true;
}

//=============================================================================
// STATIC STATE (uses types from header)
//=============================================================================

static FInspectorState s_State;
static ISceneNode* s_LastSelectedNode = nullptr;

//=============================================================================
// FORWARD DECLARATIONS
//=============================================================================

static void DrawToolbar(ISceneNode* node);
static void DrawNodeHeader(ISceneNode* node);
static void DrawComponentHeader(const char* name, const char* icon, bool& isOpen, 
                                 std::function<void()> contextMenu = nullptr);
static void DrawTransformComponent(ISceneNode* node, CommandBuffer& cb);
static void DrawMaterialComponent(ISceneNode* node, CommandBuffer& cb);
static void DrawPhysicsComponent(ISceneNode* node, CommandBuffer& cb);
static void DrawLightComponent(ISceneNode* node, CommandBuffer& cb);
static void DrawCameraComponent(ISceneNode* node, CommandBuffer& cb);
static void DrawAudioComponent(ISceneNode* node, CommandBuffer& cb);
static void DrawScriptComponent(ISceneNode* node, CommandBuffer& cb);
static void DrawAddComponentButton(ISceneNode* node, CommandBuffer& cb);

// Property drawing helpers
static bool DrawFloatProperty(const char* label, float& value, float speed = 0.1f, 
                               float min = 0.0f, float max = 0.0f, const char* format = "%.3f");
static bool DrawFloat2Property(const char* label, float* values, float speed = 0.1f);
static bool DrawFloat3Property(const char* label, float* values, float speed = 0.1f, 
                                bool isColor = false);
static bool DrawFloat4Property(const char* label, float* values, float speed = 0.1f);
static bool DrawIntProperty(const char* label, int& value, int min = 0, int max = 0);
static bool DrawBoolProperty(const char* label, bool& value);
static bool DrawStringProperty(const char* label, char* buffer, size_t bufferSize);
static bool DrawEnumProperty(const char* label, int& currentItem, const char* const* items, int itemCount);
static bool DrawAssetProperty(const char* label, const char* assetType, uint64_t& assetID, 
                               const std::string& currentName);
static void DrawPropertyLabel(const char* label, const char* tooltip = nullptr);
static void BeginPropertyRow();
static void EndPropertyRow();

//=============================================================================
// PUBLIC API IMPLEMENTATION
//=============================================================================

/**
 * @brief Main render function for the Inspector Panel
 * 
 * Called every frame by the UI system. Displays and allows editing of
 * properties for the selected scene node.
 * 
 * @param node The selected scene node (can be null)
 * @param cb Command buffer for undo/redo operations
 * 
 * Layout:
 * ┌─────────────────────────────────────────────────────────────┐
 * │ [Search...              ] [Lock 🔒] [⋮ Menu]               │
 * ├─────────────────────────────────────────────────────────────┤
 * │ Node: PlayerCharacter                          ID: 12345   │
 * │ Type: Actor                                                │
 * ├─────────────────────────────────────────────────────────────┤
 * │ ▼ Transform                                          [⋮]   │
 * │   Position    [X: 0.0  ] [Y: 0.0  ] [Z: 0.0  ]            │
 * │   Rotation    [X: 0.0  ] [Y: 0.0  ] [Z: 0.0  ]            │
 * │   Scale       [X: 1.0  ] [Y: 1.0  ] [Z: 1.0  ]            │
 * ├─────────────────────────────────────────────────────────────┤
 * │ ▼ Material                                           [⋮]   │
 * │   Shader      PBR_Standard                                 │
 * │   Base Color  [████████████████] #FF5500                   │
 * │   Metallic    [═══════════○════] 0.5                       │
 * │   Roughness   [════════○═══════] 0.3                       │
 * ├─────────────────────────────────────────────────────────────┤
 * │ ▶ Physics (collapsed)                                      │
 * ├─────────────────────────────────────────────────────────────┤
 * │                    [+ Add Component]                       │
 * └─────────────────────────────────────────────────────────────┘
 */
void InspectorPanel::OnUIRender(ISceneNode* node, CommandBuffer& cb) {
    static ImGuiContext* s_Ctx = ImGui::GetCurrentContext();
    if (!s_Ctx) return;
    ImGui::SetCurrentContext(s_Ctx);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    bool windowOpen = ImGui::Begin("Details", nullptr, ImGuiWindowFlags_MenuBar);
    ImGui::PopStyleVar();
    if (!windowOpen) {
        ImGui::End();
        return;
    }
    
    // Determine which node to display
    ISceneNode* displayNode = s_State.bLockSelection ? 
        reinterpret_cast<ISceneNode*>(s_State.LockedNodeID) : node;
    
    // Menu bar
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Show Advanced", nullptr, &s_State.bShowAdvanced);
            ImGui::MenuItem("Show Read-Only", nullptr, &s_State.bShowReadOnly);
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Actions")) {
            if (ImGui::MenuItem("Reset to Default", nullptr, false, displayNode != nullptr)) {
                // TODO: Reset all properties to default
            }
            if (ImGui::MenuItem("Copy All Properties", nullptr, false, displayNode != nullptr)) {
                // TODO: Copy properties to clipboard
            }
            if (ImGui::MenuItem("Paste Properties", nullptr, false, displayNode != nullptr)) {
                // TODO: Paste properties from clipboard
            }
            ImGui::EndMenu();
        }
        
        ImGui::EndMenuBar();
    }
    
    // Check for no selection
    if (!displayNode) {
        DrawNoSelectionPlaceholder();
        ImGui::End();
        return;
    }
    
    // Track selection changes
    if (displayNode != s_LastSelectedNode) {
        s_LastSelectedNode = displayNode;
        // Could trigger animations or initialization here
    }
    
    // Calculate label width based on window width
    float windowWidth = ImGui::GetContentRegionAvail().x;
    s_State.LabelWidth = std::clamp(
        windowWidth * InspectorConfig::LABEL_WIDTH_RATIO,
        InspectorConfig::MIN_LABEL_WIDTH,
        InspectorConfig::MAX_LABEL_WIDTH
    );
    
    // Toolbar
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 4));
    DrawToolbar(displayNode);
    ImGui::PopStyleVar();
    
    ImGui::Separator();
    
    // Scrollable content area
    ImGui::BeginChild("InspectorContent", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
    
    // Node header info
    DrawNodeHeader(displayNode);
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // === Transform Component (Always Present) ===
    DrawTransformComponent(displayNode, cb);
    
    // === Conditional Components ===
    auto* sceneNode = static_cast<SceneNode*>(displayNode);
    if (sceneNode) {
        // Material Component (if has mesh)
        if (sceneNode->hasMesh) {
            ImGui::Spacing();
            DrawMaterialComponent(displayNode, cb);
        }
        
        // Physics Component (if has physics)
        if (sceneNode->hasPhysics) {
            ImGui::Spacing();
            DrawPhysicsComponent(displayNode, cb);
        }
        
        // TODO: Add more component checks
        // if (sceneNode->hasLight) DrawLightComponent(displayNode, cb);
        // if (sceneNode->hasCamera) DrawCameraComponent(displayNode, cb);
        // if (sceneNode->hasAudio) DrawAudioComponent(displayNode, cb);
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Add Component Button
    DrawAddComponentButton(displayNode, cb);
    
    ImGui::PopStyleVar();
    ImGui::EndChild();
    
    ImGui::End();
}

/**
 * @brief Draws a placeholder when nothing is selected
 */
void InspectorPanel::DrawNoSelectionPlaceholder() {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
    
    float windowWidth = ImGui::GetContentRegionAvail().x;
    float windowHeight = ImGui::GetContentRegionAvail().y;
    
    const char* text = "Select an object to view properties";
    ImVec2 textSize = ImGui::CalcTextSize(text);
    
    ImGui::SetCursorPos(ImVec2(
        (windowWidth - textSize.x) * 0.5f,
        (windowHeight - textSize.y) * 0.5f
    ));
    
    ImGui::TextWrapped("%s", text);
    
    ImGui::PopStyleColor();
}

//=============================================================================
// INTERNAL IMPLEMENTATIONS
//=============================================================================

/**
 * @brief Draws the toolbar with search and actions
 * @param node The selected node
 */
static void DrawToolbar(ISceneNode* node) {
    (void)node;
    
    // Search filter
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 80.0f);
    if (ImGui::InputTextWithHint("##Search", "Search properties...", 
        s_State.SearchBuffer, sizeof(s_State.SearchBuffer))) {
        // Filter is applied per-property during drawing
    }
    
    ImGui::SameLine();
    
    // Lock button
    const char* lockIcon = s_State.bLockSelection ? "L" : "U";
    ImGui::PushStyleColor(ImGuiCol_Button, s_State.bLockSelection 
        ? ImVec4(0.8f, 0.4f, 0.0f, 1.0f) 
        : ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
    if (ImGui::Button(lockIcon, ImVec2(25, 0))) {
        s_State.bLockSelection = !s_State.bLockSelection;
        if (s_State.bLockSelection && node) {
            s_State.LockedNodeID = node->GetID();
        }
    }
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(s_State.bLockSelection ? "Unlock inspector" : "Lock to current selection");
    }
    
    ImGui::SameLine();
    
    // Options menu
    if (ImGui::Button("...", ImVec2(25, 0))) {
        ImGui::OpenPopup("InspectorOptions");
    }
    
    if (ImGui::BeginPopup("InspectorOptions")) {
        ImGui::MenuItem("Show Advanced", nullptr, &s_State.bShowAdvanced);
        ImGui::MenuItem("Show Read-Only", nullptr, &s_State.bShowReadOnly);
        ImGui::Separator();
        if (ImGui::MenuItem("Collapse All")) {
            for (auto& [key, value] : s_State.ComponentExpanded) {
                value = false;
            }
        }
        if (ImGui::MenuItem("Expand All")) {
            for (auto& [key, value] : s_State.ComponentExpanded) {
                value = true;
            }
        }
        ImGui::EndPopup();
    }
}

/**
 * @brief Draws the node header with name and ID
 * @param node The selected node
 */
static void DrawNodeHeader(ISceneNode* node) {
    // Node name (editable)
    char nameBuf[128];
    std::strncpy(nameBuf, node->GetName().c_str(), sizeof(nameBuf) - 1);
    nameBuf[sizeof(nameBuf) - 1] = '\0';
    
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 100.0f);
    if (ImGui::InputText("##NodeName", nameBuf, sizeof(nameBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
        node->SetName(nameBuf);
        // TODO: Push undo command
        // cb.Push({EditorCommandType::SetProperty, node->GetID(), "Name", nameBuf});
    }
    ImGui::PopStyleColor();
    
    // Node ID (read-only)
    ImGui::SameLine();
    ImGui::TextColored(Colors::AccentCyan, "ID: %u", node->GetID());
    
    // Node type
    ImGui::TextDisabled("Type: %s", "Actor"); // TODO: Get actual type
    
    // Active/enabled toggle
    // TODO: if (node->HasActiveState())
    // static bool isActive = true;
    // if (ImGui::Checkbox("Active", &isActive)) {
    //     node->SetActive(isActive);
    // }
}

/**
 * @brief Draws a collapsible component header
 * 
 * @param name Component name
 * @param icon Icon text
 * @param isOpen Reference to expansion state
 * @param contextMenu Optional context menu function
 */
static void DrawComponentHeader(const char* name, const char* icon, bool& isOpen,
                                 std::function<void()> contextMenu) {
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.18f, 0.18f, 0.22f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.25f, 0.25f, 0.30f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.30f, 0.30f, 0.35f, 1.0f));
    
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | 
                               ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_SpanAvailWidth;
    
    if (isOpen) flags |= ImGuiTreeNodeFlags_DefaultOpen;
    
    // Icon and name
    char label[64];
    snprintf(label, sizeof(label), "%s %s", icon, name);
    
    isOpen = ImGui::CollapsingHeader(label, flags);
    
    // Context menu button (right side)
    if (contextMenu) {
        ImGui::SameLine(ImGui::GetWindowWidth() - 40.0f);
        ImGui::PushID(name);
        if (ImGui::SmallButton("...")) {
            ImGui::OpenPopup("ComponentMenu");
        }
        
        if (ImGui::BeginPopup("ComponentMenu")) {
            contextMenu();
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
    
    ImGui::PopStyleColor(3);
}

/**
 * @brief Draws the Transform component editor
 * 
 * @param node The scene node
 * @param cb Command buffer for undo/redo
 */
static void DrawTransformComponent(ISceneNode* node, CommandBuffer& cb) {
    (void)cb;
    
    bool& isOpen = s_State.ComponentExpanded["Transform"];
    
    DrawComponentHeader("Transform", "[T]", isOpen, []() {
        if (ImGui::MenuItem("Reset Position")) { /* TODO */ }
        if (ImGui::MenuItem("Reset Rotation")) { /* TODO */ }
        if (ImGui::MenuItem("Reset Scale")) { /* TODO */ }
        ImGui::Separator();
        if (ImGui::MenuItem("Reset All")) { /* TODO */ }
        ImGui::Separator();
        if (ImGui::MenuItem("Copy Transform")) { /* TODO */ }
        if (ImGui::MenuItem("Paste Transform")) { /* TODO */ }
    });
    
    if (!isOpen) return;
    
    ImGui::Indent(8.0f);
    
    // Position
    Vec3 pos = node->GetLocalPosition();
    float p[3] = { pos.x, pos.y, pos.z };
    
    BeginPropertyRow();
    DrawPropertyLabel("Position", "World-space position of the object");
    
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.5f, 0.1f, 0.1f, 0.5f));
    ImGui::SetNextItemWidth(60.0f);
    if (ImGui::DragFloat("##PosX", &p[0], InspectorConfig::FLOAT_DRAG_SPEED, 0, 0, "X: %.2f")) {
        node->SetLocalPosition({ p[0], p[1], p[2] });
    }
    ImGui::PopStyleColor();
    
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.5f, 0.1f, 0.5f));
    ImGui::SetNextItemWidth(60.0f);
    if (ImGui::DragFloat("##PosY", &p[1], InspectorConfig::FLOAT_DRAG_SPEED, 0, 0, "Y: %.2f")) {
        node->SetLocalPosition({ p[0], p[1], p[2] });
    }
    ImGui::PopStyleColor();
    
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.1f, 0.5f, 0.5f));
    ImGui::SetNextItemWidth(60.0f);
    if (ImGui::DragFloat("##PosZ", &p[2], InspectorConfig::FLOAT_DRAG_SPEED, 0, 0, "Z: %.2f")) {
        node->SetLocalPosition({ p[0], p[1], p[2] });
    }
    ImGui::PopStyleColor();
    
    EndPropertyRow();
    
    // Rotation
    Vec3 rot = node->GetLocalRotation();
    float r[3] = { rot.x, rot.y, rot.z };
    
    BeginPropertyRow();
    DrawPropertyLabel("Rotation", "Euler angles in degrees");
    
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.5f, 0.1f, 0.1f, 0.5f));
    ImGui::SetNextItemWidth(60.0f);
    if (ImGui::DragFloat("##RotX", &r[0], InspectorConfig::ROTATION_DRAG_SPEED, 0, 0, "X: %.1f")) {
        node->SetLocalRotation({ r[0], r[1], r[2] });
    }
    ImGui::PopStyleColor();
    
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.5f, 0.1f, 0.5f));
    ImGui::SetNextItemWidth(60.0f);
    if (ImGui::DragFloat("##RotY", &r[1], InspectorConfig::ROTATION_DRAG_SPEED, 0, 0, "Y: %.1f")) {
        node->SetLocalRotation({ r[0], r[1], r[2] });
    }
    ImGui::PopStyleColor();
    
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.1f, 0.5f, 0.5f));
    ImGui::SetNextItemWidth(60.0f);
    if (ImGui::DragFloat("##RotZ", &r[2], InspectorConfig::ROTATION_DRAG_SPEED, 0, 0, "Z: %.1f")) {
        node->SetLocalRotation({ r[0], r[1], r[2] });
    }
    ImGui::PopStyleColor();
    
    EndPropertyRow();
    
    // Scale
    Vec3 scl = node->GetLocalScale();
    float s[3] = { scl.x, scl.y, scl.z };
    
    BeginPropertyRow();
    DrawPropertyLabel("Scale", "Local scale of the object");
    
    // Uniform scale lock
    static bool uniformScale = true;
    ImGui::Checkbox("##Uniform", &uniformScale);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Uniform scale");
    ImGui::SameLine();
    
    float prevS[3] = { s[0], s[1], s[2] };
    
    ImGui::SetNextItemWidth(50.0f);
    if (ImGui::DragFloat("##SclX", &s[0], InspectorConfig::SCALE_DRAG_SPEED, 0.001f, 1000.0f, "%.3f")) {
        if (uniformScale) {
            float ratio = s[0] / prevS[0];
            s[1] *= ratio;
            s[2] *= ratio;
        }
        node->SetLocalScale({ s[0], s[1], s[2] });
    }
    
    ImGui::SameLine();
    ImGui::SetNextItemWidth(50.0f);
    if (ImGui::DragFloat("##SclY", &s[1], InspectorConfig::SCALE_DRAG_SPEED, 0.001f, 1000.0f, "%.3f")) {
        if (uniformScale) {
            float ratio = s[1] / prevS[1];
            s[0] *= ratio;
            s[2] *= ratio;
        }
        node->SetLocalScale({ s[0], s[1], s[2] });
    }
    
    ImGui::SameLine();
    ImGui::SetNextItemWidth(50.0f);
    if (ImGui::DragFloat("##SclZ", &s[2], InspectorConfig::SCALE_DRAG_SPEED, 0.001f, 1000.0f, "%.3f")) {
        if (uniformScale) {
            float ratio = s[2] / prevS[2];
            s[0] *= ratio;
            s[1] *= ratio;
        }
        node->SetLocalScale({ s[0], s[1], s[2] });
    }
    
    EndPropertyRow();
    
    ImGui::Unindent(8.0f);
}

/**
 * @brief Draws the Material component editor
 * 
 * @param node The scene node
 * @param cb Command buffer for undo/redo
 */
static void DrawMaterialComponent(ISceneNode* node, CommandBuffer& cb) {
    (void)cb;
    
    bool& isOpen = s_State.ComponentExpanded["Material"];
    
    DrawComponentHeader("Material", "[M]", isOpen, []() {
        if (ImGui::MenuItem("Reset to Default")) { /* TODO */ }
        ImGui::Separator();
        if (ImGui::MenuItem("Create Material Instance")) { /* TODO */ }
        if (ImGui::MenuItem("Open in Material Editor")) { /* TODO */ }
    });
    
    if (!isOpen) return;
    
    ImGui::Indent(8.0f);
    
    auto* sceneNode = static_cast<SceneNode*>(node);
    
    // Shader selection
    BeginPropertyRow();
    DrawPropertyLabel("Shader", "Material shader to use");
    ImGui::TextColored(Colors::AccentCyan, "PBR_Standard");
    // TODO: Make this a dropdown/asset selector
    // DrawEnumProperty("Shader", currentShader, shaderNames, shaderCount);
    EndPropertyRow();
    
    ImGui::Spacing();
    
    // Base Color
    BeginPropertyRow();
    DrawPropertyLabel("Base Color", "Albedo/diffuse color of the material");
    float col[3] = { sceneNode->meshDesc.albedo.x, sceneNode->meshDesc.albedo.y, sceneNode->meshDesc.albedo.z };
    if (ImGui::ColorEdit3("##BaseColor", col, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel)) {
        sceneNode->meshDesc.albedo = { col[0], col[1], col[2] };
        // TODO: cb.Push({EditorCommandType::SetProperty, ...});
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.0f);
    ImGui::ColorEdit3("##BaseColorInput", col, ImGuiColorEditFlags_NoSmallPreview | ImGuiColorEditFlags_NoPicker);
    EndPropertyRow();
    
    // Metallic
    BeginPropertyRow();
    DrawPropertyLabel("Metallic", "How metallic the surface appears (0=dielectric, 1=metal)");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::SliderFloat("##Metallic", &sceneNode->meshDesc.metallic, 0.0f, 1.0f, "%.2f")) {
        // TODO: cb.Push({EditorCommandType::SetProperty, ...});
    }
    EndPropertyRow();
    
    // Roughness
    BeginPropertyRow();
    DrawPropertyLabel("Roughness", "Surface roughness (0=smooth/glossy, 1=rough/matte)");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::SliderFloat("##Roughness", &sceneNode->meshDesc.roughness, 0.0f, 1.0f, "%.2f")) {
        // TODO: cb.Push({EditorCommandType::SetProperty, ...});
    }
    EndPropertyRow();
    
    // Advanced properties (collapsed by default)
    if (s_State.bShowAdvanced) {
        ImGui::Spacing();
        ImGui::TextDisabled("Advanced");
        ImGui::Separator();
        
        // Normal Map strength
        BeginPropertyRow();
        DrawPropertyLabel("Normal Strength", "Intensity of normal map effect");
        static float normalStrength = 1.0f;
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::SliderFloat("##NormalStrength", &normalStrength, 0.0f, 2.0f, "%.2f");
        EndPropertyRow();
        
        // Emission
        BeginPropertyRow();
        DrawPropertyLabel("Emission", "Self-illumination color");
        static float emission[3] = { 0.0f, 0.0f, 0.0f };
        ImGui::ColorEdit3("##Emission", emission, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_HDR);
        EndPropertyRow();
    }
    
    ImGui::Unindent(8.0f);
}

/**
 * @brief Draws the Physics/RigidBody component editor
 * 
 * @param node The scene node
 * @param cb Command buffer for undo/redo
 */
static void DrawPhysicsComponent(ISceneNode* node, CommandBuffer& cb) {
    (void)cb;
    
    bool& isOpen = s_State.ComponentExpanded["Physics"];
    
    DrawComponentHeader("RigidBody Physics", "[P]", isOpen, []() {
        if (ImGui::MenuItem("Reset Physics")) { /* TODO */ }
        ImGui::Separator();
        if (ImGui::MenuItem("Wake Up")) { /* TODO */ }
        if (ImGui::MenuItem("Put to Sleep")) { /* TODO */ }
        ImGui::Separator();
        if (ImGui::MenuItem("Remove Component")) { /* TODO */ }
    });
    
    if (!isOpen) return;
    
    ImGui::Indent(8.0f);
    
    auto* sceneNode = static_cast<SceneNode*>(node);
    
    // Body Type
    BeginPropertyRow();
    DrawPropertyLabel("Body Type", "Physics simulation type");
    const char* bodyTypes[] = { "Static", "Kinematic", "Dynamic" };
    static int currentBodyType = 2; // Dynamic
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    ImGui::Combo("##BodyType", &currentBodyType, bodyTypes, IM_ARRAYSIZE(bodyTypes));
    EndPropertyRow();
    
    // Collider Shape
    BeginPropertyRow();
    DrawPropertyLabel("Shape", "Collision shape type");
    ImGui::Text("%s", sceneNode->physicsDesc.colliderShape.c_str());
    // TODO: Make this editable
    EndPropertyRow();
    
    ImGui::Spacing();
    ImGui::TextDisabled("Mass Properties");
    ImGui::Separator();
    
    // Mass
    BeginPropertyRow();
    DrawPropertyLabel("Mass", "Mass in kilograms");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::DragFloat("##Mass", &sceneNode->physicsDesc.mass, 0.1f, 0.0f, 10000.0f, "%.2f kg")) {
        // TODO: Update physics engine
        // cb.Push({EditorCommandType::SetProperty, ...});
    }
    EndPropertyRow();
    
    // Drag
    BeginPropertyRow();
    DrawPropertyLabel("Linear Drag", "Resistance to linear motion");
    static float linearDrag = 0.0f;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    ImGui::DragFloat("##LinearDrag", &linearDrag, 0.01f, 0.0f, 100.0f, "%.3f");
    EndPropertyRow();
    
    BeginPropertyRow();
    DrawPropertyLabel("Angular Drag", "Resistance to rotation");
    static float angularDrag = 0.05f;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    ImGui::DragFloat("##AngularDrag", &angularDrag, 0.01f, 0.0f, 100.0f, "%.3f");
    EndPropertyRow();
    
    ImGui::Spacing();
    ImGui::TextDisabled("Material");
    ImGui::Separator();
    
    // Restitution (bounciness)
    BeginPropertyRow();
    DrawPropertyLabel("Restitution", "Bounciness (0=no bounce, 1=perfect bounce)");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    ImGui::SliderFloat("##Restitution", &sceneNode->physicsDesc.restitution, 0.0f, 1.0f, "%.2f");
    EndPropertyRow();
    
    // Friction
    BeginPropertyRow();
    DrawPropertyLabel("Friction", "Surface friction coefficient");
    static float friction = 0.5f;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    ImGui::SliderFloat("##Friction", &friction, 0.0f, 1.0f, "%.2f");
    EndPropertyRow();
    
    ImGui::Spacing();
    ImGui::TextDisabled("Constraints");
    ImGui::Separator();
    
    // Freeze Position
    BeginPropertyRow();
    DrawPropertyLabel("Freeze Position", "Lock position axes");
    static bool freezePosX = false, freezePosY = false, freezePosZ = false;
    ImGui::Checkbox("X##FreezePos", &freezePosX); ImGui::SameLine();
    ImGui::Checkbox("Y##FreezePos", &freezePosY); ImGui::SameLine();
    ImGui::Checkbox("Z##FreezePos", &freezePosZ);
    EndPropertyRow();
    
    // Freeze Rotation
    BeginPropertyRow();
    DrawPropertyLabel("Freeze Rotation", "Lock rotation axes");
    static bool freezeRotX = false, freezeRotY = false, freezeRotZ = false;
    ImGui::Checkbox("X##FreezeRot", &freezeRotX); ImGui::SameLine();
    ImGui::Checkbox("Y##FreezeRot", &freezeRotY); ImGui::SameLine();
    ImGui::Checkbox("Z##FreezeRot", &freezeRotZ);
    EndPropertyRow();
    
    ImGui::Unindent(8.0f);
}

/**
 * @brief Draws the Add Component button and menu
 * 
 * @param node The scene node
 * @param cb Command buffer
 */
static void DrawAddComponentButton(ISceneNode* node, CommandBuffer& cb) {
    (void)node; (void)cb;
    
    float buttonWidth = 200.0f;
    float windowWidth = ImGui::GetContentRegionAvail().x;
    
    ImGui::SetCursorPosX((windowWidth - buttonWidth) * 0.5f);
    
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.5f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.25f, 0.45f, 0.25f, 1.0f));
    
    if (ImGui::Button("+ Add Component", ImVec2(buttonWidth, 30))) {
        ImGui::OpenPopup("AddComponentPopup");
    }
    
    ImGui::PopStyleColor(3);
    
    if (ImGui::BeginPopup("AddComponentPopup")) {
        ImGui::TextDisabled("Add Component");
        ImGui::Separator();
        
        if (ImGui::BeginMenu("Rendering")) {
            if (ImGui::MenuItem("Mesh Renderer")) { /* TODO: Add component */ }
            if (ImGui::MenuItem("Skinned Mesh Renderer")) { /* TODO */ }
            if (ImGui::MenuItem("Sprite Renderer")) { /* TODO */ }
            if (ImGui::MenuItem("Line Renderer")) { /* TODO */ }
            if (ImGui::MenuItem("Trail Renderer")) { /* TODO */ }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Physics")) {
            if (ImGui::MenuItem("Rigidbody")) { /* TODO */ }
            if (ImGui::MenuItem("Box Collider")) { /* TODO */ }
            if (ImGui::MenuItem("Sphere Collider")) { /* TODO */ }
            if (ImGui::MenuItem("Capsule Collider")) { /* TODO */ }
            if (ImGui::MenuItem("Mesh Collider")) { /* TODO */ }
            ImGui::Separator();
            if (ImGui::MenuItem("Character Controller")) { /* TODO */ }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Audio")) {
            if (ImGui::MenuItem("Audio Source")) { /* TODO */ }
            if (ImGui::MenuItem("Audio Listener")) { /* TODO */ }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Effects")) {
            if (ImGui::MenuItem("Particle System")) { /* TODO */ }
            if (ImGui::MenuItem("Light")) { /* TODO */ }
            if (ImGui::MenuItem("Reflection Probe")) { /* TODO */ }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Scripting")) {
            if (ImGui::MenuItem("New Script...")) { /* TODO: Open script creation dialog */ }
            ImGui::Separator();
            // TODO: List available script types
            ImGui::MenuItem("(No scripts available)", nullptr, false, false);
            ImGui::EndMenu();
        }
        
        ImGui::EndPopup();
    }
}

//=============================================================================
// PROPERTY DRAWING HELPERS
//=============================================================================

/**
 * @brief Draws a property label with optional tooltip
 * 
 * @param label The label text
 * @param tooltip Optional hover tooltip
 */
static void DrawPropertyLabel(const char* label, const char* tooltip) {
    ImGui::AlignTextToFramePadding();
    ImGui::Text("%s", label);
    
    if (tooltip && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
    }
    
    ImGui::SameLine(s_State.LabelWidth);
}

/**
 * @brief Begins a property row
 */
static void BeginPropertyRow() {
    ImGui::PushID(ImGui::GetID("PropertyRow"));
}

/**
 * @brief Ends a property row
 */
static void EndPropertyRow() {
    ImGui::PopID();
}

/**
 * @brief Draws a float property editor
 * @return true if value was modified
 */
static bool DrawFloatProperty(const char* label, float& value, float speed, 
                               float min, float max, const char* format) {
    DrawPropertyLabel(label, nullptr);
    
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    return ImGui::DragFloat("##Value", &value, speed, min, max, format);
}

/**
 * @brief Draws a float2/vector2 property editor
 * @return true if value was modified
 */
static bool DrawFloat2Property(const char* label, float* values, float speed) {
    DrawPropertyLabel(label, nullptr);
    
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    return ImGui::DragFloat2("##Value", values, speed);
}

/**
 * @brief Draws a float3/vector3 property editor
 * @return true if value was modified
 */
static bool DrawFloat3Property(const char* label, float* values, float speed, bool isColor) {
    DrawPropertyLabel(label, nullptr);
    
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (isColor) {
        return ImGui::ColorEdit3("##Value", values);
    }
    return ImGui::DragFloat3("##Value", values, speed);
}

/**
 * @brief Draws a float4/vector4 property editor
 * @return true if value was modified
 */
static bool DrawFloat4Property(const char* label, float* values, float speed) {
    DrawPropertyLabel(label, nullptr);
    
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    return ImGui::DragFloat4("##Value", values, speed);
}

/**
 * @brief Draws an integer property editor
 * @return true if value was modified
 */
static bool DrawIntProperty(const char* label, int& value, int min, int max) {
    DrawPropertyLabel(label, nullptr);
    
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    return ImGui::DragInt("##Value", &value, 1.0f, min, max);
}

/**
 * @brief Draws a boolean/checkbox property editor
 * @return true if value was modified
 */
static bool DrawBoolProperty(const char* label, bool& value) {
    DrawPropertyLabel(label, nullptr);
    
    return ImGui::Checkbox("##Value", &value);
}

/**
 * @brief Draws a string/text property editor
 * @return true if value was modified
 */
static bool DrawStringProperty(const char* label, char* buffer, size_t bufferSize) {
    DrawPropertyLabel(label, nullptr);
    
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    return ImGui::InputText("##Value", buffer, bufferSize);
}

/**
 * @brief Draws an enum/dropdown property editor
 * @return true if value was modified
 */
static bool DrawEnumProperty(const char* label, int& currentItem, const char* const* items, int itemCount) {
    DrawPropertyLabel(label, nullptr);
    
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    return ImGui::Combo("##Value", &currentItem, items, itemCount);
}

/**
 * @brief Draws an asset reference property editor
 * @return true if value was modified
 */
static bool DrawAssetProperty(const char* label, const char* assetType, uint64_t& assetID, 
                               const std::string& currentName) {
    (void)assetID;
    
    DrawPropertyLabel(label, nullptr);
    
    // Asset preview button
    if (ImGui::Button(currentName.empty() ? "(None)" : currentName.c_str(), 
        ImVec2(ImGui::GetContentRegionAvail().x - 30, 0))) {
        // TODO: Open asset picker
        // AssetPicker::Open(assetType, [&](uint64_t selectedID) { assetID = selectedID; });
    }
    
    // Drag-drop target
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_DRAG")) {
            uint64_t droppedID = *(uint64_t*)payload->Data;
            // TODO: Validate asset type
            assetID = droppedID;
            ImGui::EndDragDropTarget();
            return true;
        }
        ImGui::EndDragDropTarget();
    }
    
    // Clear button
    ImGui::SameLine();
    if (ImGui::Button("X", ImVec2(20, 0))) {
        assetID = 0;
        return true;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Clear %s", assetType);
    
    return false;
}

} // namespace RiftCore::UI
