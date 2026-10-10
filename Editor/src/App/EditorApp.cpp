#define NOMINMAX
#include "EditorApp.h"
#include "EditorStyle.h"

#include <Core/Logger.h>
#include <Core/PluginManager.h>
#include <OpenGLBackend/GLDevice.h>
#include <Renderer/RenderSystem.h>
#include <Renderer/SceneDrawer.h>
#include <Physics/PhysicsWorld.h>
#include <Scene/SceneSystem.h>
#include <RiftCore/Scripting/IScripting.h>
#include <RiftCore/Scene/SceneUtil.h>
#include <RiftCore/Common/EulerUtil.h>
#include <RiftCore/Common/Paths.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <ImGuizmo.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#ifdef _WIN32
    #include <commdlg.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace RiftCore {

    using namespace Ed;

    namespace {

        const char* kDefaultScript =
            "import riftcore as rc\n"
            "\n"
            "# Drop a small tower of crates onto the ground. Press Play to simulate.\n"
            "for i in range(5):\n"
            "    rc.spawn('model:crate', name='Crate %d' % i,\n"
            "             position=(0, 0.5 + i * 1.05, 0), physics='dynamic')\n"
            "print(rc.node_count(), 'nodes in the scene')\n";

        std::string GenericPath(const fs::path& p) {
            std::error_code ec;
            fs::path rel = fs::relative(p, fs::current_path(ec), ec);
            std::string s = (!ec && !rel.empty() && rel.native()[0] != '.') ? rel.generic_string()
                                                                           : p.generic_string();
            return s;
        }

        // Native open / save dialog. Returns "" when cancelled.
        std::string FileDialog(bool save, const char* filter, const char* defExt,
                               const char* initialDir)
        {
#ifdef _WIN32
            char file[MAX_PATH] = "";
            std::error_code ec;
            std::string dir = fs::absolute(initialDir, ec).make_preferred().string();
            OPENFILENAMEA ofn{};
            ofn.lStructSize     = sizeof(ofn);
            ofn.hwndOwner       = GetActiveWindow();
            ofn.lpstrFilter     = filter;
            ofn.lpstrFile       = file;
            ofn.nMaxFile        = MAX_PATH;
            ofn.lpstrInitialDir = dir.c_str();
            ofn.lpstrDefExt     = defExt;
            ofn.Flags = OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST |
                        (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
            BOOL ok = save ? GetSaveFileNameA(&ofn) : GetOpenFileNameA(&ofn);
            return ok ? GenericPath(file) : std::string();
#else
            (void)save; (void)filter; (void)defExt; (void)initialDir;
            return {};
#endif
        }

        bool EndsWith(const std::string& s, const char* ext) {
            size_t n = std::strlen(ext);
            if (s.size() < n) return false;
            for (size_t i = 0; i < n; i++) {
                if (std::tolower(static_cast<unsigned char>(s[s.size() - n + i])) != ext[i]) return false;
            }
            return true;
        }

        // Vertical (or horizontal) drag handle between two panels.
        void Splitter(const char* id, bool vertical, float thickness, float* value,
                      float sign, float minV, float maxV, float length)
        {
            ImVec2 size = vertical ? ImVec2(thickness, length) : ImVec2(length, thickness);
            ImGui::InvisibleButton(id, size);
            bool hot = ImGui::IsItemHovered() || ImGui::IsItemActive();
            if (hot) {
                ImGui::SetMouseCursor(vertical ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS);
            }
            if (ImGui::IsItemActive()) {
                ImVec2 d = ImGui::GetIO().MouseDelta;
                *value += sign * (vertical ? d.x : d.y);
            }
            if (*value < minV) *value = minV;
            if (*value > maxV) *value = maxV;
            if (hot) {
                ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
                ImVec2 c0 = vertical ? ImVec2((a.x + b.x) * 0.5f, a.y + 4) : ImVec2(a.x + 4, (a.y + b.y) * 0.5f);
                ImVec2 c1 = vertical ? ImVec2((a.x + b.x) * 0.5f, b.y - 4) : ImVec2(b.x - 4, (a.y + b.y) * 0.5f);
                ImGui::GetWindowDrawList()->AddLine(c0, c1, ImGui::GetColorU32(Col::Accent), 2.0f);
            }
        }

        bool BeginPanel(const char* id, ImVec2 size, bool padded = true) {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, padded ? ImVec2(8, 6) : ImVec2(0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 6));
            bool open = ImGui::BeginChild(id, size, ImGuiChildFlags_Borders,
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            return open;
        }

        void EndPanel() {
            ImGui::EndChild();
            ImGui::PopStyleVar(2);
        }

        const char* NodeIcon(const SceneNodeDesc& d) {
            if (d.hasLight) return Icon::Light;
            if (!d.hasMesh) return Icon::Empty;
            if (d.mesh.meshPath.rfind("primitive:", 0) == 0) return Icon::Shapes;
            return Icon::Model;
        }

        Vec3 TransformPoint(const Mat4& m, const Vec3& p) {
            return {
                m.cols[0][0] * p.x + m.cols[1][0] * p.y + m.cols[2][0] * p.z + m.cols[3][0],
                m.cols[0][1] * p.x + m.cols[1][1] * p.y + m.cols[2][1] * p.z + m.cols[3][1],
                m.cols[0][2] * p.x + m.cols[1][2] * p.y + m.cols[2][2] * p.z + m.cols[3][2] };
        }

    } // namespace

    EditorApp::EditorApp()  = default;
    EditorApp::~EditorApp() = default;

    // ============================================================
    //  Startup / shutdown
    // ============================================================

    // True once for a tab named in --show, so it is brought to the front.
    bool EditorApp::WantTab(const char* name) {
        std::string key = std::string(",") + name + ",";
        size_t at = showTabs_.find(key);
        if (at == std::string::npos) return false;
        showTabs_.erase(at, key.size() - 1);
        return true;
    }

    bool EditorApp::Init(int argc, char** argv) {
        bool haveAssets = Paths::EnterProjectRoot();

        std::string argScene, argScript, argSelect, argHouse;
        int argSheet = 0, argView = 0;
        for (int i = 1; i + 1 < argc; i++) {
            std::string a = argv[i];
            if      (a == "--scene")  argScene  = argv[++i];
            else if (a == "--script") argScript = argv[++i];
            else if (a == "--select") argSelect = argv[++i];
            else if (a == "--house")  housePrompt_ = argHouse = argv[++i];
            else if (a == "--sheet")  argSheet = std::atoi(argv[++i]);
            else if (a == "--view")   argView = std::atoi(argv[++i]);
            else if (a == "--show")   showTabs_ = std::string(",") + argv[++i] + ",";
        }
        bool argPlay = false;
        int  argStdView = -1;
        for (int i = 1; i < argc; i++) {
            if (std::string(argv[i]) == "--play") argPlay = true;
            if (std::string(argv[i]) == "--realistic") visualStyle_ = 1;
            if (std::string(argv[i]) == "--style" && i + 1 < argc) visualStyle_ = std::atoi(argv[i + 1]);
            if (std::string(argv[i]) == "--std-view" && i + 1 < argc) argStdView = std::atoi(argv[i + 1]);
        }

        EngineConfig config;
        config.appName     = "RiftCore Editor";
        config.logFilePath = "RiftCoreEditor.log";
        config.logLevel    = RiftCore::LogLevel::Info;
        if (engine_.Initialize(config).IsErr()) return false;

        auto* plugins = engine_.GetPluginManager();
        ModuleInitParams params;
        params.context  = engine_.GetContext();
        params.isEditor = true;

        if (plugins->LoadAndInit("OpenGLBackend", "RiftCore_OpenGLBackend.dll", params).IsErr()) return false;
        rhi_ = engine_.GetContext()->RHI();
        auto dev = rhi_ ? rhi_->CreateDevice(nullptr) : Result<IRHIDevice*>::Err("no RHI");
        if (dev.IsErr()) {
            engine_.GetLogger()->Error("Editor", "Could not create an OpenGL 4.6 window.");
            return false;
        }
        device_ = static_cast<GLDevice*>(dev.Value());
        window_ = device_->GetWindow();
        glfwSetWindowTitle(window_, "RiftCore Editor");
        glfwMaximizeWindow(window_);
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return false;

        if (plugins->LoadAndInit("Renderer", "RiftCore_Renderer.dll", params).IsErr()) return false;
        renderer_ = plugins->GetModuleAs<RendererModule>("Renderer")->GetRenderSystem();
        if (renderer_->Initialize(device_, 1280, 720).IsErr()) return false;

        if (plugins->LoadAndInit("Physics", "RiftCore_Physics.dll", params).IsErr()) return false;
        physMod_ = plugins->GetModuleAs<PhysicsModule>("Physics");

        if (plugins->LoadAndInit("Scene", "RiftCore_Scene.dll", params).IsErr()) return false;
        sceneMod_ = plugins->GetModuleAs<SceneModule>("Scene");
        scene_    = sceneMod_->GetSceneSystem();

        if (plugins->LoadAndInit("Scripting", "RiftCore_Scripting.dll", params).IsOk()) {
            scripting_ = plugins->GetModuleAs<IScripting>("Scripting");
        }

        drawer_ = std::make_unique<SceneDrawer>(renderer_);

        // ── ImGui ───────────────────────────────────────────
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;                 // the layout is managed by the editor
        io.ConfigWindowsMoveFromTitleBarOnly = true;
        SetupStyle();
        if (!ImGui_ImplGlfw_InitForOpenGL(window_, true)) return false;
        if (!ImGui_ImplOpenGL3_Init("#version 460"))      return false;
        ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());
        imguiReady_ = true;

        // ── Defaults ────────────────────────────────────────
        camera_.SetPosition({ 0.0f, 7.0f, 18.0f });
        camera_.SetFOV(camFov_);
        camera_.SetClipPlanes(0.1f, 2000.0f);
        camera_.RotatePitch(camPitch_);

        sun_.type      = LightType::Directional;
        sun_.color     = { 1.0f, 0.96f, 0.88f };
        sun_.intensity = 1.8f;

        pythonSource_ = kDefaultScript;
        LoadHouseDesign();      // drawings of the last generated house, if any

        Log(LogLevel::Info, "RiftCore Editor ready.");
        if (!haveAssets) Log(LogLevel::Warning, "No Assets folder found next to the executable.");
        if (!scripting_ || !scripting_->IsAvailable()) {
            Log(LogLevel::Warning, "Python scripting is not available (see the README, 'Python').");
        }

        std::error_code ec;
        if      (!argScene.empty())                              OpenScene(argScene);
        else if (fs::exists("Assets/Scenes/Showcase.json", ec))  OpenScene("Assets/Scenes/Showcase.json");
        else if (fs::exists("Assets/Scenes/TestLevel.json", ec)) OpenScene("Assets/Scenes/TestLevel.json");
        else    NewScene();
        if (scene_->GetNodeCount() == 0 && scenePath_.empty()) NewScene();
        if (!argScript.empty()) RunScript(argScript);
        if (!argHouse.empty()) { GenerateHouse(); houseSheet_ = argSheet; showDrawings_ = false; SetHouseView(argView); }
        if (!argSelect.empty()) {
            if (ISceneNode* n = scene_->FindNode(argSelect)) Select(n->GetID());
        }
        undo_.clear();
        dirty_ = false;
        UpdateTitle();
        if (argStdView >= 0) SetStandardView(argStdView);
        if (argPlay) Play();
        return true;
    }

    void EditorApp::Shutdown() {
        drawer_.reset();
        if (fbo_)      glDeleteFramebuffers(1, &fbo_);
        if (colorTex_) glDeleteTextures(1, &colorTex_);
        if (depthRbo_) glDeleteRenderbuffers(1, &depthRbo_);
        fbo_ = colorTex_ = depthRbo_ = 0;

        if (imguiReady_) {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
            imguiReady_ = false;
        }
        if (scripting_) scripting_->Shutdown();
        if (scene_)     scene_->ClearScene();
        if (rhi_ && device_) rhi_->DestroyDevice(device_);
        device_ = nullptr;
        engine_.Shutdown();
    }

    // ============================================================
    //  Main loop
    // ============================================================

    void EditorApp::Run() {
        double last = glfwGetTime();
        while (!device_->ShouldClose()) {
            device_->BeginFrame();
            glfwPollEvents();

            double now = glfwGetTime();
            float dt = static_cast<float>(now - last);
            last = now;
            if (dt > 0.1f) dt = 0.1f;

            fps_ = fps_ * 0.9f + (dt > 0.0f ? 1.0f / dt : 0.0f) * 0.1f;
            fpsHistory_[fpsOffset_] = dt * 1000.0f;
            fpsOffset_ = (fpsOffset_ + 1) % 120;

            Simulate(dt);
            RenderSceneToTarget();

            int fbw = 1, fbh = 1;
            glfwGetFramebufferSize(window_, &fbw, &fbh);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, fbw, fbh);
            glClearColor(Col::Bg.x, Col::Bg.y, Col::Bg.z, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            ImGuizmo::BeginFrame();

            HandleShortcuts();
            DrawUI();

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            device_->Present();
        }
    }

    void EditorApp::Simulate(float dt) {
        bool playing = playState_ == PlayState::Playing;
        if (scripting_) {
            scripting_->SetSimulating(playing);
            scripting_->OnUpdate(dt);
        }
        if (playing) physMod_->OnUpdate(dt);
        sceneMod_->OnUpdate(dt);
        PumpScriptOutput();
    }

    void EditorApp::RenderSceneToTarget() {
        if (wantW_ != targetW_ || wantH_ != targetH_ || !fbo_) ResizeTarget(wantW_, wantH_);
        if (!fbo_) return;

        const float d2r = 3.14159265f / 180.0f;
        float el = sunElevation_ * d2r, az = sunAzimuth_ * d2r;
        sun_.direction = { -std::cos(el) * std::cos(az), -std::sin(el), -std::cos(el) * std::sin(az) };

        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        camera_.SetFOV(camFov_);
        camera_.SetAspectRatio(static_cast<float>(targetW_) / static_cast<float>(targetH_));
        renderer_->SetClearColor(skyColor_);
        renderer_->SetWireframe(wireframe_);
        renderer_->SetVisualStyle(visualStyle_);
        camera_.SetOrthographic(ortho_, orthoHeight_);
        renderer_->SetShadows(shadows_);
        renderer_->SetExposure(exposure_);
        renderer_->BeginFrame(camera_);
        drawer_->Draw(scene_, sun_);
        renderer_->EndFrame();
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void EditorApp::ResizeTarget(int w, int h) {
        w = std::max(w, 16);
        h = std::max(h, 16);
        if (!fbo_) {
            glGenFramebuffers(1, &fbo_);
            glGenTextures(1, &colorTex_);
            glGenRenderbuffers(1, &depthRbo_);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glBindTexture(GL_TEXTURE_2D, colorTex_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex_, 0);
        glBindRenderbuffer(GL_RENDERBUFFER, depthRbo_);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthRbo_);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            Log(LogLevel::Error, "Viewport framebuffer is incomplete.");
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        targetW_ = w;
        targetH_ = h;
        renderer_->OnResize(static_cast<u32>(w), static_cast<u32>(h));
    }

    // ============================================================
    //  Logging
    // ============================================================

    void EditorApp::Log(LogLevel level, const std::string& text) {
        log_.push_back({ level, text });
        if (log_.size() > 2000) log_.erase(log_.begin(), log_.begin() + 500);
        logScroll_ = true;
    }

    void EditorApp::PumpScriptOutput() {
        if (!scripting_) return;
        const char* out = scripting_->ConsumeOutput();
        if (!out || !*out) return;
        scriptPartial_ += out;
        size_t start = 0, nl;
        while ((nl = scriptPartial_.find('\n', start)) != std::string::npos) {
            Log(LogLevel::Script, scriptPartial_.substr(start, nl - start));
            start = nl + 1;
        }
        scriptPartial_.erase(0, start);
    }

    void EditorApp::UpdateTitle() {
        std::string name = scene_ ? scene_->GetSceneInfo().name : "";
        if (name.empty()) name = "Untitled";
        std::string title = "RiftCore Editor - " + name + (dirty_ ? " *" : "");
        if (window_) glfwSetWindowTitle(window_, title.c_str());
    }

    // ============================================================
    //  Scene commands
    // ============================================================

    void EditorApp::Select(SceneNodeID id) {
        selected_ = id;
        renaming_ = INVALID_NODE;
    }

    void EditorApp::PushUndo() {
        undo_.push_back(scene_->SerializeScene());
        if (undo_.size() > 64) undo_.erase(undo_.begin());
        redo_.clear();
        dirty_ = true;
        UpdateTitle();
    }

    void EditorApp::RestoreSnapshot(const std::string& json) {
        std::string selectedName;
        if (ISceneNode* n = scene_->GetNode(selected_)) selectedName = n->GetName();
        scene_->DeserializeScene(json);
        selected_ = INVALID_NODE;
        if (!selectedName.empty()) {
            if (ISceneNode* n = scene_->FindNode(selectedName)) selected_ = n->GetID();
        }
    }

    void EditorApp::Undo() {
        if (undo_.empty() || playState_ != PlayState::Edit) return;
        redo_.push_back(scene_->SerializeScene());
        std::string snap = std::move(undo_.back());
        undo_.pop_back();
        RestoreSnapshot(snap);
        dirty_ = true;
        UpdateTitle();
    }

    void EditorApp::Redo() {
        if (redo_.empty() || playState_ != PlayState::Edit) return;
        undo_.push_back(scene_->SerializeScene());
        std::string snap = std::move(redo_.back());
        redo_.pop_back();
        RestoreSnapshot(snap);
        dirty_ = true;
        UpdateTitle();
    }

    SceneNodeID EditorApp::Spawn(const std::string& kind, const char* physics) {
        SceneNodeDesc d;
        if (!SceneUtil::MakeNodeDesc(kind.c_str(), d)) {
            Log(LogLevel::Error, "Unknown model: " + kind);
            return INVALID_NODE;
        }
        if (d.name.empty()) {
            d.name = d.hasLight ? "Sun Light" : d.hasMesh ? fs::path(kind).stem().string() : "Empty";
        }

        // Size: props are a couple of metres, buildings bigger.
        float size = 1.0f;
        const ModelCatalogEntry* e = FindModel(kind.c_str());
        if (e && std::strcmp(e->category, "Props") == 0) {
            size = 2.0f;
            for (const char* big : { "model:house", "model:tower", "model:bridge", "model:rocket",
                                     "model:tree_pine", "model:tree_round", "model:lamp_post" }) {
                if (std::strcmp(e->path, big) == 0) size = 4.0f;
            }
        }
        d.scale = { size, size, size };
        d.mesh.albedo = (e && std::strcmp(e->category, "Shapes") == 0)
            ? Vec3{ 0.80f, 0.80f, 0.82f } : Vec3{ 1.0f, 1.0f, 1.0f };

        // Place it where the camera looks at the ground, resting on it.
        Vec3 p = camera_.GetPosition(), f = camera_.GetForward();
        float t = 10.0f;
        if (f.y < -0.05f) t = std::min(-p.y / f.y, 60.0f);
        Vec3 at = { p.x + f.x * t, 0.0f, p.z + f.z * t };
        bool flat = d.mesh.meshPath == "primitive:plane";
        at.y = d.hasLight ? 8.0f : flat ? 0.01f : size * 0.5f;
        if (d.mesh.meshPath == "primitive:plane") d.scale = { 10.0f, 1.0f, 10.0f };
        d.position = at;
        if (d.hasLight) d.rotation = { 35.0f, 30.0f, 0.0f };

        if (d.hasMesh) SceneUtil::SetPhysicsMode(d, physics);

        // Unique name: "Cube", "Cube 2", ...
        std::string base = d.name;
        for (int i = 2; scene_->FindNode(d.name); i++) d.name = base + " " + std::to_string(i);

        PushUndo();
        auto r = scene_->CreateNode(d);
        if (r.IsErr()) return INVALID_NODE;
        Select(r.Value());
        return r.Value();
    }

    void EditorApp::DeleteSelected() {
        if (!scene_->GetNode(selected_)) return;
        PushUndo();
        scene_->DestroyNode(selected_);
        selected_ = INVALID_NODE;
    }

    void EditorApp::DuplicateSelected() {
        SceneNodeDesc d;
        if (!scene_->GetNodeDesc(selected_, d)) return;
        std::string base = d.name;
        size_t sp = base.find_last_of(' ');
        if (sp != std::string::npos && sp + 1 < base.size() &&
            std::all_of(base.begin() + sp + 1, base.end(), [](unsigned char c) { return std::isdigit(c); })) {
            base = base.substr(0, sp);
        }
        for (int i = 2; scene_->FindNode(d.name); i++) d.name = base + " " + std::to_string(i);
        d.position.x += std::max(1.0f, std::fabs(d.scale.x));
        PushUndo();
        auto r = scene_->CreateNode(d);
        if (r.IsOk()) Select(r.Value());
    }

    void EditorApp::NewScene() {
        if (playState_ != PlayState::Edit) Stop();
        scene_->NewScene("Untitled");
        SceneNodeDesc ground;
        ground.name = "Ground";
        SceneUtil::MakeNodeDesc("plane", ground);
        ground.name = "Ground";
        ground.scale = { 60.0f, 1.0f, 60.0f };
        ground.mesh.albedo = { 0.36f, 0.42f, 0.36f };
        ground.mesh.roughness = 0.9f;
        SceneUtil::SetPhysicsMode(ground, "static");
        scene_->CreateNode(ground);
        scenePath_.clear();
        selected_ = INVALID_NODE;
        undo_.clear();
        redo_.clear();
        dirty_ = false;
        UpdateTitle();
        Log(LogLevel::Info, "New scene.");
    }

    void EditorApp::OpenScene(const std::string& path) {
        if (playState_ != PlayState::Edit) Stop();
        auto r = scene_->LoadScene(path);
        if (r.IsErr()) {
            Log(LogLevel::Error, "Could not open " + path + ": " + r.Error().message);
            return;
        }
        scenePath_ = path;
        selected_  = INVALID_NODE;
        undo_.clear();
        redo_.clear();
        dirty_ = false;
        UpdateTitle();
        Log(LogLevel::Info, "Opened " + path + " (" + std::to_string(scene_->GetNodeCount()) + " nodes).");
    }

    void EditorApp::OpenSceneDialog() {
        std::string path = FileDialog(false, "RiftCore scene (*.json)\0*.json\0All files\0*.*\0",
                                      "json", "Assets/Scenes");
        if (!path.empty()) OpenScene(path);
    }

    void EditorApp::SaveScene(bool saveAs) {
        if (playState_ != PlayState::Edit) {
            Log(LogLevel::Warning, "Stop the simulation before saving.");
            return;
        }
        std::string path = scenePath_;
        if (saveAs || path.empty()) {
            path = FileDialog(true, "RiftCore scene (*.json)\0*.json\0", "json", "Assets/Scenes");
            if (path.empty()) return;
        }
        auto r = scene_->SaveScene(path);
        if (r.IsErr()) {
            Log(LogLevel::Error, "Save failed: " + r.Error().message);
            return;
        }
        scenePath_ = path;
        dirty_ = false;
        UpdateTitle();
        Log(LogLevel::Info, "Saved " + path);
    }

    void EditorApp::RunCode(const std::string& code) {
        if (code.empty()) return;
        if (!scripting_ || !scripting_->IsAvailable()) {
            Log(LogLevel::Error, "Python is not available in this build.");
            return;
        }
        if (playState_ == PlayState::Edit) PushUndo();
        scripting_->ExecuteString(code.c_str());
        PumpScriptOutput();
        if (!scene_->GetNode(selected_)) selected_ = INVALID_NODE;
    }

    void EditorApp::RunScript(const std::string& path) {
        if (!scripting_ || !scripting_->IsAvailable()) {
            Log(LogLevel::Error, "Python is not available in this build.");
            return;
        }
        Log(LogLevel::Info, "Running " + path);
        if (playState_ == PlayState::Edit) PushUndo();
        auto r = scripting_->LoadScript(path.c_str());
        PumpScriptOutput();
        if (r.IsErr()) Log(LogLevel::Error, r.Error().message);
        if (!scene_->GetNode(selected_)) selected_ = INVALID_NODE;
    }

    // ============================================================
    //  Play / pause / stop
    // ============================================================

    void EditorApp::Play() {
        if (playState_ == PlayState::Edit) {
            playSnapshot_ = scene_->SerializeScene();
            Log(LogLevel::Info, "Simulation started.");
        }
        playState_ = PlayState::Playing;
    }

    void EditorApp::Pause() {
        if (playState_ == PlayState::Playing) playState_ = PlayState::Paused;
    }

    void EditorApp::Stop() {
        if (playState_ == PlayState::Edit) return;
        playState_ = PlayState::Edit;
        RestoreSnapshot(playSnapshot_);
        playSnapshot_.clear();
        Log(LogLevel::Info, "Simulation stopped; scene restored.");
    }

    void EditorApp::StepOnce() {
        if (playState_ == PlayState::Edit) Play();
        playState_ = PlayState::Paused;
        const float step = 1.0f / 60.0f;
        if (scripting_) {
            scripting_->SetSimulating(true);
            scripting_->OnUpdate(step);
        }
        physMod_->OnUpdate(step);
        sceneMod_->OnUpdate(step);
    }

    // ============================================================
    //  Shortcuts
    // ============================================================

    void EditorApp::HandleShortcuts() {
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantTextInput) return;
        bool ctrl = io.KeyCtrl;

        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_N, false)) NewScene();
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_O, false)) OpenSceneDialog();
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) SaveScene(io.KeyShift);
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) Undo();
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) Redo();
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_D, false)) DuplicateSelected();
        if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))    DeleteSelected();
        if (ImGui::IsKeyPressed(ImGuiKey_F5, false)) {
            if (playState_ == PlayState::Edit) Play(); else Stop();
        }
        if (!flying_ && !ctrl) {
            if (ImGui::IsKeyPressed(ImGuiKey_Q, false)) tool_ = Tool::Select;
            if (ImGui::IsKeyPressed(ImGuiKey_W, false)) tool_ = Tool::Move;
            if (ImGui::IsKeyPressed(ImGuiKey_E, false)) tool_ = Tool::Rotate;
            if (ImGui::IsKeyPressed(ImGuiKey_R, false)) tool_ = Tool::Scale;
            if (ImGui::IsKeyPressed(ImGuiKey_F, false)) FocusSelection();
        }
    }

    // ============================================================
    //  Workspace
    // ============================================================

    void EditorApp::DrawUI() {
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->WorkPos);
        ImGui::SetNextWindowSize(vp->WorkSize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 4));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        ImGui::Begin("##Workspace", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_MenuBar |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 6));
        DrawMenuBar();
        DrawToolbar();
        ImGui::PopStyleVar();

        const float gap = 6.0f, statusH = 26.0f;
        ImVec2 avail = ImGui::GetContentRegionAvail();
        float statusY = ImGui::GetWindowHeight() - statusH - 5.0f;
        float bodyH = statusY - ImGui::GetCursorPosY() - 5.0f;
        if (bodyH < 100.0f) bodyH = 100.0f;

        float maxSide = std::max(180.0f, avail.x * 0.4f);
        leftW_   = std::clamp(leftW_,   180.0f, maxSide);
        rightW_  = std::clamp(rightW_,  220.0f, maxSide);
        bottomH_ = std::clamp(bottomH_, 120.0f, std::max(120.0f, bodyH - 160.0f));
        float centerW = avail.x - leftW_ - rightW_ - gap * 2.0f;
        if (centerW < 120.0f) centerW = 120.0f;

        // ── Left: Outliner | Place ──────────────────────────
        if (BeginPanel("##Left", ImVec2(leftW_, bodyH))) {
            if (ImGui::BeginTabBar("##LeftTabs")) {
                if (ImGui::BeginTabItem(L(Icon::Outliner, "Outliner"))) { DrawOutliner(); ImGui::EndTabItem(); }
                if (ImGui::BeginTabItem(L(Icon::Place, "Place"), nullptr,
                        WantTab("place") ? ImGuiTabItemFlags_SetSelected : 0)) { DrawPlace(); ImGui::EndTabItem(); }
                if (ImGui::BeginTabItem(L(Icon::Home, "House AI"), nullptr,
                        WantTab("house") ? ImGuiTabItemFlags_SetSelected : 0)) { DrawHouseDesigner(); ImGui::EndTabItem(); }
                ImGui::EndTabBar();
            }
        }
        EndPanel();
        ImGui::SameLine(0, 0);
        Splitter("##SplitL", true, gap, &leftW_, 1.0f, 180.0f, maxSide, bodyH);
        ImGui::SameLine(0, 0);

        // ── Centre: Viewport over Content | Console | Python ─
        ImGui::BeginGroup();
        {
            float viewH = bodyH - bottomH_ - gap;
            if (BeginPanel("##Viewport", ImVec2(centerW, viewH), false)) {
                if (ImGui::BeginTabBar("##ViewTabs")) {
                    if (ImGui::BeginTabItem(L(Icon::Viewport, "3D Viewport"))) { DrawViewport(); ImGui::EndTabItem(); }
                    bool want = showDrawings_ || WantTab("drawings");
                    showDrawings_ = false;
                    if (ImGui::BeginTabItem(L(Icon::Image, "2D Drawings"), nullptr,
                            want ? ImGuiTabItemFlags_SetSelected : 0)) { DrawDrawings(); ImGui::EndTabItem(); }
                    ImGui::EndTabBar();
                }
            }
            EndPanel();
            Splitter("##SplitB", false, gap, &bottomH_, -1.0f, 120.0f, bodyH - 160.0f, centerW);
            if (BeginPanel("##Bottom", ImVec2(centerW, bottomH_))) {
                if (ImGui::BeginTabBar("##BottomTabs")) {
                    if (ImGui::BeginTabItem(L(Icon::Content, "Content Browser"))) { DrawContent(); ImGui::EndTabItem(); }
                    if (ImGui::BeginTabItem(L(Icon::Console, "Console"), nullptr,
                            WantTab("console") ? ImGuiTabItemFlags_SetSelected : 0)) { DrawConsole(); ImGui::EndTabItem(); }
                    ImGuiTabItemFlags pyFlags = (focusPython_ || WantTab("python")) ? ImGuiTabItemFlags_SetSelected : 0;
                    focusPython_ = false;
                    if (ImGui::BeginTabItem(L(Icon::Code, "Python"), nullptr, pyFlags)) { DrawPython(); ImGui::EndTabItem(); }
                    ImGui::EndTabBar();
                }
            }
            EndPanel();
        }
        ImGui::EndGroup();
        ImGui::SameLine(0, 0);
        Splitter("##SplitR", true, gap, &rightW_, -1.0f, 220.0f, maxSide, bodyH);
        ImGui::SameLine(0, 0);

        // ── Right: Details | World | Stats ──────────────────
        if (BeginPanel("##Right", ImVec2(rightW_, bodyH))) {
            if (ImGui::BeginTabBar("##RightTabs")) {
                if (ImGui::BeginTabItem(L(Icon::Details, "Details"))) { DrawDetails(); ImGui::EndTabItem(); }
                if (ImGui::BeginTabItem(L(Icon::World, "World"), nullptr,
                        WantTab("world") ? ImGuiTabItemFlags_SetSelected : 0)) { DrawWorld(); ImGui::EndTabItem(); }
                if (ImGui::BeginTabItem(L(Icon::Stats, "Stats"), nullptr,
                        WantTab("stats") ? ImGuiTabItemFlags_SetSelected : 0)) { DrawStats(); ImGui::EndTabItem(); }
                ImGui::EndTabBar();
            }
        }
        EndPanel();

        ImGui::SetCursorPosY(statusY);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 6));
        DrawStatusBar();
        ImGui::PopStyleVar();

        ImGui::End();
        ImGui::PopStyleVar(2);

        DrawAboutPopup();
        if (!ImGui::IsAnyItemActive()) detailsEditing_ = false;
    }

    // ── Menu bar ────────────────────────────────────────────
    void EditorApp::DrawMenuBar() {
        if (!ImGui::BeginMenuBar()) return;
        bool editing = playState_ == PlayState::Edit;

        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem(L(Icon::New, "New Scene"), "Ctrl+N"))         NewScene();
            if (ImGui::MenuItem(L(Icon::Open, "Open Scene..."), "Ctrl+O"))    OpenSceneDialog();
            ImGui::Separator();
            if (ImGui::MenuItem(L(Icon::Save, "Save"), "Ctrl+S", false, editing))            SaveScene(false);
            if (ImGui::MenuItem(L(Icon::SaveAs, "Save As..."), "Ctrl+Shift+S", false, editing)) SaveScene(true);
            ImGui::Separator();
            if (ImGui::MenuItem(L(Icon::Exit, "Exit"), "Alt+F4")) glfwSetWindowShouldClose(window_, GLFW_TRUE);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem(L(Icon::Undo, "Undo"), "Ctrl+Z", false, editing && !undo_.empty())) Undo();
            if (ImGui::MenuItem(L(Icon::Redo, "Redo"), "Ctrl+Y", false, editing && !redo_.empty())) Redo();
            ImGui::Separator();
            bool has = scene_->GetNode(selected_) != nullptr;
            if (ImGui::MenuItem(L(Icon::Duplicate, "Duplicate"), "Ctrl+D", false, has)) DuplicateSelected();
            if (ImGui::MenuItem(L(Icon::Delete, "Delete"), "Del", false, has))          DeleteSelected();
            if (ImGui::MenuItem(L(Icon::Focus, "Focus Selection"), "F", false, has))    FocusSelection();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Create")) {
            u32 n = 0;
            const ModelCatalogEntry* k = GetModelCatalog(n);
            for (const char* cat : { "Shapes", "Props" }) {
                if (ImGui::BeginMenu(L(std::strcmp(cat, "Shapes") == 0 ? Icon::Shapes : Icon::Model, cat))) {
                    for (u32 i = 0; i < n; i++) {
                        if (std::strcmp(k[i].category, cat) == 0 && ImGui::MenuItem(k[i].label)) Spawn(k[i].path);
                    }
                    ImGui::EndMenu();
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem(L(Icon::Light, "Sun Light"))) Spawn("light");
            if (ImGui::MenuItem(L(Icon::Empty, "Empty Node"))) Spawn("empty");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Simulation")) {
            if (ImGui::MenuItem(L(Icon::Play, "Play"), "F5", false, playState_ != PlayState::Playing)) Play();
            if (ImGui::MenuItem(L(Icon::Pause, "Pause"), nullptr, false, playState_ == PlayState::Playing)) Pause();
            if (ImGui::MenuItem(L(Icon::Stop, "Stop"), "F5", false, !editing)) Stop();
            if (ImGui::MenuItem(L(Icon::Step, "Step One Frame"))) StepOnce();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem(L(Icon::Shapes, "Solid"), nullptr, visualStyle_ == 0)) visualStyle_ = 0;
            if (ImGui::MenuItem(L(Icon::Sun, "Realistic"), nullptr, visualStyle_ == 1)) visualStyle_ = 1;
            if (ImGui::MenuItem(L(Icon::Model, "Shaded with edges"), nullptr, visualStyle_ == 2)) visualStyle_ = 2;
            if (ImGui::MenuItem(L(Icon::Wireframe, "Hidden line"), nullptr, visualStyle_ == 3)) visualStyle_ = 3;
            ImGui::Separator();
            ImGui::MenuItem(L(Icon::Viewport, "Parallel projection"), nullptr, &ortho_);
            if (ImGui::BeginMenu(L(Icon::Camera, "Standard views"))) {
                static const char* names[] = { "Top", "Front", "Right", "Left", "Back", "Isometric" };
                for (int i = 0; i < 6; i++) if (ImGui::MenuItem(names[i])) SetStandardView(i);
                ImGui::EndMenu();
            }
            ImGui::Separator();
            ImGui::MenuItem(L(Icon::Wireframe, "Wireframe"), nullptr, &wireframe_);
            ImGui::MenuItem(L(Icon::Grid, "Selection Bounds"), nullptr, &showBounds_);
            ImGui::Separator();
            if (ImGui::MenuItem(L(Icon::Refresh, "Reset Layout"))) {
                leftW_ = 290.0f; rightW_ = 350.0f; bottomH_ = 270.0f;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem(L(Icon::Help, "About / Controls"))) showAbout_ = true;
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    // ── Toolbar ─────────────────────────────────────────────
    void EditorApp::DrawToolbar() {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 4));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, Col::Panel);
        ImGui::BeginChild("##Toolbar", ImVec2(0, 42), ImGuiChildFlags_Borders,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        bool editing = playState_ == PlayState::Edit;

        if (ToolButton(Icon::New,  "New",  "New scene (Ctrl+N)"))  NewScene();
        if (ToolButton(Icon::Open, "Open", "Open scene (Ctrl+O)")) OpenSceneDialog();
        if (ToolButton(Icon::Save, "Save", "Save scene (Ctrl+S)", false, nullptr, editing)) SaveScene(false);
        ToolSeparator();
        if (ToolButton(Icon::Undo, "Undo", "Undo (Ctrl+Z)", false, nullptr, editing && !undo_.empty())) Undo();
        if (ToolButton(Icon::Redo, "Redo", "Redo (Ctrl+Y)", false, nullptr, editing && !redo_.empty())) Redo();
        ToolSeparator();
        if (ToolButton(Icon::Select, "Sel", "Select (Q)", tool_ == Tool::Select)) tool_ = Tool::Select;
        if (ToolButton(Icon::Move,   "Mov", "Move (W)",   tool_ == Tool::Move))   tool_ = Tool::Move;
        if (ToolButton(Icon::Rotate, "Rot", "Rotate (E)", tool_ == Tool::Rotate)) tool_ = Tool::Rotate;
        if (ToolButton(Icon::Scale,  "Scl", "Scale (R)",  tool_ == Tool::Scale))  tool_ = Tool::Scale;
        ToolSeparator();
        if (ToolButton(Icon::World, "W/L", gizmoWorld_ ? "Gizmo space: World (click for Local)"
                                                       : "Gizmo space: Local (click for World)", gizmoWorld_)) {
            gizmoWorld_ = !gizmoWorld_;
        }
        if (ToolButton(Icon::Snap, "Snap", "Snap to grid while dragging", snap_)) snap_ = !snap_;
        if (ToolButton(Icon::Wireframe, "Wire", "Wireframe view", wireframe_)) wireframe_ = !wireframe_;
        if (ToolButton(Icon::Sun, "Real", visualStyle_ == 1 ? "Visual style: Realistic (click for Solid)"
                                                            : "Switch to the Realistic visual style", visualStyle_ == 1)) {
            visualStyle_ = visualStyle_ == 1 ? 0 : 1;
        }
        if (ToolButton(Icon::Focus, "Focus", "Focus the selection (F)")) FocusSelection();
        ToolSeparator();
        if (ToolButton(Icon::Add, "Add", "Add a model")) ImGui::OpenPopup("##AddMenu");
        if (ImGui::BeginPopup("##AddMenu")) {
            u32 n = 0;
            const ModelCatalogEntry* k = GetModelCatalog(n);
            for (u32 i = 0; i < n; i++) {
                if (i > 0 && std::strcmp(k[i].category, k[i - 1].category) != 0) ImGui::Separator();
                bool shape = std::strcmp(k[i].category, "Shapes") == 0;
                if (ImGui::MenuItem(L(shape ? Icon::Shapes : Icon::Model, k[i].label))) Spawn(k[i].path);
            }
            ImGui::EndPopup();
        }

        // Play controls, centred.
        const float bw = 32.0f, group = bw * 4 + 6;
        float x = (ImGui::GetWindowWidth() - group) * 0.5f;
        if (x > ImGui::GetCursorPosX()) ImGui::SameLine(x);
        if (playState_ != PlayState::Playing) {
            if (ToolButton(Icon::Play, "Play", "Play simulation (F5)", false, &Col::Green)) Play();
        } else {
            if (ToolButton(Icon::Pause, "Pause", "Pause", true, &Col::Yellow)) Pause();
        }
        if (ToolButton(Icon::Stop, "Stop", "Stop and restore the scene (F5)", false, &Col::Red, !editing)) Stop();
        if (ToolButton(Icon::Step, "Step", "Advance one frame")) StepOnce();

        // State badge on the right.
        const char* state = editing ? "EDITING" : playState_ == PlayState::Playing ? "SIMULATING" : "PAUSED";
        ImVec4 sc = editing ? Col::TextDim : playState_ == PlayState::Playing ? Col::Green : Col::Yellow;
        float tw = ImGui::CalcTextSize(state).x + 20.0f;
        ImGui::SameLine(ImGui::GetWindowWidth() - tw);
        ImGui::AlignTextToFramePadding();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.0f);
        ImGui::PushFont(GetFonts().bold);
        ImGui::TextColored(sc, "%s", state);
        ImGui::PopFont();

        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        ImGui::Dummy(ImVec2(0, 6));
    }

    // ── Status bar ──────────────────────────────────────────
    void EditorApp::DrawStatusBar() {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 3));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, Col::Panel);
        ImGui::BeginChild("##Status", ImVec2(0, 26), ImGuiChildFlags_Borders,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        SceneInfo info = scene_->GetSceneInfo();
        ImGui::TextColored(Col::TextDim, "%s", L(Icon::Scene,
            (scenePath_.empty() ? std::string("(unsaved scene)") : scenePath_).c_str()));
        if (dirty_) { ImGui::SameLine(0, 4); ImGui::TextColored(Col::Yellow, "*"); }
        ImGui::SameLine(0, 24);
        ImGui::TextColored(Col::TextDim, "%u nodes", info.nodeCount);
        if (ISceneNode* n = scene_->GetNode(selected_)) {
            ImGui::SameLine(0, 24);
            ImGui::Text("%s", L(Icon::Select, n->GetName().c_str()));
        }
        bool py = scripting_ && scripting_->IsAvailable();
        char right[96];
        std::snprintf(right, sizeof(right), "Python %s    %.0f FPS", py ? "ready" : "off", fps_);
        ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::CalcTextSize(right).x - 14.0f);
        ImGui::TextColored(Col::TextDim, "%s", right);
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    }

    void EditorApp::DrawAboutPopup() {
        if (showAbout_) { ImGui::OpenPopup("About RiftCore"); showAbout_ = false; }
        ImGui::SetNextWindowSize(ImVec2(520, 0), ImGuiCond_Appearing);
        if (ImGui::BeginPopupModal("About RiftCore", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::PushFont(GetFonts().bold);
            ImGui::Text("RiftCore Editor 0.2");
            ImGui::PopFont();
            ImGui::TextColored(Col::TextDim, "Modular C++ simulation engine with Python scripting.");
            ImGui::Separator();
            const char* rows[][2] = {
                { "Right mouse + drag",   "Look around" },
                { "Right mouse + WASD/QE", "Fly (Shift = faster)" },
                { "Mouse wheel",          "Move forward / back" },
                { "Left click",           "Select" },
                { "Q / W / E / R",        "Select / Move / Rotate / Scale" },
                { "F",                    "Focus the selection" },
                { "Ctrl+D / Del",         "Duplicate / Delete" },
                { "Ctrl+Z / Ctrl+Y",      "Undo / Redo" },
                { "F5",                   "Play / Stop the simulation" },
            };
            if (ImGui::BeginTable("##keys", 2)) {
                for (auto& r : rows) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("%s", r[0]);
                    ImGui::TableNextColumn(); ImGui::TextColored(Col::TextDim, "%s", r[1]);
                }
                ImGui::EndTable();
            }
            ImGui::Spacing();
            if (ImGui::Button("Close", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }

    // ============================================================
    //  Viewport
    // ============================================================

    void EditorApp::DrawViewport() {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImVec2 pos   = ImGui::GetCursorScreenPos();
        wantW_ = std::max(16, static_cast<int>(avail.x));
        wantH_ = std::max(16, static_cast<int>(avail.y));

        ImGui::Image(static_cast<ImTextureID>(colorTex_), avail, ImVec2(0, 1), ImVec2(1, 0));
        bool hovered = ImGui::IsItemHovered();

        if (showBounds_) DrawSelectionBox(pos.x, pos.y, avail.x, avail.y);
        DrawGizmo(pos.x, pos.y, avail.x, avail.y);

        // ---- view bar: visual style, projection, standard views ----
        static const char* styles[] = { "Solid", "Realistic", "Shaded", "Hidden Line" };
        static const char* views[]  = { "Top", "Front", "Right", "Left", "Back", "Iso" };
        bool overUi = false;
        const ImVec4 idle(0.10f, 0.11f, 0.13f, 0.88f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(7, 3));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
        ImGui::SetCursorScreenPos(ImVec2(pos.x + 10, pos.y + 8));
        for (int i = 0; i < 4; i++) {
            if (i > 0) ImGui::SameLine(0, 3);
            ImGui::PushStyleColor(ImGuiCol_Button, i == visualStyle_ ? Col::Accent : idle);
            if (ImGui::Button(styles[i])) visualStyle_ = i;
            ImGui::PopStyleColor();
            overUi |= ImGui::IsItemHovered();
        }
        ImGui::SameLine(0, 14);
        ImGui::PushStyleColor(ImGuiCol_Button, ortho_ ? Col::Accent : idle);
        if (ImGui::Button(ortho_ ? "Parallel" : "Perspective")) {
            ortho_ = !ortho_;
            Vec3 c = OrbitPivot(), p = camera_.GetPosition();
            float dist = std::sqrt((p.x-c.x)*(p.x-c.x) + (p.y-c.y)*(p.y-c.y) + (p.z-c.z)*(p.z-c.z));
            if (ortho_) orthoHeight_ = std::max(2.0f, dist * 2.0f * std::tan(camFov_ * 0.5f * 3.14159265f / 180.0f));
        }
        ImGui::PopStyleColor();
        overUi |= ImGui::IsItemHovered();
        Tooltip("Perspective / parallel (orthographic) projection");
        ImGui::PushStyleColor(ImGuiCol_Button, idle);
        for (int i = 0; i < 6; i++) {
            ImGui::SameLine(0, i == 0 ? 14.0f : 3.0f);
            if (ImGui::Button(views[i])) SetStandardView(i);
            overUi |= ImGui::IsItemHovered();
        }
        ImGui::PopStyleColor();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        ImGuiIO& io = ImGui::GetIO();
        if (hovered && !overUi && !io.KeyAlt && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
            !ImGuizmo::IsOver() && !ImGuizmo::IsUsing() && !flying_) {
            float nx = ((io.MousePos.x - pos.x) / avail.x) * 2.0f - 1.0f;
            float ny = 1.0f - ((io.MousePos.y - pos.y) / avail.y) * 2.0f;
            PickAt(nx, ny);
        }
        UpdateCamera(hovered && !overUi, io.DeltaTime);

        // Hints and play frame. Light backgrounds (hidden line, shaded) need dark text.
        ImDrawList* dl = ImGui::GetWindowDrawList();
        bool light = visualStyle_ >= 2;
        ImU32 hint = ImGui::GetColorU32(light ? ImVec4(0, 0, 0, 0.55f) : ImVec4(1, 1, 1, 0.50f));
        dl->AddText(ImVec2(pos.x + 12, pos.y + avail.y - 26), hint,
                    "MMB / Alt+LMB orbit    Shift+MMB pan    Wheel zoom    RMB + WASD fly    LMB select    F focus");
        if (playState_ != PlayState::Edit) {
            ImU32 c = ImGui::GetColorU32(playState_ == PlayState::Playing ? Col::Green : Col::Yellow);
            dl->AddRect(pos, ImVec2(pos.x + avail.x, pos.y + avail.y), c, 6.0f, 0, 2.0f);
        }
    }

    // Centre the camera orbits about: the selection, else the generated
    // house, else the last pivot.
    Vec3 EditorApp::OrbitPivot() {
        if (ISceneNode* n = scene_->GetNode(selected_)) return n->GetWorldPosition();
        if (!houseSheets_.empty()) return { 0.0f, houseTop_ / 2000.0f, 0.0f };
        return pivot_;
    }

    // Standard CAD views. All but the isometric switch to parallel projection.
    void EditorApp::SetStandardView(int view) {
        Vec3 c = OrbitPivot();
        float r = !houseSheets_.empty()
            ? std::max({ houseW_, houseD_, houseTop_ }) / 1000.0f * 1.2f + 5.0f : 18.0f;
        static const Vec3 dirs[6] = { { 0, 1, 0.002f }, { 0, 0, 1 }, { 1, 0, 0 }, { -1, 0, 0 }, { 0, 0, -1 },
                                      { 0.62f, 0.50f, 0.62f } };
        Vec3 d = Math::Normalize(dirs[view < 0 || view > 5 ? 5 : view]);
        camera_.SetPosition({ c.x + d.x * r, c.y + d.y * r, c.z + d.z * r });
        camera_.SetTarget(c);
        pivot_       = c;
        orthoHeight_ = r * 1.05f;
        ortho_       = (view != 5) || ortho_;
    }

    void EditorApp::UpdateCamera(bool hovered, float dt) {
        ImGuiIO& io = ImGui::GetIO();
        bool middle = ImGui::IsMouseDown(ImGuiMouseButton_Middle);
        bool altLmb = io.KeyAlt && ImGui::IsMouseDown(ImGuiMouseButton_Left);

        // ---- fly (right mouse) ----
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            flying_ = true;
            ImGui::SetWindowFocus();
        }
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Right)) flying_ = false;
        if (flying_) {
            camera_.RotateYaw(io.MouseDelta.x * 0.15f);
            camera_.RotatePitch(-io.MouseDelta.y * 0.15f);
            float speed = camSpeed_ * (io.KeyShift ? 3.0f : 1.0f) * dt;
            if (ImGui::IsKeyDown(ImGuiKey_W)) camera_.MoveForward( speed);
            if (ImGui::IsKeyDown(ImGuiKey_S)) camera_.MoveForward(-speed);
            if (ImGui::IsKeyDown(ImGuiKey_D)) camera_.MoveRight( speed);
            if (ImGui::IsKeyDown(ImGuiKey_A)) camera_.MoveRight(-speed);
            if (ImGui::IsKeyDown(ImGuiKey_E)) camera_.MoveUp( speed);
            if (ImGui::IsKeyDown(ImGuiKey_Q)) camera_.MoveUp(-speed);
        }

        // ---- orbit / pan (middle mouse, or Alt + left) ----
        bool startDrag = hovered && (ImGui::IsMouseClicked(ImGuiMouseButton_Middle) ||
                                     (io.KeyAlt && ImGui::IsMouseClicked(ImGuiMouseButton_Left)));
        if (startDrag) {
            panning_  = io.KeyShift && middle;
            orbiting_ = !panning_;
            Vec3 p = camera_.GetPosition(), f = camera_.GetForward();
            bool anchored = scene_->GetNode(selected_) || !houseSheets_.empty();
            pivot_ = anchored ? OrbitPivot() : Vec3{ p.x + f.x * 15.0f, p.y + f.y * 15.0f, p.z + f.z * 15.0f };
        }
        if (!middle && !altLmb) orbiting_ = panning_ = false;

        Vec3 p = camera_.GetPosition();
        Vec3 o = { p.x - pivot_.x, p.y - pivot_.y, p.z - pivot_.z };
        float dist = std::max(0.2f, std::sqrt(o.x * o.x + o.y * o.y + o.z * o.z));
        if (orbiting_ && (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f)) {
            float yaw   = std::atan2(o.z, o.x) + io.MouseDelta.x * 0.008f;
            float pitch = std::asin(std::clamp(o.y / dist, -1.0f, 1.0f)) + io.MouseDelta.y * 0.008f;
            pitch = std::clamp(pitch, -1.553f, 1.553f);
            camera_.SetPosition({ pivot_.x + dist * std::cos(pitch) * std::cos(yaw),
                                  pivot_.y + dist * std::sin(pitch),
                                  pivot_.z + dist * std::cos(pitch) * std::sin(yaw) });
            camera_.SetTarget(pivot_);
        }
        if (panning_) {
            float k = ortho_ ? orthoHeight_ / static_cast<float>(std::max(targetH_, 1)) : dist * 0.0016f;
            Vec3 r = camera_.GetRight(), u = camera_.GetUp();
            Vec3 m = { (-r.x * io.MouseDelta.x + u.x * io.MouseDelta.y) * k,
                       (-r.y * io.MouseDelta.x + u.y * io.MouseDelta.y) * k,
                       (-r.z * io.MouseDelta.x + u.z * io.MouseDelta.y) * k };
            camera_.SetPosition({ p.x + m.x, p.y + m.y, p.z + m.z });
            pivot_ = { pivot_.x + m.x, pivot_.y + m.y, pivot_.z + m.z };
        }

        // ---- zoom ----
        if (hovered && io.MouseWheel != 0.0f) {
            if (ortho_) orthoHeight_ = std::clamp(orthoHeight_ * (io.MouseWheel > 0 ? 0.88f : 1.0f / 0.88f), 0.5f, 2000.0f);
            else        camera_.MoveForward(io.MouseWheel * std::max(camSpeed_ * 0.15f, dist * 0.08f));
        }
    }

    void EditorApp::FocusSelection() {
        ISceneNode* n = scene_->GetNode(selected_);
        if (!n) return;
        Vec3 t = n->GetWorldPosition(), s = n->GetLocalScale(), f = camera_.GetForward();
        float r = std::max({ std::fabs(s.x), std::fabs(s.y), std::fabs(s.z) });
        float dist = std::min(r * 2.2f + 3.0f, 80.0f);
        camera_.SetPosition({ t.x - f.x * dist, t.y - f.y * dist, t.z - f.z * dist });
    }

    // Picks the nearest node whose oriented bounding box the ray hits.
    void EditorApp::PickAt(float ndcX, float ndcY) {
        float tanHalf = std::tan(camFov_ * 0.5f * 3.14159265f / 180.0f);
        float aspect  = static_cast<float>(targetW_) / static_cast<float>(std::max(targetH_, 1));
        Vec3 f = camera_.GetForward(), r = camera_.GetRight(), u = camera_.GetUp();
        Vec3 dir = Math::Normalize({
            f.x + r.x * ndcX * tanHalf * aspect + u.x * ndcY * tanHalf,
            f.y + r.y * ndcX * tanHalf * aspect + u.y * ndcY * tanHalf,
            f.z + r.z * ndcX * tanHalf * aspect + u.z * ndcY * tanHalf });
        Vec3 origin = camera_.GetPosition();
        if (ortho_) {                       // parallel rays from a plane through the eye
            float hh = orthoHeight_ * 0.5f, hw = hh * aspect;
            dir = f;
            origin = { origin.x + r.x * ndcX * hw + u.x * ndcY * hh - f.x * 500.0f,
                       origin.y + r.y * ndcX * hw + u.y * ndcY * hh - f.y * 500.0f,
                       origin.z + r.z * ndcX * hw + u.z * ndcY * hh - f.z * 500.0f };
        }

        SceneNodeID best = INVALID_NODE;
        float bestT = 1e30f;
        scene_->ForEachNode([&](ISceneNode* n) {
            if (!n->IsActive()) return;
            SceneNodeDesc d;
            if (!scene_->GetNodeDesc(n->GetID(), d) || (!d.hasMesh && !d.hasLight)) return;
            Vec3 c = n->GetWorldPosition();
            Vec3 he = { std::max(std::fabs(d.scale.x) * 0.5f, 0.05f),
                        std::max(std::fabs(d.scale.y) * 0.5f, 0.05f),
                        std::max(std::fabs(d.scale.z) * 0.5f, 0.05f) };
            if (!d.hasMesh) he = { 0.5f, 0.5f, 0.5f };
            // The floor is selected from the outliner, not by clicking through it.
            if (d.mesh.meshPath == "primitive:plane" && he.x > 15.0f) return;

            auto R = EulerUtil::FromEulerDeg(d.rotation);
            Vec3 rel = { origin.x - c.x, origin.y - c.y, origin.z - c.z };
            float o[3], dd[3], h[3] = { he.x, he.y, he.z };
            for (int i = 0; i < 3; i++) {       // into the box's local frame (R transposed)
                o[i]  = R.m[0][i] * rel.x + R.m[1][i] * rel.y + R.m[2][i] * rel.z;
                dd[i] = R.m[0][i] * dir.x + R.m[1][i] * dir.y + R.m[2][i] * dir.z;
            }
            float t0 = 0.0f, t1 = 1e30f;
            for (int i = 0; i < 3; i++) {
                if (std::fabs(dd[i]) < 1e-6f) {
                    if (o[i] < -h[i] || o[i] > h[i]) return;
                    continue;
                }
                float a = (-h[i] - o[i]) / dd[i], b = (h[i] - o[i]) / dd[i];
                if (a > b) std::swap(a, b);
                t0 = std::max(t0, a);
                t1 = std::min(t1, b);
                if (t0 > t1) return;
            }
            if (t0 < bestT) { bestT = t0; best = n->GetID(); }
        });
        Select(best);
    }

    void EditorApp::DrawSelectionBox(float x, float y, float w, float h) {
        ISceneNode* n = scene_->GetNode(selected_);
        if (!n) return;
        Mat4 world = SceneDrawer::WorldMatrix(scene_, n);
        Mat4 vp    = camera_.GetViewProjection();
        ImVec2 pts[8];
        for (int i = 0; i < 8; i++) {
            Vec3 c = { (i & 1) ? 0.5f : -0.5f, (i & 2) ? 0.5f : -0.5f, (i & 4) ? 0.5f : -0.5f };
            Vec3 p = TransformPoint(world, c);
            float cw = vp.cols[0][3] * p.x + vp.cols[1][3] * p.y + vp.cols[2][3] * p.z + vp.cols[3][3];
            if (cw <= 0.01f) return;                      // partly behind the camera
            Vec3 q = TransformPoint(vp, p);
            pts[i] = ImVec2(x + (q.x / cw * 0.5f + 0.5f) * w, y + (1.0f - (q.y / cw * 0.5f + 0.5f)) * h);
        }
        static const int edges[12][2] = { {0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7} };
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->PushClipRect(ImVec2(x, y), ImVec2(x + w, y + h), true);
        ImU32 col = ImGui::GetColorU32(ImVec4(1.0f, 0.62f, 0.15f, 0.9f));
        for (auto& e : edges) dl->AddLine(pts[e[0]], pts[e[1]], col, 1.5f);
        dl->PopClipRect();
    }

    void EditorApp::DrawGizmo(float x, float y, float w, float h) {
        ISceneNode* n = scene_->GetNode(selected_);
        if (!n || tool_ == Tool::Select || n->GetParentID() != INVALID_NODE) {
            gizmoWasUsing_ = false;
            return;
        }
        Vec3 pos = n->GetLocalPosition(), rot = n->GetLocalRotation(), scl = n->GetLocalScale();
        Mat4 world = Math::TRSFull(pos, rot.x, rot.y, rot.z, scl);
        float m[16], view[16], proj[16];
        std::memcpy(m,    world.DataPtr(), sizeof(m));
        std::memcpy(view, camera_.GetViewMatrix().DataPtr(), sizeof(view));
        std::memcpy(proj, camera_.GetProjectionMatrix().DataPtr(), sizeof(proj));

        ImGuizmo::SetOrthographic(ortho_);
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect(x, y, w, h);

        ImGuizmo::OPERATION op = tool_ == Tool::Move ? ImGuizmo::TRANSLATE
                               : tool_ == Tool::Rotate ? ImGuizmo::ROTATE : ImGuizmo::SCALE;
        ImGuizmo::MODE mode = (tool_ == Tool::Scale || !gizmoWorld_) ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
        float sv = tool_ == Tool::Move ? snapMove_ : tool_ == Tool::Rotate ? snapRotate_ : snapScale_;
        float snapV[3] = { sv, sv, sv };

        bool changed = ImGuizmo::Manipulate(view, proj, op, mode, m, nullptr, snap_ ? snapV : nullptr);
        bool usingNow = ImGuizmo::IsUsing();
        if (usingNow && !gizmoWasUsing_) PushUndo();
        gizmoWasUsing_ = usingNow;
        if (!changed) return;

        float s[3];
        for (int c = 0; c < 3; c++) {
            s[c] = std::sqrt(m[c * 4] * m[c * 4] + m[c * 4 + 1] * m[c * 4 + 1] + m[c * 4 + 2] * m[c * 4 + 2]);
            if (s[c] < 1e-5f) s[c] = 1e-5f;
        }
        if (tool_ == Tool::Move) {
            n->SetLocalPosition({ m[12], m[13], m[14] });
        } else if (tool_ == Tool::Rotate) {
            EulerUtil::Mat3 r;
            for (int row = 0; row < 3; row++)
                for (int c = 0; c < 3; c++) r.m[row][c] = m[c * 4 + row] / s[c];
            n->SetLocalRotation(EulerUtil::ToEulerDeg(r));
        } else {
            SceneNodeDesc d;
            scene_->GetNodeDesc(selected_, d);
            d.scale = { s[0], s[1], s[2] };
            if (d.hasPhysics) {
                SceneUtil::FitCollider(d);
                scene_->UpdateNode(selected_, d);      // collider follows the new size
            } else {
                n->SetLocalScale(d.scale);
            }
        }
    }

    // ============================================================
    //  Outliner
    // ============================================================

    void EditorApp::DrawOutliner() {
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 40.0f);
        ImGui::InputTextWithHint("##filter", L(Icon::Search, "Search nodes"), &outlinerFilter_);
        ImGui::SameLine();
        if (ImGui::Button(I(Icon::Add, "+"), ImVec2(32, 0))) ImGui::OpenPopup("##OutlinerAdd");
        Tooltip("Add a node");
        if (ImGui::BeginPopup("##OutlinerAdd")) {
            if (ImGui::MenuItem(L(Icon::Shapes, "Cube")))     Spawn("cube");
            if (ImGui::MenuItem(L(Icon::Shapes, "Sphere")))   Spawn("sphere");
            if (ImGui::MenuItem(L(Icon::Shapes, "Plane")))    Spawn("plane");
            if (ImGui::MenuItem(L(Icon::Light, "Sun Light"))) Spawn("light");
            if (ImGui::MenuItem(L(Icon::Empty, "Empty")))     Spawn("empty");
            ImGui::EndPopup();
        }

        pendingDelete_ = pendingDuplicate_ = INVALID_NODE;
        ImGui::BeginChild("##OutlinerList", ImVec2(0, 0));
        std::string filter = outlinerFilter_;
        std::transform(filter.begin(), filter.end(), filter.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (filter.empty()) {
            scene_->ForEachRootNode([&](ISceneNode* n) { DrawOutlinerNode(n, filter); });
        } else {
            scene_->ForEachNode([&](ISceneNode* n) { DrawOutlinerNode(n, filter); });
        }
        if (scene_->GetNodeCount() == 0) {
            ImGui::TextColored(Col::TextDim, "The scene is empty.\nUse the Place tab to add models.");
        }
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0) && !ImGui::IsAnyItemHovered()) {
            Select(INVALID_NODE);
        }
        ImGui::EndChild();

        if (pendingDuplicate_ != INVALID_NODE) { Select(pendingDuplicate_); DuplicateSelected(); }
        if (pendingDelete_    != INVALID_NODE) { Select(pendingDelete_);    DeleteSelected(); }
    }

    void EditorApp::DrawOutlinerNode(ISceneNode* node, const std::string& filter) {
        SceneNodeID id = node->GetID();
        SceneNodeDesc d;
        if (!scene_->GetNodeDesc(id, d)) return;

        bool flat = !filter.empty();
        if (flat) {
            std::string lower = d.name;
            std::transform(lower.begin(), lower.end(), lower.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (lower.find(filter) == std::string::npos) return;
        }
        std::vector<SceneNodeID> children = node->GetChildren();
        bool active = node->IsActive();

        ImGui::PushID(static_cast<int>(id));
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow |
                                   ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowOverlap;
        if (children.empty() || flat) flags |= ImGuiTreeNodeFlags_Leaf;
        if (id == selected_)          flags |= ImGuiTreeNodeFlags_Selected;

        bool open = false;
        if (renaming_ == id) {
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::SetKeyboardFocusHere();
            if (ImGui::InputText("##rename", &renameBuffer_,
                    ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
                if (!renameBuffer_.empty()) { PushUndo(); node->SetName(renameBuffer_); }
                renaming_ = INVALID_NODE;
            } else if (ImGui::IsItemDeactivated()) {
                renaming_ = INVALID_NODE;
            }
        } else {
            if (!active) ImGui::PushStyleColor(ImGuiCol_Text, Col::TextDim);
            open = ImGui::TreeNodeEx("##node", flags, "%s", L(NodeIcon(d), d.name.c_str()));
            if (!active) ImGui::PopStyleColor();

            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) Select(id);
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) { Select(id); FocusSelection(); }
            if (ImGui::BeginPopupContextItem("##ctx")) {
                Select(id);
                if (ImGui::MenuItem(L(Icon::Focus, "Focus"), "F")) FocusSelection();
                if (ImGui::MenuItem(L(Icon::Rename, "Rename"))) { renaming_ = id; renameBuffer_ = d.name; }
                if (ImGui::MenuItem(L(Icon::Duplicate, "Duplicate"), "Ctrl+D")) pendingDuplicate_ = id;
                ImGui::Separator();
                if (ImGui::MenuItem(L(Icon::Delete, "Delete"), "Del")) pendingDelete_ = id;
                ImGui::EndPopup();
            }
            // Visibility toggle at the right edge.
            ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 26.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_Text, active ? Col::TextDim : ImVec4(0.4f, 0.4f, 0.45f, 1.0f));
            if (ImGui::SmallButton(I(active ? Icon::Eye : Icon::EyeOff, active ? "o" : "-"))) {
                node->SetActive(!active);
                dirty_ = true;
            }
            ImGui::PopStyleColor(2);
            Tooltip(active ? "Hide" : "Show");
        }

        if (open) {
            if (!flat) {
                for (SceneNodeID c : children) {
                    if (ISceneNode* child = scene_->GetNode(c)) DrawOutlinerNode(child, filter);
                }
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    // ============================================================
    //  Place (model library)
    // ============================================================

    void EditorApp::DrawPlace() {
        static const char* modes[] = { "No physics", "Static collider", "Dynamic body" };
        PropertyLabel("Physics");
        ImGui::Combo("##placePhys", &placePhysics_, modes, 3);
        const char* phys = placePhysics_ == 1 ? "static" : placePhysics_ == 2 ? "dynamic" : nullptr;

        ImGui::BeginChild("##PlaceList", ImVec2(0, 0));
        float full = ImGui::GetContentRegionAvail().x;
        int   cols = full > 250.0f ? 3 : 2;
        float bw   = (full - 6.0f * (cols - 1)) / cols;

        auto tile = [&](const char* icon, const char* label, const char* id) {
            ImGui::PushID(id);
            ImVec2 p = ImGui::GetCursorScreenPos();
            bool clicked = ImGui::Button("##tile", ImVec2(bw, 64));
            ImDrawList* dl = ImGui::GetWindowDrawList();
            if (GetFonts().icons) {
                ImGui::PushFont(GetFonts().large);
                ImVec2 is = ImGui::CalcTextSize(icon);
                dl->AddText(ImVec2(p.x + (bw - is.x) * 0.5f, p.y + 6), ImGui::GetColorU32(Col::AccentHot), icon);
                ImGui::PopFont();
            }
            ImVec2 ts = ImGui::CalcTextSize(label);
            float tx = p.x + std::max(4.0f, (bw - ts.x) * 0.5f);
            dl->PushClipRect(p, ImVec2(p.x + bw - 2, p.y + 64), true);
            dl->AddText(ImVec2(tx, p.y + (GetFonts().icons ? 40.0f : 24.0f)), ImGui::GetColorU32(Col::Text), label);
            dl->PopClipRect();
            ImGui::PopID();
            return clicked;
        };
        int col = 0;
        auto next = [&]() { if (++col % cols != 0) ImGui::SameLine(0, 6); };

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 6));
        u32 n = 0;
        const ModelCatalogEntry* k = GetModelCatalog(n);
        for (const char* cat : { "Shapes", "Props" }) {
            bool shapes = std::strcmp(cat, "Shapes") == 0;
            if (!Section(shapes ? Icon::Shapes : Icon::Model, cat)) continue;
            col = 0;
            for (u32 i = 0; i < n; i++) {
                if (std::strcmp(k[i].category, cat) != 0) continue;
                if (tile(shapes ? Icon::Shapes : Icon::Model, k[i].label, k[i].path)) Spawn(k[i].path, phys);
                next();
            }
            ImGui::NewLine();
        }
        if (Section(Icon::Light, "Lights & helpers")) {
            col = 0;
            if (tile(Icon::Sun, "Sun Light", "light")) Spawn("light");
            next();
            if (tile(Icon::Empty, "Empty", "empty")) Spawn("empty");
            ImGui::NewLine();
        }
        if (Section(Icon::File, "Mesh files (Assets/Models)")) {
            if (!objScanned_) {
                objFiles_.clear();
                std::error_code ec;
                for (auto& e : fs::recursive_directory_iterator("Assets/Models", ec)) {
                    if (e.is_regular_file(ec) && EndsWith(e.path().string(), ".obj")) {
                        objFiles_.push_back(e.path().generic_string());
                    }
                }
                std::sort(objFiles_.begin(), objFiles_.end());
                objScanned_ = true;
            }
            col = 0;
            for (auto& f : objFiles_) {
                if (tile(Icon::Model, fs::path(f).stem().string().c_str(), f.c_str())) Spawn(f, phys);
                next();
            }
            ImGui::NewLine();
            if (ImGui::SmallButton(L(Icon::Refresh, "Rescan"))) objScanned_ = false;
        }
        ImGui::PopStyleVar();
        ImGui::EndChild();
    }

    // ============================================================
    //  Details
    // ============================================================

    void EditorApp::DrawDetails() {
        ISceneNode* node = scene_->GetNode(selected_);
        SceneNodeDesc d;
        if (!node || !scene_->GetNodeDesc(selected_, d)) {
            ImGui::Spacing();
            ImGui::TextColored(Col::TextDim, "Nothing selected.\n\nClick an object in the viewport or the\nOutliner to edit its properties.");
            return;
        }
        ImGui::BeginChild("##DetailsScroll", ImVec2(0, 0));
        bool changed = false, refit = false;

        // Header: icon, name, active.
        bool active = node->IsActive();
        if (ImGui::Checkbox("##active", &active)) { node->SetActive(active); dirty_ = true; }
        Tooltip("Visible / active");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1.0f);
        std::string name = d.name;
        if (ImGui::InputText("##name", &name) && !name.empty()) { d.name = name; changed = true; }
        ImGui::TextColored(Col::TextDim, "Node #%u%s", selected_,
                           d.parentID != INVALID_NODE ? "  (child node)" : "");
        ImGui::Spacing();

        if (Section(Icon::Transform, "Transform")) {
            float p[3] = { d.position.x, d.position.y, d.position.z };
            float r[3] = { d.rotation.x, d.rotation.y, d.rotation.z };
            float s[3] = { d.scale.x, d.scale.y, d.scale.z };
            ImGui::TextColored(Col::TextDim, "Position");
            if (DragVec3("pos", p, 0.05f)) { d.position = { p[0], p[1], p[2] }; changed = true; }
            ImGui::TextColored(Col::TextDim, "Rotation");
            if (DragVec3("rot", r, 0.5f)) { d.rotation = { r[0], r[1], r[2] }; changed = true; }
            ImGui::TextColored(Col::TextDim, "Scale");
            if (DragVec3("scl", s, 0.02f, 1.0f)) { d.scale = { s[0], s[1], s[2] }; changed = refit = true; }
            ImGui::Spacing();
        }

        if (d.hasMesh && Section(Icon::Material, "Mesh & Material")) {
            const ModelCatalogEntry* cur = FindModel(d.mesh.meshPath.c_str());
            std::string label = cur ? cur->label : d.mesh.meshPath;
            PropertyLabel("Model");
            if (ImGui::BeginCombo("##model", label.c_str())) {
                u32 n = 0;
                const ModelCatalogEntry* k = GetModelCatalog(n);
                for (u32 i = 0; i < n; i++) {
                    if (ImGui::Selectable(k[i].label, d.mesh.meshPath == k[i].path)) {
                        d.mesh.meshPath = k[i].path;
                        changed = refit = true;
                    }
                }
                ImGui::EndCombo();
            }
            float col[3] = { d.mesh.albedo.x, d.mesh.albedo.y, d.mesh.albedo.z };
            PropertyLabel("Color");
            if (ImGui::ColorEdit3("##color", col)) { d.mesh.albedo = { col[0], col[1], col[2] }; changed = true; }
            PropertyLabel("Metallic");
            changed |= ImGui::SliderFloat("##metal", &d.mesh.metallic, 0.0f, 1.0f);
            PropertyLabel("Roughness");
            changed |= ImGui::SliderFloat("##rough", &d.mesh.roughness, 0.0f, 1.0f);
            ImGui::Spacing();
        }

        if (d.hasMesh && Section(Icon::Physics, "Physics")) {
            int mode = !d.hasPhysics ? 0 : d.physics.isStatic ? 1 : 2;
            static const char* modes[] = { "None", "Static collider", "Dynamic body" };
            PropertyLabel("Body");
            if (ImGui::Combo("##physmode", &mode, modes, 3)) {
                SceneUtil::SetPhysicsMode(d, mode == 1 ? "static" : mode == 2 ? "dynamic" : "none",
                                          d.physics.mass);
                changed = true;
            }
            if (d.hasPhysics) {
                if (!d.physics.isStatic) {
                    PropertyLabel("Mass (kg)");
                    changed |= ImGui::DragFloat("##mass", &d.physics.mass, 0.05f, 0.01f, 10000.0f, "%.2f");
                }
                static const char* shapes[] = { "box", "sphere", "capsule", "plane" };
                int shape = 0;
                for (int i = 0; i < 4; i++) if (d.physics.colliderShape == shapes[i]) shape = i;
                PropertyLabel("Collider");
                if (ImGui::Combo("##shape", &shape, shapes, 4)) { d.physics.colliderShape = shapes[shape]; changed = true; }
                if (shape == 0 || shape == 2) {
                    float he[3] = { d.physics.halfExtents.x, d.physics.halfExtents.y, d.physics.halfExtents.z };
                    ImGui::TextColored(Col::TextDim, "Half extents");
                    if (DragVec3("he", he, 0.02f, 0.5f)) { d.physics.halfExtents = { he[0], he[1], he[2] }; changed = true; }
                }
                if (shape == 1 || shape == 2) {
                    PropertyLabel("Radius");
                    changed |= ImGui::DragFloat("##radius", &d.physics.radius, 0.01f, 0.01f, 1000.0f, "%.2f");
                }
                PropertyLabel("Bounciness");
                changed |= ImGui::SliderFloat("##rest", &d.physics.restitution, 0.0f, 1.0f);
                PropertyLabel("Friction");
                changed |= ImGui::SliderFloat("##fric", &d.physics.friction, 0.0f, 1.5f);
                if (ImGui::Button(L(Icon::Scale, "Fit collider to mesh"), ImVec2(-1, 0))) changed = refit = true;
            }
            ImGui::Spacing();
        }

        if (d.hasLight && Section(Icon::Light, "Light")) {
            float col[3] = { d.light.color.x, d.light.color.y, d.light.color.z };
            PropertyLabel("Color");
            if (ImGui::ColorEdit3("##lcolor", col)) { d.light.color = { col[0], col[1], col[2] }; changed = true; }
            PropertyLabel("Intensity");
            changed |= ImGui::SliderFloat("##lint", &d.light.intensity, 0.0f, 8.0f);
            ImGui::TextColored(Col::TextDim, "A light shines along its local -Y axis;\nrotate the node to aim it.");
            ImGui::Spacing();
        }

        ImGui::Separator();
        ImGui::Spacing();
        if (!d.hasMesh && ImGui::Button(L(Icon::Shapes, "Add Mesh"), ImVec2(-1, 0))) {
            d.hasMesh = true; d.mesh.meshPath = "primitive:cube"; changed = true;
        }
        if (!d.hasLight && ImGui::Button(L(Icon::Light, "Add Light"), ImVec2(-1, 0))) {
            d.hasLight = true; changed = true;
        }
        if (d.hasLight && ImGui::Button(L(Icon::Delete, "Remove Light"), ImVec2(-1, 0))) {
            d.hasLight = false; changed = true;
        }

        if (changed) {
            if (!detailsEditing_) { PushUndo(); detailsEditing_ = true; }
            if (refit && d.hasPhysics) SceneUtil::FitCollider(d);
            scene_->UpdateNode(selected_, d);
        }
        ImGui::EndChild();
    }

    // ============================================================
    //  World / Stats
    // ============================================================

    void EditorApp::DrawWorld() {
        ImGui::BeginChild("##WorldScroll", ImVec2(0, 0));
        if (Section(Icon::Viewport, "Rendering")) {
            int mode = visualStyle_;
            static const char* modes[] = { "Solid", "Realistic", "Shaded with edges", "Hidden line" };
            PropertyLabel("Mode");
            if (ImGui::Combo("##rmode", &mode, modes, 4)) visualStyle_ = mode;
            ImGui::Checkbox("Parallel projection", &ortho_);
            ImGui::BeginDisabled(visualStyle_ != 1);
            ImGui::Checkbox("Sun shadows", &shadows_);
            PropertyLabel("Exposure");
            ImGui::SliderFloat("##expo", &exposure_, 0.3f, 2.5f, "%.2f");
            ImGui::EndDisabled();
            ImGui::Spacing();
        }
        if (Section(Icon::Sun, "Sun")) {
            PropertyLabel("Direction");
            ImGui::SliderFloat("##az", &sunAzimuth_, 0.0f, 360.0f, "%.0f deg");
            PropertyLabel("Height");
            ImGui::SliderFloat("##el", &sunElevation_, 5.0f, 90.0f, "%.0f deg");
            PropertyLabel("Intensity");
            ImGui::SliderFloat("##si", &sun_.intensity, 0.0f, 5.0f);
            float c[3] = { sun_.color.x, sun_.color.y, sun_.color.z };
            PropertyLabel("Color");
            if (ImGui::ColorEdit3("##sc", c)) sun_.color = { c[0], c[1], c[2] };
            ImGui::TextColored(Col::TextDim, "Used when the scene has no Sun Light node.");
            ImGui::Spacing();
        }
        if (Section(Icon::World, "Environment")) {
            float c[3] = { skyColor_.x, skyColor_.y, skyColor_.z };
            PropertyLabel("Sky color");
            if (ImGui::ColorEdit3("##sky", c)) skyColor_ = { c[0], c[1], c[2] };
            if (IPhysics* phys = engine_.GetContext()->Get<IPhysics>()) {
                Vec3 g = phys->GetGravity();
                float gv[3] = { g.x, g.y, g.z };
                ImGui::TextColored(Col::TextDim, "Gravity (m/s2)");
                if (DragVec3("grav", gv, 0.05f)) phys->SetGravity({ gv[0], gv[1], gv[2] });
                if (ImGui::SmallButton("Earth")) phys->SetGravity({ 0, -9.81f, 0 });
                ImGui::SameLine();
                if (ImGui::SmallButton("Moon"))  phys->SetGravity({ 0, -1.62f, 0 });
                ImGui::SameLine();
                if (ImGui::SmallButton("Mars"))  phys->SetGravity({ 0, -3.71f, 0 });
                ImGui::SameLine();
                if (ImGui::SmallButton("Zero-g")) phys->SetGravity({ 0, 0, 0 });
            }
            ImGui::Spacing();
        }
        if (Section(Icon::Camera, "Editor camera")) {
            PropertyLabel("Fly speed");
            ImGui::SliderFloat("##cs", &camSpeed_, 1.0f, 100.0f, "%.0f m/s");
            PropertyLabel("Field of view");
            ImGui::SliderFloat("##fov", &camFov_, 20.0f, 110.0f, "%.0f deg");
            ImGui::Spacing();
        }
        if (Section(Icon::Snap, "Snapping")) {
            ImGui::Checkbox("Snap while dragging", &snap_);
            PropertyLabel("Move");   ImGui::DragFloat("##sm", &snapMove_, 0.05f, 0.01f, 10.0f, "%.2f m");
            PropertyLabel("Rotate"); ImGui::DragFloat("##sr", &snapRotate_, 1.0f, 1.0f, 90.0f, "%.0f deg");
            PropertyLabel("Scale");  ImGui::DragFloat("##ss", &snapScale_, 0.01f, 0.01f, 1.0f, "%.2f");
        }
        ImGui::EndChild();
    }

    void EditorApp::DrawStats() {
        ImGui::BeginChild("##StatsScroll", ImVec2(0, 0));
        if (Section(Icon::Stats, "Frame")) {
            ImGui::PlotLines("##ft", fpsHistory_, 120, fpsOffset_, nullptr, 0.0f, 50.0f,
                             ImVec2(ImGui::GetContentRegionAvail().x, 70));
            ImGui::Text("%.0f FPS   (%.2f ms)", fps_, fps_ > 0.0f ? 1000.0f / fps_ : 0.0f);
            ImGui::Spacing();
        }
        auto row = [](const char* k, const std::string& v) {
            ImGui::TextColored(Col::TextDim, "%s", k);
            ImGui::SameLine(130.0f);
            ImGui::TextUnformatted(v.c_str());
        };
        if (Section(Icon::Viewport, "Renderer")) {
            RenderStats rs = renderer_->GetStats();
            row("Draw calls", std::to_string(rs.drawCalls));
            row("Triangles",  std::to_string(rs.triangles));
            row("Viewport",   std::to_string(targetW_) + " x " + std::to_string(targetH_));
            row("Backend",    device_->GetBackendName());
            row("GPU",        device_->GetGPUName());
            ImGui::Spacing();
        }
        if (Section(Icon::Scene, "Scene")) {
            row("Nodes", std::to_string(scene_->GetNodeCount()));
            if (IPhysics* phys = engine_.GetContext()->Get<IPhysics>()) {
                row("Physics bodies", std::to_string(phys->GetBodyCount()));
            }
            row("Undo steps", std::to_string(undo_.size()));
            row("Python", scripting_ && scripting_->IsAvailable() ? "available" : "not available");
        }
        ImGui::EndChild();
    }

    // ============================================================
    //  Content browser
    // ============================================================

    void EditorApp::DrawContent() {
        std::error_code ec;
        if (!fs::is_directory(contentDir_, ec)) contentDir_ = "Assets";

        bool atRoot = fs::path(contentDir_).generic_string() == "Assets";
        ImGui::BeginDisabled(atRoot);
        if (ImGui::Button(I(Icon::Up, "Up"), ImVec2(32, 0))) {
            contentDir_ = fs::path(contentDir_).parent_path().generic_string();
        }
        ImGui::EndDisabled();
        Tooltip("Parent folder");
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(Col::TextDim, "%s", L(Icon::FolderOpen, contentDir_.c_str()));

        struct Entry { fs::path path; bool dir; };
        std::vector<Entry> entries;
        for (auto& e : fs::directory_iterator(contentDir_, ec)) {
            entries.push_back({ e.path(), e.is_directory(ec) });
        }
        std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
            if (a.dir != b.dir) return a.dir;
            return a.path.filename() < b.path.filename();
        });

        ImGui::BeginChild("##ContentGrid", ImVec2(0, 0));
        const float tileW = 96.0f, tileH = 84.0f;
        int cols = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / (tileW + 8.0f)));
        int i = 0;
        std::string navigate;
        for (auto& e : entries) {
            std::string name = e.path.filename().string();
            std::string full = e.path.generic_string();
            const char* icon = Icon::File;
            ImVec4 tint = Col::TextDim;
            if (e.dir)                         { icon = Icon::Folder; tint = Col::Yellow; }
            else if (EndsWith(name, ".json"))  { icon = Icon::Scene;  tint = Col::AccentHot; }
            else if (EndsWith(name, ".obj"))   { icon = Icon::Model;  tint = Col::Green; }
            else if (EndsWith(name, ".py"))    { icon = Icon::Code;   tint = ImVec4(0.95f, 0.8f, 0.35f, 1); }
            else if (EndsWith(name, ".wav"))   { icon = Icon::Audio; }
            else if (EndsWith(name, ".png") || EndsWith(name, ".jpg")) { icon = Icon::Image; }

            ImGui::PushID(full.c_str());
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::Button("##entry", ImVec2(tileW, tileH));
            ImGui::PopStyleColor();
            bool dbl = ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            if (GetFonts().icons) {
                ImGui::PushFont(GetFonts().large);
                ImVec2 is = ImGui::CalcTextSize(icon);
                dl->AddText(ImVec2(p.x + (tileW - is.x) * 0.5f, p.y + 10), ImGui::GetColorU32(tint), icon);
                ImGui::PopFont();
            }
            ImVec2 ts = ImGui::CalcTextSize(name.c_str());
            dl->PushClipRect(ImVec2(p.x + 2, p.y), ImVec2(p.x + tileW - 2, p.y + tileH), true);
            dl->AddText(ImVec2(p.x + std::max(4.0f, (tileW - ts.x) * 0.5f), p.y + 54),
                        ImGui::GetColorU32(Col::Text), name.c_str());
            dl->PopClipRect();
            Tooltip(e.dir ? "Double-click to open" :
                    EndsWith(name, ".json") ? "Double-click to open this scene" :
                    EndsWith(name, ".obj")  ? "Double-click to place this mesh" :
                    EndsWith(name, ".py")   ? "Double-click to open in the Python tab" : name.c_str());
            if (dbl) {
                if (e.dir) navigate = full;
                else if (EndsWith(name, ".json")) OpenScene(full);
                else if (EndsWith(name, ".obj"))  Spawn(full);
                else if (EndsWith(name, ".py")) {
                    std::ifstream f(full, std::ios::binary);
                    std::stringstream ss; ss << f.rdbuf();
                    pythonSource_ = ss.str();
                    pythonFile_   = full;
                    focusPython_  = true;
                }
            }
            ImGui::PopID();
            if (++i % cols != 0) ImGui::SameLine(0, 8);
        }
        if (entries.empty()) ImGui::TextColored(Col::TextDim, "This folder is empty.");
        ImGui::EndChild();
        if (!navigate.empty()) contentDir_ = navigate;
    }

    // ============================================================
    //  Console
    // ============================================================

    void EditorApp::DrawConsole() {
        auto toggle = [](const char* icon, const char* label, bool* v, const ImVec4& col) {
            ImGui::PushStyleColor(ImGuiCol_Text, *v ? col : Col::TextDim);
            ImGui::PushStyleColor(ImGuiCol_Button, *v ? Col::Frame : ImVec4(0, 0, 0, 0));
            if (ImGui::Button(L(icon, label))) *v = !*v;
            ImGui::PopStyleColor(2);
            ImGui::SameLine();
        };
        toggle(Icon::Info,    "Info",     &showInfo_,   Col::Text);
        toggle(Icon::Warning, "Warnings", &showWarn_,   Col::Yellow);
        toggle(Icon::Error,   "Errors",   &showError_,  Col::Red);
        toggle(Icon::Code,    "Python",   &showScript_, Col::AccentHot);
        ImGui::SameLine(ImGui::GetWindowWidth() - 90.0f);
        if (ImGui::Button(L(Icon::Clear, "Clear"))) log_.clear();

        float inputH = ImGui::GetFrameHeightWithSpacing();
        ImGui::PushStyleColor(ImGuiCol_ChildBg, Col::Bg);
        ImGui::BeginChild("##Log", ImVec2(0, -inputH), ImGuiChildFlags_Borders);
        ImGui::PushFont(GetFonts().mono);
        for (auto& line : log_) {
            ImVec4 col = Col::Text;
            const char* tag = "     ";
            switch (line.level) {
                case LogLevel::Info:    if (!showInfo_)   continue; tag = "info "; col = ImVec4(0.75f, 0.78f, 0.82f, 1); break;
                case LogLevel::Warning: if (!showWarn_)   continue; tag = "warn "; col = Col::Yellow; break;
                case LogLevel::Error:   if (!showError_)  continue; tag = "error"; col = Col::Red; break;
                case LogLevel::Script:  if (!showScript_) continue; tag = "py   "; col = Col::AccentHot; break;
            }
            ImGui::TextColored(Col::TextDim, "%s", tag);
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, col);
            ImGui::TextUnformatted(line.text.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::PopFont();
        if (logScroll_) { ImGui::SetScrollHereY(1.0f); logScroll_ = false; }
        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(Col::AccentHot, ">>>");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::PushFont(GetFonts().mono);
        if (ImGui::InputTextWithHint("##cmd", "Python, e.g.  import riftcore as rc; rc.spawn('sphere', position=(0,5,0), physics='dynamic')",
                &consoleInput_, ImGuiInputTextFlags_EnterReturnsTrue)) {
            Log(LogLevel::Script, ">>> " + consoleInput_);
            RunCode(consoleInput_);
            consoleInput_.clear();
            ImGui::SetKeyboardFocusHere(-1);
        }
        ImGui::PopFont();
    }

    // ============================================================
    //  Python editor
    // ============================================================

    void EditorApp::DrawPython() {
        bool py = scripting_ && scripting_->IsAvailable();

        // Left: scripts in Assets/Scripts.
        ImGui::BeginChild("##ScriptList", ImVec2(190, 0), ImGuiChildFlags_Borders);
        ImGui::TextColored(Col::TextDim, "%s", L(Icon::Folder, "Assets/Scripts"));
        ImGui::Separator();
        std::error_code ec;
        std::vector<std::string> scripts;
        for (auto& e : fs::directory_iterator("Assets/Scripts", ec)) {
            if (e.is_regular_file(ec) && EndsWith(e.path().string(), ".py")) {
                scripts.push_back(e.path().generic_string());
            }
        }
        std::sort(scripts.begin(), scripts.end());
        for (auto& s : scripts) {
            std::string name = fs::path(s).filename().string();
            if (ImGui::Selectable(L(Icon::Code, name.c_str()), pythonFile_ == s)) {
                std::ifstream f(s, std::ios::binary);
                std::stringstream ss; ss << f.rdbuf();
                pythonSource_ = ss.str();
                pythonFile_   = s;
            }
        }
        if (scripts.empty()) ImGui::TextColored(Col::TextDim, "No scripts yet.");
        ImGui::EndChild();
        ImGui::SameLine();

        // Right: editor.
        ImGui::BeginGroup();
        ImGui::BeginDisabled(!py);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.50f, 0.28f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.62f, 0.35f, 1.0f));
        bool run = ImGui::Button(L(Icon::Run, "Run"), ImVec2(90, 0));
        ImGui::PopStyleColor(2);
        ImGui::EndDisabled();
        Tooltip(py ? "Run the script (Ctrl+Enter)" : "Python is not available in this build");
        ImGui::SameLine();
        if (ImGui::Button(L(Icon::New, "New"))) { pythonSource_ = "import riftcore as rc\n\n"; pythonFile_.clear(); }
        ImGui::SameLine();
        if (ImGui::Button(L(Icon::Save, "Save"))) {
            std::string path = pythonFile_;
            if (path.empty()) {
                fs::create_directories("Assets/Scripts", ec);
                path = FileDialog(true, "Python script (*.py)\0*.py\0", "py", "Assets/Scripts");
            }
            if (!path.empty()) {
                std::ofstream f(path, std::ios::binary);
                f << pythonSource_;
                pythonFile_ = path;
                Log(LogLevel::Info, "Saved " + path);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(L(Icon::Help, "API"))) RunCode("import riftcore; riftcore.api()");
        Tooltip("List every riftcore function in the Console");
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(Col::TextDim, "%s", pythonFile_.empty() ? "(unsaved script)" : pythonFile_.c_str());

        ImGui::PushFont(GetFonts().mono);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, Col::Bg);
        ImGui::InputTextMultiline("##source", &pythonSource_, ImVec2(-1.0f, -1.0f),
                                  ImGuiInputTextFlags_AllowTabInput);
        if (ImGui::IsItemFocused() && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Enter, false)) run = true;
        ImGui::PopStyleColor();
        ImGui::PopFont();
        ImGui::EndGroup();

        if (run && py) {
            Log(LogLevel::Script, "--- run " + (pythonFile_.empty() ? std::string("script") : pythonFile_) + " ---");
            RunCode(pythonSource_);
        }
    }

} // namespace RiftCore
