#pragma once
/**
 * @file HierarchyPanel.h
 * @brief Production-grade Scene Hierarchy Panel for RiftCore Engine
 * 
 * Provides a tree-view representation of the scene graph similar to
 * Unreal Engine's World Outliner. Supports multi-selection, drag-drop
 * reparenting, and visibility management.
 * 
 * @author RiftCore Team
 * @version 2.0.0
 * @date 2026-04-21
 */

#include <RiftCore/Scene/ISceneSystem.h>
#include <Renderer/HUD.h>  // For CommandBuffer (consolidated UI system)
#include <imgui.h>
#include <vector>
#include <string>
#include <unordered_set>
#include <functional>
#include <cstdint>

namespace RiftCore::UI {

// Import CommandBuffer from parent namespace
using RiftCore::CommandBuffer;
using RiftCore::EditorCommandType;

//=============================================================================
// ENUMERATIONS
//=============================================================================

/**
 * @enum ENodeVisibility
 * @brief Visibility states for scene nodes
 */
enum class ENodeVisibility : uint8_t {
    Visible,        ///< Node and children visible
    Hidden,         ///< Node hidden, children visible
    HiddenAll       ///< Node and children hidden
};

/**
 * @enum EHierarchySortMode
 * @brief Sort modes for the hierarchy tree
 */
enum class EHierarchySortMode : uint8_t {
    Unsorted,       ///< Scene order
    Alphabetical,   ///< A-Z by name
    Type,           ///< By node type
    Visibility      ///< Visible first
};

/**
 * @enum ENodeSortMode
 * @brief Sort modes for nodes (alternative naming)
 */
enum class ENodeSortMode : uint8_t {
    CreationOrder,  ///< Original creation order
    Alphabetical,   ///< A-Z by name
    Type            ///< By node type
};

/**
 * @enum EHierarchyAction
 * @brief Actions that can be performed on hierarchy nodes
 */
enum class EHierarchyAction : uint8_t {
    None,
    Select,
    Rename,
    Delete,
    Duplicate,
    Copy,
    Paste,
    CreateChild,
    MoveUp,
    MoveDown,
    ToggleVisibility,
    ToggleLock,
    FocusInViewport
};

//=============================================================================
// DATA STRUCTURES
//=============================================================================

/**
 * @struct FHierarchyNode
 * @brief Extended node information for hierarchy display
 */
struct FHierarchyNode {
    SceneNodeID     ID;             ///< Node ID in scene
    std::string     Name;           ///< Display name
    std::string     TypeName;       ///< Node type name
    SceneNodeID     ParentID;       ///< Parent node ID
    std::vector<SceneNodeID> ChildIDs; ///< Child node IDs
    
    ENodeVisibility Visibility;     ///< Visibility state
    bool            bIsLocked;      ///< Edit-locked
    bool            bIsExpanded;    ///< Tree expanded
    bool            bIsSelected;    ///< Currently selected
    bool            bIsHovered;     ///< Mouse hovering
    bool            bIsRenaming;    ///< In rename mode
    bool            bMatchesFilter; ///< Matches current filter
    bool            bHasMatchingChild; ///< Has child matching filter
    int             Depth;          ///< Nesting depth
    
    // Component indicators
    bool            bHasMesh;       ///< Has mesh component
    bool            bHasPhysics;    ///< Has physics body
    bool            bHasScript;     ///< Has script component
    bool            bHasLight;      ///< Has light component
    bool            bHasCamera;     ///< Has camera component
    bool            bHasAudio;      ///< Has audio component
    
    FHierarchyNode()
        : ID(0), ParentID(0), Visibility(ENodeVisibility::Visible),
          bIsLocked(false), bIsExpanded(true), bIsSelected(false),
          bIsHovered(false), bIsRenaming(false), bMatchesFilter(true),
          bHasMatchingChild(false), Depth(0), bHasMesh(false),
          bHasPhysics(false), bHasScript(false), bHasLight(false),
          bHasCamera(false), bHasAudio(false) {}
};

/**
 * @struct FHierarchyFilter
 * @brief Filter settings for the hierarchy panel
 */
struct FHierarchyFilter {
    std::string     SearchQuery;    ///< Name search
    bool            bShowHidden;    ///< Show hidden nodes
    bool            bShowLocked;    ///< Show locked nodes
    bool            bFilterMesh;    ///< Only nodes with mesh
    bool            bFilterPhysics; ///< Only nodes with physics
    bool            bFilterScript;  ///< Only nodes with script
    bool            bFilterLight;   ///< Only nodes with light
    bool            bFilterCamera;  ///< Only nodes with camera
    
    FHierarchyFilter()
        : bShowHidden(true), bShowLocked(true),
          bFilterMesh(false), bFilterPhysics(false),
          bFilterScript(false), bFilterLight(false),
          bFilterCamera(false) {}
};

/**
 * @struct FHierarchyState
 * @brief Encapsulates all mutable state for the hierarchy panel
 */
struct FHierarchyState {
    char                            SearchBuffer[128];      ///< Search input buffer
    char                            RenameBuffer[256];      ///< Rename input buffer
    std::unordered_set<uint64_t>    SelectedNodeIDs;        ///< Currently selected node IDs
    std::unordered_set<std::string> TypeFilters;            ///< Active type filters
    ENodeSortMode                   SortMode;               ///< Current sort mode
    uint64_t                        RenamingNodeID;         ///< Node being renamed
    uint64_t                        LastClickedNodeID;      ///< For double-click detection
    uint64_t                        FocusedNodeID;          ///< Currently focused node ID
    float                           LastClickTime;          ///< Time of last click
    bool                            bNeedsRefresh;          ///< Needs hierarchy refresh
    bool                            bNeedsRefilter;         ///< Needs filter reapplication
    bool                            bShowHiddenNodes;       ///< Show hidden nodes
    bool                            bShowLockedNodes;       ///< Show locked nodes
    bool                            bShowTypeFilter;        ///< Show type filter dropdown
    bool                            bSortAscending;         ///< Sort in ascending order
    
    FHierarchyState()
        : SortMode(ENodeSortMode::CreationOrder), RenamingNodeID(0),
          LastClickedNodeID(0), FocusedNodeID(0), LastClickTime(0.0f), bNeedsRefresh(true),
          bNeedsRefilter(false), bShowHiddenNodes(true), bShowLockedNodes(true),
          bShowTypeFilter(false), bSortAscending(true) {
        SearchBuffer[0] = '\0';
        RenameBuffer[0] = '\0';
    }
};

//=============================================================================
// HIERARCHY PANEL CLASS
//=============================================================================

/**
 * @class HierarchyPanel
 * @brief Main scene hierarchy panel for the editor
 * 
 * Features:
 * - Hierarchical tree view with expand/collapse
 * - Multi-selection (Ctrl+Click, Shift+Click, box select)
 * - Drag-drop reparenting
 * - Visibility and lock toggles
 * - Search/filter with parent propagation
 * - In-place rename (F2)
 * - Context menu with common operations
 * - Keyboard navigation
 * - Component indicators (icons)
 */
class HierarchyPanel {
public:
    //-------------------------------------------------------------------------
    // Lifecycle
    //-------------------------------------------------------------------------
    
    /**
     * @brief Initializes the hierarchy panel
     */
    void Initialize();
    
    /**
     * @brief Shuts down the panel
     */
    void Shutdown();
    
    //-------------------------------------------------------------------------
    // Rendering
    //-------------------------------------------------------------------------
    
    /**
     * @brief Main render function called every frame
     * @param scene Pointer to the scene system
     * @param cb Command buffer for undo/redo operations
     */
    void OnUIRender(ISceneSystem* scene, CommandBuffer& cb);
    
    //-------------------------------------------------------------------------
    // Selection
    //-------------------------------------------------------------------------
    
    /**
     * @brief Gets the primary selected node
     * @return Selected node ID (0 if none)
     */
    SceneNodeID GetSelectedNode() const;
    
    /**
     * @brief Gets the primary selected node as uint64_t
     * @return Selected node ID (0 if none)
     */
    uint64_t GetSelectedNodeID() const;
    
    /**
     * @brief Gets all selected nodes
     * @return Vector of selected node IDs
     */
    std::vector<SceneNodeID> GetSelectedNodes() const;
    
    /**
     * @brief Sets the selected node
     * @param nodeID Node to select
     * @param addToSelection If true, adds to existing selection
     */
    void SetSelectedNode(SceneNodeID nodeID, bool addToSelection = false);
    
    /**
     * @brief Sets the selected node (uint64_t version)
     * @param nodeID Node to select
     */
    void SetSelectedNode(uint64_t nodeID);
    
    /**
     * @brief Clears all selection
     */
    void ClearSelection();
    
    /**
     * @brief Selects all nodes
     */
    void SelectAll();
    
    /**
     * @brief Focuses on the selected node in the tree
     */
    void FocusOnSelection();
    
    //-------------------------------------------------------------------------
    // Operations
    //-------------------------------------------------------------------------
    
    /**
     * @brief Creates a new empty node
     * @param parentID Parent node (0 for root)
     * @return ID of created node
     */
    SceneNodeID CreateNode(SceneNodeID parentID = 0);
    
    /**
     * @brief Deletes selected nodes
     */
    void DeleteSelected();
    
    /**
     * @brief Duplicates selected nodes
     */
    void DuplicateSelected();
    
    /**
     * @brief Begins rename mode for selected node
     */
    void BeginRename();
    
    /**
     * @brief Reparents a node
     * @param nodeID Node to move
     * @param newParentID New parent (0 for root)
     * @param insertIndex Position among siblings (-1 for end)
     */
    void ReparentNode(SceneNodeID nodeID, SceneNodeID newParentID, int insertIndex = -1);
    
    /**
     * @brief Refreshes the hierarchy tree
     */
    void Refresh();
    
    //-------------------------------------------------------------------------
    // Visibility
    //-------------------------------------------------------------------------
    
    /**
     * @brief Sets node visibility
     * @param nodeID Target node
     * @param visibility New visibility state
     */
    void SetNodeVisibility(SceneNodeID nodeID, ENodeVisibility visibility);
    
    /**
     * @brief Toggles node lock state
     * @param nodeID Target node
     */
    void ToggleNodeLock(SceneNodeID nodeID);
    
    /**
     * @brief Shows all hidden nodes
     */
    void ShowAll();
    
    /**
     * @brief Hides selected nodes
     */
    void HideSelected();
    
    //-------------------------------------------------------------------------
    // Configuration
    //-------------------------------------------------------------------------
    
    /**
     * @brief Sets the sort mode
     * @param mode Sort criterion
     */
    void SetSortMode(EHierarchySortMode mode);
    
    /**
     * @brief Gets the filter settings
     * @return Reference to filter struct
     */
    FHierarchyFilter& GetFilter();
    
    /**
     * @brief Expands all tree nodes
     */
    void ExpandAll();
    
    /**
     * @brief Collapses all tree nodes
     */
    void CollapseAll();
    
    //-------------------------------------------------------------------------
    // Callbacks
    //-------------------------------------------------------------------------
    
    /** Callback for selection change */
    using SelectionCallback = std::function<void(const std::vector<SceneNodeID>&)>;
    
    /** Callback for node operations */
    using NodeActionCallback = std::function<void(SceneNodeID, EHierarchyAction)>;
    
    void SetOnSelectionChanged(SelectionCallback callback);
    void SetOnNodeAction(NodeActionCallback callback);
    
private:
    //-------------------------------------------------------------------------
    // Internal State
    //-------------------------------------------------------------------------
    
    ISceneSystem*               m_Scene;
    CommandBuffer*              m_CommandBuffer;
    
    std::vector<FHierarchyNode> m_Nodes;
    std::vector<SceneNodeID>    m_SelectedNodes;
    SceneNodeID                 m_SelectedNode;
    SceneNodeID                 m_LastSelectedNode;
    SceneNodeID                 m_DraggedNode;
    SceneNodeID                 m_DropTargetNode;
    int                         m_DropTargetPosition;
    
    FHierarchyFilter            m_Filter;
    FHierarchyState             m_State;
    EHierarchySortMode          m_SortMode;
    char                        m_SearchBuffer[256];
    char                        m_RenameBuffer[256];
    
    bool                        m_bNeedsRefresh;
    bool                        m_bIsDragging;
    bool                        m_bShowIcons;
    bool                        m_bShowTypeColumn;
    
    SelectionCallback           m_OnSelectionChanged;
    NodeActionCallback          m_OnNodeAction;
    
    //-------------------------------------------------------------------------
    // Internal Methods
    //-------------------------------------------------------------------------
    
    void DrawToolbar();
    void DrawSearchBar();
    void DrawTree();
    void DrawTreeNode(FHierarchyNode& node);
    void DrawNodeRow(FHierarchyNode& node);
    void DrawContextMenu();
    void DrawDragDropPreview();
    
    void RefreshNodeList();
    void ApplyFilters();
    void SortNodes();
    void HandleKeyboardShortcuts();
    void HandleDragDrop();
    void HandleSelection(SceneNodeID nodeID, bool ctrlHeld, bool shiftHeld);
    
    FHierarchyNode* FindNode(SceneNodeID id);
    void CollectChildrenRecursive(SceneNodeID parentID, std::vector<SceneNodeID>& outChildren);
};

//=============================================================================
// UTILITY FUNCTIONS
//=============================================================================

/** Gets the icon for a node based on its components */
const char* GetNodeIcon(const FHierarchyNode& node);

/** Gets the display color for a node type */
ImVec4 GetNodeTypeColor(const std::string& typeName);

} // namespace RiftCore::UI
