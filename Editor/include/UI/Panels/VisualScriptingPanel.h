#pragma once
/**
 * @file VisualScriptingPanel.h
 * @brief Production-grade Visual Scripting/Blueprint Panel for RiftCore Engine
 * 
 * Provides a node-based visual programming interface similar to Unreal Engine's
 * Blueprints. Supports creating, editing, and debugging visual scripts with
 * drag-drop connections and live preview.
 * 
 * @author RiftCore Team
 * @version 2.0.0
 * @date 2026-04-21
 */

#include <imgui.h>
#include <imgui_node_editor.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <cstdint>

namespace ed = ax::NodeEditor;

// Forward declarations
namespace RiftCore {
    class IVisualScriptVM;
    class INodeRegistry;
    class IScriptAsset;
}

namespace RiftCore::UI {

//=============================================================================
// ENUMERATIONS
//=============================================================================

/**
 * @enum EPinType
 * @brief Data types for node pins
 */
enum class EPinType : uint8_t {
    Flow,       ///< Execution flow (white)
    Bool,       ///< Boolean (red)
    Int,        ///< Integer (cyan)
    Float,      ///< Float (green)
    String,     ///< String (magenta)
    Vector2,    ///< 2D Vector (yellow)
    Vector3,    ///< 3D Vector (yellow)
    Vector4,    ///< 4D Vector (yellow)
    Color,      ///< Color (orange)
    Object,     ///< Object reference (blue)
    Struct,     ///< Custom struct (purple)
    Array,      ///< Array (teal)
    Wildcard,   ///< Any type (gray)
    Delegate    ///< Event delegate (red outline)
};

/**
 * @enum EPinDirection
 * @brief Pin direction (input/output)
 */
enum class EPinDirection : uint8_t {
    Input,
    Output
};

/**
 * @enum ENodeCategory
 * @brief Categories for organizing nodes
 */
enum class ENodeCategory : uint8_t {
    Event,          ///< Event nodes (BeginPlay, Tick, etc.)
    Flow,           ///< Flow control (Branch, Loop, Sequence)
    Math,           ///< Mathematical operations
    String,         ///< String manipulation
    Transform,      ///< Transform operations
    Actor,          ///< Actor/Entity operations
    Component,      ///< Component access
    Physics,        ///< Physics operations
    Input,          ///< Input handling
    Debug,          ///< Debug utilities
    Variable,       ///< Variable get/set
    Function,       ///< Function call/definition
    Macro,          ///< Macro nodes
    Custom          ///< User-defined nodes
};

/**
 * @enum ENodeState
 * @brief Runtime state of a node (for debugging)
 */
enum class ENodeState : uint8_t {
    Inactive,       ///< Node not executed
    Pending,        ///< Node queued for execution
    Executing,      ///< Currently executing
    Completed,      ///< Successfully completed
    Error           ///< Execution error
};

//=============================================================================
// DATA STRUCTURES
//=============================================================================

/**
 * @struct FPin
 * @brief Represents a single input/output pin on a node
 */
struct FPin {
    uint64_t            ID;             ///< Unique pin identifier
    std::string         Name;           ///< Display name
    EPinType            Type;           ///< Data type
    EPinDirection       Direction;      ///< Input or output
    uint64_t            NodeID;         ///< Parent node ID
    std::string         DefaultValue;   ///< Default value (for inputs)
    bool                bIsConnected;   ///< Has active connection
    bool                bIsRequired;    ///< Required for execution
    
    FPin()
        : ID(0), Type(EPinType::Flow), Direction(EPinDirection::Input),
          NodeID(0), bIsConnected(false), bIsRequired(false) {}
};

/**
 * @struct FNode
 * @brief Represents a single node in the visual script
 */
struct FNode {
    uint64_t            ID;             ///< Unique node identifier
    std::string         Name;           ///< Display name
    std::string         ClassName;      ///< Node class/type name
    ENodeCategory       Category;       ///< Node category
    ENodeState          State;          ///< Runtime state (debug)
    ImVec2              Position;       ///< Position in graph
    ImVec2              Size;           ///< Calculated size
    std::vector<FPin>   InputPins;      ///< Input pins
    std::vector<FPin>   OutputPins;     ///< Output pins
    std::string         Comment;        ///< User comment
    bool                bIsCompact;     ///< Use compact display
    bool                bIsPure;        ///< Pure function (no exec pins)
    bool                bIsCollapsed;   ///< Node is collapsed
    bool                bHasError;      ///< Has compile/runtime error
    std::string         ErrorMessage;   ///< Error description
    
    FNode()
        : ID(0), Category(ENodeCategory::Function), State(ENodeState::Inactive),
          Position(0, 0), Size(0, 0), bIsCompact(false), bIsPure(false),
          bIsCollapsed(false), bHasError(false) {}
};

/**
 * @struct FLink
 * @brief Represents a connection between two pins
 */
struct FLink {
    uint64_t            ID;             ///< Unique link identifier
    uint64_t            StartPinID;     ///< Source pin (output)
    uint64_t            EndPinID;       ///< Destination pin (input)
    ImVec4              Color;          ///< Link color (based on type)
    float               Thickness;      ///< Line thickness
    bool                bIsValid;       ///< Connection is valid
    
    FLink()
        : ID(0), StartPinID(0), EndPinID(0),
          Color(1, 1, 1, 1), Thickness(3.0f), bIsValid(true) {}
};

/**
 * @struct FNodeTemplate
 * @brief Template for creating new nodes
 */
struct FNodeTemplate {
    std::string         Name;           ///< Display name
    std::string         ClassName;      ///< Class name
    std::string         Description;    ///< Tooltip description
    std::string         Keywords;       ///< Search keywords
    ENodeCategory       Category;       ///< Node category
    std::vector<FPin>   InputPins;      ///< Input pin templates
    std::vector<FPin>   OutputPins;     ///< Output pin templates
    bool                bIsPure;        ///< Is pure function
    
    FNodeTemplate() : Category(ENodeCategory::Function), bIsPure(false) {}
};

/**
 * @struct FScriptGraph
 * @brief Complete visual script data
 */
struct FScriptGraph {
    uint64_t                            AssetID;        ///< Associated asset ID
    std::string                         Name;           ///< Script name
    std::unordered_map<uint64_t, FNode> Nodes;          ///< All nodes
    std::unordered_map<uint64_t, FLink> Links;          ///< All links
    std::unordered_map<uint64_t, FPin>  Pins;           ///< All pins (quick lookup)
    uint64_t                            NextID;         ///< ID counter
    bool                                bIsModified;    ///< Has unsaved changes
    
    FScriptGraph() : AssetID(0), NextID(1), bIsModified(false) {}
    
    uint64_t GenerateID() { return NextID++; }
};

/**
 * @struct FVisualScriptState
 * @brief Encapsulates all mutable state for the panel
 */
struct FVisualScriptState {
    // Selection
    std::vector<uint64_t>   SelectedNodeIDs;    ///< Selected nodes
    std::vector<uint64_t>   SelectedLinkIDs;    ///< Selected links
    uint64_t                HoveredNodeID;      ///< Hovered node
    uint64_t                HoveredPinID;       ///< Hovered pin
    
    // Editing
    bool                    bIsCreatingLink;    ///< Creating new link
    uint64_t                NewLinkStartPin;    ///< Link start pin
    ImVec2                  NewNodePosition;    ///< Position for new node
    bool                    bShowNewNodeMenu;   ///< Show node creation menu
    bool                    bShowNodePalette;   ///< Show node palette
    
    // Debug
    bool                    bIsDebugging;       ///< Debug mode active
    bool                    bIsPaused;          ///< Execution paused
    uint64_t                CurrentExecutingNode; ///< Currently executing node
    
    // UI State
    float                   Zoom;               ///< Canvas zoom
    ImVec2                  ScrollOffset;       ///< Canvas scroll
    bool                    bShowMinimap;       ///< Show minimap
    bool                    bShowDetails;       ///< Show details panel
    float                   DetailsPanelWidth;  ///< Details panel width
    bool                    bSnapToGrid;        ///< Snap nodes to grid
    bool                    bShowGrid;          ///< Show background grid
    
    // Auto-save
    float                   TimeSinceLastSave;  ///< Time since save
    bool                    bAutoSaveEnabled;   ///< Auto-save enabled
    
    // Search state
    char                    SearchBuffer[256];      ///< Node search buffer
    std::vector<FNodeTemplate> SearchResults;       ///< Filtered search results
    int                     SelectedSearchIndex;    ///< Selected result index
    
    FVisualScriptState()
        : HoveredNodeID(0), HoveredPinID(0), bIsCreatingLink(false),
          NewLinkStartPin(0), bShowNewNodeMenu(false), bShowNodePalette(false),
          bIsDebugging(false), bIsPaused(false), CurrentExecutingNode(0),
          Zoom(1.0f), bShowMinimap(true), bShowDetails(true),
          DetailsPanelWidth(250.0f), bSnapToGrid(true), bShowGrid(true),
          TimeSinceLastSave(0.0f), bAutoSaveEnabled(true),
          SelectedSearchIndex(-1) {
        SearchBuffer[0] = '\0';
    }
};

//=============================================================================
// VISUAL SCRIPTING PANEL CLASS
//=============================================================================

/**
 * @class VisualScriptingPanel
 * @brief Main visual scripting panel for the editor
 * 
 * Features:
 * - Node-based visual programming
 * - Type-safe pin connections with color coding
 * - Node palette with search and categories
 * - Drag-drop node creation
 * - Copy/paste/duplicate operations
 * - Undo/redo support
 * - Compile and debug functionality
 * - Auto-save
 * - Minimap navigation
 * - Keyboard shortcuts
 */
class VisualScriptingPanel {
public:
    //-------------------------------------------------------------------------
    // Lifecycle - real .cpp implementations
    //-------------------------------------------------------------------------
    
    void Initialize();
    void Shutdown();
    
    //-------------------------------------------------------------------------
    // Rendering - real .cpp implementation
    //-------------------------------------------------------------------------
    
    void OnUIRender();
    
    //-------------------------------------------------------------------------
    // Script Management - LoadScript and SaveScript have .cpp implementations
    //-------------------------------------------------------------------------
    
    void LoadScript(uint64_t assetID);
    void SaveScript();
    void NewScript(const std::string& name = "NewScript") { m_Graph = FScriptGraph(); m_Graph.Name = name; }
    void SaveScriptAs(const std::string& /*filepath*/) {}
    FScriptGraph& GetGraph() { return m_Graph; }
    bool IsModified() const { return m_Graph.bIsModified; }
    
    //-------------------------------------------------------------------------
    // Node / Link / Selection / Debug / Templates / Config (all stubs)
    //-------------------------------------------------------------------------
    
    uint64_t CreateNode(const std::string& /*templateName*/, const ImVec2& /*position*/) { return 0; }
    void DeleteNode(uint64_t /*nodeID*/) {}
    void DeleteSelectedNodes() {}
    void DuplicateSelectedNodes() {}
    void CopySelectedNodes() {}
    void PasteNodes() {}
    bool CanCreateLink(uint64_t /*startPinID*/, uint64_t /*endPinID*/) { return false; }
    uint64_t CreateLink(uint64_t /*startPinID*/, uint64_t /*endPinID*/) { return 0; }
    void DeleteLink(uint64_t /*linkID*/) {}
    std::vector<uint64_t> GetSelectedNodes() const { return m_State.SelectedNodeIDs; }
    void SelectNode(uint64_t nodeID, bool addToSelection = false) {
        if (!addToSelection) m_State.SelectedNodeIDs.clear();
        m_State.SelectedNodeIDs.push_back(nodeID);
    }
    void ClearSelection() { m_State.SelectedNodeIDs.clear(); }
    void SelectAll() {}
    bool Compile() { return false; }
    void StartDebugging() { m_State.bIsDebugging = true; }
    void StopDebugging() { m_State.bIsDebugging = false; }
    void SetDebugPaused(bool paused) { m_State.bIsPaused = paused; }
    void StepDebug() {}
    bool IsDebugging() const { return m_State.bIsDebugging; }
    void RegisterNodeTemplate(const FNodeTemplate& tmpl) { m_NodeTemplates.push_back(tmpl); }
    const std::vector<FNodeTemplate>& GetNodeTemplates() const { return m_NodeTemplates; }
    const FNodeTemplate* FindTemplate(const std::string& className) const {
        for (auto& t : m_NodeTemplates) if (t.ClassName == className) return &t;
        return nullptr;
    }
    FVisualScriptState& GetState() { return m_State; }
    void SetAutoSaveEnabled(bool enabled) { m_State.bAutoSaveEnabled = enabled; }
    void SetSnapToGrid(bool enabled) { m_State.bSnapToGrid = enabled; }
    
    //-------------------------------------------------------------------------
    // Callbacks
    //-------------------------------------------------------------------------
    
    using ScriptModifiedCallback = std::function<void()>;
    using CompileCallback = std::function<void(bool success, const std::string& message)>;
    using NodeSelectedCallback = std::function<void(uint64_t nodeID)>;
    
    void SetOnScriptModified(ScriptModifiedCallback callback) { m_OnScriptModified = callback; }
    void SetOnCompile(CompileCallback callback) { m_OnCompile = callback; }
    void SetOnNodeSelected(NodeSelectedCallback callback) { m_OnNodeSelected = callback; }


public:
    // Made public so static helper functions can access it
    static ed::EditorContext*   m_EditorContext;
    
private:
    //-------------------------------------------------------------------------
    // Internal State
    //-------------------------------------------------------------------------
    
    FScriptGraph                m_Graph;
    FVisualScriptState          m_State;
    std::vector<FNodeTemplate>  m_NodeTemplates;
    
    char                        m_SearchBuffer[256] = {};
    std::vector<FNodeTemplate>  m_SearchResults;
    int                         m_SelectedSearchIndex = -1;
    
    std::string                 m_ClipboardData;
    std::vector<std::string>    m_CompileErrors;
    std::vector<std::string>    m_DebugLog;
    
    ScriptModifiedCallback      m_OnScriptModified;
    CompileCallback             m_OnCompile;
    NodeSelectedCallback        m_OnNodeSelected;
};

//=============================================================================
// FREE FUNCTIONS
//=============================================================================

/** Saves the current visual script (free function version) */
void SaveScript();

//=============================================================================
// UTILITY FUNCTIONS
//=============================================================================

/** Gets the color for a pin type */
ImVec4 GetPinColor(EPinType type);

/** Gets the name of a pin type */
const char* GetPinTypeName(EPinType type);

/** Gets the name of a node category */
const char* GetNodeCategoryName(ENodeCategory category);

/** Gets the icon for a node category */
const char* GetNodeCategoryIcon(ENodeCategory category);

/** Checks if two pin types are compatible for linking */
bool ArePinTypesCompatible(EPinType typeA, EPinType typeB);

} // namespace RiftCore::UI
