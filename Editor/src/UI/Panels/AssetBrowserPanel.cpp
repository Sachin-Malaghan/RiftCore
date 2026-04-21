/**
 * @file AssetBrowserPanel.cpp
 * @brief Production-grade Asset Browser Panel implementation for RiftCore Engine
 * 
 * This panel provides a Content Browser similar to Unreal Engine's Content Browser,
 * allowing users to navigate, search, filter, and manage project assets.
 * 
 * @author RiftCore Team
 * @version 2.0.0
 * @date 2026-04-21
 * 
 * @note Architecture inspired by Unreal Engine's SContentBrowser
 * 
 * ============================================================================
 * EXTERNAL DEPENDENCIES (TODO: Implement these interfaces in your engine)
 * ============================================================================
 * - IAssetRegistry: Asset database interface for querying assets
 * - IAssetThumbnailProvider: Generates/caches asset thumbnails  
 * - IAssetDragDropHandler: Handles drag-drop operations
 * - IAssetImporter: Imports external files into the project
 * - EventDispatcher: For broadcasting asset selection changes
 * ============================================================================
 */

#include <UI/Panels/AssetBrowserPanel.h>
#include <imgui.h>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <ctime>
#include <cstring>

// TODO: Include your engine's asset system headers
// #include <Assets/AssetRegistry.h>
// #include <Assets/AssetThumbnailProvider.h>
// #include <Core/EventDispatcher.h>

namespace RiftCore::UI {

//=============================================================================
// CONFIGURATION CONSTANTS
//=============================================================================

namespace AssetBrowserConfig {
    /** Minimum thumbnail size in pixels */
    constexpr float MIN_THUMBNAIL_SIZE = 32.0f;
    /** Maximum thumbnail size in pixels */
    constexpr float MAX_THUMBNAIL_SIZE = 256.0f;
    /** Default thumbnail size in pixels */
    constexpr float DEFAULT_THUMBNAIL_SIZE = 64.0f;
    /** Padding between asset tiles */
    constexpr float DEFAULT_PADDING = 16.0f;
    /** Double-click timeout in milliseconds */
    constexpr float DOUBLE_CLICK_TIME_MS = 300.0f;
}

//=============================================================================
// STATIC STATE (uses types from header)
//=============================================================================

/**
 * @brief Internal browser state for this compilation unit
 */
struct FBrowserStateInternal {
    std::string                     CurrentPath;
    std::vector<std::string>        NavigationHistory;
    int32_t                         HistoryIndex;
    float                           ThumbnailSize;
    float                           Padding;
    EViewMode                       ViewMode;
    EAssetSortMode                  SortMode;
    char                            SearchBuffer[256];
    std::unordered_set<EAssetType>  ActiveTypeFilters;
    bool                            bShowOnlyFavorites;
    std::vector<uint64_t>           SelectedAssetIDs;
    uint64_t                        LastClickedAssetID;
    float                           LastClickTime;
    bool                            bShowLeftPanel;
    bool                            bShowFilters;
    float                           LeftPanelWidth;
    
    FBrowserStateInternal()
        : HistoryIndex(-1), ThumbnailSize(AssetBrowserConfig::DEFAULT_THUMBNAIL_SIZE),
          Padding(AssetBrowserConfig::DEFAULT_PADDING), ViewMode(EViewMode::Grid),
          SortMode(EAssetSortMode::Name_Ascending), bShowOnlyFavorites(false),
          LastClickedAssetID(0), LastClickTime(0.0f), bShowLeftPanel(true),
          bShowFilters(false), LeftPanelWidth(200.0f) {
        std::memset(SearchBuffer, 0, sizeof(SearchBuffer));
        CurrentPath = "/Game";
    }
};

//=============================================================================
// STATIC STATE
//=============================================================================

static FBrowserState s_State;
static std::vector<FAssetEntry> s_CachedAssets;
static std::vector<FAssetEntry*> s_FilteredAssets;
static bool s_bNeedsRefresh = true;
static bool s_bNeedsRefilter = true;

//=============================================================================
// FORWARD DECLARATIONS
//=============================================================================

static void DrawToolbar();
static void DrawBreadcrumbs();
static void DrawFolderTree();
static void DrawAssetGrid();
static void DrawAssetList();
static void DrawAssetContextMenu(FAssetEntry* entry);
static void DrawFilterDropdown();
static void DrawStatusBar();
static void RefreshAssetCache();
static void ApplyFiltersAndSort();
static void NavigateTo(const std::string& path, bool addToHistory = true);
static void HandleAssetClick(FAssetEntry* entry, bool isDoubleClick);
static void HandleKeyboardShortcuts();
static bool PassesFilter(const FAssetEntry& entry);
static const char* GetAssetTypeIcon(EAssetType type);
static const char* GetAssetTypeName(EAssetType type);
static std::string FormatFileSize(uint64_t bytes);
static std::string FormatTimestamp(uint64_t timestamp);

//=============================================================================
// PUBLIC API IMPLEMENTATION
//=============================================================================

/**
 * @brief Main render function for the Asset Browser Panel
 * 
 * Called every frame by the UI system. Handles all rendering and user interaction.
 * 
 * Architecture Layout:
 * ┌─────────────────────────────────────────────────────────────┐
 * │ Toolbar (Search, View Mode, Sort, Import, etc.)            │
 * ├─────────────────────────────────────────────────────────────┤
 * │ Breadcrumbs / Path Navigator                               │
 * ├────────────────┬────────────────────────────────────────────┤
 * │  Folder Tree   │           Asset Grid / List                │
 * │   (Optional)   │                                            │
 * ├────────────────┴────────────────────────────────────────────┤
 * │ Status Bar (Selection count, total assets, path)           │
 * └─────────────────────────────────────────────────────────────┘
 */
void AssetBrowserPanel::OnUIRender() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    
    if (!ImGui::Begin("Content Browser", nullptr, ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }
    ImGui::PopStyleVar();
    
    // Process keyboard shortcuts (Ctrl+F, Delete, F5, etc.)
    HandleKeyboardShortcuts();
    
    // Refresh asset cache if invalidated
    if (s_bNeedsRefresh) {
        RefreshAssetCache();
        s_bNeedsRefresh = false;
        s_bNeedsRefilter = true;
    }
    
    // Apply filters and sorting if needed
    if (s_bNeedsRefilter) {
        ApplyFiltersAndSort();
        s_bNeedsRefilter = false;
    }
    
    // === Menu Bar ===
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Tiles", nullptr, s_State.ViewMode == EViewMode::Tiles))
                s_State.ViewMode = EViewMode::Tiles;
            if (ImGui::MenuItem("List", nullptr, s_State.ViewMode == EViewMode::List))
                s_State.ViewMode = EViewMode::List;
            ImGui::Separator();
            ImGui::MenuItem("Show Folder Tree", nullptr, &s_State.bShowLeftPanel);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Create")) {
            if (ImGui::MenuItem("Folder")) { /* TODO: AssetOperations::CreateFolder */ }
            ImGui::Separator();
            if (ImGui::MenuItem("Material")) { /* TODO: Create material asset */ }
            if (ImGui::MenuItem("Blueprint")) { /* TODO: Create blueprint asset */ }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
    
    // === Toolbar ===
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 4));
    DrawToolbar();
    ImGui::PopStyleVar();
    
    ImGui::Separator();
    DrawBreadcrumbs();
    ImGui::Separator();
    
    // === Main Content Area ===
    float contentHeight = ImGui::GetContentRegionAvail().y - 24.0f;
    
    if (s_State.bShowLeftPanel) {
        // Left panel - Folder tree
        ImGui::BeginChild("FolderTree", ImVec2(s_State.LeftPanelWidth, contentHeight), true);
        DrawFolderTree();
        ImGui::EndChild();
        
        // Splitter handle
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
        ImGui::Button("##Splitter", ImVec2(4, contentHeight));
        ImGui::PopStyleColor();
        
        if (ImGui::IsItemActive()) {
            s_State.LeftPanelWidth += ImGui::GetIO().MouseDelta.x;
            s_State.LeftPanelWidth = std::clamp(s_State.LeftPanelWidth, 100.0f, 400.0f);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        }
        
        ImGui::SameLine();
    }
    
    // Right panel - Asset grid/list
    ImGui::BeginChild("AssetArea", ImVec2(0, contentHeight), true);
    
    if (s_State.ViewMode == EViewMode::Tiles) {
        DrawAssetGrid();
    } else {
        DrawAssetList();
    }
    
    // Context menu for empty area
    if (ImGui::BeginPopupContextWindow("AssetAreaContext", 
        ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        if (ImGui::BeginMenu("Create")) {
            if (ImGui::MenuItem("Folder")) { /* TODO */ }
            ImGui::Separator();
            if (ImGui::MenuItem("Material")) { /* TODO */ }
            if (ImGui::MenuItem("Blueprint")) { /* TODO */ }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Import Asset...")) { /* TODO: Open file dialog */ }
        if (ImGui::MenuItem("Refresh", "F5")) { s_bNeedsRefresh = true; }
        ImGui::EndPopup();
    }
    
    ImGui::EndChild();
    
    DrawStatusBar();
    ImGui::End();
}

//=============================================================================
// INTERNAL IMPLEMENTATIONS
//=============================================================================

/**
 * @brief Draws the toolbar with search, view options, and action buttons
 */
static void DrawToolbar() {
    // Navigation buttons
    bool canGoBack = s_State.HistoryIndex > 0;
    bool canGoForward = s_State.HistoryIndex < (int32_t)s_State.NavigationHistory.size() - 1;
    
    ImGui::BeginDisabled(!canGoBack);
    if (ImGui::ArrowButton("##Back", ImGuiDir_Left)) {
        s_State.HistoryIndex--;
        NavigateTo(s_State.NavigationHistory[s_State.HistoryIndex], false);
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Back (Alt+Left)");
    
    ImGui::SameLine();
    
    ImGui::BeginDisabled(!canGoForward);
    if (ImGui::ArrowButton("##Forward", ImGuiDir_Right)) {
        s_State.HistoryIndex++;
        NavigateTo(s_State.NavigationHistory[s_State.HistoryIndex], false);
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Forward (Alt+Right)");
    
    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();
    
    // Search bar
    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::InputTextWithHint("##Search", "Search... (Ctrl+F)", 
        s_State.SearchBuffer, sizeof(s_State.SearchBuffer))) {
        s_bNeedsRefilter = true;
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Filters")) {
        s_State.bShowFilters = !s_State.bShowFilters;
    }
    if (s_State.bShowFilters) {
        DrawFilterDropdown();
    }
    
    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();
    
    // Thumbnail size slider
    ImGui::SetNextItemWidth(100.0f);
    ImGui::SliderFloat("##Size", &s_State.ThumbnailSize, 
        AssetBrowserConfig::MIN_THUMBNAIL_SIZE, 
        AssetBrowserConfig::MAX_THUMBNAIL_SIZE, "%.0f");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Thumbnail Size");
    
    // Right-aligned import button
    ImGui::SameLine(ImGui::GetWindowWidth() - 80.0f);
    if (ImGui::Button("Import")) {
        // TODO: FileDialog::OpenMultiple("Import Assets", filter, callback);
    }
}

/**
 * @brief Draws the breadcrumb path navigator for folder navigation
 */
static void DrawBreadcrumbs() {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 4));
    
    std::vector<std::string> segments;
    segments.push_back("Content");
    
    std::string pathCopy = s_State.CurrentPath;
    size_t pos = 0;
    while ((pos = pathCopy.find('/')) != std::string::npos) {
        std::string token = pathCopy.substr(0, pos);
        if (!token.empty() && token != "Game") {
            segments.push_back(token);
        }
        pathCopy.erase(0, pos + 1);
    }
    if (!pathCopy.empty()) {
        segments.push_back(pathCopy);
    }
    
    std::string buildPath = "/Game";
    for (size_t i = 0; i < segments.size(); ++i) {
        if (i > 0) {
            ImGui::SameLine();
            ImGui::TextDisabled(">");
            ImGui::SameLine();
            buildPath += "/" + segments[i];
        }
        
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::SmallButton(segments[i].c_str())) {
            NavigateTo(buildPath, true);
        }
        ImGui::PopID();
    }
    
    ImGui::PopStyleVar();
}

/**
 * @brief Draws the folder tree panel for directory navigation
 */
static void DrawFolderTree() {
    ImGui::Text("Folders");
    ImGui::Separator();
    
    ImGuiTreeNodeFlags baseFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    
    bool rootOpen = ImGui::TreeNodeEx("Content", baseFlags | ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        NavigateTo("/Game", true);
    }
    
    if (rootOpen) {
        // TODO: Replace with actual folder structure from AssetRegistry
        const char* folders[] = { "Blueprints", "Materials", "Meshes", "Textures", "Audio", "Maps" };
        
        for (const char* folder : folders) {
            bool selected = s_State.CurrentPath.find(folder) != std::string::npos;
            ImGuiTreeNodeFlags flags = baseFlags | ImGuiTreeNodeFlags_Leaf;
            if (selected) flags |= ImGuiTreeNodeFlags_Selected;
            
            bool opened = ImGui::TreeNodeEx(folder, flags);
            if (ImGui::IsItemClicked()) {
                NavigateTo(std::string("/Game/") + folder, true);
            }
            
            // Drag-drop target for moving assets into folders
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_DRAG")) {
                    // TODO: AssetOperations::Move(assetID, targetPath);
                    (void)payload;
                }
                ImGui::EndDragDropTarget();
            }
            
            if (opened) ImGui::TreePop();
        }
        ImGui::TreePop();
    }
}

/**
 * @brief Draws the main asset grid in tile view mode
 */
static void DrawAssetGrid() {
    const float cellSize = s_State.ThumbnailSize + s_State.Padding;
    const float panelWidth = ImGui::GetContentRegionAvail().x;
    int columnCount = static_cast<int>(panelWidth / cellSize);
    if (columnCount < 1) columnCount = 1;
    
    ImGui::Columns(columnCount, nullptr, false);
    
    for (FAssetEntry* entry : s_FilteredAssets) {
        ImGui::PushID(static_cast<int>(entry->AssetID));
        
        // Selection highlighting
        ImVec4 btnColor = entry->bIsSelected 
            ? ImVec4(0.3f, 0.5f, 0.8f, 1.0f) 
            : ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
        ImVec4 btnHover = entry->bIsSelected
            ? ImVec4(0.4f, 0.6f, 0.9f, 1.0f)
            : ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
        
        ImGui::PushStyleColor(ImGuiCol_Button, btnColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, btnHover);
        
        // Asset thumbnail button
        // TODO: Use actual texture: ImGui::ImageButton(entry->ThumbnailTextureID, ...)
        ImGui::Button(GetAssetTypeIcon(entry->Type), ImVec2(s_State.ThumbnailSize, s_State.ThumbnailSize));
        
        ImGui::PopStyleColor(2);
        
        // Click handling with double-click detection
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            float currentTime = static_cast<float>(ImGui::GetTime() * 1000.0f);
            bool isDoubleClick = (entry->AssetID == s_State.LastClickedAssetID) && 
                                 (currentTime - s_State.LastClickTime < AssetBrowserConfig::DOUBLE_CLICK_TIME_MS);
            HandleAssetClick(entry, isDoubleClick);
            s_State.LastClickedAssetID = entry->AssetID;
            s_State.LastClickTime = currentTime;
        }
        
        // Drag source for drag-drop operations
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            ImGui::SetDragDropPayload("ASSET_DRAG", &entry->AssetID, sizeof(uint64_t));
            ImGui::Text("%s", entry->Name.c_str());
            ImGui::EndDragDropSource();
        }
        
        // Right-click context menu
        if (ImGui::BeginPopupContextItem()) {
            DrawAssetContextMenu(entry);
            ImGui::EndPopup();
        }
        
        // Tooltip on hover
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("%s", entry->Name.c_str());
            ImGui::TextDisabled("Type: %s", GetAssetTypeName(entry->Type));
            ImGui::TextDisabled("Size: %s", FormatFileSize(entry->SizeBytes).c_str());
            ImGui::EndTooltip();
        }
        
        // Asset name label
        ImGui::TextWrapped("%s", entry->Name.c_str());
        if (entry->bIsFavorite) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "*");
        }
        
        ImGui::NextColumn();
        ImGui::PopID();
    }
    
    ImGui::Columns(1);
}

/**
 * @brief Draws the asset list in list view mode with sortable columns
 */
static void DrawAssetList() {
    ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | 
                           ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                           ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV;
    
    if (ImGui::BeginTable("AssetTable", 4, flags)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Modified", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();
        
        for (FAssetEntry* entry : s_FilteredAssets) {
            ImGui::TableNextRow();
            ImGui::PushID(static_cast<int>(entry->AssetID));
            
            ImGui::TableSetColumnIndex(0);
            ImGuiSelectableFlags selFlags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick;
            if (ImGui::Selectable(entry->Name.c_str(), entry->bIsSelected, selFlags)) {
                HandleAssetClick(entry, ImGui::IsMouseDoubleClicked(0));
            }
            
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("ASSET_DRAG", &entry->AssetID, sizeof(uint64_t));
                ImGui::Text("%s", entry->Name.c_str());
                ImGui::EndDragDropSource();
            }
            
            if (ImGui::BeginPopupContextItem()) {
                DrawAssetContextMenu(entry);
                ImGui::EndPopup();
            }
            
            ImGui::TableSetColumnIndex(1);
            ImGui::TextDisabled("%s", GetAssetTypeName(entry->Type));
            
            ImGui::TableSetColumnIndex(2);
            ImGui::TextDisabled("%s", FormatFileSize(entry->SizeBytes).c_str());
            
            ImGui::TableSetColumnIndex(3);
            ImGui::TextDisabled("%s", FormatTimestamp(entry->LastModified).c_str());
            
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}

/**
 * @brief Draws the right-click context menu for an asset
 * @param entry The asset entry being right-clicked
 */
static void DrawAssetContextMenu(FAssetEntry* entry) {
    if (ImGui::MenuItem("Open", "Enter")) {
        HandleAssetClick(entry, true);
    }
    if (ImGui::MenuItem("Open in Explorer")) {
        // TODO: Platform::ShowInExplorer(entry->FullPath);
    }
    ImGui::Separator();
    
    if (ImGui::MenuItem("Rename", "F2")) {
        entry->bIsRenaming = true;
    }
    if (ImGui::MenuItem("Duplicate", "Ctrl+D")) {
        // TODO: AssetOperations::Duplicate(entry->AssetID);
    }
    if (ImGui::MenuItem("Delete", "Delete")) {
        // TODO: Show confirmation, then AssetOperations::Delete
    }
    ImGui::Separator();
    
    if (ImGui::MenuItem(entry->bIsFavorite ? "Remove from Favorites" : "Add to Favorites")) {
        entry->bIsFavorite = !entry->bIsFavorite;
    }
    ImGui::Separator();
    
    if (ImGui::MenuItem("Copy Path")) {
        // TODO: Clipboard::SetText(entry->FullPath);
    }
}

/**
 * @brief Draws the filter dropdown popup
 */
static void DrawFilterDropdown() {
    ImGui::SetNextWindowPos(ImGui::GetCursorScreenPos());
    
    if (ImGui::Begin("FilterPopup", &s_State.bShowFilters, 
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | 
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        
        ImGui::Text("Filter by Type:");
        ImGui::Separator();
        
        if (ImGui::SmallButton("All")) {
            s_State.ActiveTypeFilters.clear();
            s_bNeedsRefilter = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("None")) {
            for (int i = 0; i < static_cast<int>(EAssetType::COUNT); ++i)
                s_State.ActiveTypeFilters.insert(static_cast<EAssetType>(i));
            s_bNeedsRefilter = true;
        }
        
        ImGui::Separator();
        for (int i = 1; i < static_cast<int>(EAssetType::COUNT); ++i) {
            EAssetType type = static_cast<EAssetType>(i);
            bool filtered = s_State.ActiveTypeFilters.find(type) == s_State.ActiveTypeFilters.end();
            if (ImGui::Checkbox(GetAssetTypeName(type), &filtered)) {
                if (filtered) s_State.ActiveTypeFilters.erase(type);
                else s_State.ActiveTypeFilters.insert(type);
                s_bNeedsRefilter = true;
            }
        }
        
        ImGui::Separator();
        if (ImGui::Checkbox("Favorites Only", &s_State.bShowOnlyFavorites)) {
            s_bNeedsRefilter = true;
        }
    }
    ImGui::End();
}

/**
 * @brief Draws the status bar showing selection and path info
 */
static void DrawStatusBar() {
    ImGui::Separator();
    size_t selectedCount = s_State.SelectedAssetIDs.size();
    if (selectedCount > 0) {
        ImGui::Text("%zu item(s) selected", selectedCount);
    } else {
        ImGui::Text("%zu items", s_FilteredAssets.size());
    }
    ImGui::SameLine(ImGui::GetWindowWidth() - 200.0f);
    ImGui::TextDisabled("%s", s_State.CurrentPath.c_str());
}

/**
 * @brief Refreshes the asset cache from the asset registry
 * 
 * TODO: Replace mock data with actual AssetRegistry query
 */
static void RefreshAssetCache() {
    s_CachedAssets.clear();
    
    // TODO: Query actual asset registry
    // auto assets = AssetRegistry::GetAssetsInPath(s_State.CurrentPath);
    
    // Mock data for testing
    for (int i = 0; i < 20; ++i) {
        FAssetEntry entry;
        entry.AssetID = static_cast<uint64_t>(i + 1);
        entry.Name = "Asset_" + std::to_string(i);
        entry.Type = static_cast<EAssetType>((i % 8) + 1);
        entry.SizeBytes = (i + 1) * 1024 * ((i % 10) + 1);
        entry.LastModified = 1713700800 - (i * 86400);
        s_CachedAssets.push_back(entry);
    }
}

/**
 * @brief Applies current filters and sorting to the cached assets
 */
static void ApplyFiltersAndSort() {
    s_FilteredAssets.clear();
    
    for (auto& asset : s_CachedAssets) {
        if (PassesFilter(asset)) {
            s_FilteredAssets.push_back(&asset);
        }
    }
    
    std::sort(s_FilteredAssets.begin(), s_FilteredAssets.end(), 
        [](const FAssetEntry* a, const FAssetEntry* b) {
            switch (s_State.SortMode) {
                case EAssetSortMode::Name_Descending: return a->Name > b->Name;
                case EAssetSortMode::Type_Ascending:  return a->Type < b->Type;
                case EAssetSortMode::Type_Descending: return a->Type > b->Type;
                case EAssetSortMode::Size_Largest:    return a->SizeBytes > b->SizeBytes;
                case EAssetSortMode::Size_Smallest:   return a->SizeBytes < b->SizeBytes;
                default: return a->Name < b->Name;
            }
        });
}

/**
 * @brief Checks if an asset passes the current filters
 * @param entry The asset to check
 * @return true if the asset should be displayed
 */
static bool PassesFilter(const FAssetEntry& entry) {
    if (s_State.ActiveTypeFilters.find(entry.Type) != s_State.ActiveTypeFilters.end())
        return false;
    if (s_State.bShowOnlyFavorites && !entry.bIsFavorite)
        return false;
    if (s_State.SearchBuffer[0] != '\0') {
        std::string searchLower = s_State.SearchBuffer;
        std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);
        std::string nameLower = entry.Name;
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
        if (nameLower.find(searchLower) == std::string::npos)
            return false;
    }
    return true;
}

/**
 * @brief Navigates to a new path
 * @param path The path to navigate to
 * @param addToHistory Whether to add this navigation to the history stack
 */
static void NavigateTo(const std::string& path, bool addToHistory) {
    if (path == s_State.CurrentPath && !addToHistory) return;
    
    s_State.CurrentPath = path;
    
    if (addToHistory) {
        if (s_State.HistoryIndex < (int32_t)s_State.NavigationHistory.size() - 1) {
            s_State.NavigationHistory.erase(
                s_State.NavigationHistory.begin() + s_State.HistoryIndex + 1,
                s_State.NavigationHistory.end());
        }
        s_State.NavigationHistory.push_back(path);
        s_State.HistoryIndex = static_cast<int32_t>(s_State.NavigationHistory.size()) - 1;
    }
    s_bNeedsRefresh = true;
}

/**
 * @brief Handles click on an asset (single or double click)
 * @param entry The clicked asset
 * @param isDoubleClick Whether this is a double-click
 */
static void HandleAssetClick(FAssetEntry* entry, bool isDoubleClick) {
    ImGuiIO& io = ImGui::GetIO();
    
    if (isDoubleClick) {
        if (entry->bIsDirectory) {
            NavigateTo(entry->FullPath, true);
        } else {
            // TODO: AssetEditor::Open(entry->AssetID);
        }
        return;
    }
    
    if (io.KeyCtrl) {
        entry->bIsSelected = !entry->bIsSelected;
        if (entry->bIsSelected) {
            s_State.SelectedAssetIDs.push_back(entry->AssetID);
        } else {
            s_State.SelectedAssetIDs.erase(
                std::remove(s_State.SelectedAssetIDs.begin(), s_State.SelectedAssetIDs.end(), entry->AssetID),
                s_State.SelectedAssetIDs.end());
        }
    } else {
        for (auto& asset : s_CachedAssets) asset.bIsSelected = false;
        s_State.SelectedAssetIDs.clear();
        entry->bIsSelected = true;
        s_State.SelectedAssetIDs.push_back(entry->AssetID);
    }
}

/**
 * @brief Handles keyboard shortcuts for the asset browser
 */
static void HandleKeyboardShortcuts() {
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) return;
    
    ImGuiIO& io = ImGui::GetIO();
    
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F)) { /* Focus search */ }
    if (ImGui::IsKeyPressed(ImGuiKey_F5)) { s_bNeedsRefresh = true; }
    if (ImGui::IsKeyPressed(ImGuiKey_Delete) && !s_State.SelectedAssetIDs.empty()) { /* TODO: Delete */ }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A)) {
        s_State.SelectedAssetIDs.clear();
        for (auto& asset : s_CachedAssets) {
            asset.bIsSelected = true;
            s_State.SelectedAssetIDs.push_back(asset.AssetID);
        }
    }
    if (io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_LeftArrow) && s_State.HistoryIndex > 0) {
        s_State.HistoryIndex--;
        NavigateTo(s_State.NavigationHistory[s_State.HistoryIndex], false);
    }
    if (io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_RightArrow) && 
        s_State.HistoryIndex < (int32_t)s_State.NavigationHistory.size() - 1) {
        s_State.HistoryIndex++;
        NavigateTo(s_State.NavigationHistory[s_State.HistoryIndex], false);
    }
}

/**
 * @brief Gets an icon string for an asset type
 * @param type The asset type
 * @return Icon text (TODO: Replace with FontAwesome glyphs)
 */
static const char* GetAssetTypeIcon(EAssetType type) {
    switch (type) {
        case EAssetType::Texture:         return "[T]";
        case EAssetType::Material:        return "[M]";
        case EAssetType::Mesh:            return "[3D]";
        case EAssetType::Audio:           return "[A]";
        case EAssetType::Animation:       return "[An]";
        case EAssetType::Blueprint:       return "[BP]";
        case EAssetType::Scene:           return "[S]";
        case EAssetType::Script:          return "[Sc]";
        case EAssetType::Shader:          return "[Sh]";
        case EAssetType::Font:            return "[F]";
        case EAssetType::Prefab:          return "[P]";
        case EAssetType::PhysicsMaterial: return "[PM]";
        case EAssetType::ParticleSystem:  return "[VFX]";
        default:                          return "[?]";
    }
}

/**
 * @brief Gets a human-readable name for an asset type
 * @param type The asset type
 * @return Human-readable type name
 */
static const char* GetAssetTypeName(EAssetType type) {
    switch (type) {
        case EAssetType::Texture:         return "Texture";
        case EAssetType::Material:        return "Material";
        case EAssetType::Mesh:            return "Static Mesh";
        case EAssetType::Audio:           return "Audio";
        case EAssetType::Animation:       return "Animation";
        case EAssetType::Blueprint:       return "Blueprint";
        case EAssetType::Scene:           return "Scene";
        case EAssetType::Script:          return "Script";
        case EAssetType::Shader:          return "Shader";
        case EAssetType::Font:            return "Font";
        case EAssetType::Prefab:          return "Prefab";
        case EAssetType::PhysicsMaterial: return "Physics Material";
        case EAssetType::ParticleSystem:  return "Particle System";
        default:                          return "Unknown";
    }
}

/**
 * @brief Formats a byte size into human-readable string
 * @param bytes Size in bytes
 * @return Formatted string (e.g., "1.5 MB")
 */
static std::string FormatFileSize(uint64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    double size = static_cast<double>(bytes);
    while (size >= 1024.0 && unitIndex < 4) {
        size /= 1024.0;
        unitIndex++;
    }
    char buffer[32];
    snprintf(buffer, sizeof(buffer), unitIndex == 0 ? "%.0f %s" : "%.1f %s", size, units[unitIndex]);
    return buffer;
}

/**
 * @brief Formats a timestamp into human-readable string
 * @param timestamp Unix timestamp
 * @return Formatted date/time string
 */
static std::string FormatTimestamp(uint64_t timestamp) {
    if (timestamp == 0) return "Unknown";
    std::time_t time = static_cast<std::time_t>(timestamp);
    std::tm* tm = std::localtime(&time);
    if (!tm) return "Invalid";
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", tm);
    return buffer;
}

} // namespace RiftCore::UI
