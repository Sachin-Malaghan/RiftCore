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

#include <EditorUI.h>

#include <glad/glad.h>

#include <iostream>
#include <cmath>
#include <string>

using namespace RiftCore;

int main()
{
    Engine engine;
    EngineConfig config;
    config.appName     = "RiftCore Editor";
    config.logFilePath = "RiftCoreEditor.log";
    config.logLevel    = LogLevel::Info;
    if (engine.Initialize(config).IsErr()) return -1;

    auto* logger  = engine.GetLogger();
    auto* plugins = engine.GetPluginManager();
    ModuleInitParams params;
    params.context = engine.GetContext();

    // ── Load modules ──────────────────────────────────────
    plugins->LoadAndInit("OpenGLBackend",
        "RiftCore_OpenGLBackend.dll", params);
    auto* rhi    = engine.GetContext()->RHI();
    auto  devRes = rhi->CreateDevice(nullptr);
    if (devRes.IsErr()) return -1;
    auto* device = static_cast<GLDevice*>(devRes.Value());

    plugins->LoadAndInit("Input",
        "RiftCore_Input.dll", params);
    auto* inputMod = plugins->GetModuleAs
        <InputModule>("Input");
    InputSystem* input =
        inputMod ? inputMod->GetInputSystem() : nullptr;

    plugins->LoadAndInit("Renderer",
        "RiftCore_Renderer.dll", params);
    auto* rendMod  = plugins->GetModuleAs
        <RendererModule>("Renderer");
    auto* renderer = rendMod->GetRenderSystem();
    if (renderer->Initialize(device,1280,720).IsErr())
        return -1;
    auto* texLoader = renderer->GetTextureLoader();

    plugins->LoadAndInit("Physics",
        "RiftCore_Physics.dll", params);
    auto* physMod = plugins->GetModuleAs
        <PhysicsModule>("Physics");
    auto* physics  = physMod->GetPhysics();

    plugins->LoadAndInit("Scene",
        "RiftCore_Scene.dll", params);
    auto* sceneMod = plugins->GetModuleAs
        <SceneModule>("Scene");
    auto* scene = sceneMod->GetSceneSystem();

    // ── Initialize Editor UI ──────────────────────────────
    EditorUI editorUI;
    if (!editorUI.Initialize(
        device->GetWindow(),
        scene,
        renderer,
        physics,
        logger)) {
        logger->Error("Editor","UI init failed");
        return -1;
    }
    logger->Info("Editor","Editor UI initialized");

    // ── Ground plane ──────────────────────────────────────
    physics->GetWorld()->AddGroundPlane(
        0.0f, 0.6f, 0.6f);

    // ── Load default scene ────────────────────────────────
    std::string scenePath =
        "..\\..\\..\\Assets\\Scenes\\TestLevel.json";
    auto loadResult = scene->LoadScene(scenePath);
    if (loadResult.IsOk()) {
        logger->Info("Editor",
            "Loaded scene: TestLevel.json ("
            + std::to_string(scene->GetNodeCount())
            + " nodes)");
    } else {
        scene->NewScene("NewScene");
        logger->Info("Editor",
            "Starting with empty scene");
    }

    // ── Meshes for rendering ──────────────────────────────
    auto* cubeMesh   = renderer->UploadMesh(
        MeshFactory::CreateCube(1.0f)).Value();
    auto* planeMesh  = renderer->UploadMesh(
        MeshFactory::CreatePlane(30.0f,10)).Value();
    auto* sphereMesh = renderer->UploadMesh(
        MeshFactory::CreateSphere(0.5f,20,20)).Value();

    // ── Textures ──────────────────────────────────────────
    u8 gA[4]={90,130,90,255}, gB[4]={60,90,60,255};
    auto* groundTex = texLoader->CreateCheckerboard(
        "ground",256,32,gA,gB).Value();
    auto* redTex    = texLoader->CreateSolidColor(
        "red",200,80,80).Value();
    auto* blueTex   = texLoader->CreateSolidColor(
        "blue",80,100,220).Value();
    auto* goldTex   = texLoader->CreateSolidColor(
        "gold",220,180,50).Value();
    auto* grayTex   = texLoader->CreateSolidColor(
        "gray",160,160,160).Value();
    auto* greenTex  = texLoader->CreateSolidColor(
        "green",80,200,80).Value();
    auto* checker   = texLoader->GetCheckerTexture();

    auto makeMat = [](Texture2D* tex,
                      f32 met=0.0f, f32 rou=0.5f) {
        MaterialData m;
        m.albedo={1,1,1}; m.metallic=met;
        m.roughness=rou;  m.albedoTex=tex;
        return m;
    };

    MaterialData groundMat = makeMat(groundTex,0,0.9f);
    groundMat.texTileX=8; groundMat.texTileY=8;

    // ── Editor camera ─────────────────────────────────────
    Camera camera;
    camera.SetPosition   ({0, 8, 16});
    camera.SetFOV        (60.0f);
    camera.SetAspectRatio(1280.0f/720.0f);
    camera.SetClipPlanes (0.1f, 500.0f);
    camera.RotatePitch   (-20.0f);

    f32 camYaw   = -90.0f;
    f32 camPitch = -20.0f;

    Light sun;
    sun.type      = LightType::Directional;
    sun.direction = {-0.5f,-1.0f,-0.5f};
    sun.color     = {1,0.95f,0.85f};
    sun.intensity = 1.8f;

    logger->Info("Editor","=================================");
    logger->Info("Editor","EDITOR CONTROLS:");
    logger->Info("Editor","  WASD+QE    = Move camera");
    logger->Info("Editor","  Arrows     = Rotate camera");
    logger->Info("Editor","  W/E/R      = Gizmo mode");
    logger->Info("Editor","  Click      = Select object");
    logger->Info("Editor","  Drag gizmo = Transform");
    logger->Info("Editor","  Delete     = Delete selected");
    logger->Info("Editor","  ESC        = Quit");
    logger->Info("Editor","=================================");

    u32 frameCount  = 0;
    u64 totalFrames = 0;
    const f32 camSpd = 8.0f;
    const f32 rotSpd = 60.0f;
    const f32 dt     = 0.016f;

    while (!device->ShouldClose())
    {
        device->BeginFrame();
        if (input) input->Update();

        editorUI.BeginFrame();

        // ── Keyboard shortcuts ────────────────────────────
        bool captureKB = editorUI.ShouldCaptureKeyboard();
        bool captureMouse = editorUI.ShouldCaptureMouse();

        if (input && !captureKB) {
            if (input->IsKeyDown(Key::Escape)) break;

            // Camera movement (only when not over UI)
            if (!captureMouse) {
                if (input->IsKeyDown(Key::W))
                    camera.MoveForward(-camSpd*dt);
                if (input->IsKeyDown(Key::S))
                    camera.MoveForward( camSpd*dt);
                if (input->IsKeyDown(Key::A))
                    camera.MoveRight(-camSpd*dt);
                if (input->IsKeyDown(Key::D))
                    camera.MoveRight( camSpd*dt);
                if (input->IsKeyDown(Key::Q))
                    camera.MoveUp(-camSpd*dt);
                if (input->IsKeyDown(Key::E))
                    camera.MoveUp( camSpd*dt);
                if (input->IsKeyDown(Key::Left)) {
                    camera.RotateYaw(-rotSpd*dt);
                    camYaw -= rotSpd*dt;
                }
                if (input->IsKeyDown(Key::Right)) {
                    camera.RotateYaw( rotSpd*dt);
                    camYaw += rotSpd*dt;
                }
                if (input->IsKeyDown(Key::Up)) {
                    camera.RotatePitch( rotSpd*dt);
                    camPitch += rotSpd*dt;
                }
                if (input->IsKeyDown(Key::Down)) {
                    camera.RotatePitch(-rotSpd*dt);
                    camPitch -= rotSpd*dt;
                }
            }
        }

        // Physics only runs in play mode
        if (editorUI.GetState().isPlaying &&
            !editorUI.GetState().isPaused) {
            physMod->OnUpdate(dt);
        }

        sceneMod->OnUpdate(dt);

        // ── 3D Render ─────────────────────────────────────
        renderer->BeginFrame(camera);
        renderer->SubmitLight(sun);

        // Ground
        {
            DrawCall dc;
            dc.mesh      = planeMesh;
            dc.material  = groundMat;
            dc.transform = Math::Translate({0,0,0});
            renderer->Submit(dc);
        }

        // Render all scene nodes
        auto sel = editorUI.GetSelection();
        scene->ForEachNode([&](ISceneNode* node) {
            if (!node->IsActive()) return;

            Vec3 pos = node->GetLocalPosition();
            Vec3 rot = node->GetLocalRotation();
            Vec3 scl = node->GetLocalScale();

            auto* sNode = static_cast<SceneNode*>(node);

            // Choose mesh
            GPUMesh* mesh = cubeMesh;
            if (sNode->hasPhysics) {
                auto& shape =
                    sNode->physicsDesc.colliderShape;
                if (shape == "sphere")
                    mesh = sphereMesh;
                else if (shape == "plane")
                    mesh = planeMesh;
            }

            // Choose material/color
            MaterialData mat = makeMat(checker);
            if (sNode->hasMesh) {
                Vec3 alb = sNode->meshDesc.albedo;
                f32  met = sNode->meshDesc.metallic;
                f32  rou = sNode->meshDesc.roughness;
                if (alb.x > 0.6f && alb.y < 0.4f)
                    mat = makeMat(redTex, met, rou);
                else if (alb.z > 0.6f)
                    mat = makeMat(blueTex, met, rou);
                else if (alb.x > 0.6f && alb.y > 0.5f)
                    mat = makeMat(goldTex, met, rou);
                else if (alb.y > 0.5f && alb.x < 0.5f)
                    mat = makeMat(greenTex, met, rou);
                else
                    mat = makeMat(grayTex, met, rou);
            }

            // Skip plane visual (ground is rendered separately)
            if (sNode->hasPhysics &&
                sNode->physicsDesc.colliderShape == "plane")
                return;

            // Scale for box colliders
            Vec3 renderScale = scl;
            if (sNode->hasPhysics &&
                sNode->physicsDesc.colliderShape == "box") {
                auto& he = sNode->physicsDesc.halfExtents;
                renderScale = {he.x*2, he.y*2, he.z*2};
            }

            DrawCall dc;
            dc.mesh      = mesh;
            dc.material  = mat;
            dc.transform = Math::TRSFull(
                pos,
                rot.x, rot.y, rot.z,
                renderScale);

            // Highlight selected
            if (sel.hasNode &&
                sel.nodeID == node->GetID()) {
                dc.material.albedo = {1.5f, 1.5f, 0.6f};
            }

            renderer->Submit(dc);
        });

        renderer->EndFrame();

        // ── Editor UI + Gizmos ────────────────────────────
        editorUI.Render(camera.GetViewMatrix(), camera.GetProjectionMatrix());

        editorUI.EndFrame();

        device->Present();
        frameCount++;
        totalFrames++;

        if (frameCount % 600 == 0) {
            auto si = scene->GetSceneInfo();
            logger->Info("Editor",
                "Frame=" + std::to_string(frameCount) +
                " Scene=" + si.name +
                " Nodes=" + std::to_string(si.nodeCount));
        }
    }

    logger->Info("Editor",
        "Done. Frames: " + std::to_string(totalFrames));

    editorUI.Shutdown();
    renderer->DestroyMesh(cubeMesh);
    renderer->DestroyMesh(planeMesh);
    renderer->DestroyMesh(sphereMesh);
    rhi->DestroyDevice(device);
    engine.Shutdown();
    return 0;
}

