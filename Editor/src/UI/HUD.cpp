/**
 * @file HUD.cpp
 * @brief Production-grade Editor UI System Implementation for RiftCore Engine
 */

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <UI/HUD.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <chrono>
#include <algorithm>

namespace RiftCore {

//=============================================================================
// CONSTRUCTOR / DESTRUCTOR
//=============================================================================

HUD::HUD() = default;

HUD::~HUD() { 
    Shutdown(); 
}

//=============================================================================
// INITIALIZATION
//=============================================================================

bool HUD::Initialize(GLFWwindow* window, const HUDConfig& config) {
    if (!window) {
        std::cerr << "[HUD] Window is null\n";
        return false;
    }

    m_Window = window;
    m_Config = config;
    m_IconsPath = config.IconsPath;
    m_Opacity = config.WindowOpacity;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    
#ifdef IMGUI_HAS_DOCK
    if (config.EnableDocking) {
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    }
#endif
#ifdef IMGUI_HAS_VIEWPORT
    if (config.EnableViewports) {
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    }
#endif
    
    if (!config.EnableKeyboardNav) {
        io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
    }
    
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    
#ifdef IMGUI_HAS_VIEWPORT
    io.ConfigViewportsNoAutoMerge = false;
    io.ConfigViewportsNoTaskBarIcon = false;
#endif

    if (config.DarkTheme) {
        ApplyDarkTheme();
    } else {
        ApplyLightTheme();
    }

    if (!config.FontPath.empty()) {
        m_DefaultFont = io.Fonts->AddFontFromFileTTF(config.FontPath.c_str(), config.FontSize);
    }
    if (!m_DefaultFont) {
        m_DefaultFont = io.Fonts->AddFontDefault();
    }

    if (!ImGui_ImplGlfw_InitForOpenGL(window, true)) {
        std::cerr << "[HUD] GLFW backend init failed\n";
        return false;
    }

    if (!ImGui_ImplOpenGL3_Init("#version 460")) {
        std::cerr << "[HUD] OpenGL3 backend init failed\n";
        return false;
    }

    LoadDefaultIcons();

    m_Initialized = true;
    m_FirstFrame = true;
    
    std::cout << "[HUD] ImGui initialized with docking support. "
              << "Version: " << IMGUI_VERSION << "\n";
    return true;
}

void HUD::Shutdown() {
    if (!m_Initialized) return;

    for (auto& [name, texID] : m_IconCache) {
        if (texID != 0) {
            glDeleteTextures(1, &texID);
        }
    }
    m_IconCache.clear();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    m_Initialized = false;
    m_Window = nullptr;
    
    std::cout << "[HUD] Shutdown complete.\n";
}

//=============================================================================
// THEME APPLICATION
//=============================================================================

void HUD::ApplyDarkTheme() {
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    
    style.WindowRounding    = 4.0f;
    style.ChildRounding     = 4.0f;
    style.FrameRounding     = 4.0f;
    style.PopupRounding     = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding      = 4.0f;
    style.TabRounding       = 4.0f;
    
    style.WindowPadding     = ImVec2(10, 10);
    style.FramePadding      = ImVec2(8, 4);
    style.ItemSpacing       = ImVec2(8, 6);
    style.ItemInnerSpacing  = ImVec2(6, 4);
    style.IndentSpacing     = 20.0f;
    style.ScrollbarSize     = 14.0f;
    style.GrabMinSize       = 12.0f;
    
    style.WindowBorderSize  = 1.0f;
    style.ChildBorderSize   = 1.0f;
    style.PopupBorderSize   = 1.0f;
    style.FrameBorderSize   = 0.0f;
    style.TabBorderSize     = 0.0f;

    auto* colors = style.Colors;
    
    colors[ImGuiCol_WindowBg]           = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_ChildBg]            = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_PopupBg]            = ImVec4(0.12f, 0.12f, 0.14f, 0.98f);
    colors[ImGuiCol_TitleBg]            = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_TitleBgActive]      = ImVec4(0.12f, 0.12f, 0.16f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]   = ImVec4(0.08f, 0.08f, 0.10f, 0.75f);
    colors[ImGuiCol_MenuBarBg]          = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_Border]             = ImVec4(0.20f, 0.20f, 0.25f, 0.80f);
    colors[ImGuiCol_BorderShadow]       = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]            = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]     = ImVec4(0.20f, 0.20f, 0.25f, 1.00f);
    colors[ImGuiCol_FrameBgActive]      = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
    colors[ImGuiCol_Tab]                = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_TabHovered]         = ImVec4(0.25f, 0.45f, 0.75f, 0.80f);
    colors[ImGuiCol_TabActive]          = ImVec4(0.20f, 0.35f, 0.60f, 1.00f);
    colors[ImGuiCol_TabUnfocused]       = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.15f, 0.25f, 0.40f, 1.00f);
    
#ifdef IMGUI_HAS_DOCK
    colors[ImGuiCol_DockingPreview]     = ImVec4(0.25f, 0.50f, 0.85f, 0.70f);
    colors[ImGuiCol_DockingEmptyBg]     = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
#endif
    
    colors[ImGuiCol_Header]             = ImVec4(0.18f, 0.30f, 0.50f, 0.80f);
    colors[ImGuiCol_HeaderHovered]      = ImVec4(0.25f, 0.40f, 0.65f, 0.90f);
    colors[ImGuiCol_HeaderActive]       = ImVec4(0.30f, 0.50f, 0.80f, 1.00f);
    colors[ImGuiCol_Button]             = ImVec4(0.18f, 0.30f, 0.50f, 1.00f);
    colors[ImGuiCol_ButtonHovered]      = ImVec4(0.25f, 0.40f, 0.65f, 1.00f);
    colors[ImGuiCol_ButtonActive]       = ImVec4(0.30f, 0.50f, 0.80f, 1.00f);
    colors[ImGuiCol_SliderGrab]         = ImVec4(0.35f, 0.55f, 0.85f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]   = ImVec4(0.45f, 0.65f, 0.95f, 1.00f);
    colors[ImGuiCol_CheckMark]          = ImVec4(0.45f, 0.70f, 1.00f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]        = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab]      = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.30f, 0.30f, 0.35f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]= ImVec4(0.35f, 0.35f, 0.40f, 1.00f);
    colors[ImGuiCol_Separator]          = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
    colors[ImGuiCol_SeparatorHovered]   = ImVec4(0.35f, 0.55f, 0.85f, 1.00f);
    colors[ImGuiCol_SeparatorActive]    = ImVec4(0.45f, 0.65f, 0.95f, 1.00f);
    colors[ImGuiCol_ResizeGrip]         = ImVec4(0.25f, 0.45f, 0.75f, 0.25f);
    colors[ImGuiCol_ResizeGripHovered]  = ImVec4(0.25f, 0.45f, 0.75f, 0.67f);
    colors[ImGuiCol_ResizeGripActive]   = ImVec4(0.25f, 0.45f, 0.75f, 0.95f);
    colors[ImGuiCol_Text]               = ImVec4(0.92f, 0.92f, 0.95f, 1.00f);
    colors[ImGuiCol_TextDisabled]       = ImVec4(0.50f, 0.50f, 0.55f, 1.00f);
    colors[ImGuiCol_TextSelectedBg]     = ImVec4(0.25f, 0.45f, 0.75f, 0.35f);
    colors[ImGuiCol_NavHighlight]       = ImVec4(0.25f, 0.45f, 0.75f, 1.00f);
}

void HUD::ApplyLightTheme() {
    ImGui::StyleColorsLight();
}

//=============================================================================
// ICON SYSTEM
//=============================================================================

void HUD::LoadDefaultIcons() {
    const std::vector<std::pair<std::string, std::string>> iconFiles = {
        {"Save",     "save.png"},
        {"Open",     "open.png"},
        {"Undo",     "undo.png"},
        {"Redo",     "redo.png"},
        {"Play",     "play.png"},
        {"Pause",    "pause.png"},
        {"Stop",     "stop.png"},
        {"Settings", "settings.png"}
    };

    for (const auto& [name, file] : iconFiles) {
        std::string path = m_IconsPath + file;
        unsigned int texID = LoadIcon(path);
        if (texID != 0) {
            m_IconCache[name] = texID;
        }
    }
}

unsigned int HUD::LoadIcon(const std::string& path) {
    int width, height, channels;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4);

    if (!data) return 0;

    int pixelCount = width * height * 4;
    for (int i = 0; i < pixelCount; i += 4) {
        data[i]     = 255 - data[i];    
        data[i + 1] = 255 - data[i + 1];
        data[i + 2] = 255 - data[i + 2];
    }

    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    stbi_image_free(data);

    return static_cast<unsigned int>(texID);
}

unsigned int HUD::GetIcon(const std::string& name) const {
    auto it = m_IconCache.find(name);
    return (it != m_IconCache.end()) ? it->second : 0;
}

//=============================================================================
// FRAME LIFECYCLE
//=============================================================================

void HUD::BeginFrame() {
    if (!m_Initialized) return;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    UpdateFPS();
}

void HUD::UpdateFPS() {
    ImGuiIO& io = ImGui::GetIO();
    m_DeltaTime = io.DeltaTime;
    m_FPSAccum += io.DeltaTime;
    m_FPSFrames++;
    
    if (m_FPSAccum >= 0.5f) {
        m_CurrentFPS = static_cast<f32>(m_FPSFrames) / m_FPSAccum;
        m_FPSAccum = 0.0f;
        m_FPSFrames = 0;
    }
}

void HUD::OnUIRender() {
    if (!m_Initialized || !m_Visible) return;
    RenderMainDockspace();
}

void HUD::EndFrame() {
    if (!m_Initialized) return;

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

#ifdef IMGUI_HAS_VIEWPORT
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        GLFWwindow* backup_context = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup_context);
    }
#endif

    m_FirstFrame = false;
}

//=============================================================================
// MAIN DOCKSPACE
//=============================================================================

void HUD::RenderMainDockspace() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
#ifdef IMGUI_HAS_VIEWPORT
    ImGui::SetNextWindowViewport(viewport->ID);
#endif
    
    ImGuiWindowFlags windowFlags = 
        ImGuiWindowFlags_MenuBar |
#ifdef IMGUI_HAS_DOCK
        ImGuiWindowFlags_NoDocking |
#endif
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoBackground;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    
    ImGui::Begin("##EditorDockSpace", &m_EditorOpen, windowFlags);
    ImGui::PopStyleVar(3);

#ifdef IMGUI_HAS_DOCK
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
        m_DockspaceID = ImGui::GetID("RiftCoreDockSpace");
        ImGui::DockSpace(m_DockspaceID, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
        
        if (m_FirstFrame) {
            SetupDockspace();
        }
    }
#endif

    RenderMenuBar();
    RenderToolbar();

    ImGui::End();
}

void HUD::SetupDockspace() {
#ifdef IMGUI_HAS_DOCK
    ImGuiID dockspaceID = m_DockspaceID;
    ImGui::DockBuilderRemoveNode(dockspaceID);
    ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace);
    
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::DockBuilderSetNodeSize(dockspaceID, viewport->WorkSize);
    
    ImGuiID dockLeft, dockCenter, dockRight;
    ImGui::DockBuilderSplitNode(dockspaceID, ImGuiDir_Left, 0.20f, &dockLeft, &dockCenter);
    ImGui::DockBuilderSplitNode(dockCenter, ImGuiDir_Right, 0.25f, &dockRight, &dockCenter);
    
    ImGuiID dockBottom;
    ImGui::DockBuilderSplitNode(dockCenter, ImGuiDir_Down, 0.30f, &dockBottom, &dockCenter);
    
    ImGui::DockBuilderDockWindow("Scene Hierarchy", dockLeft);
    ImGui::DockBuilderDockWindow("Viewport", dockCenter);
    ImGui::DockBuilderDockWindow("Details", dockRight);
    ImGui::DockBuilderDockWindow("Developer Console", dockBottom);
    ImGui::DockBuilderDockWindow("Content Browser", dockBottom);
    ImGui::DockBuilderDockWindow("Visual Scripting", dockBottom);
    
    ImGui::DockBuilderFinish(dockspaceID);
#endif
}

//=============================================================================
// MENU BAR
//=============================================================================

void HUD::RenderMenuBar() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene", "Ctrl+N")) {
                m_CommandBuffer.Push(EditorCommandType::NewScene);
                if (m_Callbacks.OnNewScene) m_Callbacks.OnNewScene();
            }
            if (ImGui::MenuItem("Open Scene...", "Ctrl+O")) {
                m_CommandBuffer.Push(EditorCommandType::OpenScene);
                if (m_Callbacks.OnOpenScene) m_Callbacks.OnOpenScene();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
                m_CommandBuffer.Push(EditorCommandType::SaveScene);
                if (m_Callbacks.OnSaveScene) m_Callbacks.OnSaveScene();
            }
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                glfwSetWindowShouldClose(m_Window, GLFW_TRUE);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z")) {
                m_CommandBuffer.Push(EditorCommandType::Undo);
                if (m_Callbacks.OnUndo) m_Callbacks.OnUndo();
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y")) {
                m_CommandBuffer.Push(EditorCommandType::Redo);
                if (m_Callbacks.OnRedo) m_Callbacks.OnRedo();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Window")) {
            ImGui::MenuItem("Viewport", nullptr, &m_ShowViewport);
            ImGui::MenuItem("Hierarchy", nullptr, &m_ShowHierarchy);
            ImGui::MenuItem("Inspector", nullptr, &m_ShowInspector);
            ImGui::MenuItem("Asset Browser", nullptr, &m_ShowAssetBrowser);
            ImGui::MenuItem("Console", nullptr, &m_ShowConsole);
            ImGui::MenuItem("Visual Scripting", nullptr, &m_ShowScripting);
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Layout")) {
                m_FirstFrame = true;
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }
}

//=============================================================================
// TOOLBAR
//=============================================================================

void HUD::RenderToolbar() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14f, 0.14f, 0.16f, 1.0f));

    float toolbarHeight = 48.0f;
    ImGui::BeginChild("##Toolbar", ImVec2(0, toolbarHeight), false, 
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImGui::BeginGroup();
    
    if (IconButton("Save", "Save")) {
        m_CommandBuffer.Push(EditorCommandType::SaveScene);
        if (m_Callbacks.OnSaveScene) m_Callbacks.OnSaveScene();
    }
    ImGui::SameLine();
    if (IconButton("Open", "Open")) {
        m_CommandBuffer.Push(EditorCommandType::OpenScene);
        if (m_Callbacks.OnOpenScene) m_Callbacks.OnOpenScene();
    }

    ImGui::EndGroup();

    float playBtnWidth = 100.0f;
    float availWidth = ImGui::GetWindowWidth();
    ImGui::SameLine(availWidth - playBtnWidth - 130);

    if (!m_IsPlaying) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.13f, 0.54f, 0.28f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.64f, 0.38f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.23f, 0.74f, 0.48f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        
        if (ImGui::Button("> PLAY", ImVec2(playBtnWidth, 36))) {
            m_IsPlaying = true;
            m_CommandBuffer.Push(EditorCommandType::Play);
            if (m_Callbacks.OnPlay) m_Callbacks.OnPlay();
        }
        
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.70f, 0.20f, 0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.80f, 0.30f, 0.30f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.90f, 0.40f, 0.40f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        
        if (ImGui::Button("[] STOP", ImVec2(playBtnWidth, 36))) {
            m_IsPlaying = false;
            m_CommandBuffer.Push(EditorCommandType::Stop);
            if (m_Callbacks.OnStop) m_Callbacks.OnStop();
        }
        
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

bool HUD::IconButton(const char* label, const char* iconKey, float size) {
    ImGui::PushID(label);
    ImGui::BeginGroup();

    unsigned int texID = GetIcon(iconKey);
    bool clicked = false;

    if (texID != 0) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.1f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.2f));

        if (ImGui::ImageButton("##Icon", (ImTextureID)(intptr_t)texID, ImVec2(size, size))) {
            clicked = true;
        }
        ImGui::PopStyleColor(3);
    } else {
        if (ImGui::Button(label, ImVec2(size + 8, size + 8))) {
            clicked = true;
        }
    }

    float textWidth = ImGui::CalcTextSize(label).x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (size + 8 - textWidth) * 0.5f);
    ImGui::TextDisabled("%s", label);

    ImGui::EndGroup();
    ImGui::PopID();

    ImGui::SameLine(0, 8);
    return clicked;
}

//=============================================================================
// LEGACY RENDER MODE (for compatibility)
//=============================================================================

void HUD::Render(const HUDRenderStats& stats, i32 selectedIdx, const std::vector<HUDObjectInfo>& objects, const HUDCameraInfo& camera) {
    if (!m_Initialized || !m_Visible) return;

    DrawTopMenuBar();
    DrawToolBar();

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, m_Opacity);

    DrawMainPanel(stats, selectedIdx, objects, camera);
    DrawSelectedObjectPanel(selectedIdx, objects);
    DrawObjectListPanel(selectedIdx, objects);

    ImGui::PopStyleVar();
    DrawConsolePanel();
}

void HUD::DrawTopMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene", "Ctrl+N")) { if(m_Callbacks.OnNewScene) m_Callbacks.OnNewScene(); }
            if (ImGui::MenuItem("Open Scene", "Ctrl+O")) { if(m_Callbacks.OnOpenScene) m_Callbacks.OnOpenScene(); }
            ImGui::Separator();
            if (ImGui::MenuItem("Save Scene", "Ctrl+S")) { if(m_Callbacks.OnSaveScene) m_Callbacks.OnSaveScene(); }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("HUD Visible", nullptr, &m_Visible);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

void HUD::DrawToolBar() {
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.12f, 0.14f, 1.0f));
    ImGui::SetNextWindowPos(ImVec2(0, 19));
    ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, 34));
    
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse;

    if (ImGui::Begin("##LegacyToolbar", nullptr, flags)) {
        if (!m_IsPlaying) {
            if (ImGui::Button(" > PLAY ")) { 
                m_IsPlaying = true; 
                if (m_Callbacks.OnPlay) m_Callbacks.OnPlay(); 
            }
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button(" [] STOP ")) { 
                m_IsPlaying = false; 
                if (m_Callbacks.OnStop) m_Callbacks.OnStop(); 
            }
            ImGui::PopStyleColor();
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
}

void HUD::DrawConsolePanel() {
    if (!m_ShowConsole) return;

    ImGui::SetNextWindowPos(ImVec2(10, 720), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(600, 150), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Developer Console", &m_ShowConsole)) {
        static char inputBuf[256] = "";
        ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue;

        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Execute Script Command:");
        ImGui::SetNextItemWidth(-1);

        if (ImGui::InputText("##CommandInput", inputBuf, IM_ARRAYSIZE(inputBuf), flags)) {
            if (m_Callbacks.OnExecuteCommand) {
                m_Callbacks.OnExecuteCommand(std::string(inputBuf));
            }
            inputBuf[0] = '\0';
            ImGui::SetKeyboardFocusHere(-1);
        }
    }
    ImGui::End();
}

void HUD::DrawMainPanel(const HUDRenderStats& stats, i32 selectedIdx, const std::vector<HUDObjectInfo>& objects, const HUDCameraInfo& camera) {
    RIFTCORE_UNUSED(selectedIdx);
    
    ImGui::SetNextWindowPos(ImVec2(10, 60), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(320, 0), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(m_Opacity);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse;

    if (ImGui::Begin("RiftCore Engine", nullptr, flags)) {
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "RiftCore Engine v0.1");
        ImGui::SameLine(0, 20);

        ImVec4 fpsColor = m_CurrentFPS >= 55 ? ImVec4(0.3f, 1.0f, 0.3f, 1.0f) :
                          m_CurrentFPS >= 30 ? ImVec4(1.0f, 0.8f, 0.2f, 1.0f) :
                                               ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
        ImGui::TextColored(fpsColor, "FPS: %.1f", m_CurrentFPS);

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.5f, 1.0f), "Render Stats");
        ImGui::Text("Draw Calls : %u", stats.drawCalls);
        ImGui::Text("Triangles  : %u", stats.triangles);
        ImGui::Text("Frame      : %llu", stats.frameIndex);
        ImGui::Text("Objects    : %zu", objects.size());

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.5f, 1.0f), "Camera");
        ImGui::Text("Pos  (%.1f, %.1f, %.1f)", camera.posX, camera.posY, camera.posZ);
        ImGui::Text("Yaw  %.1f  Pitch %.1f", camera.yaw, camera.pitch);

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "TAB=Select  F=Wire  H=HUD");
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "IJKL=Move  Num4/6=RotY  Z/X=Scale");
    }
    ImGui::End();
}

void HUD::DrawSelectedObjectPanel(i32 selectedIdx, const std::vector<HUDObjectInfo>& objects) {
    if (selectedIdx < 0 || selectedIdx >= static_cast<i32>(objects.size())) return;

    const auto& obj = objects[selectedIdx];

    ImGui::SetNextWindowPos(ImVec2(10, 320), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(320, 0), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(m_Opacity);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse;

    std::string title = "Selected: [" + std::to_string(selectedIdx) + "] " + obj.name;

    if (ImGui::Begin(title.c_str(), nullptr, flags)) {
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "%s", obj.name.c_str());

        if (obj.autoRotates) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "[AUTO]");
        }

        ImGui::Spacing();
        ImGui::Separator();

        ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.5f, 1.0f), "POSITION");
        ImGui::Text("X"); ImGui::SameLine(25);
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "% .3f", obj.posX);
        ImGui::SameLine(100); ImGui::Text("Y"); ImGui::SameLine(115);
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "% .3f", obj.posY);
        ImGui::SameLine(190); ImGui::Text("Z"); ImGui::SameLine(205);
        ImGui::TextColored(ImVec4(0.5f, 0.7f, 1.0f, 1.0f), "% .3f", obj.posZ);

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.3f, 1.0f), "ROTATION (degrees)");
        ImGui::Text("X"); ImGui::SameLine(25);
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "% .1f", obj.rotX);
        ImGui::SameLine(100); ImGui::Text("Y"); ImGui::SameLine(115);
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "% .1f", obj.rotY);
        ImGui::SameLine(190); ImGui::Text("Z"); ImGui::SameLine(205);
        ImGui::TextColored(ImVec4(0.5f, 0.7f, 1.0f, 1.0f), "% .1f", obj.rotZ);

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.7f, 0.5f, 1.0f, 1.0f), "SCALE");
        ImGui::Text("X"); ImGui::SameLine(25);
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "%.3f", obj.scaleX);
        ImGui::SameLine(100); ImGui::Text("Y"); ImGui::SameLine(115);
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "%.3f", obj.scaleY);
        ImGui::SameLine(190); ImGui::Text("Z"); ImGui::SameLine(205);
        ImGui::TextColored(ImVec4(0.5f, 0.7f, 1.0f, 1.0f), "%.3f", obj.scaleZ);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.7f, 1.0f), "C=Reset  TAB=Next Object");
    }
    ImGui::End();
}

void HUD::DrawObjectListPanel(i32 selectedIdx, const std::vector<HUDObjectInfo>& objects) {
    ImGui::SetNextWindowPos(ImVec2(10, 530), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(320, 180), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(m_Opacity);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse;

    if (ImGui::Begin("Scene Objects", nullptr, flags)) {
        for (i32 i = 0; i < static_cast<i32>(objects.size()); i++) {
            const auto& obj = objects[i];
            bool isSelected = (i == selectedIdx);

            if (isSelected) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.9f, 0.3f, 1.0f));
                ImGui::Text(">> ");
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.8f, 1.0f));
                ImGui::Text("   ");
            }

            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.5f, 0.7f, 1.0f, 1.0f), "[%d]", i);
            ImGui::SameLine();
            ImGui::Text("%s", obj.name.c_str());
            ImGui::SameLine(200);
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.6f, 1.0f), "(%.1f,%.1f,%.1f)", obj.posX, obj.posY, obj.posZ);

            ImGui::PopStyleColor();
        }
    }
    ImGui::End();
}

void HUD::DrawControlsPanel() { RIFTCORE_UNUSED(this); }
void HUD::DrawStatsPanel(const HUDRenderStats& stats) { RIFTCORE_UNUSED(stats); }
void HUD::DrawCameraPanel(const HUDCameraInfo& cam) { RIFTCORE_UNUSED(cam); }

} // namespace RiftCore
