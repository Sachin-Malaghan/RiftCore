// RiftCore Runtime - plays a scene without the editor.
//
//   RiftCoreRuntime [--scene file.json] [--script file.py] [--exec "code"]
//                   [--headless] [--frames N]
//
// --headless runs physics + scripting with no window (automation, tests,
// batch simulation); --frames stops after N simulated frames.

#define NOMINMAX
#include <Core/Engine.h>
#include <Core/Logger.h>
#include <Core/PluginManager.h>
#include <OpenGLBackend/GLDevice.h>
#include <Input/InputSystem.h>
#include <Renderer/RenderSystem.h>
#include <Renderer/SceneDrawer.h>
#include <Renderer/Camera.h>
#include <Physics/PhysicsWorld.h>
#include <Scene/SceneSystem.h>
#include <RiftCore/Scripting/IScripting.h>
#include <RiftCore/Common/Paths.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <cstring>
#include <iostream>
#include <string>

using namespace RiftCore;

namespace {

    struct Options {
        std::string scene  = "Assets/Scenes/TestLevel.json";
        std::string script;
        std::string exec;
        bool        headless = false;
        long        frames   = -1;     // -1 = until the window closes
        bool        help     = false;
        bool        realistic = false;
    };

    Options ParseArgs(int argc, char** argv) {
        Options o;
        for (int i = 1; i < argc; i++) {
            std::string a = argv[i];
            auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
            if      (a == "--scene")    o.scene  = next();
            else if (a == "--script")   o.script = next();
            else if (a == "--exec")     o.exec   = next();
            else if (a == "--frames")   o.frames = std::atol(next().c_str());
            else if (a == "--headless") o.headless = true;
            else if (a == "--realistic") o.realistic = true;
            else if (a == "--help" || a == "-h") o.help = true;
        }
        if (o.headless && o.frames < 0) o.frames = 0;
        return o;
    }

    void PrintScriptOutput(IScripting* scripting) {
        if (!scripting) return;
        const char* out = scripting->ConsumeOutput();
        if (out && *out) std::cout << out << std::flush;
    }

} // namespace

int main(int argc, char** argv)
{
    Options opt = ParseArgs(argc, argv);
    if (opt.help) {
        std::cout <<
            "RiftCoreRuntime [--scene file.json] [--script file.py] [--exec \"code\"]\n"
            "                [--headless] [--frames N]\n";
        return 0;
    }

    if (!Paths::EnterProjectRoot()) {
        std::cerr << "Warning: no Assets folder found near the executable.\n";
    }

    Engine engine;
    EngineConfig config;
    config.appName     = "RiftCore Runtime";
    config.logFilePath = "RiftCoreRuntime.log";
    config.logLevel    = LogLevel::Info;
    if (engine.Initialize(config).IsErr()) return 1;

    auto* logger  = engine.GetLogger();
    auto* plugins = engine.GetPluginManager();
    ModuleInitParams params;
    params.context = engine.GetContext();

    // ── Window + renderer (skipped when headless) ───────────
    IRHI*         rhi      = nullptr;
    GLDevice*     device   = nullptr;
    InputSystem*  input    = nullptr;
    RenderSystem* renderer = nullptr;

    if (!opt.headless) {
        if (plugins->LoadAndInit("OpenGLBackend", "RiftCore_OpenGLBackend.dll", params).IsErr()) return 1;
        rhi = engine.GetContext()->RHI();
        auto devRes = rhi ? rhi->CreateDevice(nullptr) : Result<IRHIDevice*>::Err("no RHI");
        if (devRes.IsErr()) {
            logger->Error("Runtime", "Could not create the OpenGL 4.6 window.");
            return 1;
        }
        device = static_cast<GLDevice*>(devRes.Value());
        glfwSetWindowTitle(device->GetWindow(), "RiftCore Runtime");

        if (plugins->LoadAndInit("Input", "RiftCore_Input.dll", params).IsOk()) {
            if (auto* m = plugins->GetModuleAs<InputModule>("Input")) input = m->GetInputSystem();
        }
        if (plugins->LoadAndInit("Renderer", "RiftCore_Renderer.dll", params).IsErr()) return 1;
        renderer = plugins->GetModuleAs<RendererModule>("Renderer")->GetRenderSystem();
        if (renderer->Initialize(device, 1280, 720).IsErr()) return 1;
        gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    }

    // ── Simulation modules ──────────────────────────────────
    if (plugins->LoadAndInit("Physics", "RiftCore_Physics.dll", params).IsErr()) return 1;
    auto* physMod = plugins->GetModuleAs<PhysicsModule>("Physics");

    if (plugins->LoadAndInit("Scene", "RiftCore_Scene.dll", params).IsErr()) return 1;
    auto* sceneMod = plugins->GetModuleAs<SceneModule>("Scene");
    auto* scene    = sceneMod->GetSceneSystem();

    IScripting* scripting = nullptr;
    if (plugins->LoadAndInit("Scripting", "RiftCore_Scripting.dll", params).IsOk()) {
        scripting = plugins->GetModuleAs<IScripting>("Scripting");
    }

    // ── Content ─────────────────────────────────────────────
    int exitCode = 0;
    if (!opt.scene.empty() && scene->LoadScene(opt.scene).IsErr()) {
        logger->Warning("Runtime", "Could not load " + opt.scene + "; starting empty.");
        scene->NewScene("Untitled");
    }
    if (!opt.script.empty() || !opt.exec.empty()) {
        if (!scripting || !scripting->IsAvailable()) {
            logger->Error("Runtime", "Scripting was requested but Python is not available.");
            exitCode = 2;
        } else {
            if (!opt.script.empty() && scripting->LoadScript(opt.script.c_str()).IsErr()) exitCode = 3;
            if (!opt.exec.empty()   && scripting->ExecuteString(opt.exec.c_str()).IsErr()) exitCode = 3;
        }
        PrintScriptOutput(scripting);
    }

    // ── Loop ────────────────────────────────────────────────
    Camera camera;
    camera.SetPosition({ 0, 8, 16 });
    camera.SetFOV(60.0f);
    camera.SetClipPlanes(0.1f, 1000.0f);
    camera.RotatePitch(-20.0f);

    Light sun;
    sun.type      = LightType::Directional;
    sun.direction = { -0.5f, -1.0f, -0.5f };
    sun.color     = { 1.0f, 0.95f, 0.85f };
    sun.intensity = 1.8f;

    {
        std::unique_ptr<SceneDrawer> drawer;
        if (renderer) drawer = std::make_unique<SceneDrawer>(renderer);

        const f32 fixedDt = 1.0f / 60.0f;
        f64 lastTime = device ? glfwGetTime() : 0.0;
        long frame = 0;
        int  lastW = 0, lastH = 0;

        while (true) {
            if (opt.frames >= 0 && frame >= opt.frames) break;
            if (device && device->ShouldClose()) break;

            f32 dt = fixedDt;
            if (device) {
                device->BeginFrame();
                if (input) input->Update();
                f64 now = glfwGetTime();
                dt = static_cast<f32>(now - lastTime);
                lastTime = now;
                if (dt > 0.1f) dt = 0.1f;
            }

            if (scripting) scripting->OnUpdate(dt);
            physMod->OnUpdate(dt);
            sceneMod->OnUpdate(dt);
            PrintScriptOutput(scripting);

            if (device) {
                if (input) {
                    if (input->IsKeyDown(Key::Escape)) break;
                    f32 speed = (input->IsKeyDown(Key::LeftShift) ? 24.0f : 8.0f) * dt;
                    if (input->IsKeyDown(Key::W)) camera.MoveForward( speed);
                    if (input->IsKeyDown(Key::S)) camera.MoveForward(-speed);
                    if (input->IsKeyDown(Key::A)) camera.MoveRight(-speed);
                    if (input->IsKeyDown(Key::D)) camera.MoveRight( speed);
                    if (input->IsKeyDown(Key::Q)) camera.MoveUp(-speed);
                    if (input->IsKeyDown(Key::E)) camera.MoveUp( speed);
                    if (input->IsKeyDown(Key::Left))  camera.RotateYaw(-60.0f * dt);
                    if (input->IsKeyDown(Key::Right)) camera.RotateYaw( 60.0f * dt);
                    if (input->IsKeyDown(Key::Up))    camera.RotatePitch( 60.0f * dt);
                    if (input->IsKeyDown(Key::Down))  camera.RotatePitch(-60.0f * dt);
                }

                int w = 1280, h = 720;
                glfwGetFramebufferSize(device->GetWindow(), &w, &h);
                if (w > 0 && h > 0) {
                    if (w != lastW || h != lastH) {
                        renderer->OnResize(static_cast<u32>(w), static_cast<u32>(h));
                        lastW = w;
                        lastH = h;
                    }
                    camera.SetAspectRatio(static_cast<f32>(w) / static_cast<f32>(h));
                    renderer->SetClearColor({ 0.11f, 0.13f, 0.17f });
                    renderer->SetRealistic(opt.realistic);
                    renderer->BeginFrame(camera);
                    drawer->Draw(scene, sun);
                    renderer->EndFrame();
                }
                device->Present();
            }
            frame++;
        }

        if (opt.headless) {
            std::cout << "[Runtime] Simulated " << frame << " frames, "
                      << scene->GetNodeCount() << " nodes.\n";
        }
    }

    // ── Shutdown ────────────────────────────────────────────
    if (scripting) scripting->Shutdown();
    scene->ClearScene();
    if (rhi && device) rhi->DestroyDevice(device);
    engine.Shutdown();
    return exitCode;
}
