#pragma once
/**
 * @file HUD.h
 * @brief Production-grade Editor UI System for RiftCore Engine
 * * This is the PRIMARY UI system for the RiftCore Editor. It manages:
 * - ImGui context initialization and lifecycle
 * - Main docking layout with professional workspace
 * - Menu bar with File/Edit/View/Window/Help menus
 * - Toolbar with icon-based buttons
 * - Command buffer for undo/redo and action dispatch
 * - HUDCallbacks for decoupled engine interaction
 */



#pragma warning(push)
#pragma warning(disable: 4251 4275)

#include <RiftCore/Common/Platform.h>
#include <RiftCore/Common/Types.h>

// Required for ImGui types (Fixes C2061 errors)
#include <imgui.h>

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>

struct GLFWwindow;
struct ImFont;

namespace RiftCore {

//=============================================================================
// EDITOR COMMAND SYSTEM
//=============================================================================

/**
 * @enum EditorCommandType
 * @brief Types of commands that can be dispatched through the UI
 */
enum class EditorCommandType : uint8_t {
    None = 0,
    
    // File Operations
    NewScene,
    OpenScene,
    SaveScene,
    SaveSceneAs,
    
    // Edit Operations
    Undo,
    Redo,
    Cut,
    Copy,
    Paste,
    Delete,
    DeleteEntity,
    Duplicate,
    SelectAll,
    
    // Play Controls
    Play,
    Pause,
    Stop,
    Step,
    
    // View Controls
    ToggleGrid,
    ToggleStats,
    ToggleWireframe,
    FocusSelected,
    
    // Settings
    Settings,
    Preferences
};

/**
 * @struct EditorCommand
 * @brief A command with type and optional data payload
 */
struct EditorCommand {
    EditorCommandType Type = EditorCommandType::None;
    std::string       Data;             ///< Optional string data
    uint64_t          EntityID = 0;     ///< Optional entity reference
    float             Value = 0.0f;     ///< Optional numeric value
    
    EditorCommand() = default;
    EditorCommand(EditorCommandType type) : Type(type) {}
    EditorCommand(EditorCommandType type, const std::string& data) : Type(type), Data(data) {}
    EditorCommand(EditorCommandType type, uint64_t id) : Type(type), EntityID(id) {}
};

/**
 * @class CommandBuffer
 * @brief Thread-safe buffer for queuing UI commands
 */
class CommandBuffer {
public:
    void Push(const EditorCommand& cmd) { m_Commands.push_back(cmd); }
    void Push(EditorCommandType type) { m_Commands.push_back(EditorCommand(type)); }
    void Push(EditorCommandType type, const std::string& data) { m_Commands.push_back(EditorCommand(type, data)); }
    
    std::vector<EditorCommand> Flush() {
        std::vector<EditorCommand> result = std::move(m_Commands);
        m_Commands.clear();
        return result;
    }
    
    bool HasCommands() const { return !m_Commands.empty(); }
    size_t Count() const { return m_Commands.size(); }
    void Clear() { m_Commands.clear(); }
    
private:
    std::vector<EditorCommand> m_Commands;
};

//=============================================================================
// HUD DATA STRUCTURES
//=============================================================================

struct HUDVec3Display {
    std::string label;
    f32         x = 0, y = 0, z = 0;
};

struct HUDObjectInfo {
    i32         index      = 0;
    std::string name;
    f32         posX = 0, posY = 0, posZ = 0;
    f32         rotX = 0, rotY = 0, rotZ = 0;
    f32         scaleX = 1, scaleY = 1, scaleZ = 1;
    bool        isSelected  = false;
    bool        autoRotates = false;
};

struct HUDRenderStats {
    u32 drawCalls   = 0;
    u32 triangles   = 0;
    u32 vertices    = 0;
    f32 fps         = 0.0f;
    u64 frameIndex  = 0;
};

struct HUDCameraInfo {
    f32 posX = 0, posY = 0, posZ = 0;
    f32 yaw  = 0, pitch = 0;
    f32 fov  = 60.0f;
};

struct HUDCallbacks {
    std::function<void()> OnNewScene;
    std::function<void()> OnOpenScene;
    std::function<void()> OnSaveScene;
    std::function<void()> OnSaveSceneAs;
    std::function<void()> OnPlay;
    std::function<void()> OnPause;
    std::function<void()> OnStop;
    std::function<void()> OnUndo;
    std::function<void()> OnRedo;
    std::function<void(const std::string&)> OnExecuteCommand;
    std::function<void(EditorCommandType)>  OnCommand;
};

struct HUDConfig {
    bool        EnableDocking       = true;
    bool        EnableViewports     = true;
    bool        EnableKeyboardNav   = false;
    float       FontSize            = 16.0f;
    std::string FontPath            = "";       
    std::string IconsPath           = "Assets/Icons/";
    float       WindowOpacity       = 0.95f;
    bool        DarkTheme           = true;
};

//=============================================================================
// HUD CLASS - MAIN EDITOR UI SYSTEM
//=============================================================================

class HUD {
public:
    HUD();
    ~HUD();

    RIFTCORE_NOCOPY_NOMOVE(HUD);

    bool Initialize(GLFWwindow* window, const HUDConfig& config = HUDConfig());
    void Shutdown();
    bool IsInitialized() const { return m_Initialized; }

    void BeginFrame();
    void OnUIRender();
    void EndFrame();

    // Legacy render method for compatibility if needed
    void Render(const HUDRenderStats& stats, i32 selectedIdx, const std::vector<HUDObjectInfo>& objects, const HUDCameraInfo& camera);

    CommandBuffer& GetCommandBuffer() { return m_CommandBuffer; }
    void SetCallbacks(const HUDCallbacks& callbacks) { m_Callbacks = callbacks; }
    const HUDCallbacks& GetCallbacks() const { return m_Callbacks; }

    void SetVisible(bool visible)       { m_Visible = visible; }
    bool IsVisible() const              { return m_Visible; }
    void ToggleVisible()                { m_Visible = !m_Visible; }
    void SetWindowOpacity(f32 alpha)    { m_Opacity = alpha; }
    f32  GetWindowOpacity() const       { return m_Opacity; }

    // Panel visibility
    void SetViewportVisible(bool v)     { m_ShowViewport = v; }
    void SetHierarchyVisible(bool v)    { m_ShowHierarchy = v; }
    void SetInspectorVisible(bool v)    { m_ShowInspector = v; }
    void SetAssetBrowserVisible(bool v) { m_ShowAssetBrowser = v; }
    void SetConsoleVisible(bool v)      { m_ShowConsole = v; }
    void SetScriptingVisible(bool v)    { m_ShowScripting = v; }
    void SetStatsVisible(bool v)        { m_ShowStats = v; }
    
    bool IsViewportVisible() const      { return m_ShowViewport; }
    bool IsHierarchyVisible() const     { return m_ShowHierarchy; }
    bool IsInspectorVisible() const     { return m_ShowInspector; }
    bool IsAssetBrowserVisible() const  { return m_ShowAssetBrowser; }
    bool IsConsoleVisible() const       { return m_ShowConsole; }
    bool IsScriptingVisible() const     { return m_ShowScripting; }
    bool IsStatsVisible() const         { return m_ShowStats; }

    unsigned int LoadIcon(const std::string& path);
    unsigned int GetIcon(const std::string& name) const;

    f32 GetCurrentFPS() const { return m_CurrentFPS; }
    f32 GetDeltaTime() const { return m_DeltaTime; }
    GLFWwindow* GetWindow() const { return m_Window; }

private:
    void SetupDockspace();
    void RenderMainDockspace();
    void RenderMenuBar();
    void RenderToolbar();
    
    // Legacy panels
    void DrawTopMenuBar();
    void DrawToolBar();
    void DrawConsolePanel();
    void DrawMainPanel(const HUDRenderStats& stats, i32 selectedIdx, const std::vector<HUDObjectInfo>& objects, const HUDCameraInfo& camera);
    void DrawSelectedObjectPanel(i32 selectedIdx, const std::vector<HUDObjectInfo>& objects);
    void DrawObjectListPanel(i32 selectedIdx, const std::vector<HUDObjectInfo>& objects);
    void DrawControlsPanel();
    void DrawStatsPanel(const HUDRenderStats& stats);
    void DrawCameraPanel(const HUDCameraInfo& cam);

    void ApplyDarkTheme();
    void ApplyLightTheme();
    void LoadDefaultIcons();
    void UpdateFPS();
    bool IconButton(const char* label, const char* iconKey, float size = 24.0f);

    GLFWwindow* m_Window          = nullptr;
    bool            m_Initialized     = false;
    bool            m_Visible         = true;
    bool            m_IsPlaying       = false;
    bool            m_IsPaused        = false;
    bool            m_FirstFrame      = true;
    f32             m_Opacity         = 0.95f;
    
    HUDConfig       m_Config;
    
    bool            m_ShowViewport      = true;
    bool            m_ShowHierarchy     = true;
    bool            m_ShowInspector     = true;
    bool            m_ShowAssetBrowser  = true;
    bool            m_ShowConsole       = true;
    bool            m_ShowScripting     = false;
    bool            m_ShowStats         = false;
    bool            m_EditorOpen        = true;
    
    CommandBuffer   m_CommandBuffer;
    HUDCallbacks    m_Callbacks;
    
    std::map<std::string, unsigned int> m_IconCache;
    std::string     m_IconsPath;
    
    ImFont* m_DefaultFont     = nullptr;
    
    f32             m_FPSAccum        = 0.0f;
    u32             m_FPSFrames       = 0;
    f32             m_CurrentFPS      = 0.0f;
    f32             m_DeltaTime       = 0.016f;
    
    unsigned int    m_DockspaceID     = 0;
};

} // namespace RiftCore

// Alias the command system to UI namespace for the panels
namespace RiftCore::UI {
    using ::RiftCore::CommandBuffer;
    using ::RiftCore::EditorCommand;
    using ::RiftCore::EditorCommandType;
}

#pragma warning(pop)
