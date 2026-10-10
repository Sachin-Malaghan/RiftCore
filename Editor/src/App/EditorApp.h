#pragma once
// RiftCore Editor application: owns the engine modules, the viewport
// render target and every panel. Layout is a fixed, resizable workspace:
//
//   menu bar / toolbar
//   [Outliner | Place]   [Viewport                ]   [Details | World | Stats]
//                        [Content | Console | Python]
//   status bar

#include <Core/Engine.h>
#include <Renderer/Camera.h>
#include <Renderer/RenderTypes.h>
#include <RiftCore/Scene/ISceneSystem.h>

#include <memory>
#include <string>
#include <vector>

struct GLFWwindow;

namespace RiftCore {

    class GLDevice;
    class IRHI;
    class RenderSystem;
    class SceneDrawer;
    class PhysicsModule;
    class SceneModule;
    class IScripting;
    class IPhysics;

    class EditorApp {
    public:
        EditorApp();
        ~EditorApp();

        bool Init(int argc = 0, char** argv = nullptr);
        void Run();
        void Shutdown();

    private:
        enum class PlayState { Edit, Playing, Paused };
        enum class LogLevel  { Info, Warning, Error, Script };
        enum class Tool      { Select, Move, Rotate, Scale };

        struct LogLine { LogLevel level; std::string text; };

        // ── Frame ───────────────────────────────────────────
        void Simulate(float dt);
        void RenderSceneToTarget();
        void DrawUI();
        void HandleShortcuts();

        // ── Chrome ──────────────────────────────────────────
        void DrawMenuBar();
        void DrawToolbar();
        void DrawStatusBar();
        void DrawAboutPopup();

        // ── Panels ──────────────────────────────────────────
        void DrawViewport();
        void DrawOutliner();
        void DrawOutlinerNode(ISceneNode* node, const std::string& filter);
        void DrawPlace();
        void DrawDetails();
        void DrawWorld();
        void DrawStats();
        void DrawContent();
        void DrawConsole();
        void DrawPython();
        void DrawHouseDesigner();
        void DrawDrawings();
        void GenerateHouse();
        bool LoadHouseDesign();
        void SetHouseView(int preset);   // 0 exterior, 1 dollhouse, 2 inside

        // ── Viewport helpers ────────────────────────────────
        void ResizeTarget(int w, int h);
        void UpdateCamera(bool hovered, float dt);
        void PickAt(float ndcX, float ndcY);
        void DrawGizmo(float x, float y, float w, float h);
        void DrawSelectionBox(float x, float y, float w, float h);
        void FocusSelection();
        Vec3 OrbitPivot();
        void SetStandardView(int view);   // 0 top, 1 front, 2 right, 3 left, 4 back, 5 isometric

        // ── Scene commands ──────────────────────────────────
        SceneNodeID Spawn(const std::string& kind, const char* physics = nullptr);
        void DeleteSelected();
        void DuplicateSelected();
        void Select(SceneNodeID id);
        void NewScene();
        void OpenScene(const std::string& path);
        void OpenSceneDialog();
        void SaveScene(bool saveAs);
        void RunScript(const std::string& path);
        void RunCode(const std::string& code);

        // ── Undo / play ─────────────────────────────────────
        void PushUndo();
        void Undo();
        void Redo();
        void RestoreSnapshot(const std::string& json);
        void Play();
        void Pause();
        void Stop();
        void StepOnce();

        void Log(LogLevel level, const std::string& text);
        void PumpScriptOutput();
        void UpdateTitle();

        // ── Engine ──────────────────────────────────────────
        Engine         engine_;
        IRHI*          rhi_       = nullptr;
        GLDevice*      device_    = nullptr;
        GLFWwindow*    window_    = nullptr;
        RenderSystem*  renderer_  = nullptr;
        PhysicsModule* physMod_   = nullptr;
        SceneModule*   sceneMod_  = nullptr;
        ISceneSystem*  scene_     = nullptr;
        IScripting*    scripting_ = nullptr;
        std::unique_ptr<SceneDrawer> drawer_;
        bool           imguiReady_ = false;

        // ── Viewport ────────────────────────────────────────
        unsigned int fbo_ = 0, colorTex_ = 0, depthRbo_ = 0;
        int    targetW_ = 0, targetH_ = 0;
        int    wantW_ = 1280, wantH_ = 720;
        Camera camera_;
        float  camYaw_ = -90.0f, camPitch_ = -20.0f;
        float  camSpeed_ = 10.0f, camFov_ = 60.0f;
        bool   wireframe_ = false;
        int    visualStyle_ = 0;        // 0 Solid, 1 Realistic, 2 Shaded + edges, 3 Hidden line
        bool   ortho_ = false;          // parallel projection
        float  orthoHeight_ = 20.0f;    // metres shown top to bottom in a parallel view
        Vec3   pivot_ = { 0.0f, 1.0f, 0.0f };   // orbit centre
        bool   orbiting_ = false, panning_ = false;
        bool   shadows_ = true;
        float  exposure_ = 1.0f;
        bool   showBounds_ = true;
        bool   flying_ = false;

        // ── World ───────────────────────────────────────────
        Light  sun_;
        float  sunAzimuth_ = 45.0f, sunElevation_ = 55.0f;
        Vec3   skyColor_ = { 0.11f, 0.13f, 0.17f };

        // ── Editing state ───────────────────────────────────
        SceneNodeID selected_ = INVALID_NODE;
        Tool        tool_ = Tool::Move;
        bool        gizmoWorld_ = true;
        bool        snap_ = false;
        float       snapMove_ = 0.5f, snapRotate_ = 15.0f, snapScale_ = 0.1f;
        bool        gizmoWasUsing_ = false;
        bool        detailsEditing_ = false;
        bool        dirty_ = false;
        std::string scenePath_;
        SceneNodeID renaming_ = INVALID_NODE;
        std::string renameBuffer_;
        std::string outlinerFilter_;
        SceneNodeID pendingDelete_ = INVALID_NODE, pendingDuplicate_ = INVALID_NODE;
        int         placePhysics_ = 0;          // 0 none, 1 static, 2 dynamic
        std::vector<std::string> objFiles_;
        bool        objScanned_ = false;
        std::string scriptPartial_;

        PlayState   playState_ = PlayState::Edit;
        std::string playSnapshot_;
        std::vector<std::string> undo_, redo_;

        // ── Layout ──────────────────────────────────────────
        float leftW_ = 290.0f, rightW_ = 350.0f, bottomH_ = 270.0f;

        // ── Console / Python / content ──────────────────────
        std::vector<LogLine> log_;
        bool        logScroll_ = false;
        bool        showInfo_ = true, showWarn_ = true, showError_ = true, showScript_ = true;
        std::string consoleInput_;
        std::string pythonSource_;
        std::string pythonFile_;
        std::string contentDir_ = "Assets";
        bool        showAbout_ = false;
        bool        focusPython_ = false;
        std::string showTabs_;                  // tabs to bring forward once (--show)
        bool        WantTab(const char* name);

        // ── House AI (see HouseDesigner.cpp) ─────────────────
        struct HousePrim {
            int   kind = 0;                 // 0 line, 1 rect, 2 poly, 3 arc, 4 text
            float v[5] = {};
            std::vector<float> pts;
            std::string text, layer;
            bool  fill = false;
            int   anchor = 1;
        };
        struct HouseSheet {
            std::string title;
            float bounds[4] = {};
            std::vector<HousePrim> prims;
        };
        std::string housePrompt_ = "2BHK house with 2 floors, vastu compliant, north facing";
        std::vector<HouseSheet> houseSheets_;
        std::vector<std::pair<std::string, std::string>> houseInfo_, houseVastu_;
        std::string houseDir_;
        int   houseSheet_ = 0;
        float houseZoom_ = 1.0f, housePanX_ = 0.0f, housePanY_ = 0.0f;
        float houseW_ = 0.0f, houseD_ = 0.0f, houseTop_ = 0.0f;
        float houseLivingX_ = 0.0f, houseLivingZ_ = 0.0f;   // living room centre, world metres
        bool  houseRoof_ = true, houseUpper_ = true;
        int   houseUnits_ = 0;              // 0 = as the prompt says, then mm, cm, m, in, ft-in
        bool  showDrawings_ = false;        // bring the 2D Drawings tab forward once

        float fpsHistory_[120] = {};
        int   fpsOffset_ = 0;
        float fps_ = 0.0f;
    };

} // namespace RiftCore
