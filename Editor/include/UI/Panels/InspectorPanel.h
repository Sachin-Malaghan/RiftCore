#pragma once
/**
 * @file InspectorPanel.h
 * @brief Production-grade Inspector/Details Panel for RiftCore Engine
 * 
 * Provides a comprehensive property editor for scene nodes and their
 * components, similar to Unreal Engine's Details panel.
 * 
 * @author RiftCore Team
 * @version 2.0.0
 * @date 2026-04-21
 */

#include <RiftCore/Scene/ISceneSystem.h>
#include <UI/HUD.h>  // For CommandBuffer (consolidated UI system)
#include <vector>
#include <string>
#include <functional>
#include <any>
#include <cstdint>
#include <unordered_map>
#include <algorithm>

namespace RiftCore::UI {

// Bring CommandBuffer into UI namespace for convenience
using ::RiftCore::CommandBuffer;
using ::RiftCore::EditorCommandType;
using ::RiftCore::EditorCommand;

//=============================================================================
// ENUMERATIONS
//=============================================================================

/**
 * @enum EPropertyType
 * @brief Types of properties that can be edited
 */
enum class EPropertyType : uint8_t {
    Bool,           ///< Boolean checkbox
    Int,            ///< Integer input
    Float,          ///< Float input with drag
    Double,         ///< Double precision float
    String,         ///< Text input
    Vector2,        ///< 2D vector (X, Y)
    Vector3,        ///< 3D vector (X, Y, Z)
    Vector4,        ///< 4D vector (X, Y, Z, W)
    Quaternion,     ///< Rotation (Euler or Quat)
    Color3,         ///< RGB color
    Color4,         ///< RGBA color
    Enum,           ///< Dropdown enum
    Flags,          ///< Bitfield flags
    Asset,          ///< Asset reference
    Object,         ///< Object reference
    Array,          ///< Dynamic array
    Struct,         ///< Nested struct
    Custom          ///< Custom widget
};

/**
 * @enum EComponentType
 * @brief Built-in component types
 */
enum class EComponentType : uint8_t {
    Transform,
    Mesh,
    SkeletalMesh,
    Material,
    Light,
    Camera,
    Physics,
    Collider,
    Audio,
    Script,
    Particle,
    Animation,
    Custom,
    COUNT
};

//=============================================================================
// DATA STRUCTURES
//=============================================================================

/**
 * @struct FPropertyMetadata
 * @brief Metadata for property display and editing
 */
struct FPropertyMetadata {
    std::string     Name;           ///< Display name
    std::string     Category;       ///< Property category
    std::string     Tooltip;        ///< Hover tooltip
    EPropertyType   Type;           ///< Property type
    
    float           MinValue;       ///< Minimum (for numbers)
    float           MaxValue;       ///< Maximum (for numbers)
    float           Step;           ///< Drag step
    std::string     Format;         ///< Printf format
    
    bool            bReadOnly;      ///< Cannot edit
    bool            bHidden;        ///< Not displayed
    bool            bAdvanced;      ///< In advanced section
    bool            bTransient;     ///< Not serialized
    
    std::vector<std::string> EnumOptions; ///< For enum dropdowns
    std::string     AssetFilter;    ///< Asset type filter
    
    FPropertyMetadata()
        : Type(EPropertyType::Float), MinValue(0), MaxValue(0),
          Step(0.1f), bReadOnly(false), bHidden(false),
          bAdvanced(false), bTransient(false) {}
};

/**
 * @struct FComponentInfo
 * @brief Information about a component for display
 */
struct FComponentInfo {
    uint64_t        ID;             ///< Component instance ID
    std::string     Name;           ///< Display name
    std::string     TypeName;       ///< Type name
    EComponentType  Type;           ///< Component category
    bool            bIsExpanded;    ///< Section expanded
    bool            bIsEnabled;     ///< Component active
    bool            bCanRemove;     ///< Can be deleted
    bool            bCanDisable;    ///< Can be toggled
    
    std::vector<FPropertyMetadata> Properties;
    
    FComponentInfo()
        : ID(0), Type(EComponentType::Custom),
          bIsExpanded(true), bIsEnabled(true),
          bCanRemove(true), bCanDisable(true) {}
};

/**
 * @struct FInspectorState
 * @brief Current state of the inspector
 */
struct FInspectorState {
    bool            bLocked;            ///< Lock to current selection
    bool            bLockSelection;     ///< Lock selection (alias)
    bool            bShowAdvanced;      ///< Show advanced properties
    bool            bShowReadOnly;      ///< Show read-only properties
    bool            bShowCategories;    ///< Group by category
    std::string     SearchFilter;       ///< Property search
    char            SearchBuffer[256];  ///< Search input buffer
    SceneNodeID     LockedNodeID;       ///< Locked node ID when selection locked
    float           LabelWidth;         ///< Label column width
    std::unordered_map<std::string, bool> ComponentExpanded; ///< Component expanded states (by name)
    
    FInspectorState()
        : bLocked(false), bLockSelection(false), bShowAdvanced(false),
          bShowReadOnly(true), bShowCategories(true), LockedNodeID(0), LabelWidth(120.0f) {
        SearchBuffer[0] = '\0';
    }
};

//=============================================================================
// INSPECTOR PANEL CLASS
//=============================================================================

/**
 * @class InspectorPanel
 * @brief Main inspector/details panel for the editor
 * 
 * Features:
 * - Dynamic property editing for all types
 * - Component-based layout with collapsible sections
 * - Color-coded transform inputs (X=Red, Y=Green, Z=Blue)
 * - Undo/redo support via CommandBuffer
 * - Lock to selection
 * - Search/filter properties
 * - Add/Remove components
 * - Copy/paste component values
 * - Reset to default
 */
class InspectorPanel {
public:
    //-------------------------------------------------------------------------
    // Lifecycle
    //-------------------------------------------------------------------------
    
    void Initialize() {}
    void Shutdown() {}
    
    //-------------------------------------------------------------------------
    // Rendering
    //-------------------------------------------------------------------------
    
    /**
     * @brief Main render function called every frame
     * @param selectedNode Currently selected scene node
     * @param cb Command buffer for undo/redo
     */
    void OnUIRender(ISceneNode* selectedNode, CommandBuffer& cb);
    
    //-------------------------------------------------------------------------
    // Selection / Component Operations / Configuration (inline stubs below)
    //-------------------------------------------------------------------------

    void SetShowAdvanced(bool show) { m_State.bShowAdvanced = show; }
    void SetSearchFilter(const std::string& filter) { m_State.SearchFilter = filter; }
    
    //-------------------------------------------------------------------------
    // Custom Property Drawers
    //-------------------------------------------------------------------------
    
    /** Custom property drawer function type */
    using PropertyDrawer = std::function<bool(const FPropertyMetadata&, void* value)>;
    
    void RegisterPropertyDrawer(const std::string& typeName, PropertyDrawer drawer) {
        m_CustomDrawers[typeName] = drawer;
    }
    void UnregisterPropertyDrawer(const std::string& typeName) {
        m_CustomDrawers.erase(typeName);
    }
    
    //-------------------------------------------------------------------------
    // Callbacks
    //-------------------------------------------------------------------------
    
    /** Callback when a property is modified */
    using PropertyChangedCallback = std::function<void(const std::string& propertyPath, const std::any& newValue)>;
    
    void SetOnPropertyChanged(PropertyChangedCallback callback) { m_OnPropertyChanged = callback; }
    
    // Stub implementations for methods not yet implemented in .cpp
    void SetInspectedNode(ISceneNode* node) { m_InspectedNode = node; }
    ISceneNode* GetInspectedNode() const { return m_InspectedNode; }
    void SetLocked(bool locked) { m_State.bLocked = locked; }
    bool IsLocked() const { return m_State.bLocked; }
    void AddComponent(EComponentType /*type*/) {}
    void RemoveComponent(uint64_t /*componentID*/) {}
    void CopyComponent(uint64_t /*componentID*/) {}
    void PasteComponent(uint64_t /*componentID*/) {}
    void ResetComponent(uint64_t /*componentID*/) {}
    FInspectorState& GetState() { return m_State; }

private:
    //-------------------------------------------------------------------------
    // Internal State
    //-------------------------------------------------------------------------
    
    ISceneNode*                 m_InspectedNode = nullptr;
    CommandBuffer*              m_CommandBuffer = nullptr;
    
    std::vector<FComponentInfo> m_Components;
    FInspectorState             m_State;
    char                        m_SearchBuffer[256] = {};
    
    std::unordered_map<std::string, PropertyDrawer> m_CustomDrawers;
    std::any                    m_CopiedComponentData;
    EComponentType              m_CopiedComponentType = EComponentType::Custom;
    
    PropertyChangedCallback     m_OnPropertyChanged;
    
    // Internal methods with real implementations in .cpp
    void DrawNoSelectionPlaceholder();
};

//=============================================================================
// UTILITY FUNCTIONS
//=============================================================================

/** Gets the display name for a component type */
const char* GetComponentTypeName(EComponentType type);

/** Gets the icon for a component type */
const char* GetComponentTypeIcon(EComponentType type);

/** Gets the color for a property type */
uint32_t GetPropertyTypeColor(EPropertyType type);

} // namespace RiftCore::UI
