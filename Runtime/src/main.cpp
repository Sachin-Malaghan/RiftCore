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

#include <iostream>
#include <cmath>
#include <string>
#include <vector>
#include <random>

using namespace RiftCore;

// One-shot key press detector
struct KeyTracker {
    bool wasDown = false;
    bool Pressed(bool isDown) {
        bool p  = isDown && !wasDown;
        wasDown = isDown;
        return p;
    }
};

struct PhysicsObject {
    std::string  name;
    u32          bodyID    = 0;
    GPUMesh*     mesh      = nullptr;
    MaterialData material;
    Vec3         scale     = {1,1,1};
    bool         isStatic  = false;
    bool         isGround  = false;   // skip rendering
    bool         isPlatform= false;   // render differently
};

int main()
{
    Engine engine;
    EngineConfig config;
    config.appName     = "RiftCore Physics Demo";
    config.logFilePath = "RiftCore.log";
    config.logLevel    = LogLevel::Info;
    if (engine.Initialize(config).IsErr()) return -1;

    auto* logger  = engine.GetLogger();
    auto* plugins = engine.GetPluginManager();
    ModuleInitParams params;
    params.context = engine.GetContext();

    // ── Modules ───────────────────────────────────────────
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

    // Physics
    auto physResult = plugins->LoadAndInit(
        "Physics","RiftCore_Physics.dll",params);
    if (physResult.IsErr()) {
        logger->Error("Main","Physics failed: " +
            physResult.Error().message);
        return -1;
    }
    auto* physMod = plugins->GetModuleAs
        <PhysicsModule>("Physics");
    auto* physics  = physMod->GetPhysics();
    logger->Info("Main","Physics loaded OK");

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
    auto* greenTex  = texLoader->CreateSolidColor(
        "green",80,200,80).Value();
    auto* grayTex   = texLoader->CreateSolidColor(
        "gray",160,160,160).Value();
    auto* cyanTex   = texLoader->CreateSolidColor(
        "cyan",80,200,200).Value();

    auto makeMat = [](Texture2D* tex,
                      f32 met=0.0f, f32 rou=0.6f) {
        MaterialData m;
        m.albedo={1,1,1};
        m.metallic=met; m.roughness=rou;
        m.albedoTex=tex;
        return m;
    };

    MaterialData groundMat = makeMat(groundTex,0,0.9f);
    groundMat.texTileX=8; groundMat.texTileY=8;

    // ── Build scene ───────────────────────────────────────
    std::vector<PhysicsObject> physObjects;

    // Ground plane at y=0 (physics only, no visual mesh)
    // Using AddGroundPlane which sets planeOffset=0
    {
        u32 id = physics->GetWorld()->AddGroundPlane(
            0.0f, 0.6f, 0.6f);
        PhysicsObject obj;
        obj.name="Ground"; obj.bodyID=id;
        obj.isStatic=true; obj.isGround=true;
        physObjects.push_back(obj);
    }

    // Platform - uses PLANE collider for reliable collision
    // Visual mesh is a box, but physics uses an infinite plane at y=4
    // This prevents ANY object from falling through
    u32 platformBodyID = 0;
    {
        RigidBodyDesc desc;
        desc.position   = {0,4,0};
        desc.isStatic   = true;
        desc.useGravity = false;
        desc.collider.shape       = ColliderShape::Plane;
        desc.collider.planeNormal = {0, 1, 0};
        desc.collider.planeOffset = 4.0f;  // at y=4
        desc.collider.restitution = 0.5f;
        desc.collider.friction    = 0.5f;
        platformBodyID = physics->GetWorld()->AddBody(desc);

        PhysicsObject obj;
        obj.name="Platform"; obj.bodyID=platformBodyID;
        obj.mesh=cubeMesh;
        obj.scale={6,0.4f,6};
        obj.material=makeMat(grayTex,0.5f,0.3f);
        obj.isStatic=true; obj.isPlatform=true;
        physObjects.push_back(obj);
    }

    std::mt19937 rng(42);
    auto randF = [&](f32 lo, f32 hi) {
        return lo + (hi-lo) *
            static_cast<f32>(rng()) /
            static_cast<f32>(rng.max());
    };

    // 5 spheres
    for (int i = 0; i < 5; i++) {
        RigidBodyDesc desc;
        desc.position = {
            randF(-4,4), randF(6,14), randF(-4,4)};
        desc.mass     = randF(0.5f,3.0f);
        desc.collider.shape       = ColliderShape::Sphere;
        desc.collider.radius      = 0.5f;
        desc.collider.restitution = randF(0.3f,0.8f);
        desc.collider.friction    = 0.5f;
        desc.velocity = {randF(-2,2),0,randF(-2,2)};

        u32 id = physics->GetWorld()->AddBody(desc);
        PhysicsObject obj;
        obj.name    = "Sphere_"+std::to_string(i);
        obj.bodyID  = id;
        obj.mesh    = sphereMesh;
        obj.scale   = {1,1,1};
        obj.material= makeMat(
            (i%2==0)?redTex:blueTex, 0.3f,0.4f);
        physObjects.push_back(obj);
    }

    // 5 boxes
    for (int i = 0; i < 5; i++) {
        f32 s = randF(0.4f,1.2f);
        RigidBodyDesc desc;
        desc.position = {
            randF(-5,5), randF(8,16), randF(-5,5)};
        desc.mass     = s*2.0f;
        desc.collider.shape       = ColliderShape::Box;
        desc.collider.halfExtents = {s,s,s};
        desc.collider.restitution = 0.3f;
        desc.collider.friction    = 0.6f;
        desc.angularVelocity = {
            randF(-2,2), randF(-2,2), randF(-2,2)};

        u32 id = physics->GetWorld()->AddBody(desc);
        PhysicsObject obj;
        obj.name    = "Box_"+std::to_string(i);
        obj.bodyID  = id;
        obj.mesh    = cubeMesh;
        obj.scale   = {s*2,s*2,s*2};
        obj.material= makeMat(
            (i%2==0)?goldTex:greenTex, 0.2f,0.5f);
        physObjects.push_back(obj);
    }

    // Dynamic objects list (for TAB selection)
    // Indices into physObjects that are dynamic
    std::vector<u32> dynamicIndices;
    for (u32 i = 0;
         i < static_cast<u32>(physObjects.size()); i++) {
        if (!physObjects[i].isStatic &&
            !physObjects[i].isGround &&
            !physObjects[i].isPlatform) {
            dynamicIndices.push_back(i);
        }
    }

    i32 selectedDynIdx = 0;  // index into dynamicIndices

    // ── Camera ────────────────────────────────────────────
    Camera camera;
    Vec3 defaultCamPos = {0,10,20};
    f32  defaultYaw    = -90.0f;
    f32  defaultPitch  = -25.0f;

    camera.SetPosition   (defaultCamPos);
    camera.SetFOV        (60.0f);
    camera.SetAspectRatio(1280.0f/720.0f);
    camera.SetClipPlanes (0.1f,200.0f);
    camera.RotatePitch   (defaultPitch);

    f32 camYaw   = defaultYaw;
    f32 camPitch = defaultPitch;

    Light sun;
    sun.type      = LightType::Directional;
    sun.direction = {-0.5f,-1.0f,-0.5f};
    sun.color     = {1,0.95f,0.85f};
    sun.intensity = 1.8f;

    logger->Info("Main","=================================");
    logger->Info("Main","PHYSICS DEMO");
    logger->Info("Main","  WASD+QE   = Move camera");
    logger->Info("Main","  Arrows    = Rotate camera");
    logger->Info("Main","  SPACE     = Drop sphere");
    logger->Info("Main","  B         = Drop box");
    logger->Info("Main","  TAB       = Select next object");
    logger->Info("Main","  G         = Toggle gravity");
    logger->Info("Main","  R         = Reset camera");
    logger->Info("Main","  F         = Wireframe");
    logger->Info("Main","  H         = Toggle HUD");
    logger->Info("Main","  ESC       = Quit");
    logger->Info("Main","=================================");

    logger->Info("Main",
        "Selected: " +
        physObjects[dynamicIndices[
            selectedDynIdx]].name);

    // Key trackers
    KeyTracker spaceKey, bKey, gKey, rKey,
               fKey,    hKey, tabKey;
    bool gravityOn = true;
    bool wireframe = false;
    u32  frameCount  = 0;
    u64  totalFrames = 0;
    u32  dropCount   = 0;

    const f32 camSpd = 8.0f;
    const f32 rotSpd = 60.0f;
    const f32 dt     = 0.016f;

    // ── Main Loop ─────────────────────────────────────────
    while (!device->ShouldClose())
    {
        device->BeginFrame();
        if (input) input->Update();
        hud.BeginFrame();

        if (input && input->IsKeyDown(Key::Escape)) break;

        if (input) {
            // Camera (held)
            if (input->IsKeyDown(Key::W))
                camera.MoveForward(-camSpd*dt);
            if (input->IsKeyDown(Key::S))
                camera.MoveForward( camSpd*dt);
            if (input->IsKeyDown(Key::A))
                camera.MoveRight  (-camSpd*dt);
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

            // TAB — cycle selected dynamic object
            if (tabKey.Pressed(
                    input->IsKeyDown(Key::Tab))) {
                if (!dynamicIndices.empty()) {
                    selectedDynIdx =
                        (selectedDynIdx + 1) %
                        static_cast<i32>(
                            dynamicIndices.size());
                    logger->Info("Main",
                        "Selected: " +
                        physObjects[
                            dynamicIndices[
                                selectedDynIdx]].name);
                }
            }

            // SPACE — drop sphere (once per press)
            if (spaceKey.Pressed(
                    input->IsKeyDown(Key::Space))) {
                Vec3 p = camera.GetPosition();
                RigidBodyDesc desc;
                desc.position = {p.x, p.y+1, p.z-4};
                desc.mass     = 1.0f;
                desc.collider.shape       =
                    ColliderShape::Sphere;
                desc.collider.radius      = 0.5f;
                desc.collider.restitution = 0.7f;
                desc.velocity = {0, 3, -10};

                u32 id = physics->GetWorld()->AddBody(desc);
                u32 objIdx = static_cast<u32>(
                    physObjects.size());

                PhysicsObject obj;
                obj.name    = "Sphere_drop_" +
                    std::to_string(++dropCount);
                obj.bodyID  = id;
                obj.mesh    = sphereMesh;
                obj.scale   = {1,1,1};
                obj.material= makeMat(cyanTex,0.5f,0.3f);
                physObjects.push_back(obj);
                dynamicIndices.push_back(objIdx);

                logger->Info("Main",
                    "Dropped sphere " +
                    std::to_string(dropCount));
            }

            // B — drop box (once per press)
            if (bKey.Pressed(
                    input->IsKeyDown(Key::B))) {
                Vec3 p = camera.GetPosition();
                RigidBodyDesc desc;
                desc.position = {p.x, p.y+1, p.z-4};
                desc.mass     = 2.0f;
                desc.collider.shape       =
                    ColliderShape::Box;
                desc.collider.halfExtents =
                    {0.5f,0.5f,0.5f};
                desc.collider.restitution = 0.3f;
                desc.velocity = {0, 3, -10};
                desc.angularVelocity = {2,3,1};

                u32 id = physics->GetWorld()->AddBody(desc);
                u32 objIdx = static_cast<u32>(
                    physObjects.size());

                PhysicsObject obj;
                obj.name    = "Box_drop_" +
                    std::to_string(++dropCount);
                obj.bodyID  = id;
                obj.mesh    = cubeMesh;
                obj.scale   = {1,1,1};
                obj.material= makeMat(
                    goldTex,0.3f,0.5f);
                physObjects.push_back(obj);
                dynamicIndices.push_back(objIdx);

                logger->Info("Main","Dropped box " +
                    std::to_string(dropCount));
            }

            // G — toggle gravity
            if (gKey.Pressed(
                    input->IsKeyDown(Key::G))) {
                gravityOn = !gravityOn;
                physics->SetGravity(
                    gravityOn ?
                    Vec3{0,-9.81f,0} : Vec3{0,0,0});
                logger->Info("Main",
                    gravityOn ?
                    "Gravity ON  (9.81 m/s^2)" :
                    "Gravity OFF (zero-G mode)");
            }

            // R — reset camera to default position
            if (rKey.Pressed(
                    input->IsKeyDown(Key::R))) {
                // Recreate camera from scratch
                camera = Camera();
                camera.SetPosition   (defaultCamPos);
                camera.SetFOV        (60.0f);
                camera.SetAspectRatio(1280.0f/720.0f);
                camera.SetClipPlanes (0.1f,200.0f);
                camera.RotatePitch   (defaultPitch);
                camYaw   = defaultYaw;
                camPitch = defaultPitch;
                logger->Info("Main","Camera reset to default");
            }

            // F — wireframe
            if (fKey.Pressed(
                    input->IsKeyDown(Key::F))) {
                wireframe = !wireframe;
                renderer->SetWireframe(wireframe);
                logger->Info("Main",
                    wireframe ? "Wireframe ON"
                              : "Wireframe OFF");
            }

            // H — toggle HUD
            if (hKey.Pressed(
                    input->IsKeyDown(Key::H))) {
                hud.ToggleVisible();
            }
        }

        // Step physics
        physMod->OnUpdate(dt);

        // ── Render ────────────────────────────────────────
        renderer->BeginFrame(camera);
        renderer->SubmitLight(sun);

        // Ground plane (visual only)
        {
            DrawCall dc;
            dc.mesh      = planeMesh;
            dc.material  = groundMat;
            dc.transform = Math::Translate({0,0,0});
            renderer->Submit(dc);
        }

        // Platform (use stored platformBodyID)
        {
            auto* body = physics->GetBody(platformBodyID);
            if (body) {
                DrawCall dc;
                dc.mesh      = cubeMesh;
                dc.material  = makeMat(grayTex,0.5f,0.3f);
                dc.transform = Math::TRS(
                    body->GetPosition(), 0, {6,0.4f,6});
                renderer->Submit(dc);
            }
        }

        // All dynamic objects
        i32 dynCount = 0;
        for (u32 dynIdx = 0;
             dynIdx < static_cast<u32>(
                 dynamicIndices.size());
             dynIdx++)
        {
            u32 objIdx = dynamicIndices[dynIdx];
            auto& obj  = physObjects[objIdx];
            if (!obj.mesh) continue;

            auto* body = physics->GetBody(obj.bodyID);
            if (!body) continue;

            Vec3 pos = body->GetPosition();
            Vec3 rot = body->GetRotation();

            DrawCall dc;
            dc.mesh      = obj.mesh;
            dc.material  = obj.material;
            dc.transform = Math::TRSFull(
                pos,
                rot.x * 57.2958f,
                rot.y * 57.2958f,
                rot.z * 57.2958f,
                obj.scale
            );

            // Highlight selected object
            if (dynIdx == static_cast<u32>(
                    selectedDynIdx)) {
                dc.material.albedo = {1.5f,1.5f,0.8f};
            }

            renderer->Submit(dc);
            dynCount++;
        }

        renderer->EndFrame();

        // ── HUD ───────────────────────────────────────────
        auto stats     = renderer->GetStats();
        auto physStats = physics->GetWorld()->GetStats();

        HUDRenderStats hudStats;
        hudStats.drawCalls  = stats.drawCalls;
        hudStats.triangles  = stats.triangles;
        hudStats.frameIndex = totalFrames;

        Vec3 camPos = camera.GetPosition();
        HUDCameraInfo hudCam;
        hudCam.posX=camPos.x; hudCam.posY=camPos.y;
        hudCam.posZ=camPos.z;
        hudCam.yaw=camYaw; hudCam.pitch=camPitch;

        std::vector<HUDObjectInfo> hudObjs;
        for (u32 dynIdx = 0;
             dynIdx < static_cast<u32>(
                 dynamicIndices.size());
             dynIdx++)
        {
            u32 objIdx = dynamicIndices[dynIdx];
            auto& obj  = physObjects[objIdx];
            auto* body = physics->GetBody(obj.bodyID);
            if (!body) continue;

            Vec3 pos = body->GetPosition();
            Vec3 vel = body->GetVelocity();

            HUDObjectInfo info;
            info.index  = static_cast<i32>(dynIdx);
            info.name   = obj.name +
                (body->IsAwake() ? "" : " [sleep]");
            info.posX   = pos.x;
            info.posY   = pos.y;
            info.posZ   = pos.z;
            info.rotX   = vel.x;
            info.rotY   = vel.y;
            info.rotZ   = vel.z;
            info.scaleX = body->GetMass();
            info.scaleY = physStats.stepTimeMs;
            info.scaleZ = static_cast<f32>(
                physStats.activeCount);
            info.isSelected =
                (dynIdx == static_cast<u32>(
                    selectedDynIdx));
            hudObjs.push_back(info);
        }

        hud.Render(hudStats, selectedDynIdx,
                   hudObjs, hudCam);
        hud.EndFrame();

        device->Present();
        frameCount++;
        totalFrames++;

        if (frameCount % 300 == 0) {
            auto& selObj = physObjects[
                dynamicIndices[selectedDynIdx]];
            auto* selBody = physics->GetBody(
                selObj.bodyID);
            if (selBody) {
                Vec3 p = selBody->GetPosition();
                logger->Info("Main",
                    "Selected: " + selObj.name +
                    " pos=(" +
                    std::to_string(
                        static_cast<int>(p.x)) + "," +
                    std::to_string(
                        static_cast<int>(p.y)) + "," +
                    std::to_string(
                        static_cast<int>(p.z)) + ")" +
                    " Bodies=" +
                    std::to_string(physStats.bodyCount) +
                    " Active=" +
                    std::to_string(physStats.activeCount)
                );
            }
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



