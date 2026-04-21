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
#include <UI/Commands/CommandBuffer.h>
#include <vector>
#include <string>
#include <functional>
#include <any>
#include <cstdint>
#include <unordered_map>

namespace RiftCore::UI {

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
    bool            bLocked;        ///< Lock to current selection
    bool            bShowAdvanced;  ///< Show advanced properties
    bool            bShowReadOnly;  ///< Show read-only properties
    bool            bShowCategories;///< Group by category
    std::string     SearchFilter;   ///< Property search
    
    FInspectorState()
        : bLocked(false), bShowAdvanced(false),
          bShowReadOnly(true), bShowCategories(true) {}
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
    
    /**
     * @brief Initializes the inspector panel
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
     * @param selectedNode Currently selected scene node
     * @param cb Command buffer for undo/redo
     */
    void OnUIRender(ISceneNode* selectedNode, CommandBuffer& cb);
    
    //-------------------------------------------------------------------------
    // Selection
    //-------------------------------------------------------------------------
    
    /**
     * @brief Sets the inspected node
     * @param node Node to inspect (nullptr to clear)
     */
    void SetInspectedNode(ISceneNode* node);
    
    /**
     * @brief Gets the currently inspected node
     * @return Inspected node pointer
     */
    ISceneNode* GetInspectedNode() const;
    
    /**
     * @brief Locks/unlocks the inspector to current selection
     * @param locked Lock state
     */
    void SetLocked(bool locked);
    
    /**
     * @brief Checks if inspector is locked
     * @return Lock state
     */
    bool IsLocked() const;
    
    //-------------------------------------------------------------------------
    // Component Operations
    //-------------------------------------------------------------------------
    
    /**
     * @brief Adds a component to the selected node
     * @param type Component type to add
     */
    void AddComponent(EComponentType type);
    
    /**
     * @brief Removes a component from the selected node
     * @param componentID Component instance ID
     */
    void RemoveComponent(uint64_t componentID);
    
    /**
     * @brief Copies a component's values
     * @param componentID Component to copy
     */
    void CopyComponent(uint64_t componentID);
    
    /**
     * @brief Pastes copied component values
     * @param componentID Target component
     */
    void PasteComponent(uint64_t componentID);
    
    /**
     * @brief Resets a component to default values
     * @param componentID Component to reset
     */
    void ResetComponent(uint64_t componentID);
    
    //-------------------------------------------------------------------------
    // Configuration
    //-------------------------------------------------------------------------
    
    /**
     * @brief Gets the inspector state
     * @return Reference to state struct
     */
    FInspectorState& GetState();
    
    /**
     * @brief Sets whether to show advanced properties
     * @param show Show advanced
     */
    void SetShowAdvanced(bool show);
    
    /**
     * @brief Sets the property search filter
     * @param filter Search string
     */
    void SetSearchFilter(const std::string& filter);
    
    //-------------------------------------------------------------------------
    // Custom Property Drawers
    //-------------------------------------------------------------------------
    
    /** Custom property drawer function type */
    using PropertyDrawer = std::function<bool(const FPropertyMetadata&, void* value)>;
    
    /**
     * @brief Registers a custom property drawer
     * @param typeName Property type name
     * @param drawer Drawer function
     */
    void RegisterPropertyDrawer(const std::string& typeName, PropertyDrawer drawer);
    
    /**
     * @brief Unregisters a custom property drawer
     * @param typeName Property type name
     */
    void UnregisterPropertyDrawer(const std::string& typeName);
    
    //-------------------------------------------------------------------------
    // Callbacks
    //-------------------------------------------------------------------------
    
    /** Callback when a property is modified */
    using PropertyChangedCallback = std::function<void(const std::string& propertyPath, const std::any& newValue)>;
    
    void SetOnPropertyChanged(PropertyChangedCallback callback);
    
private:
    //-------------------------------------------------------------------------
    // Internal State
    //-------------------------------------------------------------------------
    
    ISceneNode*                 m_InspectedNode;
    CommandBuffer*              m_CommandBuffer;
    
    std::vector<FComponentInfo> m_Components;
    FInspectorState             m_State;
    char                        m_SearchBuffer[256];
    
    std::unordered_map<std::string, PropertyDrawer> m_CustomDrawers;
    std::any                    m_CopiedComponentData;
    EComponentType              m_CopiedComponentType;
    
    PropertyChangedCallback     m_OnPropertyChanged;
    
    //-------------------------------------------------------------------------
    // Component Drawing
    //-------------------------------------------------------------------------
    
    void DrawToolbar();
    void DrawNodeHeader();
    void DrawComponents();
    void DrawComponentSection(FComponentInfo& component);
    void DrawAddComponentButton();
    void DrawAddComponentMenu();
    
    //-------------------------------------------------------------------------
    // Built-in Component Drawers
    //-------------------------------------------------------------------------
    
    void DrawTransform(ISceneNode* node);
    void DrawMesh(ISceneNode* node);
    void DrawMaterial(ISceneNode* node);
    void DrawLight(ISceneNode* node);
    void DrawCamera(ISceneNode* node);
    void DrawPhysics(ISceneNode* node);
    void DrawCollider(ISceneNode* node);
    void DrawAudio(ISceneNode* node);
    void DrawScript(ISceneNode* node);
    
    //-------------------------------------------------------------------------
    // Property Widgets
    //-------------------------------------------------------------------------
    
    bool DrawProperty(const FPropertyMetadata& meta, void* value);
    bool DrawBoolProperty(const FPropertyMetadata& meta, bool* value);
    bool DrawIntProperty(const FPropertyMetadata& meta, int* value);
    bool DrawFloatProperty(const FPropertyMetadata& meta, float* value);
    bool DrawStringProperty(const FPropertyMetadata& meta, std::string* value);
    bool DrawVector2Property(const FPropertyMetadata& meta, float* value);
    bool DrawVector3Property(const FPropertyMetadata& meta, float* value);
    bool DrawVector4Property(const FPropertyMetadata& meta, float* value);
    bool DrawColorProperty(const FPropertyMetadata& meta, float* value, bool alpha);
    bool DrawEnumProperty(const FPropertyMetadata& meta, int* value);
    bool DrawAssetProperty(const FPropertyMetadata& meta, uint64_t* assetID);
    
    //-------------------------------------------------------------------------
    // Utility
    //-------------------------------------------------------------------------
    
    void RefreshComponents();
    void ApplyFilter();
    void HandleKeyboardShortcuts();
    bool MatchesFilter(const std::string& name);
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
