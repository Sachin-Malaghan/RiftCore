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
#include <Renderer/HUD.h>
#include <Physics/PhysicsWorld.h>
#include <Scene/SceneSystem.h>

#include <iostream>
#include <cmath>
#include <string>
#include <vector>

using namespace RiftCore;

struct KeyTracker {
    bool wasDown = false;
    bool Pressed(bool isDown) {
        bool p  = isDown && !wasDown;
        wasDown = isDown;
        return p;
    }
};

int main()
{
    Engine engine;
    EngineConfig config;
    config.appName     = "RiftCore Scene System Demo";
    config.logFilePath = "RiftCore.log";
    config.logLevel    = LogLevel::Info;
    if (engine.Initialize(config).IsErr()) return -1;

    auto* logger  = engine.GetLogger();
    auto* plugins = engine.GetPluginManager();
    ModuleInitParams params;
    params.context = engine.GetContext();

    // ── Load all modules ──────────────────────────────────
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
    auto* rendMod = plugins->GetModuleAs
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

    // ── Load Scene DLL ────────────────────────────────────
    logger->Info("Main","Loading Scene module...");
    auto sceneResult = plugins->LoadAndInit(
        "Scene","RiftCore_Scene.dll",params);
    if (sceneResult.IsErr()) {
        logger->Error("Main",
            "Scene failed: " + sceneResult.Error().message);
        return -1;
    }
    auto* sceneMod = plugins->GetModuleAs
        <SceneModule>("Scene");
    auto* scene = sceneMod->GetSceneSystem();
    logger->Info("Main","Scene module loaded OK");

    // ── HUD ───────────────────────────────────────────────
    HUD hud;
    hud.Initialize(device->GetWindow());

    // ── Meshes ────────────────────────────────────────────
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

    auto makeMat = [](Texture2D* tex,
                      f32 met=0.0f, f32 rou=0.5f) {
        MaterialData m;
        m.albedo={1,1,1}; m.metallic=met;
        m.roughness=rou;  m.albedoTex=tex;
        return m;
    };

    MaterialData groundMat = makeMat(groundTex,0,0.9f);
    groundMat.texTileX=8; groundMat.texTileY=8;

    // ── Ground plane (physics only) ───────────────────────
    physics->GetWorld()->AddGroundPlane(0.0f, 0.6f, 0.6f);

    // ── Load scene from JSON ──────────────────────────────
    std::string scenePath =
        "..\\..\\..\\Assets\\Scenes\\TestLevel.json";

    logger->Info("Main","Loading scene: TestLevel.json");
    auto loadResult = scene->LoadScene(scenePath);
    if (loadResult.IsOk()) {
        logger->Info("Main",
            "Scene loaded! Nodes: " +
            std::to_string(scene->GetNodeCount()));
    } else {
        logger->Warning("Main",
            "Scene load failed: " +
            loadResult.Error().message);

        // Fallback: create scene manually
        logger->Info("Main",
            "Creating scene manually...");
        scene->NewScene("ManualScene");

        // Platform
        SceneNodeDesc platDesc;
        platDesc.name     = "Platform";
        platDesc.position = {0, 4, 0};
        platDesc.hasPhysics = true;
        platDesc.physics.isStatic = true;
        platDesc.physics.colliderShape = "box";
        platDesc.physics.halfExtents   = {3,0.3f,3};
        scene->CreateNode(platDesc);

        // Red sphere
        SceneNodeDesc sphDesc;
        sphDesc.name     = "RedSphere";
        sphDesc.position = {-2, 10, 0};
        sphDesc.hasPhysics = true;
        sphDesc.physics.mass = 1.0f;
        sphDesc.physics.colliderShape = "sphere";
        sphDesc.physics.radius = 0.5f;
        sphDesc.physics.restitution = 0.7f;
        scene->CreateNode(sphDesc);

        // Gold box
        SceneNodeDesc boxDesc;
        boxDesc.name     = "GoldBox";
        boxDesc.position = {2, 12, 1};
        boxDesc.hasPhysics = true;
        boxDesc.physics.mass = 2.0f;
        boxDesc.physics.colliderShape = "box";
        boxDesc.physics.halfExtents   = {0.5f,0.5f,0.5f};
        scene->CreateNode(boxDesc);

        logger->Info("Main","Manual scene created");
    }

    // ── Camera ────────────────────────────────────────────
    Camera camera;
    camera.SetPosition   ({0,10,20});
    camera.SetFOV        (60.0f);
    camera.SetAspectRatio(1280.0f/720.0f);
    camera.SetClipPlanes (0.1f,200.0f);
    camera.RotatePitch   (-25.0f);
    f32 camYaw=-90.0f, camPitch=-25.0f;

    Light sun;
    sun.type      = LightType::Directional;
    sun.direction = {-0.5f,-1.0f,-0.5f};
    sun.color     = {1,0.95f,0.85f};
    sun.intensity = 1.8f;

    logger->Info("Main","=================================");
    logger->Info("Main","SCENE SYSTEM DEMO");
    logger->Info("Main","  WASD+QE   = Move camera");
    logger->Info("Main","  Arrows    = Rotate camera");
    logger->Info("Main","  S_KEY     = Save scene");
    logger->Info("Main","  L_KEY     = Load scene");
    logger->Info("Main","  N_KEY     = New scene");
    logger->Info("Main","  F         = Wireframe");
    logger->Info("Main","  H         = Toggle HUD");
    logger->Info("Main","  ESC       = Quit");
    logger->Info("Main","=================================");

    KeyTracker saveKey, loadKey, newKey, fKey, hKey;
    bool wireframe  = false;
    u32  frameCount = 0;
    u64  totalFrames= 0;
    const f32 camSpd = 8.0f;
    const f32 rotSpd = 60.0f;
    const f32 dt     = 0.016f;

    while (!device->ShouldClose())
    {
        device->BeginFrame();
        if (input) input->Update();
        hud.BeginFrame();

        if (input && input->IsKeyDown(Key::Escape)) break;

        if (input) {
            if (input->IsKeyDown(Key::W))
                camera.MoveForward(-camSpd*dt);
            if (input->IsKeyDown(Key::A))
                camera.MoveRight  (-camSpd*dt);
            if (input->IsKeyDown(Key::S) &&
                !saveKey.wasDown)
                camera.MoveForward( camSpd*dt);
            if (input->IsKeyDown(Key::D))
                camera.MoveRight  ( camSpd*dt);
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

            // S = Save scene
            bool sDown = input->IsKeyDown(Key::S);
            if (saveKey.Pressed(sDown)) {
                auto r = scene->SaveScene(scenePath);
                if (r.IsOk()) {
                    logger->Info("Main",
                        "Scene SAVED to: " + scenePath);
                } else {
                    logger->Error("Main",
                        "Save failed: " +
                        r.Error().message);
                }
            }

            // L = Load scene
            if (loadKey.Pressed(
                    input->IsKeyDown(Key::L))) {
                // Re-add ground plane first
                // (ClearScene removes all physics bodies)
                physics->GetWorld()->AddGroundPlane(
                    0.0f, 0.6f, 0.6f);

                auto r = scene->LoadScene(scenePath);
                if (r.IsOk()) {
                    logger->Info("Main",
                        "Scene LOADED. Nodes: " +
                        std::to_string(
                            scene->GetNodeCount()));
                } else {
                    logger->Error("Main",
                        "Load failed: " +
                        r.Error().message);
                }
            }

            // N = New empty scene
            if (newKey.Pressed(
                    input->IsKeyDown(Key::N))) {
                // Re-add ground plane after clear
                physics->GetWorld()->AddGroundPlane(
                    0.0f, 0.6f, 0.6f);
                scene->NewScene("NewScene");
                logger->Info("Main",
                    "New empty scene created");
            }

            // F = wireframe
            if (fKey.Pressed(
                    input->IsKeyDown(Key::F))) {
                wireframe = !wireframe;
                renderer->SetWireframe(wireframe);
            }

            // H = HUD
            if (hKey.Pressed(
                    input->IsKeyDown(Key::H))) {
                hud.ToggleVisible();
            }
        }

        // Update systems
        physMod->OnUpdate(dt);
        sceneMod->OnUpdate(dt);

        // ── Render ────────────────────────────────────────
        renderer->BeginFrame(camera);
        renderer->SubmitLight(sun);

        // Ground (always rendered)
        {
            DrawCall dc;
            dc.mesh      = planeMesh;
            dc.material  = groundMat;
            dc.transform = Math::Translate({0,0,0});
            renderer->Submit(dc);
        }

        // Render all scene nodes
        scene->ForEachNode([&](ISceneNode* node) {
            if (!node->IsActive()) return;

            Vec3 pos = node->GetLocalPosition();
            Vec3 rot = node->GetLocalRotation();
            Vec3 scl = node->GetLocalScale();

            // Get physics position if available
            auto* sNode =
                static_cast<SceneNode*>(node);
            if (sNode->hasPhysics &&
                sNode->GetPhysicsBodyID() == 0) {
                // Get from physics via ECS
                EntityID eid = node->GetEntityID();
                if (eid != 0) {
                    Vec3 physPos =
                        physics->GetVelocity(eid);
                    RIFTCORE_UNUSED(physPos);
                }
            }

            // Choose mesh based on physics shape
            GPUMesh* mesh = cubeMesh;
            MaterialData mat = makeMat(grayTex);

            if (sNode->hasPhysics) {
                String shape =
                    sNode->physicsDesc.colliderShape;
                if (shape == "sphere") {
                    mesh = sphereMesh;
                } else if (shape == "plane") {
                    mesh = planeMesh;
                    mat  = groundMat;
                }
            }

            // Material color from mesh desc
            if (sNode->hasMesh) {
                Vec3 alb = sNode->meshDesc.albedo;
                if (alb.x > 0.7f && alb.y < 0.4f)
                    mat = makeMat(redTex,
                        sNode->meshDesc.metallic,
                        sNode->meshDesc.roughness);
                else if (alb.z > 0.7f)
                    mat = makeMat(blueTex,
                        sNode->meshDesc.metallic,
                        sNode->meshDesc.roughness);
                else if (alb.x > 0.7f && alb.y > 0.5f)
                    mat = makeMat(goldTex,
                        sNode->meshDesc.metallic,
                        sNode->meshDesc.roughness);
                else if (alb.x < 0.5f && alb.y > 0.5f)
                    mat = makeMat(groundTex,0,0.9f);
                else
                    mat = makeMat(grayTex,
                        sNode->meshDesc.metallic,
                        sNode->meshDesc.roughness);
            }

            // Skip infinite ground plane visual
            if (sNode->hasPhysics &&
                sNode->physicsDesc.colliderShape ==
                "plane") return;

            // Platform scaling
            Vec3 renderScale = scl;
            if (sNode->hasPhysics) {
                auto& he =
                    sNode->physicsDesc.halfExtents;
                if (sNode->physicsDesc.colliderShape
                    == "box") {
                    renderScale = {
                        he.x * 2.0f,
                        he.y * 2.0f,
                        he.z * 2.0f
                    };
                }
            }

            DrawCall dc;
            dc.mesh      = mesh;
            dc.material  = mat;
            dc.transform = Math::TRSFull(
                pos,
                rot.x, rot.y, rot.z,
                renderScale);
            renderer->Submit(dc);
        });

        renderer->EndFrame();

        // HUD
        auto stats = renderer->GetStats();
        HUDRenderStats hudStats;
        hudStats.drawCalls  = stats.drawCalls;
        hudStats.triangles  = stats.triangles;
        hudStats.frameIndex = totalFrames;

        Vec3 camPos = camera.GetPosition();
        HUDCameraInfo hudCam;
        hudCam.posX=camPos.x; hudCam.posY=camPos.y;
        hudCam.posZ=camPos.z;
        hudCam.yaw=camYaw; hudCam.pitch=camPitch;

        // Scene nodes as HUD objects
        std::vector<HUDObjectInfo> hudObjs;
        i32 idx = 0;
        scene->ForEachNode([&](ISceneNode* node) {
            HUDObjectInfo info;
            info.index  = idx++;
            info.name   = node->GetName();
            Vec3 p      = node->GetLocalPosition();
            info.posX=p.x; info.posY=p.y;
            info.posZ=p.z;
            hudObjs.push_back(info);
        });

        hud.Render(hudStats, 0, hudObjs, hudCam);
        hud.EndFrame();

        device->Present();
        frameCount++;
        totalFrames++;

        if (frameCount % 300 == 0) {
            auto si = scene->GetSceneInfo();
            logger->Info("Main",
                "Scene: " + si.name +
                " Nodes: " +
                std::to_string(si.nodeCount) +
                " Frame: " +
                std::to_string(frameCount));
        }
    }

    logger->Info("Main",
        "Done. Frames: " + std::to_string(totalFrames));

    hud.Shutdown();
    renderer->DestroyMesh(cubeMesh);
    renderer->DestroyMesh(planeMesh);
    renderer->DestroyMesh(sphereMesh);
    rhi->DestroyDevice(device);
    engine.Shutdown();
    return 0;
}


