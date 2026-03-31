#include <Renderer/HUD.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <chrono>
#include <string>

namespace RiftCore {

    HUD::HUD()  = default;
    HUD::~HUD() { Shutdown(); }

    bool HUD::Initialize(GLFWwindow* window) {
        if (!window) {
            std::cerr << "[HUD] Window is null\n";
            return false;
        }

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        // Do NOT enable keyboard navigation
        // This allows our game to receive ALL key events
        // ImGui will only capture keys when a widget is focused
        io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
        io.IniFilename  = nullptr;
        // Tell ImGui not to set OS cursor
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
        RIFTCORE_UNUSED(io);

        // Style — dark theme with custom colors
        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding    = 6.0f;
        style.FrameRounding     = 4.0f;
        style.ItemSpacing       = ImVec2(8, 6);
        style.WindowPadding     = ImVec2(10, 10);
        style.FramePadding      = ImVec2(6, 4);
        style.ScrollbarRounding = 4.0f;
        style.GrabRounding      = 4.0f;

        // Custom RiftCore color scheme
        auto* colors = style.Colors;
        colors[ImGuiCol_WindowBg]     = ImVec4(0.08f,0.08f,0.12f,0.88f);
        colors[ImGuiCol_TitleBg]      = ImVec4(0.10f,0.10f,0.25f,1.00f);
        colors[ImGuiCol_TitleBgActive]= ImVec4(0.15f,0.15f,0.40f,1.00f);
        colors[ImGuiCol_Header]       = ImVec4(0.15f,0.25f,0.45f,0.80f);
        colors[ImGuiCol_HeaderHovered]= ImVec4(0.20f,0.35f,0.60f,0.90f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.25f,0.45f,0.75f,1.00f);
        colors[ImGuiCol_FrameBg]      = ImVec4(0.12f,0.12f,0.20f,0.80f);
        colors[ImGuiCol_FrameBgHovered]=ImVec4(0.18f,0.18f,0.30f,0.90f);
        colors[ImGuiCol_SliderGrab]   = ImVec4(0.30f,0.50f,0.90f,1.00f);
        colors[ImGuiCol_CheckMark]    = ImVec4(0.40f,0.70f,1.00f,1.00f);
        colors[ImGuiCol_Button]       = ImVec4(0.15f,0.25f,0.50f,0.80f);
        colors[ImGuiCol_ButtonHovered]= ImVec4(0.25f,0.40f,0.70f,0.90f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.35f,0.55f,0.90f,1.00f);
        colors[ImGuiCol_Separator]    = ImVec4(0.25f,0.25f,0.45f,0.80f);
        colors[ImGuiCol_Text]         = ImVec4(0.90f,0.90f,0.95f,1.00f);
        colors[ImGuiCol_TextDisabled] = ImVec4(0.50f,0.50f,0.60f,1.00f);

        // Setup backends
        if (!ImGui_ImplGlfw_InitForOpenGL(window, true)) {
            std::cerr << "[HUD] GLFW backend init failed\n";
            return false;
        }

        if (!ImGui_ImplOpenGL3_Init("#version 460")) {
            std::cerr << "[HUD] OpenGL3 backend init failed\n";
            return false;
        }

        initialized_ = true;
        std::cout << "[HUD] ImGui initialized. "
                  << "Version: " << IMGUI_VERSION << "\n";
        return true;
    }

    void HUD::Shutdown() {
        if (!initialized_) return;
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        initialized_ = false;
        std::cout << "[HUD] Shutdown complete.\n";
    }

    void HUD::BeginFrame() {
        if (!initialized_) return;
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Track FPS
        ImGuiIO& hudIO = ImGui::GetIO();
        deltaTime_   = hudIO.DeltaTime;
        fpsAccum_   += hudIO.DeltaTime;
        fpsFrames_++;
        if (fpsAccum_ >= 0.5f) {
            currentFPS_ = static_cast<f32>(fpsFrames_)
                         / fpsAccum_;
            fpsAccum_  = 0;
            fpsFrames_ = 0;
        }
    }

    void HUD::EndFrame() {
        if (!initialized_) return;
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(
            ImGui::GetDrawData());
    }

    void HUD::Render(
        const HUDRenderStats&             stats,
        i32                               selectedIdx,
        const std::vector<HUDObjectInfo>& objects,
        const HUDCameraInfo&              camera
    ) {
        if (!initialized_ || !visible_) return;

        ImGui::PushStyleVar(
            ImGuiStyleVar_Alpha, opacity_);

        DrawMainPanel(stats, selectedIdx, objects, camera);
        DrawSelectedObjectPanel(selectedIdx, objects);
        DrawObjectListPanel(selectedIdx, objects);

        ImGui::PopStyleVar();
    }

    void HUD::DrawMainPanel(
        const HUDRenderStats&             stats,
        i32                               selectedIdx,
        const std::vector<HUDObjectInfo>& objects,
        const HUDCameraInfo&              camera
    ) {
        RIFTCORE_UNUSED(selectedIdx);
        // Top-left corner stats bar
        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowPos(
            ImVec2(10, 10), ImGuiCond_Always);
        ImGui::SetNextWindowSize(
            ImVec2(320, 0), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(opacity_);

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoResize         |
            ImGuiWindowFlags_NoMove           |
            ImGuiWindowFlags_NoSavedSettings  |
            ImGuiWindowFlags_NoCollapse;

        if (ImGui::Begin("RiftCore Engine", nullptr, flags))
        {
            // Header bar
            ImGui::TextColored(
                ImVec4(0.4f, 0.8f, 1.0f, 1.0f),
                "RiftCore Engine v0.1");
            ImGui::SameLine(0, 20);

            // FPS color: green=good, yellow=ok, red=bad
            ImVec4 fpsColor =
                currentFPS_ >= 55 ?
                    ImVec4(0.3f,1.0f,0.3f,1.0f) :
                currentFPS_ >= 30 ?
                    ImVec4(1.0f,0.8f,0.2f,1.0f) :
                    ImVec4(1.0f,0.3f,0.3f,1.0f);

            ImGui::TextColored(fpsColor,
                "FPS: %.1f", currentFPS_);

            ImGui::Separator();

            // Render stats
            ImGui::TextColored(
                ImVec4(0.8f,0.8f,0.5f,1.0f),
                "Render Stats");
            ImGui::Text("Draw Calls : %u",
                stats.drawCalls);
            ImGui::Text("Triangles  : %u",
                stats.triangles);
            ImGui::Text("Frame      : %llu",
                stats.frameIndex);
            ImGui::Text("Objects    : %zu",
                objects.size());

            ImGui::Separator();

            // Camera info
            ImGui::TextColored(
                ImVec4(0.8f,0.8f,0.5f,1.0f),
                "Camera");
            ImGui::Text(
                "Pos  (%.1f, %.1f, %.1f)",
                camera.posX, camera.posY, camera.posZ);
            ImGui::Text(
                "Yaw  %.1f  Pitch %.1f",
                camera.yaw, camera.pitch);

            ImGui::Separator();

            // Controls reminder
            ImGui::TextColored(
                ImVec4(0.6f,0.6f,0.6f,1.0f),
                "TAB=Select  F=Wire  H=HUD");
            ImGui::TextColored(
                ImVec4(0.6f,0.6f,0.6f,1.0f),
                "IJKL=Move  Num4/6=RotY  Z/X=Scale");
        }
        ImGui::End();
    }

    void HUD::DrawSelectedObjectPanel(
        i32                               selectedIdx,
        const std::vector<HUDObjectInfo>& objects
    ) {
        if (selectedIdx < 0 ||
            selectedIdx >= static_cast<i32>(objects.size()))
            return;

        const auto& obj = objects[selectedIdx];

        // Position: top-left below main panel
        ImGui::SetNextWindowPos(
            ImVec2(10, 280), ImGuiCond_Always);
        ImGui::SetNextWindowSize(
            ImVec2(320, 0), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(opacity_);

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoResize        |
            ImGuiWindowFlags_NoMove          |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoCollapse;

        std::string title = "Selected: [" +
            std::to_string(selectedIdx) + "] " +
            obj.name;

        if (ImGui::Begin(title.c_str(), nullptr, flags))
        {
            // Object name highlighted
            ImGui::TextColored(
                ImVec4(1.0f, 0.85f, 0.3f, 1.0f),
                "%s", obj.name.c_str());

            if (obj.autoRotates) {
                ImGui::SameLine();
                ImGui::TextColored(
                    ImVec4(0.5f,1.0f,0.5f,1.0f),
                    "[AUTO]");
            }

            ImGui::Spacing();
            ImGui::Separator();

            // Position
            ImGui::TextColored(
                ImVec4(0.5f, 0.9f, 0.5f, 1.0f),
                "POSITION");

            ImGui::Text("X"); ImGui::SameLine(25);
            ImGui::TextColored(
                ImVec4(1.0f,0.5f,0.5f,1.0f),
                "% .3f", obj.posX);
            ImGui::SameLine(100);
            ImGui::Text("Y"); ImGui::SameLine(115);
            ImGui::TextColored(
                ImVec4(0.5f,1.0f,0.5f,1.0f),
                "% .3f", obj.posY);
            ImGui::SameLine(190);
            ImGui::Text("Z"); ImGui::SameLine(205);
            ImGui::TextColored(
                ImVec4(0.5f,0.7f,1.0f,1.0f),
                "% .3f", obj.posZ);

            ImGui::Spacing();

            // Rotation
            ImGui::TextColored(
                ImVec4(0.9f,0.7f,0.3f,1.0f),
                "ROTATION (degrees)");

            ImGui::Text("X"); ImGui::SameLine(25);
            ImGui::TextColored(
                ImVec4(1.0f,0.5f,0.5f,1.0f),
                "% .1f", obj.rotX);
            ImGui::SameLine(100);
            ImGui::Text("Y"); ImGui::SameLine(115);
            ImGui::TextColored(
                ImVec4(0.5f,1.0f,0.5f,1.0f),
                "% .1f", obj.rotY);
            ImGui::SameLine(190);
            ImGui::Text("Z"); ImGui::SameLine(205);
            ImGui::TextColored(
                ImVec4(0.5f,0.7f,1.0f,1.0f),
                "% .1f", obj.rotZ);

            ImGui::Spacing();

            // Scale
            ImGui::TextColored(
                ImVec4(0.7f,0.5f,1.0f,1.0f),
                "SCALE");

            ImGui::Text("X"); ImGui::SameLine(25);
            ImGui::TextColored(
                ImVec4(1.0f,0.5f,0.5f,1.0f),
                "%.3f", obj.scaleX);
            ImGui::SameLine(100);
            ImGui::Text("Y"); ImGui::SameLine(115);
            ImGui::TextColored(
                ImVec4(0.5f,1.0f,0.5f,1.0f),
                "%.3f", obj.scaleY);
            ImGui::SameLine(190);
            ImGui::Text("Z"); ImGui::SameLine(205);
            ImGui::TextColored(
                ImVec4(0.5f,0.7f,1.0f,1.0f),
                "%.3f", obj.scaleZ);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextColored(
                ImVec4(0.5f,0.5f,0.7f,1.0f),
                "C=Reset  TAB=Next Object");
        }
        ImGui::End();
    }

    void HUD::DrawObjectListPanel(
        i32                               selectedIdx,
        const std::vector<HUDObjectInfo>& objects
    ) {
        // Bottom-left: list of all objects
        ImGui::SetNextWindowPos(
            ImVec2(10, 530), ImGuiCond_Always);
        ImGui::SetNextWindowSize(
            ImVec2(320, 180), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(opacity_);

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoResize        |
            ImGuiWindowFlags_NoMove          |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoCollapse;

        if (ImGui::Begin("Scene Objects", nullptr, flags))
        {
            for (i32 i = 0;
                 i < static_cast<i32>(objects.size()); i++)
            {
                const auto& obj = objects[i];
                bool isSelected = (i == selectedIdx);

                if (isSelected) {
                    // Highlight selected row
                    ImGui::PushStyleColor(
                        ImGuiCol_Text,
                        ImVec4(1.0f,0.9f,0.3f,1.0f));
                    ImGui::Text(">> ");
                } else {
                    ImGui::PushStyleColor(
                        ImGuiCol_Text,
                        ImVec4(0.7f,0.7f,0.8f,1.0f));
                    ImGui::Text("  ");
                }

                ImGui::SameLine();

                // Index badge
                ImGui::TextColored(
                    ImVec4(0.5f,0.7f,1.0f,1.0f),
                    "[%d]", i);
                ImGui::SameLine();

                // Name
                ImGui::Text("%s", obj.name.c_str());
                ImGui::SameLine(200);

                // Position summary
                ImGui::TextColored(
                    ImVec4(0.5f,0.5f,0.6f,1.0f),
                    "(%.1f,%.1f,%.1f)",
                    obj.posX, obj.posY, obj.posZ);

                ImGui::PopStyleColor();
            }
        }
        ImGui::End();
    }

} // namespace RiftCore



