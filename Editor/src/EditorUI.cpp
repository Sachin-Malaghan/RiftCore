#include <EditorUI.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <ImGuizmo.h>

#include <RiftCore/Core/ILogger.h>
#include <RiftCore/Scene/ISceneSystem.h>
#include <Renderer/RenderSystem.h>
#include <Renderer/Camera.h>
#include <Physics/PhysicsWorld.h>
#include <Scene/SceneNode.h>
#include <Scene/SceneSystem.h>

#include <iostream>
#include <cstring>

namespace RiftCore {

    EditorUI::EditorUI()  = default;
    EditorUI::~EditorUI() { Shutdown(); }

    bool EditorUI::Initialize(
        GLFWwindow*        window,
        ISceneSystem*      scene,
        RenderSystem*      renderer,
        PhysicsSystemImpl* physics,
        ILogger*           logger
    ) {
        scene_    = scene;
        renderer_ = renderer;
        physics_  = physics;
        logger_   = logger;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;

        ImGui::StyleColorsDark();

        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 4.0f;
        style.FrameRounding  = 3.0f;
        style.WindowPadding  = ImVec2(8, 8);
        style.FramePadding   = ImVec2(6, 3);
        style.ItemSpacing    = ImVec2(8, 5);

        auto* colors = style.Colors;
        colors[ImGuiCol_WindowBg]     =
            ImVec4(0.12f,0.12f,0.15f,1.00f);
        colors[ImGuiCol_TitleBg]      =
            ImVec4(0.08f,0.08f,0.18f,1.00f);
        colors[ImGuiCol_TitleBgActive]=
            ImVec4(0.12f,0.12f,0.30f,1.00f);
        colors[ImGuiCol_Header]       =
            ImVec4(0.15f,0.25f,0.45f,0.80f);
        colors[ImGuiCol_HeaderHovered]=
            ImVec4(0.20f,0.35f,0.60f,0.90f);
        colors[ImGuiCol_FrameBg]      =
            ImVec4(0.08f,0.08f,0.14f,1.00f);
        colors[ImGuiCol_Button]       =
            ImVec4(0.15f,0.25f,0.50f,0.80f);
        colors[ImGuiCol_ButtonHovered]=
            ImVec4(0.25f,0.40f,0.70f,0.90f);
        colors[ImGuiCol_Text]         =
            ImVec4(0.90f,0.90f,0.95f,1.00f);
        colors[ImGuiCol_Separator]    =
            ImVec4(0.25f,0.25f,0.40f,1.00f);

        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 460");

        AddLog("RiftCore Editor initialized");

        initialized_ = true;
        std::cout << "[Editor] UI initialized\n";
        return true;
    }

    void EditorUI::Shutdown() {
        if (!initialized_) return;
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        initialized_ = false;
    }

    bool EditorUI::ShouldCaptureMouse() const {
        ImGuiIO& io = ImGui::GetIO();
        return io.WantCaptureMouse || gizmo_.IsUsing();
    }

    bool EditorUI::ShouldCaptureKeyboard() const {
        return ImGui::GetIO().WantCaptureKeyboard;
    }

    void EditorUI::BeginFrame() {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();
    }

    void EditorUI::Render(
        const Mat4& viewMatrix,
        const Mat4& projMatrix
    ) {
        if (!initialized_) return;

        DrawMenuBar();
        DrawToolbar();
        DrawHierarchyPanel();
        DrawInspectorPanel();
        DrawConsolePanel();
        DrawStatsPanel();
        DrawViewportOverlay(viewMatrix, projMatrix);
    }

    void EditorUI::EndFrame() {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(
            ImGui::GetDrawData());
    }

    // ── Menu Bar ──────────────────────────────────────────
    void EditorUI::DrawMenuBar() {
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("New Scene")) {
                    scene_->NewScene("NewScene");
                    selection_.Clear();
                    AddLog("New scene created");
                }
                if (ImGui::MenuItem("Open Scene")) {
                    auto r = scene_->LoadScene(scenePath_);
                    if (r.IsOk()) {
                        selection_.Clear();
                        AddLog("Scene loaded");
                    } else {
                        AddLog("Load failed: " +
                            r.Error().message, true);
                    }
                }
                if (ImGui::MenuItem("Save Scene")) {
                    auto r = scene_->SaveScene(scenePath_);
                    if (r.IsOk()) {
                        AddLog("Scene saved");
                    } else {
                        AddLog("Save failed: " +
                            r.Error().message, true);
                    }
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Create")) {
                if (ImGui::MenuItem("Cube"))
                    CreateNode("Cube");
                if (ImGui::MenuItem("Sphere"))
                    CreateNode("Sphere");
                if (ImGui::MenuItem("Plane"))
                    CreateNode("Plane");
                if (ImGui::MenuItem("Empty"))
                    CreateNode("Empty");
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("Hierarchy",
                    nullptr, &state_.showHierarchy);
                ImGui::MenuItem("Inspector",
                    nullptr, &state_.showInspector);
                ImGui::MenuItem("Console",
                    nullptr, &state_.showConsole);
                ImGui::MenuItem("Stats",
                    nullptr, &state_.showStats);
                ImGui::EndMenu();
            }

            // Scene info center
            if (scene_) {
                auto si = scene_->GetSceneInfo();
                std::string info = "  Scene: " +
                    si.name + " (" +
                    std::to_string(si.nodeCount) +
                    " nodes)";
                ImGui::SetCursorPosX(
                    ImGui::GetWindowWidth() * 0.35f);
                ImGui::TextColored(
                    ImVec4(0.7f,0.9f,0.5f,1),
                    "%s", info.c_str());
            }

            // FPS right
            ImGui::SetCursorPosX(
                ImGui::GetWindowWidth() - 80);
            ImGui::TextColored(
                ImVec4(0.5f,1.0f,0.5f,1),
                "%.0f FPS", ImGui::GetIO().Framerate);

            ImGui::EndMainMenuBar();
        }
    }

    // ── Toolbar ───────────────────────────────────────────
    void EditorUI::DrawToolbar() {
        ImGui::SetNextWindowPos(
            ImVec2(0, 19), ImGuiCond_Always);
        ImGui::SetNextWindowSize(
            ImVec2(ImGui::GetIO().DisplaySize.x, 36),
            ImGuiCond_Always);

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove       |
            ImGuiWindowFlags_NoScrollbar  |
            ImGuiWindowFlags_NoSavedSettings;

        ImGui::PushStyleVar(
            ImGuiStyleVar_WindowPadding, ImVec2(4,4));
        ImGui::PushStyleColor(ImGuiCol_WindowBg,
            ImVec4(0.15f,0.15f,0.20f,1.0f));

        if (ImGui::Begin("##toolbar", nullptr, flags)) {
            ImGui::SetCursorPosY(6);

            ImVec4 active = ImVec4(0.3f,0.6f,1.0f,1.0f);

            // Translate (W)
            bool isTrans = gizmo_.GetOperation() ==
                GizmoOperation::Translate;
            if (isTrans) ImGui::PushStyleColor(
                ImGuiCol_Button, active);
            if (ImGui::Button("[W] Move"))
                gizmo_.SetOperation(
                    GizmoOperation::Translate);
            if (isTrans) ImGui::PopStyleColor();

            ImGui::SameLine();

            // Rotate (E)
            bool isRot = gizmo_.GetOperation() ==
                GizmoOperation::Rotate;
            if (isRot) ImGui::PushStyleColor(
                ImGuiCol_Button, active);
            if (ImGui::Button("[E] Rotate"))
                gizmo_.SetOperation(
                    GizmoOperation::Rotate);
            if (isRot) ImGui::PopStyleColor();

            ImGui::SameLine();

            // Scale (R)
            bool isScale = gizmo_.GetOperation() ==
                GizmoOperation::Scale;
            if (isScale) ImGui::PushStyleColor(
                ImGuiCol_Button, active);
            if (ImGui::Button("[R] Scale"))
                gizmo_.SetOperation(
                    GizmoOperation::Scale);
            if (isScale) ImGui::PopStyleColor();

            ImGui::SameLine(0, 20);

            // World/Local toggle
            bool isWorld =
                gizmo_.GetSpace() == GizmoSpace::World;
            if (ImGui::Button(
                isWorld ? "[World]" : "[Local]")) {
                gizmo_.SetSpace(isWorld ?
                    GizmoSpace::Local : GizmoSpace::World);
            }

            ImGui::SameLine(0, 20);

            // Play/Pause/Stop - ASCII only
            ImGui::PushStyleColor(ImGuiCol_Button,
                state_.isPlaying ?
                ImVec4(0.2f,0.7f,0.2f,1) :
                ImVec4(0.3f,0.3f,0.3f,1));
            if (ImGui::Button("  PLAY  ")) {
                state_.isPlaying = true;
                state_.isPaused  = false;
                AddLog("Play mode started");
            }
            ImGui::PopStyleColor();

            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_Button,
                state_.isPaused ?
                ImVec4(0.8f,0.6f,0.1f,1) :
                ImVec4(0.3f,0.3f,0.3f,1));
            if (ImGui::Button(" PAUSE ")) {
                state_.isPaused = !state_.isPaused;
            }
            ImGui::PopStyleColor();

            ImGui::SameLine();

            if (ImGui::Button(" STOP ")) {
                state_.isPlaying = false;
                state_.isPaused  = false;
                AddLog("Play mode stopped");
            }
        }
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    }

    // ── Hierarchy Panel ───────────────────────────────────
    void EditorUI::DrawHierarchyPanel() {
        if (!state_.showHierarchy) return;

        ImGui::SetNextWindowPos(
            ImVec2(0, 57), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(
            ImVec2(260, 420), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Hierarchy",
            &state_.showHierarchy)) {

            if (scene_) {
                auto si = scene_->GetSceneInfo();
                ImGui::TextColored(
                    ImVec4(0.8f,0.8f,0.5f,1),
                    "Scene: %s (%u)",
                    si.name.c_str(), si.nodeCount);
            }

            ImGui::Separator();

            if (ImGui::SmallButton("+Cube"))
                CreateNode("Cube");
            ImGui::SameLine();
            if (ImGui::SmallButton("+Sphere"))
                CreateNode("Sphere");
            ImGui::SameLine();
            if (ImGui::SmallButton("+Empty"))
                CreateNode("Empty");

            ImGui::Separator();

            if (scene_) {
                scene_->ForEachNode(
                    [&](ISceneNode* node) {
                    if (!node) return;

                    bool isSelected =
                        selection_.hasNode &&
                        selection_.nodeID == node->GetID();

                    auto* sNode =
                        static_cast<SceneNode*>(node);

                    const char* icon =
                        (sNode->hasMesh && sNode->hasPhysics)
                        ? "[MP]" :
                        sNode->hasMesh    ? "[M] " :
                        sNode->hasPhysics ? "[P] " :
                        sNode->hasAudio   ? "[A] " : "[ ] ";

                    std::string label = std::string(icon) +
                        " " + node->GetName() +
                        "##" + std::to_string(node->GetID());

                    if (!node->IsActive()) {
                        ImGui::PushStyleColor(
                            ImGuiCol_Text,
                            ImVec4(0.5f,0.5f,0.5f,1));
                    }

                    if (ImGui::Selectable(
                        label.c_str(), isSelected)) {
                        SelectNode(node->GetID());
                    }

                    if (!node->IsActive()) {
                        ImGui::PopStyleColor();
                    }

                    // Right-click context
                    if (ImGui::BeginPopupContextItem()) {
                        SelectNode(node->GetID());
                        if (ImGui::MenuItem("Delete"))
                            DeleteSelectedNode();
                        if (ImGui::MenuItem("Duplicate"))
                            DuplicateSelectedNode();
                        if (ImGui::MenuItem(
                            node->IsActive() ?
                            "Hide" : "Show")) {
                            node->SetActive(
                                !node->IsActive());
                        }
                        ImGui::EndPopup();
                    }
                });
            }
        }
        ImGui::End();
    }

    // ── Inspector Panel ───────────────────────────────────
    void EditorUI::DrawInspectorPanel() {
        if (!state_.showInspector) return;

        ImGui::SetNextWindowPos(
            ImVec2(1010, 57), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(
            ImVec2(270, 500), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Inspector",
            &state_.showInspector)) {

            if (!selection_.hasNode) {
                ImGui::TextColored(
                    ImVec4(0.5f,0.5f,0.6f,1),
                    "No object selected.");
                ImGui::TextColored(
                    ImVec4(0.4f,0.4f,0.5f,1),
                    "Click a node in Hierarchy.");
            } else {
                ISceneNode* node =
                    scene_->GetNode(selection_.nodeID);
                if (!node) {
                    selection_.Clear();
                    ImGui::End();
                    return;
                }

                // Name field
                char nameBuf[128];
                std::strncpy(nameBuf,
                    node->GetName().c_str(),
                    sizeof(nameBuf)-1);
                nameBuf[sizeof(nameBuf)-1] = 0;

                ImGui::TextColored(
                    ImVec4(1.0f,0.9f,0.4f,1),
                    "SELECTED OBJECT");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::InputText("##name",
                    nameBuf, sizeof(nameBuf))) {
                    node->SetName(nameBuf);
                }

                bool active = node->IsActive();
                if (ImGui::Checkbox("Active", &active)) {
                    node->SetActive(active);
                }

                ImGui::Separator();
                DrawTransformSection(node);

                auto* sNode =
                    static_cast<SceneNode*>(node);
                if (sNode->hasPhysics) {
                    ImGui::Separator();
                    DrawPhysicsSection(node);
                }
                if (sNode->hasMesh) {
                    ImGui::Separator();
                    DrawMeshSection(node);
                }
                if (sNode->hasAudio) {
                    ImGui::Separator();
                    DrawAudioSection(node);
                }

                ImGui::Separator();
                ImGui::PushStyleColor(ImGuiCol_Button,
                    ImVec4(0.7f,0.1f,0.1f,1));
                if (ImGui::Button("Delete Node",
                    ImVec2(-1,0))) {
                    DeleteSelectedNode();
                }
                ImGui::PopStyleColor();
            }
        }
        ImGui::End();
    }

    void EditorUI::DrawTransformSection(
        ISceneNode* node
    ) {
        if (!ImGui::CollapsingHeader("Transform",
            ImGuiTreeNodeFlags_DefaultOpen)) return;

        Vec3 pos = node->GetLocalPosition();
        Vec3 rot = node->GetLocalRotation();
        Vec3 scl = node->GetLocalScale();

        float p[3]={pos.x,pos.y,pos.z};
        float r[3]={rot.x,rot.y,rot.z};
        float s[3]={scl.x,scl.y,scl.z};

        ImGui::Text("Position");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::DragFloat3("##pos",
            p, 0.1f, -1000, 1000, "%.2f"))
            node->SetLocalPosition({p[0],p[1],p[2]});

        ImGui::Text("Rotation (deg)");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::DragFloat3("##rot",
            r, 1.0f, -360, 360, "%.1f"))
            node->SetLocalRotation({r[0],r[1],r[2]});

        ImGui::Text("Scale");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::DragFloat3("##scl",
            s, 0.01f, 0.001f, 100, "%.3f"))
            node->SetLocalScale({s[0],s[1],s[2]});

        if (ImGui::Button("Reset")) {
            node->SetLocalPosition(Vec3::Zero());
            node->SetLocalRotation(Vec3::Zero());
            node->SetLocalScale(Vec3::One());
        }
    }

    void EditorUI::DrawPhysicsSection(
        ISceneNode* node
    ) {
        auto* sNode = static_cast<SceneNode*>(node);
        if (!ImGui::CollapsingHeader("Physics",
            ImGuiTreeNodeFlags_DefaultOpen)) return;

        ImGui::Text("Shape:  %s",
            sNode->physicsDesc.colliderShape.c_str());
        ImGui::Text("Mass:   %.2f",
            sNode->physicsDesc.mass);
        ImGui::Text("Static: %s",
            sNode->physicsDesc.isStatic ? "Yes" : "No");

        float rest = sNode->physicsDesc.restitution;
        if (ImGui::SliderFloat("Restitution",
            &rest, 0, 1))
            sNode->physicsDesc.restitution = rest;

        float fric = sNode->physicsDesc.friction;
        if (ImGui::SliderFloat("Friction",
            &fric, 0, 1))
            sNode->physicsDesc.friction = fric;
    }

    void EditorUI::DrawMeshSection(ISceneNode* node) {
        auto* sNode = static_cast<SceneNode*>(node);
        if (!ImGui::CollapsingHeader("Mesh")) return;

        float alb[3] = {
            sNode->meshDesc.albedo.x,
            sNode->meshDesc.albedo.y,
            sNode->meshDesc.albedo.z
        };
        if (ImGui::ColorEdit3("Albedo", alb))
            sNode->meshDesc.albedo = {alb[0],alb[1],alb[2]};

        ImGui::SliderFloat("Metallic",
            &sNode->meshDesc.metallic, 0, 1);
        ImGui::SliderFloat("Roughness",
            &sNode->meshDesc.roughness, 0, 1);
    }

    void EditorUI::DrawAudioSection(ISceneNode* node) {
        auto* sNode = static_cast<SceneNode*>(node);
        if (!ImGui::CollapsingHeader("Audio")) return;

        ImGui::Text("Clip: %s",
            sNode->audioDesc.clipPath.empty() ?
            "(none)" : sNode->audioDesc.clipPath.c_str());
        ImGui::SliderFloat("Volume",
            &sNode->audioDesc.volume, 0, 1);
        ImGui::Checkbox("Looping",
            &sNode->audioDesc.looping);
    }

    // ── Viewport Gizmo Overlay ────────────────────────────
    void EditorUI::DrawViewportOverlay(
        const Mat4& view, const Mat4& proj
    ) {
        if (!state_.showGizmos) return;
        if (!selection_.hasNode) return;

        ISceneNode* node =
            scene_->GetNode(selection_.nodeID);
        if (!node) return;

        // Full screen gizmo rect
        ImGuiIO& io = ImGui::GetIO();
        gizmo_.SetViewport(0, 0,
            io.DisplaySize.x, io.DisplaySize.y);

        // Build object matrix
        Vec3 pos = node->GetLocalPosition();
        Vec3 rot = node->GetLocalRotation();
        Vec3 scl = node->GetLocalScale();
        Mat4 objMat = GizmoSystem::ComposeMatrix(
            pos, rot, scl);

        // Draw and get result
        auto result = gizmo_.Draw(objMat, view, proj);

        if (result.changed) {
            node->SetLocalPosition(result.position);
            node->SetLocalRotation(result.rotation);
            node->SetLocalScale   (result.scale);
        }

        // View cube helper (top right corner)
        float viewMat[16];
        std::memcpy(viewMat, &view.cols[0][0],
                    16 * sizeof(float));
        ImGuizmo::ViewManipulate(
            viewMat,
            8.0f,
            ImVec2(io.DisplaySize.x - 128, 57),
            ImVec2(128, 128),
            0x10101080
        );
    }

    // ── Console ───────────────────────────────────────────
    void EditorUI::DrawConsolePanel() {
        if (!state_.showConsole) return;

        ImGui::SetNextWindowPos(
            ImVec2(0, 500), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(
            ImVec2(700, 200), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Console",
            &state_.showConsole)) {
            if (ImGui::SmallButton("Clear"))
                logs_.clear();
            ImGui::SameLine();
            ImGui::Text("(%zu entries)",
                logs_.size());
            ImGui::Separator();

            ImGui::BeginChild("##log",
                ImVec2(0,-1), false);
            for (auto& log : logs_) {
                ImVec4 col = log.isError ?
                    ImVec4(1,0.3f,0.3f,1) :
                    ImVec4(0.9f,0.9f,0.9f,1);
                ImGui::PushStyleColor(
                    ImGuiCol_Text, col);
                ImGui::TextUnformatted(
                    log.message.c_str());
                ImGui::PopStyleColor();
            }
            if (scrollToBottom_) {
                ImGui::SetScrollHereY(1.0f);
                scrollToBottom_ = false;
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }

    // ── Stats ─────────────────────────────────────────────
    void EditorUI::DrawStatsPanel() {
        if (!state_.showStats) return;

        ImGui::SetNextWindowPos(
            ImVec2(700, 500), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(
            ImVec2(280, 200), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Stats", &state_.showStats)) {
            ImGuiIO& io = ImGui::GetIO();
            ImGui::Text("FPS: %.1f", io.Framerate);
            ImGui::Text("dt:  %.2f ms",
                1000.0f/io.Framerate);

            if (renderer_) {
                auto s = renderer_->GetStats();
                ImGui::Separator();
                ImGui::TextColored(
                    ImVec4(0.8f,0.8f,0.5f,1), "Render");
                ImGui::Text("Draw Calls: %u", s.drawCalls);
                ImGui::Text("Triangles:  %u", s.triangles);
            }

            if (scene_) {
                auto si = scene_->GetSceneInfo();
                ImGui::Separator();
                ImGui::TextColored(
                    ImVec4(0.8f,0.8f,0.5f,1), "Scene");
                ImGui::Text("Nodes: %u", si.nodeCount);
                ImGui::Text("Name:  %s", si.name.c_str());
            }

            ImGui::Separator();
            ImGui::TextColored(
                ImVec4(0.8f,0.8f,0.5f,1), "Gizmo");
            const char* ops[] = {
                "Translate","Rotate","Scale"};
            ImGui::Text("Op:    %s",
                ops[static_cast<int>(
                    gizmo_.GetOperation())]);
            ImGui::Text("Over:  %s  Using: %s",
                gizmo_.IsOver()  ? "Yes":"No",
                gizmo_.IsUsing() ? "Yes":"No");
        }
        ImGui::End();
    }

    // ── Node operations ───────────────────────────────────
    void EditorUI::SelectNode(SceneNodeID id) {
        selection_.nodeID  = id;
        selection_.hasNode = true;
        auto* node = scene_->GetNode(id);
        if (node) AddLog("Selected: " + node->GetName());
    }

    void EditorUI::DeleteSelectedNode() {
        if (!selection_.hasNode) return;
        auto* node = scene_->GetNode(selection_.nodeID);
        std::string name = node ? node->GetName() : "?";
        scene_->DestroyNode(selection_.nodeID);
        selection_.Clear();
        AddLog("Deleted: " + name);
    }

    void EditorUI::DuplicateSelectedNode() {
        if (!selection_.hasNode) return;
        auto* node = scene_->GetNode(selection_.nodeID);
        if (!node) return;

        SceneNodeDesc desc;
        desc.name     = node->GetName() + "_copy";
        desc.position = node->GetLocalPosition() +
                        Vec3{1.0f, 0, 0};
        desc.rotation = node->GetLocalRotation();
        desc.scale    = node->GetLocalScale();

        auto* sNode = static_cast<SceneNode*>(node);
        desc.hasMesh    = sNode->hasMesh;
        desc.mesh       = sNode->meshDesc;
        desc.hasPhysics = sNode->hasPhysics;
        desc.physics    = sNode->physicsDesc;

        auto r = scene_->CreateNode(desc);
        if (r.IsOk()) {
            SelectNode(r.Value());
            AddLog("Duplicated: " + desc.name);
        }
    }

    void EditorUI::CreateNode(const std::string& type) {
        SceneNodeDesc desc;
        desc.name     = type + "_" +
            std::to_string(scene_->GetNodeCount()+1);
        desc.position = {0, 2, 0};

        if (type == "Cube") {
            desc.hasMesh  = true;
            desc.mesh.albedo = {0.8f, 0.8f, 0.8f};
            desc.hasPhysics  = true;
            desc.physics.mass  = 1.0f;
            desc.physics.colliderShape = "box";
            desc.physics.halfExtents   = {0.5f,0.5f,0.5f};
        } else if (type == "Sphere") {
            desc.hasMesh  = true;
            desc.mesh.albedo = {0.8f, 0.3f, 0.3f};
            desc.hasPhysics  = true;
            desc.physics.mass  = 1.0f;
            desc.physics.colliderShape = "sphere";
            desc.physics.radius = 0.5f;
        } else if (type == "Plane") {
            desc.hasMesh  = true;
            desc.mesh.albedo = {0.4f, 0.7f, 0.4f};
            desc.scale    = {5, 0.1f, 5};
            desc.hasPhysics  = true;
            desc.physics.isStatic = true;
            desc.physics.colliderShape = "box";
            desc.physics.halfExtents   = {5,0.1f,5};
        }

        auto r = scene_->CreateNode(desc);
        if (r.IsOk()) {
            SelectNode(r.Value());
            AddLog("Created: " + desc.name);
        }
    }

    void EditorUI::AddLog(
        const std::string& msg, bool isError
    ) {
        logs_.push_back({msg, isError});
        if (logs_.size() > 200) {
            logs_.erase(logs_.begin());
        }
        scrollToBottom_ = true;
    }

} // namespace RiftCore
