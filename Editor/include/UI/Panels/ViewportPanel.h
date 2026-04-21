#pragma once
/**
 * @file ViewportPanel.h
 * @brief Production-grade 3D Viewport Panel for RiftCore Engine
 * 
 * Provides a comprehensive 3D scene viewport similar to Unreal Engine's
 * Level Viewport. Includes camera controls, gizmo manipulation, render
 * mode switching, and debug visualization.
 * 
 * @author RiftCore Team
 * @version 2.0.0
 * @date 2026-04-21
 */

#include <UI/Styling/ImGuiTheme.h>
#include <GizmoSystem.h>
#include <vector>
#include <string>
#include <functional>
#include <cstdint>

// Forward declarations
struct ImVec2;
struct ImVec4;

namespace RiftCore {
    class ISceneSystem;
    class IRenderer;
    class ICamera;
    struct Ray;
}

namespace RiftCore::UI {

//=============================================================================
// ENUMERATIONS
//=============================================================================

/**
 * @enum EGizmoMode
 * @brief Transform gizmo operation modes
 */
enum class EGizmoMode : uint8_t {
    Translate,      ///< Move objects
    Rotate,         ///< Rotate objects
    Scale,          ///< Scale objects
    Universal       ///< Combined transform
};

/**
 * @enum EGizmoSpace
 * @brief Coordinate space for gizmo operations
 */
enum class EGizmoSpace : uint8_t {
    Local,          ///< Object local space
    World           ///< World space
};

/**
 * @enum ERenderMode
 * @brief Viewport render/visualization modes
 */
enum class ERenderMode : uint8_t {
    Lit,                ///< Full lighting
    Unlit,              ///< No lighting
    Wireframe,          ///< Wireframe only
    WireframeOnShaded,  ///< Wireframe overlay
    DetailLighting,     ///< Lighting only
    LightingOnly,       ///< No textures
    LightComplexity,    ///< Light cost visualization
    ShaderComplexity,   ///< Shader cost visualization
    Normals,            ///< Normal vectors
    UV,                 ///< UV coordinates
    VertexColors,       ///< Vertex color display
    Overdraw            ///< Overdraw visualization
};

/**
 * @enum ECameraMode
 * @brief Viewport camera modes
 */
enum class ECameraMode : uint8_t {
    Perspective,    ///< 3D perspective
    Orthographic,   ///< Orthographic projection
    Top,            ///< Top-down view
    Bottom,         ///< Bottom-up view
    Front,          ///< Front view
    Back,           ///< Back view
    Left,           ///< Left view
    Right           ///< Right view
};

/**
 * @enum EShowFlag
 * @brief Toggleable viewport display options
 */
enum class EShowFlag : uint32_t {
    Grid            = 1 << 0,   ///< Ground grid
    Axis            = 1 << 1,   ///< World axis
    Bounds          = 1 << 2,   ///< Bounding boxes
    Wireframe       = 1 << 3,   ///< Wireframe overlay
    Colliders       = 1 << 4,   ///< Physics colliders
    NavMesh         = 1 << 5,   ///< Navigation mesh
    LightRadius     = 1 << 6,   ///< Light influence
    CameraFrustum   = 1 << 7,   ///< Camera frustums
    AudioRadius     = 1 << 8,   ///< Audio falloff
    Bones           = 1 << 9,   ///< Skeletal bones
    Particles       = 1 << 10,  ///< Particle systems
    Fog             = 1 << 11,  ///< Fog effects
    PostProcess     = 1 << 12,  ///< Post-processing
    AO              = 1 << 13,  ///< Ambient occlusion
    Bloom           = 1 << 14,  ///< Bloom effect
    MotionBlur      = 1 << 15,  ///< Motion blur
    DOF             = 1 << 16,  ///< Depth of field
    AntiAliasing    = 1 << 17,  ///< Anti-aliasing
    Shadows         = 1 << 18,  ///< Shadow rendering
    Reflections     = 1 << 19,  ///< Reflections
    
    // Default flags
    Default = Grid | Axis | Shadows | AO | AntiAliasing
};

inline EShowFlag operator|(EShowFlag a, EShowFlag b) {
    return static_cast<EShowFlag>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline EShowFlag operator&(EShowFlag a, EShowFlag b) {
    return static_cast<EShowFlag>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

//=============================================================================
// DATA STRUCTURES
//=============================================================================

/**
 * @struct FViewportCamera
 * @brief Camera state for the viewport
 */
struct FViewportCamera {
    float           Position[3];    ///< World position
    float           Rotation[3];    ///< Euler rotation (pitch, yaw, roll)
    float           FOV;            ///< Field of view
    float           NearClip;       ///< Near clipping plane
    float           FarClip;        ///< Far clipping plane
    float           OrthoSize;      ///< Orthographic size
    float           MoveSpeed;      ///< Movement speed
    float           RotateSpeed;    ///< Rotation sensitivity
    ECameraMode     Mode;           ///< Camera mode
    
    FViewportCamera()
        : FOV(60.0f), NearClip(0.1f), FarClip(10000.0f),
          OrthoSize(10.0f), MoveSpeed(10.0f), RotateSpeed(0.3f),
          Mode(ECameraMode::Perspective) {
        Position[0] = Position[1] = Position[2] = 0.0f;
        Rotation[0] = Rotation[1] = Rotation[2] = 0.0f;
    }
};

/**
 * @struct FRenderStats
 * @brief Frame rendering statistics
 */
struct FRenderStats {
    float           FrameTimeMS;    ///< Frame time in ms
    float           FPS;            ///< Frames per second
    uint32_t        DrawCalls;      ///< Draw call count
    uint32_t        Triangles;      ///< Triangle count
    uint32_t        Vertices;       ///< Vertex count
    uint32_t        TextureMemMB;   ///< Texture memory
    uint32_t        MeshMemMB;      ///< Mesh memory
    
    FRenderStats()
        : FrameTimeMS(0), FPS(0), DrawCalls(0), Triangles(0),
          Vertices(0), TextureMemMB(0), MeshMemMB(0) {}
};

/**
 * @struct FViewportState
 * @brief Complete viewport state
 */
struct FViewportState {
    EGizmoMode      GizmoMode;      ///< Current gizmo mode
    EGizmoSpace     GizmoSpace;     ///< Gizmo coordinate space
    ERenderMode     RenderMode;     ///< Visualization mode
    EShowFlag       ShowFlags;      ///< Display flags
    float           GridSize;       ///< Grid cell size
    float           SnapTranslate;  ///< Position snap
    float           SnapRotate;     ///< Rotation snap (degrees)
    float           SnapScale;      ///< Scale snap
    bool            bSnapEnabled;   ///< Snapping active
    bool            bShowStats;     ///< Show statistics
    bool            bShowGizmo;     ///< Show transform gizmo
    bool            bIsPlaying;     ///< Play-in-editor mode
    
    FViewportState()
        : GizmoMode(EGizmoMode::Translate), GizmoSpace(EGizmoSpace::World),
          RenderMode(ERenderMode::Lit), ShowFlags(EShowFlag::Default),
          GridSize(1.0f), SnapTranslate(1.0f), SnapRotate(15.0f),
          SnapScale(0.1f), bSnapEnabled(false), bShowStats(true),
          bShowGizmo(true), bIsPlaying(false) {}
};

//=============================================================================
// VIEWPORT PANEL CLASS
//=============================================================================

/**
 * @class ViewportPanel
 * @brief Main 3D viewport panel for the editor
 * 
 * Features:
 * - Full 3D scene rendering with multiple modes
 * - ImGuizmo-based transform gizmos
 * - WASD + mouse camera navigation
 * - Snap-to-grid for transforms
 * - Multiple camera modes (perspective, ortho, axis-aligned)
 * - Render statistics overlay
 * - Orientation gizmo
 * - Debug visualization toggles
 * - Play-in-editor support
 * - Mouse picking and selection
 */
class ViewportPanel {
public:
    //-------------------------------------------------------------------------
    // Lifecycle
    //-------------------------------------------------------------------------
    
    /**
     * @brief Initializes the viewport panel
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
     * @param sceneTextureID Rendered scene texture
     * @param viewportSize Viewport dimensions
     */
    void OnUIRender(uint32_t sceneTextureID, const ImVec2& viewportSize);
    
    //-------------------------------------------------------------------------
    // Camera Control
    //-------------------------------------------------------------------------
    
    /**
     * @brief Gets the viewport camera
     * @return Reference to camera struct
     */
    FViewportCamera& GetCamera();
    
    /**
     * @brief Sets the camera mode
     * @param mode Perspective/Ortho/Axis view
     */
    void SetCameraMode(ECameraMode mode);
    
    /**
     * @brief Focuses camera on a position
     * @param x World X
     * @param y World Y
     * @param z World Z
     * @param distance Distance from target
     */
    void FocusOnPoint(float x, float y, float z, float distance = 10.0f);
    
    /**
     * @brief Focuses on selected objects
     */
    void FocusOnSelection();
    
    /**
     * @brief Resets camera to default position
     */
    void ResetCamera();
    
    //-------------------------------------------------------------------------
    // Gizmo Control
    //-------------------------------------------------------------------------
    
    /**
     * @brief Sets the gizmo mode
     * @param mode Translate/Rotate/Scale
     */
    void SetGizmoMode(EGizmoMode mode);
    
    /**
     * @brief Sets the gizmo coordinate space
     * @param space Local/World
     */
    void SetGizmoSpace(EGizmoSpace space);
    
    /**
     * @brief Toggles gizmo visibility
     * @param visible Show gizmo
     */
    void SetGizmoVisible(bool visible);
    
    /**
     * @brief Checks if gizmo is being manipulated
     * @return true if user is interacting with gizmo
     */
    bool IsGizmoInUse() const;
    
    //-------------------------------------------------------------------------
    // Render Settings
    //-------------------------------------------------------------------------
    
    /**
     * @brief Sets the render mode
     * @param mode Visualization mode
     */
    void SetRenderMode(ERenderMode mode);
    
    /**
     * @brief Gets the current render mode
     * @return Active render mode
     */
    ERenderMode GetRenderMode() const;
    
    /**
     * @brief Sets show flags
     * @param flags Display option flags
     */
    void SetShowFlags(EShowFlag flags);
    
    /**
     * @brief Gets show flags
     * @return Current show flags
     */
    EShowFlag GetShowFlags() const;
    
    /**
     * @brief Toggles a show flag
     * @param flag Flag to toggle
     */
    void ToggleShowFlag(EShowFlag flag);
    
    //-------------------------------------------------------------------------
    // Snapping
    //-------------------------------------------------------------------------
    
    /**
     * @brief Enables/disables snapping
     * @param enabled Snap state
     */
    void SetSnappingEnabled(bool enabled);
    
    /**
     * @brief Sets snap values
     * @param translate Position snap increment
     * @param rotate Rotation snap in degrees
     * @param scale Scale snap increment
     */
    void SetSnapValues(float translate, float rotate, float scale);
    
    //-------------------------------------------------------------------------
    // Statistics
    //-------------------------------------------------------------------------
    
    /**
     * @brief Gets render statistics
     * @return Reference to stats struct
     */
    const FRenderStats& GetRenderStats() const;
    
    /**
     * @brief Shows/hides stats overlay
     * @param show Display stats
     */
    void SetShowStats(bool show);
    
    //-------------------------------------------------------------------------
    // Play Mode
    //-------------------------------------------------------------------------
    
    /**
     * @brief Enters play-in-editor mode
     */
    void BeginPlay();
    
    /**
     * @brief Exits play-in-editor mode
     */
    void EndPlay();
    
    /**
     * @brief Pauses/resumes play mode
     * @param paused Pause state
     */
    void SetPaused(bool paused);
    
    /**
     * @brief Checks if in play mode
     * @return Play state
     */
    bool IsPlaying() const;
    
    //-------------------------------------------------------------------------
    // Picking
    //-------------------------------------------------------------------------
    
    /**
     * @brief Performs mouse pick in viewport
     * @param screenX Mouse X in viewport
     * @param screenY Mouse Y in viewport
     * @return Hit object ID (0 if none)
     */
    uint64_t Pick(float screenX, float screenY);
    
    /**
     * @brief Gets the world ray from screen position
     * @param screenX Mouse X
     * @param screenY Mouse Y
     * @param outOrigin Ray origin
     * @param outDirection Ray direction
     */
    void ScreenToWorldRay(float screenX, float screenY,
                          float* outOrigin, float* outDirection);
    
    //-------------------------------------------------------------------------
    // Configuration
    //-------------------------------------------------------------------------
    
    /**
     * @brief Gets the viewport state
     * @return Reference to state struct
     */
    FViewportState& GetState();
    
    //-------------------------------------------------------------------------
    // Callbacks
    //-------------------------------------------------------------------------
    
    /** Called when an object is selected via picking */
    using SelectionCallback = std::function<void(uint64_t objectID)>;
    
    /** Called when transform gizmo modifies an object */
    using TransformCallback = std::function<void(uint64_t objectID, const float* matrix)>;
    
    void SetOnObjectSelected(SelectionCallback callback);
    void SetOnObjectTransformed(TransformCallback callback);
    
private:
    //-------------------------------------------------------------------------
    // Internal State
    //-------------------------------------------------------------------------
    
    GizmoSystem                 m_Gizmo;
    FViewportCamera             m_Camera;
    FViewportState              m_State;
    FRenderStats                m_Stats;
    
    float                       m_ViewportWidth;
    float                       m_ViewportHeight;
    float                       m_ViewMatrix[16];
    float                       m_ProjectionMatrix[16];
    
    bool                        m_bIsFocused;
    bool                        m_bIsHovered;
    bool                        m_bRightMouseDown;
    bool                        m_bMiddleMouseDown;
    float                       m_LastMouseX;
    float                       m_LastMouseY;
    
    uint64_t                    m_SelectedObjectID;
    
    SelectionCallback           m_OnObjectSelected;
    TransformCallback           m_OnObjectTransformed;
    
    //-------------------------------------------------------------------------
    // Internal Methods
    //-------------------------------------------------------------------------
    
    void DrawToolbar();
    void DrawGizmoModeButtons();
    void DrawRenderModeDropdown();
    void DrawShowFlagsMenu();
    void DrawCameraModeMenu();
    void DrawSnapSettings();
    void DrawStatsOverlay();
    void DrawOrientationGizmo();
    void DrawPlayControls();
    
    void HandleCameraInput(float deltaTime);
    void HandleMousePicking();
    void HandleKeyboardShortcuts();
    void UpdateGizmo();
    void UpdateMatrices();
    void UpdateStats();
};

//=============================================================================
// UTILITY FUNCTIONS
//=============================================================================

/** Gets the display name for a gizmo mode */
const char* GetGizmoModeName(EGizmoMode mode);

/** Gets the display name for a render mode */
const char* GetRenderModeName(ERenderMode mode);

/** Gets the display name for a camera mode */
const char* GetCameraModeName(ECameraMode mode);

/** Gets the icon for a gizmo mode */
const char* GetGizmoModeIcon(EGizmoMode mode);

} // namespace RiftCore::UI
