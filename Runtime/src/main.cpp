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
#include <Renderer/OBJLoader.h>
#include <Renderer/HUD.h>

#include <iostream>
#include <cmath>
#include <string>
#include <vector>

using namespace RiftCore;

struct ObjectTransform {
    Vec3 position = {0, 0, 0};
    Vec3 rotation = {0, 0, 0};
    Vec3 scale    = {1, 1, 1};

    Mat4 GetMatrix() const {
        return Math::TRSFull(
            position,
            rotation.x, rotation.y, rotation.z,
            scale);
    }

    void Reset() {
        position = {0,0,0};
        rotation = {0,0,0};
        scale    = {1,1,1};
    }
};

struct SceneObject {
    std::string     name;
    GPUMesh*        mesh         = nullptr;
    MaterialData    material;
    ObjectTransform transform;
    bool            autoRotate   = false;
    f32             autoRotSpeed = 0.0f;
};

int main()
{
    Engine engine;
    EngineConfig config;
    config.appName     = "RiftCore HUD Demo";
    config.logFilePath = "RiftCore.log";
    config.logLevel    = LogLevel::Info;
    if (engine.Initialize(config).IsErr()) return -1;

    auto* logger  = engine.GetLogger();
    auto* plugins = engine.GetPluginManager();
    ModuleInitParams params;
    params.context = engine.GetContext();

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

    // ── Initialize HUD ────────────────────────────────────
    HUD hud;
    if (!hud.Initialize(device->GetWindow())) {
        logger->Error("Main", "HUD init failed");
    } else {
        logger->Info("Main", "HUD initialized");
    }

    // ── Load meshes ───────────────────────────────────────
    OBJLoader objLoader;
    std::string assetPath = "..\\..\\..\\Assets\\Models\\";

    GPUMesh* houseMesh  = nullptr;
    GPUMesh* pillarMesh = nullptr;
    {
        auto r = objLoader.LoadMesh(
            assetPath + "house.obj");
        if (r.IsOk())
            houseMesh = renderer->UploadMesh(
                r.Value()).Value();
    }
    {
        auto r = objLoader.LoadMesh(
            assetPath + "pillar.obj");
        if (r.IsOk())
            pillarMesh = renderer->UploadMesh(
                r.Value()).Value();
    }

    auto* cubeMesh    = renderer->UploadMesh(
        MeshFactory::CreateCube(1.0f)).Value();
    auto* planeMesh   = renderer->UploadMesh(
        MeshFactory::CreatePlane(30.0f,10)).Value();
    auto* sphereMesh  = renderer->UploadMesh(
        MeshFactory::CreateSphere(0.5f,24,24)).Value();
    auto* pyramidMesh = renderer->UploadMesh(
        MeshFactory::CreatePyramid(1.0f,1.5f)).Value();

    // ── Textures ──────────────────────────────────────────
    u8 gA[4]={90,130,90,255}, gB[4]={60,90,60,255};
    auto* groundTex = texLoader->CreateCheckerboard(
        "ground",256,32,gA,gB).Value();
    auto* wallTex   = texLoader->CreateGrid(
        "wall",256,64,3).Value();
    auto* stoneTex  = texLoader->CreateCheckerboard(
        "stone",256,16,nullptr,nullptr).Value();
    auto* goldTex   = texLoader->CreateSolidColor(
        "gold",220,180,50).Value();
    auto* redTex    = texLoader->CreateSolidColor(
        "red",200,60,60).Value();
    auto* blueTex   = texLoader->CreateSolidColor(
        "blue",60,100,200).Value();
    auto* greenTex  = texLoader->CreateSolidColor(
        "green",60,180,60).Value();
    auto* purpleTex = texLoader->CreateSolidColor(
        "purple",160,60,200).Value();

    auto makeMat = [](Texture2D* tex,
                      f32 met=0.0f, f32 rou=0.6f,
                      f32 tx=1.0f,  f32 ty=1.0f) {
        MaterialData m;
        m.albedo={1,1,1}; m.metallic=met;
        m.roughness=rou;  m.albedoTex=tex;
        m.texTileX=tx;    m.texTileY=ty;
        return m;
    };

    MaterialData groundMat = makeMat(
        groundTex,0,0.9f,8,8);

    // ── Build scene objects ───────────────────────────────
    std::vector<SceneObject> objects;

    auto addObj = [&](const std::string& name,
                      GPUMesh* mesh,
                      MaterialData mat,
                      Vec3 pos,
                      Vec3 rot   = {0,0,0},
                      Vec3 scl   = {1,1,1},
                      bool autoRot = false,
                      f32  rotSpd  = 0) {
        SceneObject o;
        o.name             = name;
        o.mesh             = mesh;
        o.material         = mat;
        o.transform.position = pos;
        o.transform.rotation = rot;
        o.transform.scale    = scl;
        o.autoRotate         = autoRot;
        o.autoRotSpeed       = rotSpd;
        objects.push_back(o);
    };

    addObj("House (OBJ)",
        houseMesh ? houseMesh : cubeMesh,
        makeMat(wallTex),
        {0,0,0});

    addObj("Pillar (OBJ)",
        pillarMesh ? pillarMesh : sphereMesh,
        makeMat(stoneTex,0.1f,0.6f),
        {-5,0,0});

    addObj("Spinning Cube",
        cubeMesh,
        makeMat(redTex,0.3f,0.4f),
        {5,0.5f,0}, {0,0,0}, {1.5f,1.5f,1.5f},
        true, 45.0f);

    addObj("Gold Sphere",
        sphereMesh,
        makeMat(goldTex,0.9f,0.1f),
        {3,1,-4});

    addObj("Blue Cube",
        cubeMesh,
        makeMat(blueTex,0.7f,0.2f),
        {-3,0.5f,-4}, {0,0,0}, {0.8f,0.8f,0.8f});

    addObj("Pyramid",
        pyramidMesh,
        makeMat(greenTex,0,0.8f),
        {0,0,-6});

    addObj("Purple Tower",
        cubeMesh,
        makeMat(purpleTex,0.2f,0.7f),
        {6,0,-3}, {0,0,0}, {0.5f,3.0f,0.5f});

    // ── Camera ────────────────────────────────────────────
    Camera camera;
    camera.SetPosition   ({0,6,16});
    camera.SetFOV        (60.0f);
    camera.SetAspectRatio(1280.0f/720.0f);
    camera.SetClipPlanes (0.1f,200.0f);
    camera.RotatePitch   (-15.0f);

    // Internal camera state tracking for HUD
    f32 camYaw   = -90.0f;
    f32 camPitch = -15.0f;

    Light sun;
    sun.type      = LightType::Directional;
    sun.direction = {-0.6f,-1.0f,-0.4f};
    sun.color     = {1.0f,0.95f,0.85f};
    sun.intensity = 1.8f;

    // ── State ─────────────────────────────────────────────
    i32  selectedIdx = 0;
    bool wireframe   = false;
    bool tabWasDown  = false;
    bool fWasDown    = false;
    bool hWasDown    = false;
    bool cWasDown    = false;
    u32  frameCount  = 0;
    u64  totalFrames = 0;

    const f32 camSpd    = 6.0f;
    const f32 rotSpd    = 60.0f;
    const f32 moveSpd   = 3.0f;
    const f32 scaleSpd  = 0.5f;
    const f32 rotObjSpd = 90.0f;
    const f32 dt        = 0.016f;

    logger->Info("Main", "HUD Demo started");
    logger->Info("Main", "Press H to toggle HUD");

    // ── Main Loop ─────────────────────────────────────────
    while (!device->ShouldClose())
    {
        device->BeginFrame();
        if (input) input->Update();

        // Begin ImGui frame AFTER polling events
        hud.BeginFrame();

        if (input && input->IsKeyDown(Key::Escape)) break;

        SceneObject& sel = objects[selectedIdx];

        if (input) {
            // Camera
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

            // TAB — select next object
            bool tabIsDown = input->IsKeyDown(Key::Tab);
            if (tabIsDown && !tabWasDown) {
                selectedIdx = (selectedIdx + 1) %
                    static_cast<i32>(objects.size());
                logger->Info("Main",
                    "Selected: [" +
                    std::to_string(selectedIdx) + "] " +
                    objects[selectedIdx].name);
            }
            tabWasDown = tabIsDown;

            // Object position
            if (input->IsKeyDown(Key::I))
                sel.transform.position.z -= moveSpd*dt;
            if (input->IsKeyDown(Key::K))
                sel.transform.position.z += moveSpd*dt;
            if (input->IsKeyDown(Key::J))
                sel.transform.position.x -= moveSpd*dt;
            if (input->IsKeyDown(Key::L))
                sel.transform.position.x += moveSpd*dt;
            if (input->IsKeyDown(Key::U))
                sel.transform.position.y += moveSpd*dt;
            if (input->IsKeyDown(Key::O))
                sel.transform.position.y -= moveSpd*dt;

            // Object rotation
            if (input->IsKeyDown(Key::Num4))
                sel.transform.rotation.y -= rotObjSpd*dt;
            if (input->IsKeyDown(Key::Num6))
                sel.transform.rotation.y += rotObjSpd*dt;
            if (input->IsKeyDown(Key::Num8))
                sel.transform.rotation.x -= rotObjSpd*dt;
            if (input->IsKeyDown(Key::Num2))
                sel.transform.rotation.x += rotObjSpd*dt;
            if (input->IsKeyDown(Key::Num7))
                sel.transform.rotation.z -= rotObjSpd*dt;
            if (input->IsKeyDown(Key::Num9))
                sel.transform.rotation.z += rotObjSpd*dt;

            // Object scale
            if (input->IsKeyDown(Key::Z)) {
                f32 s = sel.transform.scale.x - scaleSpd*dt;
                if (s < 0.05f) s = 0.05f;
                sel.transform.scale = {s,s,s};
            }
            if (input->IsKeyDown(Key::X)) {
                f32 s = sel.transform.scale.x + scaleSpd*dt;
                if (s > 8.0f) s = 8.0f;
                sel.transform.scale = {s,s,s};
            }

            // Reset transform
            bool cIsDown = input->IsKeyDown(Key::C);
            if (cIsDown && !cWasDown)
                sel.transform.Reset();
            cWasDown = cIsDown;

            // Wireframe
            bool fIsDown = input->IsKeyDown(Key::F);
            if (fIsDown && !fWasDown) {
                wireframe = !wireframe;
                renderer->SetWireframe(wireframe);
            }
            fWasDown = fIsDown;

            // Toggle HUD
            bool hIsDown = input->IsKeyDown(Key::H);
            if (hIsDown && !hWasDown)
                hud.ToggleVisible();
            hWasDown = hIsDown;
        }

        // Auto-rotate
        for (auto& obj : objects) {
            if (obj.autoRotate) {
                obj.transform.rotation.y +=
                    obj.autoRotSpeed * dt;
                if (obj.transform.rotation.y > 360.0f)
                    obj.transform.rotation.y -= 360.0f;
            }
        }

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

        // Scene objects
        for (i32 i = 0;
             i < static_cast<i32>(objects.size()); i++)
        {
            auto& obj = objects[i];
            if (!obj.mesh) continue;

            DrawCall dc;
            dc.mesh      = obj.mesh;
            dc.material  = obj.material;
            dc.transform = obj.transform.GetMatrix();

            // Selected object: bright yellow tint
            if (i == selectedIdx)
                dc.material.albedo = {1.4f,1.4f,0.8f};

            renderer->Submit(dc);
        }

        renderer->EndFrame();

        // ── HUD render (after 3D, before present) ─────────
        auto stats3D = renderer->GetStats();

        // Build HUD data structs
        HUDRenderStats hudStats;
        hudStats.drawCalls  = stats3D.drawCalls;
        hudStats.triangles  = stats3D.triangles;
        hudStats.frameIndex = totalFrames;

        Vec3 camPos = camera.GetPosition();
        HUDCameraInfo hudCam;
        hudCam.posX  = camPos.x;
        hudCam.posY  = camPos.y;
        hudCam.posZ  = camPos.z;
        hudCam.yaw   = camYaw;
        hudCam.pitch = camPitch;
        hudCam.fov   = 60.0f;

        std::vector<HUDObjectInfo> hudObjs;
        hudObjs.reserve(objects.size());
        for (i32 i = 0;
             i < static_cast<i32>(objects.size()); i++) {
            auto& obj = objects[i];
            HUDObjectInfo info;
            info.index      = i;
            info.name       = obj.name;
            info.posX       = obj.transform.position.x;
            info.posY       = obj.transform.position.y;
            info.posZ       = obj.transform.position.z;
            info.rotX       = obj.transform.rotation.x;
            info.rotY       = obj.transform.rotation.y;
            info.rotZ       = obj.transform.rotation.z;
            info.scaleX     = obj.transform.scale.x;
            info.scaleY     = obj.transform.scale.y;
            info.scaleZ     = obj.transform.scale.z;
            info.isSelected = (i == selectedIdx);
            info.autoRotates= obj.autoRotate;
            hudObjs.push_back(info);
        }

        hud.Render(hudStats, selectedIdx,
                   hudObjs, hudCam);
        hud.EndFrame();

        device->Present();
        frameCount++;
        totalFrames++;
    }

    logger->Info("Main",
        "Done. Frames: " + std::to_string(totalFrames));

    hud.Shutdown();

    if (houseMesh)  renderer->DestroyMesh(houseMesh);
    if (pillarMesh) renderer->DestroyMesh(pillarMesh);
    renderer->DestroyMesh(cubeMesh);
    renderer->DestroyMesh(planeMesh);
    renderer->DestroyMesh(sphereMesh);
    renderer->DestroyMesh(pyramidMesh);

    rhi->DestroyDevice(device);
    engine.Shutdown();
    return 0;
}
