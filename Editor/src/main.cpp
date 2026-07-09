#define NOMINMAX
#include <Core/Engine.h>
#include <Core/Logger.h>
#include <Core/PluginManager.h>
#include <OpenGLBackend/GLDevice.h>
#include <Input/InputSystem.h>
#include <Renderer/RenderSystem.h>
#include <Renderer/Camera.h>
#include <Renderer/RenderTypes.h>
#include <Renderer/TextureLoader.h>
#include <Physics/PhysicsWorld.h>
#include <Scene/SceneSystem.h>
#include <Scene/SceneNode.h>


// HUD is now part of the Editor UI
#include <UI/HUD.h>

// Panel Includes
#include <UI/Panels/ViewportPanel.h>
#include <UI/Panels/HierarchyPanel.h>
#include <UI/Panels/InspectorPanel.h>
#include <UI/Panels/AssetBrowserPanel.h>
#include <UI/Panels/VisualScriptingPanel.h>
#include <UI/Panels/ConsolePanel.h>

#include <ImGuizmo.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <cmath>
#include <string>

using namespace RiftCore;

int main()
{
    // ===== INITIALIZATION =====
    Engine engine;
    EngineConfig config;
    config.appName = "RiftCore Editor";
    config.logFilePath = "RiftCoreEditor.log";
    config.logLevel = LogLevel::Info;
    if (engine.Initialize(config).IsErr()) return -1;

    auto* logger = engine.GetLogger();
    auto* plugins = engine.GetPluginManager();
    ModuleInitParams params;
    params.context = engine.GetContext();

    // ── Load modules ──────────────────────────────────────
    logger->Info("Editor", "Loading modules...");
    plugins->LoadAndInit("OpenGLBackend", "RiftCore_OpenGLBackend.dll", params);
    auto* rhi = engine.GetContext()->RHI();
    auto  devRes = rhi->CreateDevice(nullptr);
    if (devRes.IsErr()) return -1;
    auto* device = static_cast<GLDevice*>(devRes.Value());

    plugins->LoadAndInit("Input", "RiftCore_Input.dll", params);
    auto* inputMod = plugins->GetModuleAs<InputModule>("Input");
    InputSystem* input = inputMod ? inputMod->GetInputSystem() : nullptr;

    plugins->LoadAndInit("Renderer", "RiftCore_Renderer.dll", params);
    auto* rendMod = plugins->GetModuleAs<RendererModule>("Renderer");
    auto* renderer = rendMod->GetRenderSystem();
    if (renderer->Initialize(device, 1280, 720).IsErr()) return -1;
    auto* texLoader = renderer->GetTextureLoader();

    plugins->LoadAndInit("Physics", "RiftCore_Physics.dll", params);
    auto* physMod = plugins->GetModuleAs<PhysicsModule>("Physics");
    auto* physics = physMod->GetPhysics();

    plugins->LoadAndInit("Scene", "RiftCore_Scene.dll", params);
    auto* sceneMod = plugins->GetModuleAs<SceneModule>("Scene");
    auto* scene = sceneMod->GetSceneSystem();

    plugins->LoadAndInit("Scripting", "RiftCore_Scripting.dll", params);
    IScripting* scriptMod = plugins->GetModuleAs<IScripting>("Scripting");

    logger->Info("Editor", "All modules loaded successfully");

    // ── Initialize HUD (Main UI System) ──────────────────
    logger->Info("Editor", "Initializing HUD...");
    HUD hud;
    HUDConfig hudConfig;
    hudConfig.EnableDocking = true;
    hudConfig.EnableViewports = true;
    hudConfig.DarkTheme = true;
    hudConfig.IconsPath = "Assets/Icons/";

    if (!hud.Initialize(device->GetWindow(), hudConfig)) {
        logger->Error("Editor", "HUD initialization failed!");
        return -1;
    }

    // Because HUD and Editor are now compiled together in the same EXE,
    // we don't need any complex DLL boundary syncs! 
    // Just sync ImGuizmo to the local context.
    ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());

    // Set up HUD callbacks for engine interaction
    std::string scenePath;
    HUDCallbacks callbacks;
    callbacks.OnNewScene = [&]() {
        scene->NewScene("NewScene");
        logger->Info("Editor", "Created new scene");
        };
    callbacks.OnOpenScene = [&]() {
        auto result = scene->LoadScene(scenePath);
        if (result.IsOk()) logger->Info("Editor", "Scene loaded");
        };
    callbacks.OnSaveScene = [&]() {
        scene->SaveScene(scenePath);
        logger->Info("Editor", "Scene saved");
        };
    callbacks.OnPlay = [&]() { logger->Info("Editor", "Play mode started"); };
    callbacks.OnStop = [&]() { logger->Info("Editor", "Play mode stopped"); };
    callbacks.OnExecuteCommand = [&](const std::string& cmd) {
        if (scriptMod) scriptMod->ExecuteString(const_cast<char*>(cmd.c_str()));
        logger->Info("Script", ("Executed: " + cmd).c_str());
        };
    hud.SetCallbacks(callbacks);

    // Initialize Visual Scripting Panel
    UI::VisualScriptingPanel scriptPanel;
    scriptPanel.Initialize();

    // Panel Instances
    UI::ViewportPanel viewportPanel;
    UI::HierarchyPanel hierarchyPanel;
    UI::InspectorPanel inspectorPanel;
    UI::AssetBrowserPanel assetPanel;
    UI::ConsolePanel consolePanel;

    logger->Info("Editor", "HUD and panels initialized");

    // ── Physics Setup ────────────────────────────────────
    logger->Info("Physics", "Creating ground plane...");
    physics->GetWorld()->AddGroundPlane(0.0f, 0.6f, 0.6f);

    // ── Load default scene ───────────────────────────────
    logger->Info("Scene", "Loading scene...");
    scenePath = "..\\..\\..\\Assets\\Scenes\\TestLevel.json";
    auto loadResult = scene->LoadScene(scenePath);
    if (loadResult.IsOk()) {
        logger->Info("Scene", "Loaded scene: TestLevel.json (" + std::to_string(scene->GetNodeCount()) + " nodes)");
    }
    else {
        logger->Warning("Scene", "Failed to load TestLevel.json, creating new scene");
        scene->NewScene("NewScene");
    }

    // ── Meshes for rendering ─────────────────────────────
    logger->Info("Renderer", "Loading meshes...");
    auto* cubeMesh = renderer->UploadMesh(MeshFactory::CreateCube(1.0f)).Value();
    auto* planeMesh = renderer->UploadMesh(MeshFactory::CreatePlane(30.0f, 10)).Value();
    auto* sphereMesh = renderer->UploadMesh(MeshFactory::CreateSphere(0.5f, 20, 20)).Value();

    // ── Textures ─────────────────────────────────────────
    logger->Info("Renderer", "Loading textures...");
    u8 gA[4] = { 90, 130, 90, 255 }, gB[4] = { 60, 90, 60, 255 };
    auto* groundTex = texLoader->CreateCheckerboard("ground", 256, 32, gA, gB).Value();
    auto* redTex = texLoader->CreateSolidColor("red", 200, 80, 80).Value();
    auto* blueTex = texLoader->CreateSolidColor("blue", 80, 100, 220).Value();
    auto* goldTex = texLoader->CreateSolidColor("gold", 220, 180, 50).Value();
    auto* grayTex = texLoader->CreateSolidColor("gray", 160, 160, 160).Value();
    auto* greenTex = texLoader->CreateSolidColor("green", 80, 200, 80).Value();
    auto* checker = texLoader->GetCheckerTexture();

    auto makeMat = [](Texture2D* tex, f32 met = 0.0f, f32 rou = 0.5f) {
        MaterialData m;
        m.albedo = { 1, 1, 1 };
        m.metallic = met;
        m.roughness = rou;
        m.albedoTex = tex;
        return m;
        };

    MaterialData groundMat = makeMat(groundTex, 0, 0.9f);
    groundMat.texTileX = 8;
    groundMat.texTileY = 8;

    // ── Editor camera ────────────────────────────────────
    Camera camera;
    camera.SetPosition({ 0, 8, 16 });
    camera.SetFOV(60.0f);
    camera.SetAspectRatio(1280.0f / 720.0f);
    camera.SetClipPlanes(0.1f, 500.0f);
    camera.RotatePitch(-20.0f);

    f32 camYaw = -90.0f;
    f32 camPitch = -20.0f;

    Light sun;
    sun.type = LightType::Directional;
    sun.direction = { -0.5f, -1.0f, -0.5f };
    sun.color = { 1, 0.95f, 0.85f };
    sun.intensity = 1.8f;

    u32 frameCount = 0;
    u64 totalFrames = 0;
    const f32 camSpd = 8.0f;
    const f32 rotSpd = 60.0f;
    const f32 dt = 0.016f;

    // ── Load GL Function Pointers ────────────────────────
    logger->Info("Renderer", "Loading OpenGL function pointers...");
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        logger->Error("Editor", "Failed to initialize GLAD pointers!");
        return -1;
    }

    // ── Create Framebuffer ───────────────────────────────
    logger->Info("Renderer", "Creating framebuffer...");
    uint32_t fbo, sceneTexture, rbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glGenTextures(1, &sceneTexture);
    glBindTexture(GL_TEXTURE_2D, sceneTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1280, 720, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneTexture, 0);

    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 1280, 720);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        logger->Error("Renderer", "Framebuffer is not complete!");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    logger->Info("Editor", "Initialization complete. Starting main loop...");

    // ===== MAIN LOOP =====
    while (!device->ShouldClose())
    {
        device->BeginFrame();
        if (input) input->Update();

        if (scriptMod) scriptMod->OnUpdate(dt);
        physMod->OnUpdate(dt);
        sceneMod->OnUpdate(dt);

        hud.BeginFrame();

        // ── Editor Camera Input ──────────────────────────
        if (input) {
            if (input->IsKeyDown(Key::Escape)) break;

            if (input->IsKeyDown(Key::W)) camera.MoveForward(-camSpd * dt);
            if (input->IsKeyDown(Key::S)) camera.MoveForward(camSpd * dt);
            if (input->IsKeyDown(Key::A)) camera.MoveRight(-camSpd * dt);
            if (input->IsKeyDown(Key::D)) camera.MoveRight(camSpd * dt);
            if (input->IsKeyDown(Key::Q)) camera.MoveUp(-camSpd * dt);
            if (input->IsKeyDown(Key::E)) camera.MoveUp(camSpd * dt);
            if (input->IsKeyDown(Key::Left)) {
                camera.RotateYaw(-rotSpd * dt);
                camYaw -= rotSpd * dt;
            }
            if (input->IsKeyDown(Key::Right)) {
                camera.RotateYaw(rotSpd * dt);
                camYaw += rotSpd * dt;
            }
            if (input->IsKeyDown(Key::Up)) {
                camera.RotatePitch(rotSpd * dt);
                camPitch += rotSpd * dt;
            }
            if (input->IsKeyDown(Key::Down)) {
                camera.RotatePitch(-rotSpd * dt);
                camPitch -= rotSpd * dt;
            }
        }

        // ── 3D Scene Rendering (to FBO) ──────────────────
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, 1280, 720);
        glClearColor(0.08f, 0.08f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        renderer->BeginFrame(camera);
        renderer->SubmitLight(sun);

        {
            DrawCall dc;
            dc.mesh = planeMesh;
            dc.material = groundMat;
            dc.transform = Math::Translate({ 0, 0, 0 });
            renderer->Submit(dc);
        }

        scene->ForEachNode([&](ISceneNode* node) {
            if (!node->IsActive()) return;

            Vec3 pos = node->GetLocalPosition();
            Vec3 rot = node->GetLocalRotation();
            Vec3 scl = node->GetLocalScale();

            auto* sNode = static_cast<SceneNode*>(node);
            GPUMesh* mesh = cubeMesh;
            if (sNode->hasPhysics) {
                auto& shape = sNode->physicsDesc.colliderShape;
                if (shape == "sphere") mesh = sphereMesh;
                else if (shape == "plane") mesh = planeMesh;
            }

            MaterialData mat = makeMat(checker);
            if (sNode->hasMesh) {
                Vec3 alb = sNode->meshDesc.albedo;
                f32  met = sNode->meshDesc.metallic;
                f32  rou = sNode->meshDesc.roughness;
                if (alb.x > 0.6f && alb.y < 0.4f) mat = makeMat(redTex, met, rou);
                else if (alb.z > 0.6f) mat = makeMat(blueTex, met, rou);
                else if (alb.x > 0.6f && alb.y > 0.5f) mat = makeMat(goldTex, met, rou);
                else if (alb.y > 0.5f && alb.x < 0.5f) mat = makeMat(greenTex, met, rou);
                else mat = makeMat(grayTex, met, rou);
            }

            if (sNode->hasPhysics && sNode->physicsDesc.colliderShape == "plane") return;

            Vec3 renderScale = scl;
            if (sNode->hasPhysics && sNode->physicsDesc.colliderShape == "box") {
                auto& he = sNode->physicsDesc.halfExtents;
                renderScale = { he.x * 2, he.y * 2, he.z * 2 };
            }

            DrawCall dc;
            dc.mesh = mesh;
            dc.material = mat;
            dc.transform = Math::TRSFull(pos, rot.x, rot.y, rot.z, renderScale);
            renderer->Submit(dc);
            });

        renderer->EndFrame();

        // ── UI Rendering (to main screen) ────────────────
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, 1280, 720);
        glClearColor(0.05f, 0.05f, 0.07f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        hud.OnUIRender();

        if (hud.IsViewportVisible()) viewportPanel.OnUIRender((uint32_t)(intptr_t)sceneTexture, ImVec2(1280, 720));
        if (hud.IsHierarchyVisible()) hierarchyPanel.OnUIRender(scene, hud.GetCommandBuffer());
        if (hud.IsInspectorVisible()) inspectorPanel.OnUIRender(nullptr, hud.GetCommandBuffer());
        if (hud.IsAssetBrowserVisible()) assetPanel.OnUIRender();
        if (hud.IsScriptingVisible()) scriptPanel.OnUIRender();
        if (hud.IsConsoleVisible()) consolePanel.OnUIRender();

        auto commands = hud.GetCommandBuffer().Flush();
        for (auto& cmd : commands) {
            switch (cmd.Type) {
            case EditorCommandType::Play: consolePanel.AddLog("[Engine] Play Mode Started"); break;
            case EditorCommandType::Stop: consolePanel.AddLog("[Engine] Play Mode Stopped"); break;
            case EditorCommandType::NewScene: consolePanel.AddLog("[Scene] New scene created"); break;
            case EditorCommandType::SaveScene: consolePanel.AddLog("[Scene] Scene saved"); break;
            case EditorCommandType::OpenScene: consolePanel.AddLog("[Scene] Scene loaded"); break;
            case EditorCommandType::Undo: consolePanel.AddLog("[Edit] Undo"); break;
            case EditorCommandType::Redo: consolePanel.AddLog("[Edit] Redo"); break;
            default: break;
            }
        }

        hud.EndFrame();
        device->Present();
        frameCount++;
        totalFrames++;

        if (frameCount % 600 == 0) {
            auto si = scene->GetSceneInfo();
            logger->Info("Editor", "Frame=" + std::to_string(frameCount) + " Scene=" + si.name + " Nodes=" + std::to_string(si.nodeCount));
        }
    }

    // ===== CLEANUP =====
    logger->Info("Editor", "Done. Frames: " + std::to_string(totalFrames));

    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &sceneTexture);
    glDeleteRenderbuffers(1, &rbo);

    scriptPanel.Shutdown();
    if (scriptMod) scriptMod->Shutdown();
    hud.Shutdown();
    renderer->DestroyMesh(cubeMesh);
    renderer->DestroyMesh(planeMesh);
    renderer->DestroyMesh(sphereMesh);
    rhi->DestroyDevice(device);
    engine.Shutdown();
    return 0;
}
