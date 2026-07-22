/**
 * @file VisualScriptingPanel.cpp
 * @brief Production-grade Visual Scripting/Blueprint Panel for RiftCore Engine
 * 
 * This panel provides a node-based visual programming interface similar to
 * Unreal Engine's Blueprints. Supports creating, editing, and debugging
 * visual scripts with drag-drop connections and live preview.
 * 
 * @author RiftCore Team
 * @version 2.0.0
 * @date 2026-04-21
 * 
 * @note Architecture inspired by Unreal Engine's Blueprint Editor
 * @note Uses imgui-node-editor for node graph rendering
 * 
 * ============================================================================
 * EXTERNAL DEPENDENCIES (TODO: Implement these interfaces)
 * ============================================================================
 * - imgui-node-editor (ax::NodeEditor): Node graph rendering library
 * - IVisualScriptVM: Script execution virtual machine
 * - INodeRegistry: Registry of available node types
 * - IScriptAsset: Visual script asset serialization
 * - IDebugger: Script debugging interface
 * ============================================================================
 */








#include <UI/Panels/VisualScriptingPanel.h>
#include <UI/Styling/ImGuiTheme.h>
#include <imgui.h>
#include <algorithm>
#include <unordered_map>
#include <vector>
#include <string>
#include <functional>
#include <cstring>

// Namespace alias for node editor
namespace ed = ax::NodeEditor;

// TODO: Include your engine's scripting system headers
// #include <Scripting/VisualScriptVM.h>
// #include <Scripting/NodeRegistry.h>
// #include <Assets/ScriptAsset.h>

namespace RiftCore::UI {

//=============================================================================
// CONFIGURATION CONSTANTS
//=============================================================================

namespace VisualScriptConfig {
    /** Default node width */
    constexpr float DEFAULT_NODE_WIDTH = 200.0f;
    
    /** Pin radius for connection points */
    constexpr float PIN_RADIUS = 6.0f;
    
    /** Connection line thickness */
    constexpr float LINK_THICKNESS = 3.0f;
    
    /** Grid snap size */
    constexpr float GRID_SNAP = 16.0f;
    
    /** Maximum undo history size */
    constexpr size_t MAX_UNDO_HISTORY = 100;
    
    /** Auto-save interval in seconds */
    constexpr float AUTO_SAVE_INTERVAL = 60.0f;
    
    /** Node search results limit */
    constexpr int MAX_SEARCH_RESULTS = 20;
}

//=============================================================================
// STATIC STATE (uses types from header)
//=============================================================================

static FVisualScriptState s_State;
static FScriptGraph s_CurrentGraph;
static std::vector<FNodeTemplate> s_NodeTemplates;
static bool s_bInitialized = false;

//=============================================================================
// FORWARD DECLARATIONS
//=============================================================================

static void DrawToolbar();
static void DrawNodePalette();
static void DrawNodeGraph();
static void DrawDetailsPanel();
static void DrawMinimap();
static void DrawDebugPanel();
static void DrawNodeSearchPopup();

static void DrawNode(FNode& node);
static void DrawPin(const FPin& pin, bool isInput);
static void DrawLink(const FLink& link);
static void DrawNewLinkPreview();

static void InitializeNodeTemplates();
static FNode CreateNodeFromTemplate(const FNodeTemplate& tmpl, const ImVec2& position);
static void DeleteSelectedNodes();
static void DuplicateSelectedNodes();
static void CopySelectedNodes();
static void PasteNodes();

static bool CanCreateLink(uint64_t startPinID, uint64_t endPinID);
static void CreateLink(uint64_t startPinID, uint64_t endPinID);
static void DeleteLink(uint64_t linkID);

static void HandleKeyboardShortcuts();
static void HandleContextMenu();
static void UpdateDebugState();
static void SaveScript();

static ImVec4 GetPinColor(EPinType type);
static const char* GetPinTypeName(EPinType type);
static const char* GetCategoryName(ENodeCategory category);
static const char* GetCategoryIcon(ENodeCategory category);

//=============================================================================
// PUBLIC API IMPLEMENTATION
//=============================================================================

/**
 * @brief Initializes the Visual Scripting Panel
 * 
 * Must be called once before using the panel. Sets up the node editor
 * context and loads available node templates.
 */
void VisualScriptingPanel::Initialize() {
    if (s_bInitialized) return;
    
    // Create node editor context
    ed::Config config;
    config.SettingsFile = "VisualScripting.json";
    
    // Configure node editor style
    // TODO: Apply custom styling to match engine theme
    
    m_EditorContext = ed::CreateEditor(&config);
    
    // Initialize node templates
    InitializeNodeTemplates();
    
    s_bInitialized = true;
    
    // LOG_INFO("VisualScriptingPanel", "Initialized with %zu node templates", s_NodeTemplates.size());
}

/**
 * @brief Shuts down the Visual Scripting Panel
 * 
 * Cleans up the node editor context. Call during editor shutdown.
 */
void VisualScriptingPanel::Shutdown() {
    if (!s_bInitialized) return;
    
    if (m_EditorContext) {
        ed::DestroyEditor(m_EditorContext);
        m_EditorContext = nullptr;
    }
    
    s_NodeTemplates.clear();
    s_CurrentGraph = FScriptGraph();
    
    s_bInitialized = false;
    
    // LOG_INFO("VisualScriptingPanel", "Shutdown complete");
}

/**
 * @brief Loads a visual script for editing
 * 
 * @param assetID The asset ID of the script to load
 * 
 * TODO: Implement actual asset loading
 */
void VisualScriptingPanel::LoadScript(uint64_t assetID) {
    // TODO: Load script from asset system
    // IScriptAsset* asset = AssetSystem::Load<IScriptAsset>(assetID);
    // if (asset) {
    //     s_CurrentGraph = asset->GetGraph();
    // }
    
    s_CurrentGraph = FScriptGraph();
    s_CurrentGraph.AssetID = assetID;
    s_CurrentGraph.Name = "NewScript";
    
    // Create default event nodes for testing
    // This would normally come from the loaded asset
    
    // LOG_INFO("VisualScriptingPanel", "Loaded script: %s", s_CurrentGraph.Name.c_str());
}

/**
 * @brief Internal static save helper (called by free DrawToolbar)
 */
static void SaveScript() {
    s_CurrentGraph.bIsModified = false;
    s_State.TimeSinceLastSave = 0.0f;
}

/**
 * @brief Saves the current visual script
 * 
 * TODO: Implement actual asset saving
 */
void VisualScriptingPanel::SaveScript() {
    // TODO: Save script to asset system
    // IScriptAsset* asset = AssetSystem::Get<IScriptAsset>(s_CurrentGraph.AssetID);
    // if (asset) {
    //     asset->SetGraph(s_CurrentGraph);
    //     asset->Save();
    // }
    
    s_CurrentGraph.bIsModified = false;
    s_State.TimeSinceLastSave = 0.0f;
    
    // LOG_INFO("VisualScriptingPanel", "Saved script: %s", s_CurrentGraph.Name.c_str());
}

/**
 * @brief Main render function for the Visual Scripting Panel
 * 
 * Called every frame by the UI system.
 * 
 * Layout:
 * ┌───────────────────────────────────────────────────────────────────────┐
 * │ [Compile] [Save] [Play] [Pause] [Step] │ Search...  │ [Minimap] [...]│
 * ├──────────────┬────────────────────────────────────────┬───────────────┤
 * │              │                                        │               │
 * │  Node        │                                        │   Details     │
 * │  Palette     │         Node Graph Canvas              │   Panel       │
 * │  (toggle)    │                                        │   (toggle)    │
 * │              │                                        │               │
 * │              │                                        ├───────────────┤
 * │              │                                        │   Minimap     │
 * │              │                                        │   (toggle)    │
 * └──────────────┴────────────────────────────────────────┴───────────────┘
 */
void VisualScriptingPanel::OnUIRender() {
    static ImGuiContext* s_Ctx = ImGui::GetCurrentContext();
    if (!s_Ctx) return;
    ImGui::SetCurrentContext(s_Ctx);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    bool windowOpen = ImGui::Begin("Visual Scripting", nullptr, ImGuiWindowFlags_MenuBar);
    ImGui::PopStyleVar();
    if (!windowOpen) {
        ImGui::End();
        return;
    }
    
    // Validation check
    if (!m_EditorContext) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        ImGui::TextWrapped("Node Editor Context not initialized! Call Initialize() first.");
        ImGui::PopStyleColor();
        ImGui::End();
        return;
    }
    
    // Menu bar
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Script", "Ctrl+N")) {
                s_CurrentGraph = FScriptGraph();
            }
            if (ImGui::MenuItem("Open...", "Ctrl+O")) {
                // TODO: Open file dialog
            }
            if (ImGui::MenuItem("Save", "Ctrl+S", false, s_CurrentGraph.bIsModified)) {
                SaveScript();
            }
            if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) {
                // TODO: Save as dialog
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Import...")) { /* TODO */ }
            if (ImGui::MenuItem("Export...")) { /* TODO */ }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z")) { /* TODO */ }
            if (ImGui::MenuItem("Redo", "Ctrl+Y")) { /* TODO */ }
            ImGui::Separator();
            if (ImGui::MenuItem("Cut", "Ctrl+X")) { CopySelectedNodes(); DeleteSelectedNodes(); }
            if (ImGui::MenuItem("Copy", "Ctrl+C")) { CopySelectedNodes(); }
            if (ImGui::MenuItem("Paste", "Ctrl+V")) { PasteNodes(); }
            if (ImGui::MenuItem("Duplicate", "Ctrl+D")) { DuplicateSelectedNodes(); }
            if (ImGui::MenuItem("Delete", "Delete")) { DeleteSelectedNodes(); }
            ImGui::Separator();
            if (ImGui::MenuItem("Select All", "Ctrl+A")) { /* TODO */ }
            if (ImGui::MenuItem("Deselect All", "Escape")) { s_State.SelectedNodeIDs.clear(); }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Node Palette", nullptr, &s_State.bShowNodePalette);
            ImGui::MenuItem("Details Panel", nullptr, &s_State.bShowDetails);
            ImGui::MenuItem("Minimap", nullptr, &s_State.bShowMinimap);
            ImGui::Separator();
            ImGui::MenuItem("Show Grid", nullptr, &s_State.bShowGrid);
            ImGui::MenuItem("Snap to Grid", nullptr, &s_State.bSnapToGrid);
            ImGui::Separator();
            if (ImGui::MenuItem("Zoom to Fit", "Home")) {
                // TODO: ed::NavigateToContent();
            }
            if (ImGui::MenuItem("Zoom to Selection", "F")) {
                // TODO: ed::NavigateToSelection();
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Debug")) {
            if (ImGui::MenuItem("Compile", "F7")) {
                // TODO: Compile script
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Start Debugging", "F5", false, !s_State.bIsDebugging)) {
                s_State.bIsDebugging = true;
            }
            if (ImGui::MenuItem("Stop Debugging", "Shift+F5", false, s_State.bIsDebugging)) {
                s_State.bIsDebugging = false;
            }
            if (ImGui::MenuItem("Pause", nullptr, s_State.bIsPaused, s_State.bIsDebugging)) {
                s_State.bIsPaused = !s_State.bIsPaused;
            }
            if (ImGui::MenuItem("Step", "F10", false, s_State.bIsDebugging && s_State.bIsPaused)) {
                // TODO: Step execution
            }
            ImGui::EndMenu();
        }
        
        ImGui::EndMenuBar();
    }
    
    // Handle keyboard shortcuts
    HandleKeyboardShortcuts();
    
    // Update auto-save timer
    if (s_State.bAutoSaveEnabled && s_CurrentGraph.bIsModified) {
        s_State.TimeSinceLastSave += ImGui::GetIO().DeltaTime;
        if (s_State.TimeSinceLastSave >= VisualScriptConfig::AUTO_SAVE_INTERVAL) {
            SaveScript();
        }
    }
    
    // Update debug state
    if (s_State.bIsDebugging) {
        UpdateDebugState();
    }
    
    // Toolbar
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 4));
    DrawToolbar();
    ImGui::PopStyleVar();
    
    ImGui::Separator();
    
    // Main content area with panels
    float contentHeight = ImGui::GetContentRegionAvail().y;
    
    // Left panel - Node Palette
    if (s_State.bShowNodePalette) {
        ImGui::BeginChild("NodePalette", ImVec2(200, contentHeight), true);
        DrawNodePalette();
        ImGui::EndChild();
        
        ImGui::SameLine();
    }
    
    // Center - Node Graph
    float graphWidth = ImGui::GetContentRegionAvail().x;
    if (s_State.bShowDetails) {
        graphWidth -= s_State.DetailsPanelWidth + 4;
    }
    
    ImGui::BeginChild("GraphCanvas", ImVec2(graphWidth, contentHeight), true, ImGuiWindowFlags_NoScrollbar);
    DrawNodeGraph();
    ImGui::EndChild();
    
    // Right panel - Details
    if (s_State.bShowDetails) {
        ImGui::SameLine();
        
        ImGui::BeginChild("DetailsPanel", ImVec2(s_State.DetailsPanelWidth, contentHeight), true);
        DrawDetailsPanel();
        
        // Minimap at bottom of details panel
        if (s_State.bShowMinimap) {
            DrawMinimap();
        }
        
        ImGui::EndChild();
    }
    
    // Node search popup
    if (s_State.bShowNewNodeMenu) {
        DrawNodeSearchPopup();
    }
    
    ImGui::End();
}

//=============================================================================
// INTERNAL IMPLEMENTATIONS
//=============================================================================

/**
 * @brief Draws the toolbar with common actions
 */
static void DrawToolbar() {
    // Compile button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.6f, 1.0f));
    if (ImGui::Button("Compile", ImVec2(70, 0))) {
        // TODO: Compile script
    }
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Compile script (F7)");
    
    ImGui::SameLine();
    
    // Save button
    bool hasChanges = s_CurrentGraph.bIsModified;
    if (!hasChanges) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
    }
    if (ImGui::Button("Save", ImVec2(50, 0))) {
        SaveScript();
    }
    if (!hasChanges) {
        ImGui::PopStyleColor(2);
    }
    
    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();
    
    // Debug controls
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    
    // Play button
    ImVec4 playColor = s_State.bIsDebugging 
        ? ImVec4(0.8f, 0.3f, 0.3f, 1.0f)  // Red when running
        : ImVec4(0.2f, 0.6f, 0.2f, 1.0f); // Green when stopped
    
    ImGui::PushStyleColor(ImGuiCol_Button, playColor);
    const char* playLabel = s_State.bIsDebugging ? "Stop" : "Play";
    if (ImGui::Button(playLabel, ImVec2(50, 0))) {
        s_State.bIsDebugging = !s_State.bIsDebugging;
        if (!s_State.bIsDebugging) {
            s_State.bIsPaused = false;
        }
    }
    ImGui::PopStyleColor();
    
    ImGui::SameLine();
    
    // Pause button (only when debugging)
    ImGui::BeginDisabled(!s_State.bIsDebugging);
    ImVec4 pauseColor = s_State.bIsPaused 
        ? ImVec4(0.8f, 0.6f, 0.2f, 1.0f) 
        : ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, pauseColor);
    if (ImGui::Button("||", ImVec2(25, 0))) {
        s_State.bIsPaused = !s_State.bIsPaused;
    }
    ImGui::PopStyleColor();
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Pause execution");
    
    ImGui::SameLine();
    
    // Step button
    ImGui::BeginDisabled(!s_State.bIsDebugging || !s_State.bIsPaused);
    if (ImGui::Button(">|", ImVec2(25, 0))) {
        // TODO: Step one node
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Step (F10)");
    
    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();
    
    // Search bar
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    
    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::InputTextWithHint("##Search", "Search nodes... (Tab)", 
        s_State.SearchBuffer, sizeof(s_State.SearchBuffer))) {
        // TODO: Filter node search
    }
    
    // Right-aligned info
    ImGui::SameLine(ImGui::GetWindowWidth() - 200.0f);
    
    // Script name
    if (s_CurrentGraph.bIsModified) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "%s*", s_CurrentGraph.Name.c_str());
    } else {
        ImGui::Text("%s", s_CurrentGraph.Name.c_str());
    }
    
    ImGui::SameLine();
    
    // Node count
    ImGui::TextDisabled("(%zu nodes)", s_CurrentGraph.Nodes.size());
}

/**
 * @brief Draws the node palette panel
 */
static void DrawNodePalette() {
    ImGui::Text("Node Palette");
    ImGui::Separator();
    
    // Search within palette
    static char paletteSearch[128] = "";
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    ImGui::InputTextWithHint("##PaletteSearch", "Filter...", paletteSearch, sizeof(paletteSearch));
    
    ImGui::Spacing();
    
    // Categorized nodes
    ENodeCategory lastCategory = ENodeCategory::Custom;
    bool categoryOpen = false;
    
    for (const auto& tmpl : s_NodeTemplates) {
        // Filter by search
        if (paletteSearch[0] != '\0') {
            std::string searchLower = paletteSearch;
            std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);
            
            std::string nameLower = tmpl.Name;
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
            
            if (nameLower.find(searchLower) == std::string::npos) {
                continue;
            }
        }
        
        // Category header
        if (tmpl.Category != lastCategory) {
            if (categoryOpen) ImGui::TreePop();
            
            lastCategory = tmpl.Category;
            categoryOpen = ImGui::TreeNodeEx(GetCategoryName(tmpl.Category), 
                ImGuiTreeNodeFlags_DefaultOpen);
        }
        
        if (categoryOpen) {
            // Selectable node template
            if (ImGui::Selectable(tmpl.Name.c_str())) {
                // Create node at center of view
                // TODO: Get center position from node editor
                FNode newNode = CreateNodeFromTemplate(tmpl, ImVec2(0, 0));
                s_CurrentGraph.Nodes[newNode.ID] = newNode;
                s_CurrentGraph.bIsModified = true;
            }
            
            // Drag source for dropping onto canvas
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("NODE_TEMPLATE", &tmpl, sizeof(FNodeTemplate*));
                ImGui::Text("Create: %s", tmpl.Name.c_str());
                ImGui::EndDragDropSource();
            }
            
            // Tooltip with description
            if (ImGui::IsItemHovered() && !tmpl.Description.empty()) {
                ImGui::SetTooltip("%s", tmpl.Description.c_str());
            }
        }
    }
    
    if (categoryOpen) ImGui::TreePop();
}

/**
 * @brief Draws the main node graph canvas
 */
static void DrawNodeGraph() {
    ed::SetCurrentEditor(VisualScriptingPanel::m_EditorContext);
    
    // Style customization
    ed::PushStyleColor(ed::StyleColor_Bg, ImColor(25, 25, 28, 255));
    ed::PushStyleColor(ed::StyleColor_Grid, ImColor(50, 50, 55, 128));
    ed::PushStyleVar(ed::StyleVar_NodeRounding, 4.0f);
    ed::PushStyleVar(ed::StyleVar_NodeBorderWidth, 1.0f);
    ed::PushStyleVar(ed::StyleVar_PinRadius, VisualScriptConfig::PIN_RADIUS);
    ed::PushStyleVar(ed::StyleVar_LinkStrength, 100.0f);
    
    ed::Begin("NodeGraph");
    
    // Draw all nodes
    for (auto& [id, node] : s_CurrentGraph.Nodes) {
        DrawNode(node);
    }
    
    // Draw all links
    for (auto& [id, link] : s_CurrentGraph.Links) {
        DrawLink(link);
    }
    
    // Handle link creation
    if (ed::BeginCreate()) {
        ed::PinId startPinId, endPinId;
        if (ed::QueryNewLink(&startPinId, &endPinId)) {
            if (startPinId && endPinId) {
                if (CanCreateLink(startPinId.Get(), endPinId.Get())) {
                    if (ed::AcceptNewItem(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), 2.0f)) {
                        CreateLink(startPinId.Get(), endPinId.Get());
                    }
                } else {
                    ed::RejectNewItem(ImVec4(0.8f, 0.3f, 0.3f, 1.0f), 2.0f);
                }
            }
        }
        
        // Handle new node creation from pin drag
        ed::PinId pinId;
        if (ed::QueryNewNode(&pinId)) {
            if (ed::AcceptNewItem()) {
                s_State.NewLinkStartPin = pinId.Get();
                s_State.bShowNewNodeMenu = true;
                s_State.NewNodePosition = ImGui::GetMousePos();
            }
        }
    }
    ed::EndCreate();
    
    // Handle deletion
    if (ed::BeginDelete()) {
        ed::LinkId linkId;
        while (ed::QueryDeletedLink(&linkId)) {
            if (ed::AcceptDeletedItem()) {
                DeleteLink(linkId.Get());
            }
        }
        
        ed::NodeId nodeId;
        while (ed::QueryDeletedNode(&nodeId)) {
            if (ed::AcceptDeletedItem()) {
                s_CurrentGraph.Nodes.erase(nodeId.Get());
                s_CurrentGraph.bIsModified = true;
            }
        }
    }
    ed::EndDelete();
    
    // Context menu
    HandleContextMenu();
    
    ed::End();
    
    ed::PopStyleVar(4);
    ed::PopStyleColor(2);
    
    ed::SetCurrentEditor(nullptr);
}

/**
 * @brief Draws a single node
 * 
 * @param node The node to draw
 */
static void DrawNode(FNode& node) {
    // Determine node color based on category and state
    ImVec4 headerColor;
    switch (node.Category) {
        case ENodeCategory::Event:
            headerColor = ImVec4(0.7f, 0.2f, 0.2f, 1.0f);  // Red
            break;
        case ENodeCategory::Flow:
            headerColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);  // Gray
            break;
        case ENodeCategory::Math:
            headerColor = ImVec4(0.2f, 0.5f, 0.2f, 1.0f);  // Green
            break;
        case ENodeCategory::Variable:
            headerColor = ImVec4(0.2f, 0.2f, 0.7f, 1.0f);  // Blue
            break;
        case ENodeCategory::Function:
        default:
            headerColor = ImVec4(0.3f, 0.3f, 0.5f, 1.0f);  // Purple-ish
            break;
    }
    
    // Highlight if debugging
    if (s_State.bIsDebugging && node.ID == s_State.CurrentExecutingNode) {
        headerColor = ImVec4(0.8f, 0.6f, 0.1f, 1.0f);  // Orange when executing
    }
    
    // Error highlight
    if (node.bHasError) {
        headerColor = ImVec4(0.9f, 0.1f, 0.1f, 1.0f);  // Bright red for errors
    }
    
    ed::PushStyleColor(ed::StyleColor_NodeBg, ImColor(40, 40, 45, 230));
    ed::PushStyleColor(ed::StyleColor_NodeBorder, ImColor(headerColor));
    
    ed::BeginNode(node.ID);
    
    // Node header
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    
    // Category icon
    ImGui::TextColored(ImVec4(headerColor.x, headerColor.y, headerColor.z, 1.0f), 
        "%s", GetCategoryIcon(node.Category));
    ImGui::SameLine();
    
    // Node title
    ImGui::Text("%s", node.Name.c_str());
    ImGui::PopStyleColor();
    
    // Error indicator
    if (node.bHasError) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), " [!]");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", node.ErrorMessage.c_str());
        }
    }
    
    ImGui::Spacing();
    
    // Draw pins
    ImGui::BeginGroup();
    for (const auto& pin : node.InputPins) {
        DrawPin(pin, true);
    }
    ImGui::EndGroup();
    
    ImGui::SameLine(VisualScriptConfig::DEFAULT_NODE_WIDTH - 80);
    
    ImGui::BeginGroup();
    for (const auto& pin : node.OutputPins) {
        DrawPin(pin, false);
    }
    ImGui::EndGroup();
    
    // Comment (if any)
    if (!node.Comment.empty() && !node.bIsCollapsed) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
        ImGui::TextWrapped("%s", node.Comment.c_str());
        ImGui::PopStyleColor();
    }
    
    ed::EndNode();
    
    ed::PopStyleColor(2);
}

/**
 * @brief Draws a single pin
 * 
 * @param pin The pin to draw
 * @param isInput Whether this is an input pin
 */
static void DrawPin(const FPin& pin, bool isInput) {
    ImVec4 pinColor = GetPinColor(pin.Type);
    
    ed::PushStyleColor(ed::StyleColor_PinRect, ImColor(pinColor));
    ed::PushStyleColor(ed::StyleColor_PinRectBorder, ImColor(pinColor.x * 0.8f, pinColor.y * 0.8f, pinColor.z * 0.8f, 1.0f));
    
    if (isInput) {
        ed::BeginPin(pin.ID, ed::PinKind::Input);
    } else {
        ed::BeginPin(pin.ID, ed::PinKind::Output);
    }
    
    // Pin icon based on type
    if (pin.Type == EPinType::Flow) {
        // Triangle for flow pins
        ImGui::Text(isInput ? ">" : ">");
    } else {
        // Circle for data pins
        ImGui::Text("O");
    }
    
    ImGui::SameLine();
    
    // Pin label
    ImGui::Text("%s", pin.Name.c_str());
    
    ed::EndPin();
    
    ed::PopStyleColor(2);
    
    // Tooltip
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s (%s)", pin.Name.c_str(), GetPinTypeName(pin.Type));
    }
}

/**
 * @brief Draws a link between two pins
 * 
 * @param link The link to draw
 */
static void DrawLink(const FLink& link) {
    ed::Link(link.ID, link.StartPinID, link.EndPinID, 
        ImColor(link.Color), link.Thickness);
}

/**
 * @brief Draws the details panel for selected nodes
 */
static void DrawDetailsPanel() {
    ImGui::Text("Details");
    ImGui::Separator();
    
    if (s_State.SelectedNodeIDs.empty()) {
        ImGui::TextDisabled("Select a node to view details");
        return;
    }
    
    // Show details for first selected node
    uint64_t nodeID = s_State.SelectedNodeIDs[0];
    auto it = s_CurrentGraph.Nodes.find(nodeID);
    if (it == s_CurrentGraph.Nodes.end()) return;
    
    FNode& node = it->second;
    
    // Node info
    ImGui::Text("Node: %s", node.Name.c_str());
    ImGui::TextDisabled("ID: %llu", (unsigned long long)node.ID);
    ImGui::TextDisabled("Type: %s", node.ClassName.c_str());
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Comment editor
    ImGui::Text("Comment:");
    static char commentBuf[512];
    strncpy(commentBuf, node.Comment.c_str(), sizeof(commentBuf) - 1);
    if (ImGui::InputTextMultiline("##Comment", commentBuf, sizeof(commentBuf), ImVec2(-1, 60))) {
        node.Comment = commentBuf;
        s_CurrentGraph.bIsModified = true;
    }
    
    ImGui::Spacing();
    
    // Input pin values
    if (!node.InputPins.empty()) {
        ImGui::Text("Inputs:");
        ImGui::Indent();
        for (auto& pin : node.InputPins) {
            if (pin.Type != EPinType::Flow && !pin.bIsConnected) {
                ImGui::Text("%s:", pin.Name.c_str());
                ImGui::SameLine();
                
                // Simple value editor based on type
                // TODO: Proper type-specific editors
                static char valueBuf[256];
                strncpy(valueBuf, pin.DefaultValue.c_str(), sizeof(valueBuf) - 1);
                ImGui::SetNextItemWidth(100);
                if (ImGui::InputText(("##" + pin.Name).c_str(), valueBuf, sizeof(valueBuf))) {
                    pin.DefaultValue = valueBuf;
                    s_CurrentGraph.bIsModified = true;
                }
            }
        }
        ImGui::Unindent();
    }
    
    ImGui::Spacing();
    
    // Debug info
    if (s_State.bIsDebugging) {
        ImGui::Separator();
        ImGui::Text("Debug:");
        
        const char* stateNames[] = { "Inactive", "Pending", "Executing", "Completed", "Error" };
        ImGui::TextColored(
            node.State == ENodeState::Error ? ImVec4(1, 0, 0, 1) : ImVec4(0.7f, 0.7f, 0.7f, 1),
            "State: %s", stateNames[static_cast<int>(node.State)]
        );
    }
}

/**
 * @brief Draws the minimap
 */
static void DrawMinimap() {
    ImGui::Separator();
    ImGui::Text("Minimap");
    
    ImVec2 size = ImVec2(ImGui::GetContentRegionAvail().x, 100);
    
    // TODO: Draw actual minimap using ed::GetScreenSize(), node positions, etc.
    ImGui::Button("##Minimap", size);
    
    // Click to navigate
    if (ImGui::IsItemClicked()) {
        // TODO: Navigate to clicked position
    }
}

/**
 * @brief Draws the node search popup
 */
static void DrawNodeSearchPopup() {
    ImGui::SetNextWindowPos(s_State.NewNodePosition);
    ImGui::SetNextWindowSize(ImVec2(250, 300));
    
    if (ImGui::Begin("Create Node", &s_State.bShowNewNodeMenu, 
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
        
        // Search input
        if (ImGui::IsWindowAppearing()) {
            ImGui::SetKeyboardFocusHere();
        }
        
        static char searchBuf[128] = "";
        if (ImGui::InputText("##NodeSearch", searchBuf, sizeof(searchBuf))) {
            // Filter templates
            s_State.SearchResults.clear();
            s_State.SelectedSearchIndex = -1;
            
            std::string searchLower = searchBuf;
            std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);
            
            for (const auto& tmpl : s_NodeTemplates) {
                std::string nameLower = tmpl.Name;
                std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                
                if (nameLower.find(searchLower) != std::string::npos ||
                    tmpl.Keywords.find(searchLower) != std::string::npos) {
                    s_State.SearchResults.push_back(tmpl);
                    if (s_State.SearchResults.size() >= VisualScriptConfig::MAX_SEARCH_RESULTS) {
                        break;
                    }
                }
            }
        }
        
        ImGui::Separator();
        
        // Results list
        for (size_t i = 0; i < s_State.SearchResults.size(); ++i) {
            const auto& tmpl = s_State.SearchResults[i];
            bool isSelected = (int)i == s_State.SelectedSearchIndex;
            
            if (ImGui::Selectable(tmpl.Name.c_str(), isSelected)) {
                // Create node
                FNode newNode = CreateNodeFromTemplate(tmpl, s_State.NewNodePosition);
                s_CurrentGraph.Nodes[newNode.ID] = newNode;
                s_CurrentGraph.bIsModified = true;
                
                // Connect if we started from a pin
                if (s_State.NewLinkStartPin != 0) {
                    // Find compatible pin on new node
                    // TODO: Implement automatic connection
                }
                
                s_State.bShowNewNodeMenu = false;
                searchBuf[0] = '\0';
            }
            
            if (ImGui::IsItemHovered() && !tmpl.Description.empty()) {
                ImGui::SetTooltip("%s", tmpl.Description.c_str());
            }
        }
        
        // Handle keyboard navigation
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) && 
            s_State.SelectedSearchIndex < (int)s_State.SearchResults.size() - 1) {
            s_State.SelectedSearchIndex++;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) && s_State.SelectedSearchIndex > 0) {
            s_State.SelectedSearchIndex--;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Enter) && s_State.SelectedSearchIndex >= 0) {
            const auto& tmpl = s_State.SearchResults[s_State.SelectedSearchIndex];
            FNode newNode = CreateNodeFromTemplate(tmpl, s_State.NewNodePosition);
            s_CurrentGraph.Nodes[newNode.ID] = newNode;
            s_CurrentGraph.bIsModified = true;
            s_State.bShowNewNodeMenu = false;
            searchBuf[0] = '\0';
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            s_State.bShowNewNodeMenu = false;
            searchBuf[0] = '\0';
        }
    }
    ImGui::End();
}

/**
 * @brief Initializes the node template library
 */
static void InitializeNodeTemplates() {
    s_NodeTemplates.clear();
    
    // === Event Nodes ===
    {
        FNodeTemplate tmpl;
        tmpl.Name = "Event BeginPlay";
        tmpl.ClassName = "K2Node_Event_BeginPlay";
        tmpl.Description = "Called when the game starts or when the actor is spawned";
        tmpl.Keywords = "start begin initialize";
        tmpl.Category = ENodeCategory::Event;
        tmpl.bIsPure = false;
        
        FPin execOut;
        execOut.Name = "Execute";
        execOut.Type = EPinType::Flow;
        execOut.Direction = EPinDirection::Output;
        tmpl.OutputPins.push_back(execOut);
        
        s_NodeTemplates.push_back(tmpl);
    }
    
    {
        FNodeTemplate tmpl;
        tmpl.Name = "Event Tick";
        tmpl.ClassName = "K2Node_Event_Tick";
        tmpl.Description = "Called every frame";
        tmpl.Keywords = "update frame";
        tmpl.Category = ENodeCategory::Event;
        tmpl.bIsPure = false;
        
        FPin execOut;
        execOut.Name = "Execute";
        execOut.Type = EPinType::Flow;
        execOut.Direction = EPinDirection::Output;
        tmpl.OutputPins.push_back(execOut);
        
        FPin deltaTime;
        deltaTime.Name = "Delta Time";
        deltaTime.Type = EPinType::Float;
        deltaTime.Direction = EPinDirection::Output;
        tmpl.OutputPins.push_back(deltaTime);
        
        s_NodeTemplates.push_back(tmpl);
    }
    
    // === Flow Control ===
    {
        FNodeTemplate tmpl;
        tmpl.Name = "Branch";
        tmpl.ClassName = "K2Node_Branch";
        tmpl.Description = "If-else conditional branching";
        tmpl.Keywords = "if else condition";
        tmpl.Category = ENodeCategory::Flow;
        
        FPin execIn;
        execIn.Name = "Execute";
        execIn.Type = EPinType::Flow;
        execIn.Direction = EPinDirection::Input;
        tmpl.InputPins.push_back(execIn);
        
        FPin condition;
        condition.Name = "Condition";
        condition.Type = EPinType::Bool;
        condition.Direction = EPinDirection::Input;
        tmpl.InputPins.push_back(condition);
        
        FPin trueOut;
        trueOut.Name = "True";
        trueOut.Type = EPinType::Flow;
        trueOut.Direction = EPinDirection::Output;
        tmpl.OutputPins.push_back(trueOut);
        
        FPin falseOut;
        falseOut.Name = "False";
        falseOut.Type = EPinType::Flow;
        falseOut.Direction = EPinDirection::Output;
        tmpl.OutputPins.push_back(falseOut);
        
        s_NodeTemplates.push_back(tmpl);
    }
    
    // === Math Nodes ===
    {
        FNodeTemplate tmpl;
        tmpl.Name = "Add (Float)";
        tmpl.ClassName = "K2Node_Add_Float";
        tmpl.Description = "Adds two float values";
        tmpl.Keywords = "plus + sum";
        tmpl.Category = ENodeCategory::Math;
        tmpl.bIsPure = true;
        
        FPin a;
        a.Name = "A";
        a.Type = EPinType::Float;
        a.Direction = EPinDirection::Input;
        tmpl.InputPins.push_back(a);
        
        FPin b;
        b.Name = "B";
        b.Type = EPinType::Float;
        b.Direction = EPinDirection::Input;
        tmpl.InputPins.push_back(b);
        
        FPin result;
        result.Name = "Result";
        result.Type = EPinType::Float;
        result.Direction = EPinDirection::Output;
        tmpl.OutputPins.push_back(result);
        
        s_NodeTemplates.push_back(tmpl);
    }
    
    // === Transform Nodes ===
    {
        FNodeTemplate tmpl;
        tmpl.Name = "Add Local Offset";
        tmpl.ClassName = "K2Node_AddLocalOffset";
        tmpl.Description = "Adds an offset to the actor's position in local space";
        tmpl.Keywords = "move translate position";
        tmpl.Category = ENodeCategory::Transform;
        
        FPin execIn;
        execIn.Name = "Execute";
        execIn.Type = EPinType::Flow;
        execIn.Direction = EPinDirection::Input;
        tmpl.InputPins.push_back(execIn);
        
        FPin delta;
        delta.Name = "Delta Location";
        delta.Type = EPinType::Vector3;
        delta.Direction = EPinDirection::Input;
        tmpl.InputPins.push_back(delta);
        
        FPin execOut;
        execOut.Name = "Then";
        execOut.Type = EPinType::Flow;
        execOut.Direction = EPinDirection::Output;
        tmpl.OutputPins.push_back(execOut);
        
        s_NodeTemplates.push_back(tmpl);
    }
    
    // === Debug Nodes ===
    {
        FNodeTemplate tmpl;
        tmpl.Name = "Print String";
        tmpl.ClassName = "K2Node_PrintString";
        tmpl.Description = "Prints a string to the output log";
        tmpl.Keywords = "log debug console";
        tmpl.Category = ENodeCategory::Debug;
        
        FPin execIn;
        execIn.Name = "Execute";
        execIn.Type = EPinType::Flow;
        execIn.Direction = EPinDirection::Input;
        tmpl.InputPins.push_back(execIn);
        
        FPin message;
        message.Name = "String";
        message.Type = EPinType::String;
        message.Direction = EPinDirection::Input;
        message.DefaultValue = "Hello";
        tmpl.InputPins.push_back(message);
        
        FPin execOut;
        execOut.Name = "Then";
        execOut.Type = EPinType::Flow;
        execOut.Direction = EPinDirection::Output;
        tmpl.OutputPins.push_back(execOut);
        
        s_NodeTemplates.push_back(tmpl);
    }
    
    // TODO: Add more node templates for:
    // - Variable Get/Set
    // - For/While loops
    // - Switch/Select
    // - Component access
    // - Physics operations
    // - Audio playback
    // - Timer functions
    // - Array operations
    // - String manipulation
    // - Custom events/functions
}

/**
 * @brief Creates a new node from a template
 * 
 * @param tmpl The template to use
 * @param position Position in graph space
 * @return The created node
 */
static FNode CreateNodeFromTemplate(const FNodeTemplate& tmpl, const ImVec2& position) {
    FNode node;
    node.ID = s_CurrentGraph.GenerateID();
    node.Name = tmpl.Name;
    node.ClassName = tmpl.ClassName;
    node.Category = tmpl.Category;
    node.Position = position;
    node.bIsPure = tmpl.bIsPure;
    
    // Create pins with unique IDs
    for (const auto& pinTmpl : tmpl.InputPins) {
        FPin pin = pinTmpl;
        pin.ID = s_CurrentGraph.GenerateID();
        pin.NodeID = node.ID;
        node.InputPins.push_back(pin);
        s_CurrentGraph.Pins[pin.ID] = pin;
    }
    
    for (const auto& pinTmpl : tmpl.OutputPins) {
        FPin pin = pinTmpl;
        pin.ID = s_CurrentGraph.GenerateID();
        pin.NodeID = node.ID;
        node.OutputPins.push_back(pin);
        s_CurrentGraph.Pins[pin.ID] = pin;
    }
    
    return node;
}

/**
 * @brief Checks if a link can be created between two pins
 * 
 * @param startPinID Source pin ID
 * @param endPinID Destination pin ID
 * @return true if the connection is valid
 */
static bool CanCreateLink(uint64_t startPinID, uint64_t endPinID) {
    auto startIt = s_CurrentGraph.Pins.find(startPinID);
    auto endIt = s_CurrentGraph.Pins.find(endPinID);
    
    if (startIt == s_CurrentGraph.Pins.end() || endIt == s_CurrentGraph.Pins.end()) {
        return false;
    }
    
    const FPin& startPin = startIt->second;
    const FPin& endPin = endIt->second;
    
    // Can't connect to same node
    if (startPin.NodeID == endPin.NodeID) {
        return false;
    }
    
    // Must be opposite directions
    if (startPin.Direction == endPin.Direction) {
        return false;
    }
    
    // Type compatibility (simplified)
    if (startPin.Type != endPin.Type && 
        startPin.Type != EPinType::Wildcard && 
        endPin.Type != EPinType::Wildcard) {
        // TODO: Add type conversion rules
        return false;
    }
    
    return true;
}

/**
 * @brief Creates a link between two pins
 * 
 * @param startPinID Source pin ID
 * @param endPinID Destination pin ID
 */
static void CreateLink(uint64_t startPinID, uint64_t endPinID) {
    FLink link;
    link.ID = s_CurrentGraph.GenerateID();
    link.StartPinID = startPinID;
    link.EndPinID = endPinID;
    
    // Set color based on pin type
    auto pinIt = s_CurrentGraph.Pins.find(startPinID);
    if (pinIt != s_CurrentGraph.Pins.end()) {
        link.Color = GetPinColor(pinIt->second.Type);
    }
    
    s_CurrentGraph.Links[link.ID] = link;
    s_CurrentGraph.bIsModified = true;
    
    // Mark pins as connected
    auto startIt = s_CurrentGraph.Pins.find(startPinID);
    auto endIt = s_CurrentGraph.Pins.find(endPinID);
    if (startIt != s_CurrentGraph.Pins.end()) startIt->second.bIsConnected = true;
    if (endIt != s_CurrentGraph.Pins.end()) endIt->second.bIsConnected = true;
}

/**
 * @brief Deletes a link
 * 
 * @param linkID The link ID to delete
 */
static void DeleteLink(uint64_t linkID) {
    auto it = s_CurrentGraph.Links.find(linkID);
    if (it == s_CurrentGraph.Links.end()) return;
    
    // Unmark pins as connected
    // TODO: Check if pins have other connections
    
    s_CurrentGraph.Links.erase(it);
    s_CurrentGraph.bIsModified = true;
}

/**
 * @brief Deletes all selected nodes
 */
static void DeleteSelectedNodes() {
    for (uint64_t nodeID : s_State.SelectedNodeIDs) {
        s_CurrentGraph.Nodes.erase(nodeID);
        
        // Delete associated links
        std::vector<uint64_t> linksToDelete;
        for (const auto& [linkID, link] : s_CurrentGraph.Links) {
            auto startPinIt = s_CurrentGraph.Pins.find(link.StartPinID);
            auto endPinIt = s_CurrentGraph.Pins.find(link.EndPinID);
            
            if ((startPinIt != s_CurrentGraph.Pins.end() && startPinIt->second.NodeID == nodeID) ||
                (endPinIt != s_CurrentGraph.Pins.end() && endPinIt->second.NodeID == nodeID)) {
                linksToDelete.push_back(linkID);
            }
        }
        
        for (uint64_t linkID : linksToDelete) {
            s_CurrentGraph.Links.erase(linkID);
        }
    }
    
    s_State.SelectedNodeIDs.clear();
    s_CurrentGraph.bIsModified = true;
}

/**
 * @brief Duplicates selected nodes
 */
static void DuplicateSelectedNodes() {
    // TODO: Implement node duplication with offset
    s_CurrentGraph.bIsModified = true;
}

/**
 * @brief Copies selected nodes to clipboard
 */
static void CopySelectedNodes() {
    // TODO: Serialize selected nodes to clipboard
}

/**
 * @brief Pastes nodes from clipboard
 */
static void PasteNodes() {
    // TODO: Deserialize nodes from clipboard
    s_CurrentGraph.bIsModified = true;
}

/**
 * @brief Handles keyboard shortcuts
 */
static void HandleKeyboardShortcuts() {
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) return;
    
    ImGuiIO& io = ImGui::GetIO();
    
    // Ctrl+S - Save
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
        SaveScript();
    }
    
    // Ctrl+Z - Undo
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
        // TODO: Undo
    }
    
    // Ctrl+Y - Redo
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y)) {
        // TODO: Redo
    }
    
    // Delete - Delete selected
    if (ImGui::IsKeyPressed(ImGuiKey_Delete) && !s_State.SelectedNodeIDs.empty()) {
        DeleteSelectedNodes();
    }
    
    // Ctrl+D - Duplicate
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D)) {
        DuplicateSelectedNodes();
    }
    
    // Tab - Open node search
    if (ImGui::IsKeyPressed(ImGuiKey_Tab) && !s_State.bShowNewNodeMenu) {
        s_State.bShowNewNodeMenu = true;
        s_State.NewNodePosition = ImGui::GetMousePos();
        s_State.NewLinkStartPin = 0;
    }
    
    // F5 - Start/Stop debugging
    if (ImGui::IsKeyPressed(ImGuiKey_F5)) {
        if (io.KeyShift) {
            s_State.bIsDebugging = false;
            s_State.bIsPaused = false;
        } else {
            s_State.bIsDebugging = true;
        }
    }
    
    // F7 - Compile
    if (ImGui::IsKeyPressed(ImGuiKey_F7)) {
        // TODO: Compile script
    }
}

/**
 * @brief Handles context menu
 */
static void HandleContextMenu() {
    // Background context menu
    if (ed::ShowBackgroundContextMenu()) {
        ImGui::OpenPopup("BackgroundContextMenu");
        s_State.NewNodePosition = ImGui::GetMousePos();
    }
    
    if (ImGui::BeginPopup("BackgroundContextMenu")) {
        if (ImGui::MenuItem("Create Node...")) {
            s_State.bShowNewNodeMenu = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Paste", "Ctrl+V")) {
            PasteNodes();
        }
        ImGui::EndPopup();
    }
    
    // Node context menu
    ed::NodeId nodeId;
    if (ed::ShowNodeContextMenu(&nodeId)) {
        ImGui::OpenPopup("NodeContextMenu");
    }
    
    if (ImGui::BeginPopup("NodeContextMenu")) {
        if (ImGui::MenuItem("Delete", "Delete")) {
            DeleteSelectedNodes();
        }
        if (ImGui::MenuItem("Duplicate", "Ctrl+D")) {
            DuplicateSelectedNodes();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Copy", "Ctrl+C")) {
            CopySelectedNodes();
        }
        ImGui::EndPopup();
    }
}

/**
 * @brief Updates debug state
 */
static void UpdateDebugState() {
    // TODO: Query script VM for current execution state
    // Update node states, breakpoints, variable values, etc.
}

/**
 * @brief Gets the color for a pin type
 * 
 * @param type The pin type
 * @return ImVec4 color
 */
static ImVec4 GetPinColor(EPinType type) {
    switch (type) {
        case EPinType::Flow:    return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);  // White
        case EPinType::Bool:    return ImVec4(0.9f, 0.2f, 0.2f, 1.0f);  // Red
        case EPinType::Int:     return ImVec4(0.2f, 0.8f, 0.8f, 1.0f);  // Cyan
        case EPinType::Float:   return ImVec4(0.4f, 0.9f, 0.4f, 1.0f);  // Green
        case EPinType::String:  return ImVec4(0.9f, 0.2f, 0.9f, 1.0f);  // Magenta
        case EPinType::Vector2:
        case EPinType::Vector3:
        case EPinType::Vector4: return ImVec4(0.9f, 0.9f, 0.2f, 1.0f);  // Yellow
        case EPinType::Color:   return ImVec4(0.9f, 0.5f, 0.2f, 1.0f);  // Orange
        case EPinType::Object:  return ImVec4(0.2f, 0.4f, 0.9f, 1.0f);  // Blue
        case EPinType::Struct:  return ImVec4(0.5f, 0.2f, 0.7f, 1.0f);  // Purple
        case EPinType::Array:   return ImVec4(0.2f, 0.7f, 0.7f, 1.0f);  // Teal
        case EPinType::Wildcard:return ImVec4(0.5f, 0.5f, 0.5f, 1.0f);  // Gray
        case EPinType::Delegate:return ImVec4(0.9f, 0.3f, 0.3f, 1.0f);  // Red outline
        default:                return ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
    }
}

/**
 * @brief Gets the name of a pin type
 * 
 * @param type The pin type
 * @return Type name string
 */
static const char* GetPinTypeName(EPinType type) {
    switch (type) {
        case EPinType::Flow:    return "Execution";
        case EPinType::Bool:    return "Boolean";
        case EPinType::Int:     return "Integer";
        case EPinType::Float:   return "Float";
        case EPinType::String:  return "String";
        case EPinType::Vector2: return "Vector2";
        case EPinType::Vector3: return "Vector3";
        case EPinType::Vector4: return "Vector4";
        case EPinType::Color:   return "Color";
        case EPinType::Object:  return "Object Reference";
        case EPinType::Struct:  return "Struct";
        case EPinType::Array:   return "Array";
        case EPinType::Wildcard:return "Wildcard";
        case EPinType::Delegate:return "Event Delegate";
        default:                return "Unknown";
    }
}

/**
 * @brief Gets the name of a category
 * 
 * @param category The category
 * @return Category name string
 */
static const char* GetCategoryName(ENodeCategory category) {
    switch (category) {
        case ENodeCategory::Event:     return "Events";
        case ENodeCategory::Flow:      return "Flow Control";
        case ENodeCategory::Math:      return "Math";
        case ENodeCategory::String:    return "String";
        case ENodeCategory::Transform: return "Transform";
        case ENodeCategory::Actor:     return "Actor";
        case ENodeCategory::Component: return "Components";
        case ENodeCategory::Physics:   return "Physics";
        case ENodeCategory::Input:     return "Input";
        case ENodeCategory::Debug:     return "Debug";
        case ENodeCategory::Variable:  return "Variables";
        case ENodeCategory::Function:  return "Functions";
        case ENodeCategory::Macro:     return "Macros";
        case ENodeCategory::Custom:    return "Custom";
        default:                       return "Other";
    }
}

/**
 * @brief Gets an icon for a category
 * 
 * @param category The category
 * @return Icon text
 */
static const char* GetCategoryIcon(ENodeCategory category) {
    switch (category) {
        case ENodeCategory::Event:     return "[E]";
        case ENodeCategory::Flow:      return "[F]";
        case ENodeCategory::Math:      return "[M]";
        case ENodeCategory::String:    return "[S]";
        case ENodeCategory::Transform: return "[T]";
        case ENodeCategory::Actor:     return "[A]";
        case ENodeCategory::Component: return "[C]";
        case ENodeCategory::Physics:   return "[P]";
        case ENodeCategory::Input:     return "[I]";
        case ENodeCategory::Debug:     return "[D]";
        case ENodeCategory::Variable:  return "[V]";
        case ENodeCategory::Function:  return "[Fn]";
        case ENodeCategory::Macro:     return "[Mc]";
        default:                       return "[?]";
    }
}

// Static member for editor context
ed::EditorContext* VisualScriptingPanel::m_EditorContext = nullptr;

} // namespace RiftCore::UI
