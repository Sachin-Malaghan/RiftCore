#pragma once

#pragma warning(push)
#pragma warning(disable: 4251 4275)

#include <RiftCore/Common/Platform.h>
#include <RiftCore/Common/Types.h>

#include <string>
#include <vector>
#include <functional>

struct GLFWwindow;

#ifdef RENDERER_EXPORTS
    #define RENDERER_API RIFTCORE_EXPORT
#else
    #define RENDERER_API RIFTCORE_IMPORT
#endif

namespace RiftCore {

    // ── HUD Item types ────────────────────────────────────────
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

    /**
     * NEW: HUDCallbacks struct
     * This allows the Editor project to tell the Renderer DLL what should happen 
     * when Menu items or Toolbar buttons are clicked, without the DLL needing 
     * to know about the SceneSystem or Physics classes.
     */
    struct HUDCallbacks {
        std::function<void()> OnNewScene;
        std::function<void()> OnOpenScene;
        std::function<void()> OnSaveScene;
        std::function<void()> OnPlay;
        std::function<void()> OnStop;
        std::function<void(const std::string&)> OnExecuteCommand;
    };

    // ── HUD class ─────────────────────────────────────────────
    class RENDERER_API HUD {
    public:
        HUD();
        ~HUD();

        RIFTCORE_NOCOPY_NOMOVE(HUD);

        // Initialize ImGui with GLFW + OpenGL3
        bool Initialize(GLFWwindow* window);
        void Shutdown();
        void DrawConsolePanel();

        // Call at start of each frame (before rendering)
        void BeginFrame();

        // Render all HUD windows
        void Render(
            const HUDRenderStats&          stats,
            i32                            selectedIdx,
            const std::vector<HUDObjectInfo>& objects,
            const HUDCameraInfo&           camera
        );

        // Call at end of each frame (after rendering)
        void EndFrame();

        // Toggle visibility
        void SetVisible(bool visible) { visible_ = visible; }
        bool IsVisible() const        { return visible_;    }
        void ToggleVisible()          { visible_ = !visible_; }

        // Settings
        void SetWindowOpacity(f32 alpha) { opacity_ = alpha; }

        /** * NEW: Method to hook up engine-level logic to the UI 
         */
        void SetCallbacks(const HUDCallbacks& callbacks) { callbacks_ = callbacks; }

    private:
        /**
         * NEW: Methods for the top-level Editor interface
         */
        void DrawTopMenuBar();
        void DrawToolBar();

        void DrawMainPanel(
            const HUDRenderStats& stats,
            i32 selectedIdx,
            const std::vector<HUDObjectInfo>& objects,
            const HUDCameraInfo& camera
        );

        void DrawSelectedObjectPanel(
            i32 selectedIdx,
            const std::vector<HUDObjectInfo>& objects
        );

        void DrawObjectListPanel(
            i32 selectedIdx,
            const std::vector<HUDObjectInfo>& objects
        );

        void DrawControlsPanel();
        void DrawStatsPanel(const HUDRenderStats& stats);
        void DrawCameraPanel(const HUDCameraInfo& cam);

        bool initialized_ = false;
        bool visible_     = true;
        f32  opacity_     = 0.85f;
        bool isPlaying_   = false; // Internal UI state for play/stop button toggle

        // Callbacks storage
        HUDCallbacks callbacks_;

        // FPS tracking
        f32  fpsAccum_    = 0.0f;
        u32  fpsFrames_   = 0;
        f32  currentFPS_  = 0.0f;
        f32  deltaTime_   = 0.016f;
    };

} // namespace RiftCore

#pragma warning(pop)
