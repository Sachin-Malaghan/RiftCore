#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define GL_CLAMP_TO_EDGE 0x812F
#include <UI/RiftCoreUI.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <filesystem>
#include <GLFW/glfw3.h>



// Automatically link OpenGL library for Windows to prevent LNK unresolved externals
#ifdef _WIN32
#pragma comment(lib, "opengl32.lib")
#endif

namespace RiftCore::UI {

    RiftCoreUI::RiftCoreUI() = default;
    RiftCoreUI::~RiftCoreUI() { Shutdown(); }

    bool RiftCoreUI::Initialize(GLFWwindow* window) {
        m_Window = window;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();

        // Enable Docking and Viewports
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

        ImGui::StyleColorsDark();

        // 1. Load Main Font
        std::string fontPath = "../../../Assets/Fonts/Inter-Medium.ttf";
        if (std::filesystem::exists(fontPath)) {
            io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 16.0f);
        }
        else {
            io.Fonts->AddFontDefault();
        }

        std::filesystem::path exePath = std::filesystem::current_path();

        // Let's try to find the Assets folder by walking up the tree if needed
        std::filesystem::path assetsPath = "";
        std::filesystem::path current = exePath;

        // Check current dir, then parent, then grandparent for an "Assets" folder
        for (int i = 0; i < 5; ++i) {
            if (std::filesystem::exists(current / "Assets")) {
                assetsPath = current / "Assets";
                break;
            }
            current = current.parent_path();
        }

        if (assetsPath.empty()) {
            printf("CRITICAL ERROR: Could not locate Assets folder starting from %s\n", exePath.string().c_str());
        }
        else {
            std::string iconDir = (assetsPath / "Fonts/").string();
            printf("Assets located at: %s\n", iconDir.c_str());

            m_IconCache["Save"] = LoadTexture(iconDir + "save.png");
            m_IconCache["Open"] = LoadTexture(iconDir + "open.png");
            m_IconCache["Undo"] = LoadTexture(iconDir + "undo.png");
            m_IconCache["Redo"] = LoadTexture(iconDir + "redo.png");
            m_IconCache["Play"] = LoadTexture(iconDir + "play.png");
            m_IconCache["Settings"] = LoadTexture(iconDir + "settings.png");
        }

        io.IniFilename = "RiftCore_Layout.ini";

        ImGui_ImplGlfw_InitForOpenGL(m_Window, true);
        ImGui_ImplOpenGL3_Init("#version 460");

        return true;
    }

    void RiftCoreUI::BeginFrame() {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void RiftCoreUI::OnUIRender() {
        RenderMainDockspace();
    }

    void RiftCoreUI::RenderMainDockspace() {
        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

        // Setup the Root Window to cover the entire viewport
        ImGuiWindowFlags window_flags =
            ImGuiWindowFlags_MenuBar |
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNavFocus |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse;

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        ImGui::Begin("RiftCoreMainShell", nullptr, window_flags);
        ImGui::PopStyleVar(3);

        // --- STEP 1: RENDER MENU BAR ---
        RenderMenuBar();

        // --- STEP 2: RENDER TOOLBAR ---
        RenderTopToolbar();

        // --- STEP 3: CALCULATE DOCKSPACE POSITION ---
        float menuBarHeight = ImGui::GetFrameHeight();
        float toolbarHeight = 60.0f; // Matches the Child Window height in RenderTopToolbar
        ImGui::SetCursorPosY(menuBarHeight + toolbarHeight);

        // --- STEP 4: RENDER DOCKSPACE ---
        ImGuiID dockspace_id = ImGui::GetID("RiftCoreMainDock");

        static bool first_time = true;
        if (first_time) {
            first_time = false;
            ImGui::DockBuilderRemoveNode(dockspace_id);
            ImGui::DockBuilderAddNode(dockspace_id, dockspace_flags | ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

            ImGuiID dock_main_id = dockspace_id;
            ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.15f, nullptr, &dock_main_id);
            ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.20f, nullptr, &dock_main_id);
            ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.30f, nullptr, &dock_main_id);

            ImGui::DockBuilderDockWindow("Scene Hierarchy", dock_id_left);
            ImGui::DockBuilderDockWindow("Details", dock_id_right);
            ImGui::DockBuilderDockWindow("Project Browser", dock_id_bottom);
            ImGui::DockBuilderDockWindow("Viewport", dock_main_id);
            ImGui::DockBuilderFinish(dockspace_id);
        }

        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

        ImGui::End(); // End RiftCoreMainShell
    }

    void RiftCoreUI::RenderMenuBar() {
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("New Project", "Ctrl+N")) {}
                if (ImGui::MenuItem("Open Project...", "Ctrl+O")) {}
                ImGui::Separator();
                if (ImGui::MenuItem("Save Project", "Ctrl+S")) {}
                ImGui::Separator();
                if (ImGui::MenuItem("Exit", "Alt+F4")) {
                    if (m_Window) glfwSetWindowShouldClose(m_Window, true);
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("Show Statistics", nullptr, &m_ShowStatsWindow);
                ImGui::MenuItem("Show Content Browser", nullptr, &m_ShowAssetBrowser);
                ImGui::MenuItem("Show Log", nullptr, &m_ShowLogWindow);
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }
    }

    void RiftCoreUI::RenderTopToolbar() {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 5));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12, 0));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.09f, 0.09f, 0.11f, 1.0f));

        ImGui::BeginChild("##MainToolbar", ImVec2(0, 60), false, ImGuiWindowFlags_NoScrollbar);

        ImGui::SetCursorPosX(10);
        ImGui::BeginGroup();

        // IconButton Lambda utilizing the updated ImGui 1.89+ signature
        auto IconButton = [&](const char* iconKey, const char* label) -> bool {
            // 1. Push a unique ID based on the label/iconKey to prevent conflicts
            ImGui::PushID(iconKey);
            ImGui::BeginGroup();

            unsigned int texID = m_IconCache[iconKey];
            bool clicked = false;

            if (texID != 0) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.1f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.2f));

                // Use "##Image" to ensure the button has a unique internal name 
                // within this Pushed ID scope
                if (ImGui::ImageButton("##Image", (ImTextureID)(intptr_t)texID, ImVec2(24, 24))) {
                    clicked = true;
                }
                ImGui::PopStyleColor(3);
            }
            else {
                if (ImGui::Button("??", ImVec2(24, 24))) clicked = true;
            }

            float textWidth = ImGui::CalcTextSize(label).x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (32.0f - textWidth) * 0.5f);
            ImGui::TextDisabled("%s", label);

            ImGui::EndGroup();
            ImGui::PopID(); // 2. Always match PushID with PopID

            ImGui::SameLine(0, 15);
            return clicked;
        };

        if (IconButton("Save", "Save")) m_CommandBuffer.Push({ EditorCommandType::Save, "Save" });
        if (IconButton("Open", "Open")) m_CommandBuffer.Push({ EditorCommandType::Load, "Load" });

        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine(0, 15);

        if (IconButton("Undo", "Undo")) m_CommandBuffer.Push({ EditorCommandType::Undo, "Undo" });
        if (IconButton("Redo", "Redo")) m_CommandBuffer.Push({ EditorCommandType::Redo, "Redo" });

        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine(0, 15);

        if (IconButton("Settings", "Settings")) m_CommandBuffer.Push({ EditorCommandType::Settings, "Settings" });

        ImGui::EndGroup();

        // Run Button (Far Right)
        float runBtnWidth = 100.0f;
        ImGui::SameLine(ImGui::GetWindowWidth() - runBtnWidth - 20);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.13f, 0.54f, 0.28f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

        if (ImGui::Button("RUN", ImVec2(runBtnWidth, 38))) {
            m_CommandBuffer.Push({ EditorCommandType::Play, "Play" });
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
    }

    unsigned int RiftCoreUI::LoadTexture(const std::string& path) {
        int width, height, channels;
        unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4);

        if (!data) {
            printf("Failed to load texture: %s\n", path.c_str());
            return 0;
        }

        // --- INVERSE LOGIC ---
        // Loop through every pixel and invert RGB, but keep Alpha (transparency)
        int pixelCount = width * height * 4;
        for (int i = 0; i < pixelCount; i += 4) {
            data[i] = 255 - data[i];     // Invert Red
            data[i + 1] = 255 - data[i + 1]; // Invert Green
            data[i + 2] = 255 - data[i + 2]; // Invert Blue
            // data[i + 3] is Alpha, we leave that alone!
        }
        // ---------------------

        GLuint texID;
        glGenTextures(1, &texID);
        glBindTexture(GL_TEXTURE_2D, texID);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        stbi_image_free(data);
        return (unsigned int)texID;
    }

    void RiftCoreUI::EndFrame() {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow* backup_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_context);
        }
    }

    void RiftCoreUI::Shutdown() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

} // namespace RiftCore::UI