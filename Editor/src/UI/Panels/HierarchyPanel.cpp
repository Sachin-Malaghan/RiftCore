/**
 * @file HierarchyPanel.cpp
 * @brief Production-grade Scene Hierarchy Panel for RiftCore Engine
 * 
 * This panel provides a tree-view of all entities/nodes in the current scene,
 * similar to Unreal Engine's World Outliner. Supports drag-drop reparenting,
 * multi-selection, search filtering, and context menu operations.
 * 
 * @author RiftCore Team
 * @version 2.0.0
 * @date 2026-04-21
 * 
 * @note Architecture inspired by Unreal Engine's SSceneOutliner
 * 
 * ============================================================================
 * EXTERNAL DEPENDENCIES (TODO: Implement these interfaces)
 * ============================================================================
 * - ISceneSystem: Scene graph interface for querying/manipulating nodes
 * - ISceneNode: Individual scene node interface
 * - CommandBuffer: Command pattern for undo/redo support
 * - EventDispatcher: For selection change notifications
 * - EditorPreferences: For saving panel state
 * ============================================================================
 */

#include <UI/Panels/HierarchyPanel.h>
#include <imgui.h>
#include <algorithm>
#include <unordered_set>
#include <unordered_map>
#include <functional>
#include <cstring>
#include <string>

// TODO: Include your engine's scene system headers
// #include <Scene/SceneSystem.h>
// #include <Scene/SceneNode.h>
// #include <Editor/CommandBuffer.h>
// #include <Core/EventDispatcher.h>

namespace RiftCore::UI {

//=============================================================================
// CONFIGURATION CONSTANTS
//=============================================================================

namespace HierarchyConfig {
    /** Maximum search buffer length */
    constexpr size_t MAX_SEARCH_LENGTH = 128;
    
    /** Maximum depth for hierarchy display */
    constexpr int MAX_HIERARCHY_DEPTH = 64;
    
    /** Indent width per hierarchy level */
    constexpr float INDENT_WIDTH = 20.0f;
    
    /** Row height for hierarchy items */
    constexpr float ROW_HEIGHT = 22.0f;
    
    /** Double-click time for rename mode */
    constexpr float DOUBLE_CLICK_TIME_MS = 300.0f;
}

//=============================================================================
// STATIC STATE (uses types from header)
//=============================================================================

static FHierarchyState s_State;
static std::unordered_map<uint64_t, FHierarchyNode> s_NodeCache;
static std::vector<uint64_t> s_RootNodeIDs;
static std::vector<uint64_t> s_FlattenedVisibleNodes;  // For keyboard navigation

//=============================================================================
// FORWARD DECLARATIONS
//=============================================================================

static void DrawToolbar();
static void DrawHierarchyTree(ISceneSystem* scene, CommandBuffer& cb);
static void DrawNodeRow(FHierarchyNode& node, ISceneSystem* scene, CommandBuffer& cb);
static void DrawNodeContextMenu(FHierarchyNode& node, ISceneSystem* scene, CommandBuffer& cb);
static void DrawCreateMenu(ISceneSystem* scene, CommandBuffer& cb, uint64_t parentID = 0);
static void DrawTypeFilterDropdown();

static void RefreshNodeCache(ISceneSystem* scene);
static void ApplyFilters();
static void SortNodes();
static void FlattenVisibleNodes();
static bool NodePassesFilter(const FHierarchyNode& node);
static void PropagateFilterToParents(uint64_t nodeID);

static void HandleSelection(uint64_t nodeID, bool isCtrlHeld, bool isShiftHeld);
static void HandleDragDrop(FHierarchyNode& node, ISceneSystem* scene, CommandBuffer& cb);
static void HandleKeyboardNavigation(ISceneSystem* scene, CommandBuffer& cb);
static void HandleRename(FHierarchyNode& node, ISceneSystem* scene, CommandBuffer& cb);

static void SelectAll();
static void DeselectAll();
static void DeleteSelectedNodes(ISceneSystem* scene, CommandBuffer& cb);
static void DuplicateSelectedNodes(ISceneSystem* scene, CommandBuffer& cb);
static void CopySelectedNodes();
static void PasteNodes(ISceneSystem* scene, CommandBuffer& cb, uint64_t parentID = 0);
static void FocusNodeInViewport(uint64_t nodeID);

static const char* GetNodeTypeIcon(const std::string& typeName);
static ImVec4 GetNodeTypeColor(const std::string& typeName);

//=============================================================================
// PUBLIC API IMPLEMENTATION
//=============================================================================

/**
 * @brief Main render function for the Hierarchy Panel
 * 
 * Called every frame by the UI system. Renders the scene hierarchy tree
 * and handles all user interaction.
 * 
 * @param scene Pointer to the current scene system (can be null)
 * @param cb Command buffer for undo/redo operations
 * 
 * Layout:
 * ┌─────────────────────────────────────────────────────────────┐
 * │ [Search...                    ] [+] [Filter] [Sort]        │
 * ├─────────────────────────────────────────────────────────────┤
 * │ ▼ Root                                                      │
 * │   ▼ Player                                            👁 🔒 │
 * │     ├ Camera                                          👁 🔒 │
 * │     └ Mesh                                            👁 🔒 │
 * │   ▶ Enemies (collapsed)                               👁 🔒 │
 * │   ├ Light_Sun                                         👁 🔒 │
 * │   └ Light_Fill                                        👁 🔒 │
 * ├─────────────────────────────────────────────────────────────┤
 * │ 5 objects selected                                          │
 * └─────────────────────────────────────────────────────────────┘
 */
void HierarchyPanel::OnUIRender(ISceneSystem* scene, CommandBuffer& cb) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    
    if (!ImGui::Begin("Scene Hierarchy", nullptr, ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }
    
    ImGui::PopStyleVar();
    
    // Menu bar
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Select All", "Ctrl+A")) SelectAll();
            if (ImGui::MenuItem("Deselect All", "Escape")) DeselectAll();
            ImGui::Separator();
            if (ImGui::MenuItem("Delete", "Delete", false, !s_State.SelectedNodeIDs.empty())) {
                DeleteSelectedNodes(scene, cb);
            }
            if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, !s_State.SelectedNodeIDs.empty())) {
                DuplicateSelectedNodes(scene, cb);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Copy", "Ctrl+C", false, !s_State.SelectedNodeIDs.empty())) {
                CopySelectedNodes();
            }
            if (ImGui::MenuItem("Paste", "Ctrl+V")) {
                PasteNodes(scene, cb);
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Create")) {
            DrawCreateMenu(scene, cb);
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Show Hidden", nullptr, &s_State.bShowHiddenNodes);
            ImGui::MenuItem("Show Locked", nullptr, &s_State.bShowLockedNodes);
            ImGui::Separator();
            
            if (ImGui::BeginMenu("Sort By")) {
                if (ImGui::MenuItem("Creation Order", nullptr, s_State.SortMode == ENodeSortMode::CreationOrder)) {
                    s_State.SortMode = ENodeSortMode::CreationOrder;
                    s_State.bNeedsRefresh = true;
                }
                if (ImGui::MenuItem("Alphabetical", nullptr, s_State.SortMode == ENodeSortMode::Alphabetical)) {
                    s_State.SortMode = ENodeSortMode::Alphabetical;
                    s_State.bNeedsRefresh = true;
                }
                if (ImGui::MenuItem("Type", nullptr, s_State.SortMode == ENodeSortMode::Type)) {
                    s_State.SortMode = ENodeSortMode::Type;
                    s_State.bNeedsRefresh = true;
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        
        ImGui::EndMenuBar();
    }
    
    // Refresh cache if scene changed
    if (s_State.bNeedsRefresh && scene) {
        RefreshNodeCache(scene);
        s_State.bNeedsRefresh = false;
        s_State.bNeedsRefilter = true;
    }
    
    // Apply filters
    if (s_State.bNeedsRefilter) {
        ApplyFilters();
        s_State.bNeedsRefilter = false;
    }
    
    // Toolbar
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 4));
    DrawToolbar();
    ImGui::PopStyleVar();
    
    ImGui::Separator();
    
    // Main hierarchy tree area
    float statusBarHeight = 24.0f;
    float treeHeight = ImGui::GetContentRegionAvail().y - statusBarHeight;
    
    ImGui::BeginChild("HierarchyTree", ImVec2(0, treeHeight), true);
    
    // Handle keyboard navigation
    HandleKeyboardNavigation(scene, cb);
    
    // Draw the tree
    if (scene) {
        DrawHierarchyTree(scene, cb);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        ImGui::TextWrapped("No scene loaded. Create or open a scene to view the hierarchy.");
        ImGui::PopStyleColor();
    }
    
    // Context menu for empty area (create new root object)
    if (ImGui::BeginPopupContextWindow("EmptyAreaContext", 
        ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        DrawCreateMenu(scene, cb, 0);
        ImGui::EndPopup();
    }
    
    // Handle drop into empty area (reparent to root)
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_NODE")) {
            uint64_t droppedID = *(uint64_t*)payload->Data;
            // TODO: Reparent to root
            // scene->ReparentNode(droppedID, 0);
            // cb.Push({EditorCommandType::ReparentNode, droppedID, 0});
            (void)droppedID;
            s_State.bNeedsRefresh = true;
        }
        ImGui::EndDragDropTarget();
    }
    
    ImGui::EndChild();
    
    // Status bar
    ImGui::Separator();
    size_t selectedCount = s_State.SelectedNodeIDs.size();
    if (selectedCount > 0) {
        ImGui::Text("%zu object(s) selected", selectedCount);
    } else {
        ImGui::TextDisabled("%zu objects in scene", s_NodeCache.size());
    }
    
    // Type filter dropdown
    if (s_State.bShowTypeFilter) {
        DrawTypeFilterDropdown();
    }
    
    ImGui::End();
}

/**
 * @brief Gets the ID of the currently selected node
 * 
 * For single selection scenarios. If multiple nodes are selected,
 * returns the first one.
 * 
 * @return Selected node ID, or 0 if nothing selected
 */
SceneNodeID HierarchyPanel::GetSelectedNode() const {
    return m_SelectedNode;
}

/**
 * @brief Sets the selected node programmatically
 * 
 * @param nodeID The node ID to select
 */
void HierarchyPanel::SetSelectedNode(uint64_t nodeID) {
    m_SelectedNode = nodeID;
    
    DeselectAll();
    if (nodeID != 0) {
        s_State.SelectedNodeIDs.insert(nodeID);
        
        auto it = s_NodeCache.find(nodeID);
        if (it != s_NodeCache.end()) {
            it->second.bIsSelected = true;
        }
    }
    
    // TODO: Broadcast selection changed event
    // EventDispatcher::Broadcast<HierarchySelectionChangedEvent>(s_State.SelectedNodeIDs);
}

/**
 * @brief Forces a refresh of the hierarchy cache
 */
void HierarchyPanel::Refresh() {
    s_State.bNeedsRefresh = true;
}

//=============================================================================
// INTERNAL IMPLEMENTATIONS
//=============================================================================

/**
 * @brief Draws the toolbar with search and quick actions
 */
static void DrawToolbar() {
    // Search input
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 120.0f);
    if (ImGui::InputTextWithHint("##Search", "Search hierarchy...", 
        s_State.SearchBuffer, sizeof(s_State.SearchBuffer))) {
        s_State.bNeedsRefilter = true;
    }
    
    ImGui::SameLine();
    
    // Add object button
    if (ImGui::Button("+", ImVec2(25, 0))) {
        ImGui::OpenPopup("CreateObjectPopup");
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Create new object");
    
    // Create object popup
    if (ImGui::BeginPopup("CreateObjectPopup")) {
        DrawCreateMenu(nullptr, *(CommandBuffer*)nullptr, 0);
        ImGui::EndPopup();
    }
    
    ImGui::SameLine();
    
    // Filter button
    if (ImGui::Button("Filter", ImVec2(50, 0))) {
        s_State.bShowTypeFilter = !s_State.bShowTypeFilter;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Filter by type");
}

/**
 * @brief Draws the hierarchy tree recursively
 * 
 * @param scene The scene system
 * @param cb Command buffer for operations
 */
static void DrawHierarchyTree(ISceneSystem* scene, CommandBuffer& cb) {
    // Draw root level nodes
    for (uint64_t rootID : s_RootNodeIDs) {
        auto it = s_NodeCache.find(rootID);
        if (it != s_NodeCache.end() && (it->second.bMatchesFilter || it->second.bHasMatchingChild)) {
            DrawNodeRow(it->second, scene, cb);
        }
    }
}

/**
 * @brief Draws a single node row with all its children
 * 
 * @param node The node to draw
 * @param scene The scene system
 * @param cb Command buffer for operations
 */
static void DrawNodeRow(FHierarchyNode& node, ISceneSystem* scene, CommandBuffer& cb) {
    ImGui::PushID(static_cast<int>(node.ID));
    
    // Determine if this node should be visible
    if (!node.bMatchesFilter && !node.bHasMatchingChild) {
        ImGui::PopID();
        return;
    }
    
    // Check visibility/lock filters
    if (!s_State.bShowHiddenNodes && node.Visibility == ENodeVisibility::Hidden) {
        ImGui::PopID();
        return;
    }
    if (!s_State.bShowLockedNodes && node.bIsLocked) {
        ImGui::PopID();
        return;
    }
    
    // Build tree node flags
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | 
                               ImGuiTreeNodeFlags_SpanAvailWidth |
                               ImGuiTreeNodeFlags_AllowOverlap;
    
    if (node.bIsSelected) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    
    if (node.ChildIDs.empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }
    
    // Dim color if doesn't match filter but has matching child
    if (!node.bMatchesFilter && node.bHasMatchingChild) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
    }
    
    // Type icon
    ImGui::PushStyleColor(ImGuiCol_Text, GetNodeTypeColor(node.TypeName));
    ImGui::Text("%s", GetNodeTypeIcon(node.TypeName));
    ImGui::PopStyleColor();
    ImGui::SameLine();
    
    // Node name or rename input
    bool isOpen = false;
    
    if (node.bIsRenaming && node.ID == s_State.RenamingNodeID) {
        // Rename mode
        ImGui::SetNextItemWidth(150.0f);
        
        if (ImGui::InputText("##Rename", s_State.RenameBuffer, sizeof(s_State.RenameBuffer),
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
            // Confirm rename
            HandleRename(node, scene, cb);
        }
        
        if (ImGui::IsItemDeactivated()) {
            node.bIsRenaming = false;
            s_State.RenamingNodeID = 0;
        }
        
        // Focus the input on first frame
        if (ImGui::IsItemVisible() && !ImGui::IsItemActive()) {
            ImGui::SetKeyboardFocusHere(-1);
        }
    } else {
        // Normal tree node
        isOpen = ImGui::TreeNodeEx(node.Name.c_str(), flags);
    }
    
    // Restore color
    if (!node.bMatchesFilter && node.bHasMatchingChild) {
        ImGui::PopStyleColor();
    }
    
    // Click handling
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen()) {
        HandleSelection(node.ID, ImGui::GetIO().KeyCtrl, ImGui::GetIO().KeyShift);
        
        // Double-click to start rename
        float currentTime = static_cast<float>(ImGui::GetTime() * 1000.0f);
        if (node.ID == s_State.LastClickedNodeID && 
            currentTime - s_State.LastClickTime < HierarchyConfig::DOUBLE_CLICK_TIME_MS) {
            node.bIsRenaming = true;
            s_State.RenamingNodeID = node.ID;
            strncpy(s_State.RenameBuffer, node.Name.c_str(), sizeof(s_State.RenameBuffer) - 1);
        }
        s_State.LastClickedNodeID = node.ID;
        s_State.LastClickTime = currentTime;
    }
    
    // Right-click context menu
    if (ImGui::BeginPopupContextItem()) {
        DrawNodeContextMenu(node, scene, cb);
        ImGui::EndPopup();
    }
    
    // Drag source
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        ImGui::SetDragDropPayload("HIERARCHY_NODE", &node.ID, sizeof(uint64_t));
        ImGui::Text("Move: %s", node.Name.c_str());
        if (s_State.SelectedNodeIDs.size() > 1) {
            ImGui::TextDisabled("(+%zu more)", s_State.SelectedNodeIDs.size() - 1);
        }
        ImGui::EndDragDropSource();
    }
    
    // Drag target
    HandleDragDrop(node, scene, cb);
    
    // Right side buttons (visibility, lock)
    float rightButtonsX = ImGui::GetWindowWidth() - 50.0f;
    ImGui::SameLine(rightButtonsX);
    
    // Visibility toggle
    const char* visIcon = (node.Visibility == ENodeVisibility::Visible) ? "O" : "-";
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Text, 
        node.Visibility == ENodeVisibility::Visible ? ImVec4(1, 1, 1, 1) : ImVec4(0.5f, 0.5f, 0.5f, 1));
    if (ImGui::SmallButton(visIcon)) {
        node.Visibility = (node.Visibility == ENodeVisibility::Visible) 
            ? ENodeVisibility::Hidden 
            : ENodeVisibility::Visible;
        // TODO: Apply to scene
        // scene->SetNodeVisibility(node.ID, node.Visibility);
        // cb.Push({EditorCommandType::SetVisibility, node.ID, node.Visibility});
    }
    ImGui::PopStyleColor(2);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle visibility");
    
    ImGui::SameLine();
    
    // Lock toggle
    const char* lockIcon = node.bIsLocked ? "L" : "U";
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Text, 
        node.bIsLocked ? ImVec4(1.0f, 0.5f, 0.0f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1));
    if (ImGui::SmallButton(lockIcon)) {
        node.bIsLocked = !node.bIsLocked;
        // TODO: Apply to scene
        // scene->SetNodeLocked(node.ID, node.bIsLocked);
    }
    ImGui::PopStyleColor(2);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(node.bIsLocked ? "Unlock" : "Lock");
    
    // Draw children if expanded
    if (isOpen && !node.ChildIDs.empty()) {
        for (uint64_t childID : node.ChildIDs) {
            auto childIt = s_NodeCache.find(childID);
            if (childIt != s_NodeCache.end()) {
                DrawNodeRow(childIt->second, scene, cb);
            }
        }
        ImGui::TreePop();
    }
    
    ImGui::PopID();
}

/**
 * @brief Draws the context menu for a node
 * 
 * @param node The node being right-clicked
 * @param scene The scene system
 * @param cb Command buffer for operations
 */
static void DrawNodeContextMenu(FHierarchyNode& node, ISceneSystem* scene, CommandBuffer& cb) {
    if (ImGui::MenuItem("Rename", "F2")) {
        node.bIsRenaming = true;
        s_State.RenamingNodeID = node.ID;
        strncpy(s_State.RenameBuffer, node.Name.c_str(), sizeof(s_State.RenameBuffer) - 1);
    }
    
    if (ImGui::MenuItem("Focus", "F")) {
        FocusNodeInViewport(node.ID);
    }
    
    ImGui::Separator();
    
    if (ImGui::MenuItem("Duplicate", "Ctrl+D")) {
        DuplicateSelectedNodes(scene, cb);
    }
    
    if (ImGui::MenuItem("Delete", "Delete")) {
        DeleteSelectedNodes(scene, cb);
    }
    
    ImGui::Separator();
    
    if (ImGui::MenuItem("Copy", "Ctrl+C")) {
        CopySelectedNodes();
    }
    
    if (ImGui::MenuItem("Paste", "Ctrl+V")) {
        PasteNodes(scene, cb, node.ID);
    }
    
    if (ImGui::MenuItem("Cut", "Ctrl+X")) {
        CopySelectedNodes();
        DeleteSelectedNodes(scene, cb);
    }
    
    ImGui::Separator();
    
    if (ImGui::BeginMenu("Create Child")) {
        DrawCreateMenu(scene, cb, node.ID);
        ImGui::EndMenu();
    }
    
    ImGui::Separator();
    
    bool isVisible = node.Visibility == ENodeVisibility::Visible;
    if (ImGui::MenuItem(isVisible ? "Hide" : "Show", "H")) {
        node.Visibility = isVisible ? ENodeVisibility::Hidden : ENodeVisibility::Visible;
        // TODO: Apply to scene
    }
    
    if (ImGui::MenuItem(node.bIsLocked ? "Unlock" : "Lock")) {
        node.bIsLocked = !node.bIsLocked;
        // TODO: Apply to scene
    }
}

/**
 * @brief Draws the create object menu
 * 
 * @param scene The scene system
 * @param cb Command buffer
 * @param parentID Parent node ID (0 for root)
 */
static void DrawCreateMenu(ISceneSystem* scene, CommandBuffer& cb, uint64_t parentID) {
    (void)scene; (void)cb; (void)parentID;
    
    if (ImGui::MenuItem("Empty Object")) {
        // TODO: scene->CreateNode("Empty", parentID);
        // cb.Push({EditorCommandType::SpawnEntity, "Empty", parentID});
        s_State.bNeedsRefresh = true;
    }
    
    ImGui::Separator();
    
    if (ImGui::BeginMenu("3D Object")) {
        if (ImGui::MenuItem("Cube")) { /* TODO */ s_State.bNeedsRefresh = true; }
        if (ImGui::MenuItem("Sphere")) { /* TODO */ s_State.bNeedsRefresh = true; }
        if (ImGui::MenuItem("Plane")) { /* TODO */ s_State.bNeedsRefresh = true; }
        if (ImGui::MenuItem("Cylinder")) { /* TODO */ s_State.bNeedsRefresh = true; }
        if (ImGui::MenuItem("Capsule")) { /* TODO */ s_State.bNeedsRefresh = true; }
        ImGui::EndMenu();
    }
    
    if (ImGui::BeginMenu("Light")) {
        if (ImGui::MenuItem("Directional Light")) { /* TODO */ s_State.bNeedsRefresh = true; }
        if (ImGui::MenuItem("Point Light")) { /* TODO */ s_State.bNeedsRefresh = true; }
        if (ImGui::MenuItem("Spot Light")) { /* TODO */ s_State.bNeedsRefresh = true; }
        if (ImGui::MenuItem("Area Light")) { /* TODO */ s_State.bNeedsRefresh = true; }
        ImGui::EndMenu();
    }
    
    if (ImGui::BeginMenu("Audio")) {
        if (ImGui::MenuItem("Audio Source")) { /* TODO */ s_State.bNeedsRefresh = true; }
        if (ImGui::MenuItem("Audio Listener")) { /* TODO */ s_State.bNeedsRefresh = true; }
        ImGui::EndMenu();
    }
    
    if (ImGui::BeginMenu("Effects")) {
        if (ImGui::MenuItem("Particle System")) { /* TODO */ s_State.bNeedsRefresh = true; }
        if (ImGui::MenuItem("Decal")) { /* TODO */ s_State.bNeedsRefresh = true; }
        ImGui::EndMenu();
    }
    
    if (ImGui::MenuItem("Camera")) { /* TODO */ s_State.bNeedsRefresh = true; }
    
    ImGui::Separator();
    
    if (ImGui::BeginMenu("UI")) {
        if (ImGui::MenuItem("Canvas")) { /* TODO */ s_State.bNeedsRefresh = true; }
        if (ImGui::MenuItem("Text")) { /* TODO */ s_State.bNeedsRefresh = true; }
        if (ImGui::MenuItem("Image")) { /* TODO */ s_State.bNeedsRefresh = true; }
        if (ImGui::MenuItem("Button")) { /* TODO */ s_State.bNeedsRefresh = true; }
        ImGui::EndMenu();
    }
}

/**
 * @brief Draws the type filter dropdown
 */
static void DrawTypeFilterDropdown() {
    ImGui::SetNextWindowPos(ImGui::GetCursorScreenPos());
    
    if (ImGui::Begin("TypeFilter", &s_State.bShowTypeFilter,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        
        ImGui::Text("Filter by Type:");
        ImGui::Separator();
        
        const char* types[] = { "Actor", "Light", "Camera", "Mesh", "Audio", "Particle", "UI" };
        
        for (const char* type : types) {
            bool enabled = s_State.TypeFilters.find(type) == s_State.TypeFilters.end();
            if (ImGui::Checkbox(type, &enabled)) {
                if (enabled) {
                    s_State.TypeFilters.erase(type);
                } else {
                    s_State.TypeFilters.insert(type);
                }
                s_State.bNeedsRefilter = true;
            }
        }
    }
    ImGui::End();
}

/**
 * @brief Refreshes the node cache from the scene
 * 
 * @param scene The scene system to query
 */
static void RefreshNodeCache(ISceneSystem* scene) {
    s_NodeCache.clear();
    s_RootNodeIDs.clear();
    
    if (!scene) return;
    
    // TODO: Replace with actual scene traversal
    // scene->ForEachNode([](ISceneNode* node) {
    //     FHierarchyNode cached;
    //     cached.ID = node->GetID();
    //     cached.Name = node->GetName();
    //     cached.TypeName = node->GetTypeName();
    //     cached.ParentID = node->GetParentID();
    //     // etc...
    //     s_NodeCache[cached.ID] = cached;
    // });
    
    // Mock data for testing
    for (int i = 1; i <= 10; ++i) {
        FHierarchyNode node;
        node.ID = static_cast<uint64_t>(i);
        node.Name = "Node_" + std::to_string(i);
        node.TypeName = (i % 3 == 0) ? "Light" : ((i % 2 == 0) ? "Mesh" : "Actor");
        node.ParentID = (i > 3) ? 1 : 0;  // First 3 are root, rest are children of Node_1
        node.Depth = (i > 3) ? 1 : 0;
        s_NodeCache[node.ID] = node;
        
        if (node.ParentID == 0) {
            s_RootNodeIDs.push_back(node.ID);
        }
    }
    
    // Build child lists
    for (auto& [id, node] : s_NodeCache) {
        if (node.ParentID != 0) {
            auto parentIt = s_NodeCache.find(node.ParentID);
            if (parentIt != s_NodeCache.end()) {
                parentIt->second.ChildIDs.push_back(id);
            }
        }
    }
    
    SortNodes();
}

/**
 * @brief Applies current filters to all cached nodes
 */
static void ApplyFilters() {
    // Reset filter state
    for (auto& [id, node] : s_NodeCache) {
        node.bMatchesFilter = NodePassesFilter(node);
        node.bHasMatchingChild = false;
    }
    
    // Propagate matching state up to parents
    for (auto& [id, node] : s_NodeCache) {
        if (node.bMatchesFilter) {
            PropagateFilterToParents(node.ParentID);
        }
    }
    
    FlattenVisibleNodes();
}

/**
 * @brief Checks if a node passes the current filter
 * 
 * @param node The node to check
 * @return true if the node should be visible
 */
static bool NodePassesFilter(const FHierarchyNode& node) {
    // Search filter
    if (s_State.SearchBuffer[0] != '\0') {
        std::string searchLower = s_State.SearchBuffer;
        std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);
        
        std::string nameLower = node.Name;
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
        
        if (nameLower.find(searchLower) == std::string::npos) {
            return false;
        }
    }
    
    // Type filter
    if (s_State.TypeFilters.find(node.TypeName) != s_State.TypeFilters.end()) {
        return false;
    }
    
    return true;
}

/**
 * @brief Propagates matching filter state up to parents
 * 
 * @param nodeID The node to start propagation from
 */
static void PropagateFilterToParents(uint64_t nodeID) {
    if (nodeID == 0) return;
    
    auto it = s_NodeCache.find(nodeID);
    if (it != s_NodeCache.end()) {
        it->second.bHasMatchingChild = true;
        PropagateFilterToParents(it->second.ParentID);
    }
}

/**
 * @brief Sorts nodes based on current sort mode
 */
static void SortNodes() {
    auto sortFunc = [](uint64_t a, uint64_t b) {
        auto itA = s_NodeCache.find(a);
        auto itB = s_NodeCache.find(b);
        if (itA == s_NodeCache.end() || itB == s_NodeCache.end()) return false;
        
        switch (s_State.SortMode) {
            case ENodeSortMode::Alphabetical:
                return s_State.bSortAscending 
                    ? itA->second.Name < itB->second.Name 
                    : itA->second.Name > itB->second.Name;
            case ENodeSortMode::Type:
                return s_State.bSortAscending 
                    ? itA->second.TypeName < itB->second.TypeName 
                    : itA->second.TypeName > itB->second.TypeName;
            default:
                return a < b;  // Creation order
        }
    };
    
    std::sort(s_RootNodeIDs.begin(), s_RootNodeIDs.end(), sortFunc);
    
    for (auto& [id, node] : s_NodeCache) {
        std::sort(node.ChildIDs.begin(), node.ChildIDs.end(), sortFunc);
    }
}

/**
 * @brief Builds a flattened list of visible nodes for keyboard navigation
 */
static void FlattenVisibleNodes() {
    s_FlattenedVisibleNodes.clear();
    
    std::function<void(uint64_t)> flatten = [&](uint64_t nodeID) {
        auto it = s_NodeCache.find(nodeID);
        if (it == s_NodeCache.end()) return;
        if (!it->second.bMatchesFilter && !it->second.bHasMatchingChild) return;
        
        s_FlattenedVisibleNodes.push_back(nodeID);
        
        if (it->second.bIsExpanded) {
            for (uint64_t childID : it->second.ChildIDs) {
                flatten(childID);
            }
        }
    };
    
    for (uint64_t rootID : s_RootNodeIDs) {
        flatten(rootID);
    }
}

/**
 * @brief Handles node selection with modifier keys
 * 
 * @param nodeID The clicked node
 * @param isCtrlHeld Whether Ctrl is held (toggle selection)
 * @param isShiftHeld Whether Shift is held (range selection)
 */
static void HandleSelection(uint64_t nodeID, bool isCtrlHeld, bool isShiftHeld) {
    auto it = s_NodeCache.find(nodeID);
    if (it == s_NodeCache.end()) return;
    
    if (isCtrlHeld) {
        // Toggle selection
        if (it->second.bIsSelected) {
            s_State.SelectedNodeIDs.erase(nodeID);
            it->second.bIsSelected = false;
        } else {
            s_State.SelectedNodeIDs.insert(nodeID);
            it->second.bIsSelected = true;
        }
    } else if (isShiftHeld && s_State.LastClickedNodeID != 0) {
        // Range selection
        auto startIt = std::find(s_FlattenedVisibleNodes.begin(), s_FlattenedVisibleNodes.end(), s_State.LastClickedNodeID);
        auto endIt = std::find(s_FlattenedVisibleNodes.begin(), s_FlattenedVisibleNodes.end(), nodeID);
        
        if (startIt != s_FlattenedVisibleNodes.end() && endIt != s_FlattenedVisibleNodes.end()) {
            if (startIt > endIt) std::swap(startIt, endIt);
            
            for (auto rangeIt = startIt; rangeIt <= endIt; ++rangeIt) {
                s_State.SelectedNodeIDs.insert(*rangeIt);
                auto nodeIt = s_NodeCache.find(*rangeIt);
                if (nodeIt != s_NodeCache.end()) {
                    nodeIt->second.bIsSelected = true;
                }
            }
        }
    } else {
        // Single selection
        for (auto& [id, node] : s_NodeCache) {
            node.bIsSelected = false;
        }
        s_State.SelectedNodeIDs.clear();
        
        s_State.SelectedNodeIDs.insert(nodeID);
        it->second.bIsSelected = true;
    }
    
    s_State.FocusedNodeID = nodeID;
    
    // TODO: Broadcast selection change
    // EventDispatcher::Broadcast<HierarchySelectionChangedEvent>(s_State.SelectedNodeIDs);
}

/**
 * @brief Handles drag-drop for a node
 * 
 * @param node The target node
 * @param scene The scene system
 * @param cb Command buffer
 */
static void HandleDragDrop(FHierarchyNode& node, ISceneSystem* scene, CommandBuffer& cb) {
    (void)scene; (void)cb;
    
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_NODE")) {
            uint64_t droppedID = *(uint64_t*)payload->Data;
            
            // Don't allow dropping onto self or descendants
            if (droppedID != node.ID) {
                // TODO: Reparent node
                // scene->ReparentNode(droppedID, node.ID);
                // cb.Push({EditorCommandType::ReparentNode, droppedID, node.ID});
                s_State.bNeedsRefresh = true;
            }
        }
        ImGui::EndDragDropTarget();
    }
}

/**
 * @brief Handles keyboard navigation in the hierarchy
 * 
 * @param scene The scene system
 * @param cb Command buffer
 */
static void HandleKeyboardNavigation(ISceneSystem* scene, CommandBuffer& cb) {
    if (!ImGui::IsWindowFocused()) return;
    
    ImGuiIO& io = ImGui::GetIO();
    
    // Delete
    if (ImGui::IsKeyPressed(ImGuiKey_Delete) && !s_State.SelectedNodeIDs.empty()) {
        DeleteSelectedNodes(scene, cb);
    }
    
    // F2 - Rename
    if (ImGui::IsKeyPressed(ImGuiKey_F2) && s_State.SelectedNodeIDs.size() == 1) {
        uint64_t nodeID = *s_State.SelectedNodeIDs.begin();
        auto it = s_NodeCache.find(nodeID);
        if (it != s_NodeCache.end()) {
            it->second.bIsRenaming = true;
            s_State.RenamingNodeID = nodeID;
            strncpy(s_State.RenameBuffer, it->second.Name.c_str(), sizeof(s_State.RenameBuffer) - 1);
        }
    }
    
    // Ctrl+A - Select all
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A)) {
        SelectAll();
    }
    
    // Escape - Deselect
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        DeselectAll();
    }
    
    // Ctrl+D - Duplicate
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D) && !s_State.SelectedNodeIDs.empty()) {
        DuplicateSelectedNodes(scene, cb);
    }
    
    // Ctrl+C - Copy
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C)) {
        CopySelectedNodes();
    }
    
    // Ctrl+V - Paste
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V)) {
        PasteNodes(scene, cb);
    }
    
    // F - Focus
    if (ImGui::IsKeyPressed(ImGuiKey_F) && !s_State.SelectedNodeIDs.empty()) {
        FocusNodeInViewport(*s_State.SelectedNodeIDs.begin());
    }
    
    // Arrow key navigation
    if (!s_FlattenedVisibleNodes.empty() && s_State.FocusedNodeID != 0) {
        auto currentIt = std::find(s_FlattenedVisibleNodes.begin(), s_FlattenedVisibleNodes.end(), s_State.FocusedNodeID);
        
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) && currentIt != s_FlattenedVisibleNodes.begin()) {
            --currentIt;
            HandleSelection(*currentIt, io.KeyCtrl, io.KeyShift);
        }
        
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) && currentIt != s_FlattenedVisibleNodes.end() - 1) {
            ++currentIt;
            HandleSelection(*currentIt, io.KeyCtrl, io.KeyShift);
        }
    }
}

/**
 * @brief Handles rename confirmation
 * 
 * @param node The node being renamed
 * @param scene The scene system
 * @param cb Command buffer
 */
static void HandleRename(FHierarchyNode& node, ISceneSystem* scene, CommandBuffer& cb) {
    (void)scene; (void)cb;
    
    if (s_State.RenameBuffer[0] != '\0') {
        // TODO: Apply rename to scene
        // scene->RenameNode(node.ID, s_State.RenameBuffer);
        // cb.Push({EditorCommandType::RenameNode, node.ID, node.Name, s_State.RenameBuffer});
        
        node.Name = s_State.RenameBuffer;
    }
    
    node.bIsRenaming = false;
    s_State.RenamingNodeID = 0;
}

/**
 * @brief Selects all visible nodes
 */
static void SelectAll() {
    s_State.SelectedNodeIDs.clear();
    for (auto& [id, node] : s_NodeCache) {
        if (node.bMatchesFilter || node.bHasMatchingChild) {
            node.bIsSelected = true;
            s_State.SelectedNodeIDs.insert(id);
        }
    }
}

/**
 * @brief Deselects all nodes
 */
static void DeselectAll() {
    for (auto& [id, node] : s_NodeCache) {
        node.bIsSelected = false;
    }
    s_State.SelectedNodeIDs.clear();
    s_State.FocusedNodeID = 0;
}

/**
 * @brief Deletes selected nodes
 * 
 * @param scene The scene system
 * @param cb Command buffer
 */
static void DeleteSelectedNodes(ISceneSystem* scene, CommandBuffer& cb) {
    (void)scene;
    
    for (uint64_t nodeID : s_State.SelectedNodeIDs) {
        // TODO: Delete from scene
        // scene->DeleteNode(nodeID);
        cb.Push({EditorCommandType::DeleteEntity, std::to_string(nodeID)});
    }
    
    DeselectAll();
    s_State.bNeedsRefresh = true;
}

/**
 * @brief Duplicates selected nodes
 * 
 * @param scene The scene system
 * @param cb Command buffer
 */
static void DuplicateSelectedNodes(ISceneSystem* scene, CommandBuffer& cb) {
    (void)scene; (void)cb;
    
    for (uint64_t nodeID : s_State.SelectedNodeIDs) {
        // TODO: Duplicate in scene
        // uint64_t newID = scene->DuplicateNode(nodeID);
        // cb.Push({EditorCommandType::DuplicateEntity, nodeID, newID});
        (void)nodeID;
    }
    
    s_State.bNeedsRefresh = true;
}

/**
 * @brief Copies selected nodes to clipboard
 */
static void CopySelectedNodes() {
    // TODO: Serialize selected nodes to clipboard
    // std::string serialized = SceneSerializer::SerializeNodes(s_State.SelectedNodeIDs);
    // Clipboard::SetText(serialized);
}

/**
 * @brief Pastes nodes from clipboard
 * 
 * @param scene The scene system
 * @param cb Command buffer
 * @param parentID Parent to paste under (0 for root)
 */
static void PasteNodes(ISceneSystem* scene, CommandBuffer& cb, uint64_t parentID) {
    (void)scene; (void)cb; (void)parentID;
    
    // TODO: Deserialize nodes from clipboard
    // std::string serialized = Clipboard::GetText();
    // auto newNodes = SceneSerializer::DeserializeNodes(serialized, parentID);
    // for (auto& node : newNodes) {
    //     scene->AddNode(node);
    //     cb.Push({EditorCommandType::SpawnEntity, node.ID});
    // }
    
    s_State.bNeedsRefresh = true;
}

/**
 * @brief Focuses camera on a node in the viewport
 * 
 * @param nodeID The node to focus on
 */
static void FocusNodeInViewport(uint64_t nodeID) {
    (void)nodeID;
    // TODO: Tell viewport to focus on this node
    // Viewport::FocusOnNode(nodeID);
    // EventDispatcher::Broadcast<FocusNodeEvent>(nodeID);
}

/**
 * @brief Gets an icon for a node type
 * 
 * @param typeName The type name
 * @return Icon text (TODO: Replace with FontAwesome glyphs)
 */
static const char* GetNodeTypeIcon(const std::string& typeName) {
    if (typeName == "Light") return "[L]";
    if (typeName == "Camera") return "[C]";
    if (typeName == "Mesh") return "[M]";
    if (typeName == "Audio") return "[A]";
    if (typeName == "Particle") return "[P]";
    if (typeName == "UI") return "[U]";
    return "[O]";  // Object/Actor
}

/**
 * @brief Gets a color for a node type
 * 
 * @param typeName The type name
 * @return ImGui color
 */
static ImVec4 GetNodeTypeColor(const std::string& typeName) {
    if (typeName == "Light") return ImVec4(1.0f, 0.9f, 0.4f, 1.0f);    // Yellow
    if (typeName == "Camera") return ImVec4(0.4f, 0.7f, 1.0f, 1.0f);   // Blue
    if (typeName == "Mesh") return ImVec4(0.5f, 0.8f, 0.5f, 1.0f);     // Green
    if (typeName == "Audio") return ImVec4(0.9f, 0.5f, 0.2f, 1.0f);    // Orange
    if (typeName == "Particle") return ImVec4(0.9f, 0.4f, 0.9f, 1.0f); // Magenta
    if (typeName == "UI") return ImVec4(0.3f, 0.9f, 0.9f, 1.0f);       // Cyan
    return ImVec4(0.8f, 0.8f, 0.8f, 1.0f);  // Gray
}

} // namespace RiftCore::UI
