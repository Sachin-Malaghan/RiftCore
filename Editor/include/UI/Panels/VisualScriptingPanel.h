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
    // Lifecycle
    //-------------------------------------------------------------------------
    
    /**
     * @brief Initializes the visual scripting panel
     * 
     * Creates the node editor context and loads node templates.
     */
    void Initialize();
    
    /**
     * @brief Shuts down the panel and releases resources
     */
    void Shutdown();
    
    //-------------------------------------------------------------------------
    // Rendering
    //-------------------------------------------------------------------------
    
    /**
     * @brief Main render function called every frame
     */
    void OnUIRender();
    
    //-------------------------------------------------------------------------
    // Script Management
    //-------------------------------------------------------------------------
    
    /**
     * @brief Creates a new empty script
     * @param name Script name
     */
    void NewScript(const std::string& name = "NewScript");
    
    /**
     * @brief Loads a visual script for editing
     * @param assetID The asset ID of the script to load
     */
    void LoadScript(uint64_t assetID);
    
    /**
     * @brief Saves the current visual script
     */
    void SaveScript();
    
    /**
     * @brief Saves the script to a new asset
     * @param filepath Output file path
     */
    void SaveScriptAs(const std::string& filepath);
    
    /**
     * @brief Gets the current script graph
     * @return Reference to the script graph
     */
    FScriptGraph& GetGraph();
    
    /**
     * @brief Checks if the script has unsaved changes
     * @return true if modified
     */
    bool IsModified() const;
    
    //-------------------------------------------------------------------------
    // Node Operations
    //-------------------------------------------------------------------------
    
    /**
     * @brief Creates a node from a template
     * @param templateName Template class name
     * @param position Position in graph
     * @return Created node ID
     */
    uint64_t CreateNode(const std::string& templateName, const ImVec2& position);
    
    /**
     * @brief Deletes a node
     * @param nodeID Node to delete
     */
    void DeleteNode(uint64_t nodeID);
    
    /**
     * @brief Deletes all selected nodes
     */
    void DeleteSelectedNodes();
    
    /**
     * @brief Duplicates selected nodes
     */
    void DuplicateSelectedNodes();
    
    /**
     * @brief Copies selected nodes to clipboard
     */
    void CopySelectedNodes();
    
    /**
     * @brief Pastes nodes from clipboard
     */
    void PasteNodes();
    
    //-------------------------------------------------------------------------
    // Link Operations
    //-------------------------------------------------------------------------
    
    /**
     * @brief Checks if a link can be created between two pins
     * @param startPinID Source pin
     * @param endPinID Destination pin
     * @return true if connection is valid
     */
    bool CanCreateLink(uint64_t startPinID, uint64_t endPinID);
    
    /**
     * @brief Creates a link between two pins
     * @param startPinID Source pin
     * @param endPinID Destination pin
     * @return Created link ID
     */
    uint64_t CreateLink(uint64_t startPinID, uint64_t endPinID);
    
    /**
     * @brief Deletes a link
     * @param linkID Link to delete
     */
    void DeleteLink(uint64_t linkID);
    
    //-------------------------------------------------------------------------
    // Selection
    //-------------------------------------------------------------------------
    
    /**
     * @brief Gets selected node IDs
     * @return Vector of selected node IDs
     */
    std::vector<uint64_t> GetSelectedNodes() const;
    
    /**
     * @brief Selects a node
     * @param nodeID Node to select
     * @param addToSelection Add to existing selection
     */
    void SelectNode(uint64_t nodeID, bool addToSelection = false);
    
    /**
     * @brief Clears selection
     */
    void ClearSelection();
    
    /**
     * @brief Selects all nodes
     */
    void SelectAll();
    
    //-------------------------------------------------------------------------
    // Compile & Debug
    //-------------------------------------------------------------------------
    
    /**
     * @brief Compiles the script
     * @return true if compilation succeeded
     */
    bool Compile();
    
    /**
     * @brief Starts debugging the script
     */
    void StartDebugging();
    
    /**
     * @brief Stops debugging
     */
    void StopDebugging();
    
    /**
     * @brief Pauses/resumes debug execution
     * @param paused Pause state
     */
    void SetDebugPaused(bool paused);
    
    /**
     * @brief Steps to the next node
     */
    void StepDebug();
    
    /**
     * @brief Checks if debugging is active
     * @return Debug state
     */
    bool IsDebugging() const;
    
    //-------------------------------------------------------------------------
    // Node Templates
    //-------------------------------------------------------------------------
    
    /**
     * @brief Registers a node template
     * @param tmpl Node template definition
     */
    void RegisterNodeTemplate(const FNodeTemplate& tmpl);
    
    /**
     * @brief Gets all registered node templates
     * @return Vector of templates
     */
    const std::vector<FNodeTemplate>& GetNodeTemplates() const;
    
    /**
     * @brief Finds a template by class name
     * @param className Template class name
     * @return Pointer to template (nullptr if not found)
     */
    const FNodeTemplate* FindTemplate(const std::string& className) const;
    
    //-------------------------------------------------------------------------
    // Configuration
    //-------------------------------------------------------------------------
    
    /**
     * @brief Gets the panel state
     * @return Reference to state struct
     */
    FVisualScriptState& GetState();
    
    /**
     * @brief Sets auto-save enabled
     * @param enabled Auto-save state
     */
    void SetAutoSaveEnabled(bool enabled);
    
    /**
     * @brief Sets grid snapping
     * @param enabled Snap state
     */
    void SetSnapToGrid(bool enabled);
    
    //-------------------------------------------------------------------------
    // Callbacks
    //-------------------------------------------------------------------------
    
    /** Called when script is modified */
    using ScriptModifiedCallback = std::function<void()>;
    
    /** Called when script is compiled */
    using CompileCallback = std::function<void(bool success, const std::string& message)>;
    
    /** Called when node is selected */
    using NodeSelectedCallback = std::function<void(uint64_t nodeID)>;
    
    void SetOnScriptModified(ScriptModifiedCallback callback);
    void SetOnCompile(CompileCallback callback);
    void SetOnNodeSelected(NodeSelectedCallback callback);
    
private:
    //-------------------------------------------------------------------------
    // Internal State
    //-------------------------------------------------------------------------
    
    static ed::EditorContext*   m_EditorContext;
    FScriptGraph                m_Graph;
    FVisualScriptState          m_State;
    std::vector<FNodeTemplate>  m_NodeTemplates;
    
    char                        m_SearchBuffer[256];
    std::vector<FNodeTemplate>  m_SearchResults;
    int                         m_SelectedSearchIndex;
    
    std::string                 m_ClipboardData;
    std::vector<std::string>    m_CompileErrors;
    std::vector<std::string>    m_DebugLog;
    
    ScriptModifiedCallback      m_OnScriptModified;
    CompileCallback             m_OnCompile;
    NodeSelectedCallback        m_OnNodeSelected;
    
    //-------------------------------------------------------------------------
    // Internal Methods
    //-------------------------------------------------------------------------
    
    void DrawToolbar();
    void DrawNodePalette();
    void DrawNodeGraph();
    void DrawDetailsPanel();
    void DrawMinimap();
    void DrawNodeSearchPopup();
    void DrawDebugPanel();
    
    void DrawNode(FNode& node);
    void DrawPin(const FPin& pin, bool isInput);
    void DrawLink(const FLink& link);
    void DrawNewLinkPreview();
    
    void InitializeNodeTemplates();
    FNode CreateNodeFromTemplate(const FNodeTemplate& tmpl, const ImVec2& position);
    
    void HandleKeyboardShortcuts();
    void HandleContextMenu();
    void UpdateDebugState();
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
