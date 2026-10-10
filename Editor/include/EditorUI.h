#pragma once

#include <RiftCore/Common/Types.h>
#include <RiftCore/Scene/ISceneSystem.h>

#include <Renderer/Camera.h>
#include <GizmoSystem.h>

#include <string>
#include <vector>
#include <functional>

struct GLFWwindow;

namespace RiftCore {

    class ILogger;
    class RenderSystem;
    class PhysicsSystemImpl;

    struct EditorSelection {
        SceneNodeID nodeID  = INVALID_NODE;
        bool        hasNode = false;
        void Clear() { nodeID = INVALID_NODE; hasNode = false; }
    };

    struct EditorState {
        bool isPlaying     = false;
        bool isPaused      = false;
        bool showGrid      = true;
        bool showGizmos    = true;
        bool showStats     = true;
        bool showHierarchy = true;
        bool showInspector = true;
        bool showConsole   = true;
        f32  cameraSpeed   = 5.0f;
    };

    class EditorUI {
    public:
        EditorUI();
        ~EditorUI();

        bool Initialize(
            GLFWwindow*        window,
            ISceneSystem*      scene,
            RenderSystem*      renderer,
            PhysicsSystemImpl* physics,
            ILogger*           logger
        );

        void Shutdown();

        void BeginFrame();
        void Render(
            const Mat4& viewMatrix,
            const Mat4& projMatrix
        );
        std::function<void(const std::string&)> OnExecuteCommand;
        void EndFrame();

        EditorSelection  GetSelection() const {
            return selection_;
        }
        EditorState& GetState() { return state_; }

        bool ShouldCaptureMouse()    const;
        bool ShouldCaptureKeyboard() const;

    private:
        void DrawMenuBar();
        void DrawToolbar();
        void DrawHierarchyPanel();
        void DrawInspectorPanel();
        void DrawViewportOverlay(
            const Mat4& view, const Mat4& proj);
        void DrawConsolePanel();
        void DrawStatsPanel();

        void DrawTransformSection (ISceneNode* node);
        void DrawPhysicsSection   (ISceneNode* node);
        void DrawMeshSection      (ISceneNode* node);
        void DrawAudioSection     (ISceneNode* node);

        void SelectNode          (SceneNodeID id);
        void DeleteSelectedNode  ();
        void DuplicateSelectedNode();
        void CreateNode(const std::string& type);

        void AddLog(const std::string& msg,
                    bool isError = false);

    private:
        void DrawDeveloperConsole();

        ISceneSystem*      scene_    = nullptr;
        RenderSystem*      renderer_ = nullptr;
        PhysicsSystemImpl* physics_  = nullptr;
        ILogger*           logger_   = nullptr;

        EditorSelection    selection_;
        EditorState        state_;
        GizmoSystem        gizmo_;

        struct LogEntry {
            std::string message;
            bool        isError = false;
        };
        std::vector<LogEntry> logs_;
        bool                  scrollToBottom_ = false;

        std::string scenePath_ =
            "..\\..\\..\\Assets\\Scenes\\TestLevel.json";

        bool initialized_ = false;
    };

} // namespace RiftCore
