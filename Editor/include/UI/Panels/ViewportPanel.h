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
    Universal,      ///< Combined transform
    Bounds,         ///< Bounds editing mode
    COUNT           ///< Number of modes
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
    Overdraw,           ///< Overdraw visualization
    QuadOverdraw,       ///< Quad overdraw visualization
    LightmapDensity,    ///< Lightmap density visualization
    COUNT               ///< Number of render modes
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
    Lights          = 1 << 20,  ///< Light icons/gizmos
    Cameras         = 1 << 21,  ///< Camera icons
    Icons           = 1 << 22,  ///< Component icons
    Stats           = 1 << 23,  ///< Statistics overlay
    
    // Default flags
    Default = Grid | Axis | Shadows | AO | AntiAliasing
};

inline EShowFlag operator|(EShowFlag a, EShowFlag b) {
    return static_cast<EShowFlag>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline EShowFlag operator&(EShowFlag a, EShowFlag b) {
    return static_cast<EShowFlag>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline EShowFlag operator|(EShowFlag a, uint32_t b) {
    return static_cast<EShowFlag>(static_cast<uint32_t>(a) | b);
}

inline EShowFlag operator&(EShowFlag a, uint32_t b) {
    return static_cast<EShowFlag>(static_cast<uint32_t>(a) & b);
}

inline EShowFlag& operator|=(EShowFlag& a, EShowFlag b) {
    a = a | b;
    return a;
}

inline EShowFlag& operator&=(EShowFlag& a, EShowFlag b) {
    a = a & b;
    return a;
}

inline EShowFlag& operator|=(EShowFlag& a, uint32_t b) {
    a = static_cast<EShowFlag>(static_cast<uint32_t>(a) | b);
    return a;
}

inline EShowFlag& operator&=(EShowFlag& a, uint32_t b) {
    a = static_cast<EShowFlag>(static_cast<uint32_t>(a) & b);
    return a;
}

inline EShowFlag operator~(EShowFlag a) {
    return static_cast<EShowFlag>(~static_cast<uint32_t>(a));
}

inline bool operator!=(EShowFlag a, EShowFlag b) {
    return static_cast<uint32_t>(a) != static_cast<uint32_t>(b);
}

inline bool operator==(EShowFlag a, EShowFlag b) {
    return static_cast<uint32_t>(a) == static_cast<uint32_t>(b);
}

inline bool operator!=(EShowFlag a, uint32_t b) {
    return static_cast<uint32_t>(a) != b;
}

inline bool operator==(EShowFlag a, uint32_t b) {
    return static_cast<uint32_t>(a) == b;
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
    float           FrameTimeMs;    ///< Frame time in ms (alias)
    float           GPUTimeMs;      ///< GPU time in ms
    float           FPS;            ///< Frames per second
    uint32_t        DrawCalls;      ///< Draw call count
    uint32_t        Triangles;      ///< Triangle count
    uint32_t        Vertices;       ///< Vertex count
    uint32_t        TextureMemMB;   ///< Texture memory
    uint32_t        MeshMemMB;      ///< Mesh memory
    uint64_t        VRAMUsedMB;     ///< VRAM used
    uint64_t        VRAMTotalMB;    ///< Total VRAM
    
    FRenderStats()
        : FrameTimeMS(0), FrameTimeMs(0), GPUTimeMs(0), FPS(0), DrawCalls(0), Triangles(0),
          Vertices(0), TextureMemMB(0), MeshMemMB(0), VRAMUsedMB(0), VRAMTotalMB(0) {}
};

/**
 * @struct FViewportState
 * @brief Complete viewport state
 */
struct FViewportState {
    EGizmoMode      GizmoMode;          ///< Current gizmo mode
    EGizmoSpace     GizmoSpace;         ///< Gizmo coordinate space
    ERenderMode     RenderMode;         ///< Visualization mode
    EShowFlag       ShowFlags;          ///< Display flags
    float           GridSize;           ///< Grid cell size
    float           SnapTranslate;      ///< Position snap
    float           SnapRotate;         ///< Rotation snap (degrees)
    float           SnapScale;          ///< Scale snap
    bool            bSnapEnabled;       ///< Snapping active
    bool            bGizmoSnap;         ///< Gizmo snap enabled
    bool            bShowStats;         ///< Show statistics
    bool            bShowGizmo;         ///< Show transform gizmo
    bool            bShowToolbar;       ///< Show top toolbar
    bool            bShowTelemetry;     ///< Show performance overlay
    bool            bShowGizmoControls; ///< Show gizmo controls
    bool            bIsPlaying;         ///< Play-in-editor mode
    bool            bRealtime;          ///< Realtime rendering
    bool            bMaximized;         ///< Viewport maximized
    bool            bIsFocused;         ///< Viewport has focus
    bool            bIsHovered;         ///< Mouse is hovering
    bool            bDraggingGizmo;     ///< Gizmo being dragged
    bool            bCameraControlActive; ///< Camera being controlled
    ImVec2          ViewportSize;       ///< Current viewport size
    ImVec2          ViewportPos;        ///< Viewport screen position
    float           LastMouseX;         ///< Previous mouse X
    float           LastMouseY;         ///< Previous mouse Y
    FViewportCamera Camera;             ///< Camera state
    FRenderStats    Stats;              ///< Render statistics
    
    FViewportState()
        : GizmoMode(EGizmoMode::Translate), GizmoSpace(EGizmoSpace::World),
          RenderMode(ERenderMode::Lit), ShowFlags(EShowFlag::Default),
          GridSize(1.0f), SnapTranslate(1.0f), SnapRotate(15.0f),
          SnapScale(0.1f), bSnapEnabled(false), bGizmoSnap(false),
          bShowStats(true), bShowGizmo(true), bShowToolbar(true),
          bShowTelemetry(false), bShowGizmoControls(true),
          bIsPlaying(false), bRealtime(true),
          bMaximized(false), bIsFocused(false), bIsHovered(false),
          bDraggingGizmo(false), bCameraControlActive(false),
          ViewportSize(1280, 720), ViewportPos(0, 0),
          LastMouseX(0.0f), LastMouseY(0.0f) {}
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
    
    void Initialize() {}
    void Shutdown() {}
    
    //-------------------------------------------------------------------------
    // Rendering
    //-------------------------------------------------------------------------
    
    void OnUIRender(uint32_t sceneTextureID, const ImVec2& viewportSize);
    
    //-------------------------------------------------------------------------
    // Camera / Gizmo / Render / Snap / Stats / Play / Pick (all inline stubs)
    //-------------------------------------------------------------------------
    
    FViewportCamera& GetCamera() { return m_Camera; }
    void SetCameraMode(ECameraMode mode) { m_Camera.Mode = mode; }
    void FocusOnPoint(float x, float y, float z, float /*distance*/ = 10.0f) {
        m_Camera.Position[0] = x; m_Camera.Position[1] = y; m_Camera.Position[2] = z;
    }
    void FocusOnSelection() {}
    void ResetCamera() { m_Camera = FViewportCamera(); }
    void SetGizmoMode(EGizmoMode mode) { m_State.GizmoMode = mode; }
    void SetGizmoSpace(EGizmoSpace space) { m_State.GizmoSpace = space; }
    void SetGizmoVisible(bool visible) { m_State.bShowGizmo = visible; }
    bool IsGizmoInUse() const { return m_State.bDraggingGizmo; }
    void SetRenderMode(ERenderMode mode) { m_State.RenderMode = mode; }
    ERenderMode GetRenderMode() const { return m_State.RenderMode; }
    void SetShowFlags(EShowFlag flags) { m_State.ShowFlags = flags; }
    EShowFlag GetShowFlags() const { return m_State.ShowFlags; }
    void ToggleShowFlag(EShowFlag flag) {
        uint32_t cur = static_cast<uint32_t>(m_State.ShowFlags);
        uint32_t f   = static_cast<uint32_t>(flag);
        m_State.ShowFlags = static_cast<EShowFlag>(cur ^ f);
    }
    void SetSnappingEnabled(bool enabled) { m_State.bSnapEnabled = enabled; }
    void SetSnapValues(float t, float r, float s) { m_State.SnapTranslate = t; m_State.SnapRotate = r; m_State.SnapScale = s; }
    const FRenderStats& GetRenderStats() const { return m_Stats; }
    void SetShowStats(bool show) { m_State.bShowStats = show; }
    void BeginPlay() { m_State.bIsPlaying = true; }
    void EndPlay() { m_State.bIsPlaying = false; }
    void SetPaused(bool /*paused*/) {}
    bool IsPlaying() const { return m_State.bIsPlaying; }
    uint64_t Pick(float /*screenX*/, float /*screenY*/) { return 0; }
    void ScreenToWorldRay(float /*sx*/, float /*sy*/, float* /*org*/, float* /*dir*/) {}
    FViewportState& GetState() { return m_State; }
    
    //-------------------------------------------------------------------------
    // Callbacks
    //-------------------------------------------------------------------------
    
    using SelectionCallback = std::function<void(uint64_t objectID)>;
    using TransformCallback = std::function<void(uint64_t objectID, const float* matrix)>;
    
    void SetOnObjectSelected(SelectionCallback callback) { m_OnObjectSelected = callback; }
    void SetOnObjectTransformed(TransformCallback callback) { m_OnObjectTransformed = callback; }


private:
    //-------------------------------------------------------------------------
    // Internal State
    //-------------------------------------------------------------------------
    
    GizmoSystem                 m_Gizmo;
    FViewportCamera             m_Camera;
    FViewportState              m_State;
    FRenderStats                m_Stats;
    
    float                       m_ViewportWidth = 1280.0f;
    float                       m_ViewportHeight = 720.0f;
    float                       m_ViewMatrix[16] = {};
    float                       m_ProjectionMatrix[16] = {};
    
    bool                        m_bIsFocused = false;
    bool                        m_bIsHovered = false;
    bool                        m_bRightMouseDown = false;
    bool                        m_bMiddleMouseDown = false;
    float                       m_LastMouseX = 0.0f;
    float                       m_LastMouseY = 0.0f;
    
    uint64_t                    m_SelectedObjectID = 0;
    
    SelectionCallback           m_OnObjectSelected;
    TransformCallback           m_OnObjectTransformed;
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
