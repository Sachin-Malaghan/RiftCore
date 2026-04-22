/**
 * @file ViewportPanel.cpp
 * @brief Production-grade Viewport Panel for RiftCore Engine
 * 
 * This panel provides the main 3D viewport for scene visualization, similar to
 * Unreal Engine's Level Viewport. Includes telemetry overlay, gizmo controls,
 * camera manipulation, and render mode switching.
 * 
 * @author RiftCore Team
 * @version 2.0.0
 * @date 2026-04-21
 * 
 * @note Architecture inspired by Unreal Engine's SLevelViewport
 * 
 * ============================================================================
 * EXTERNAL DEPENDENCIES (TODO: Implement these interfaces)
 * ============================================================================
 * - IRenderTarget: Render target for the viewport
 * - ICamera: Camera system for view manipulation
 * - ISelectionSystem: Object picking and selection
 * - IGizmoSystem: Transform gizmo rendering
 * - IInputSystem: Mouse/keyboard input handling
 * - IRenderStats: Performance statistics
 * ============================================================================
 */

#include <UI/Panels/ViewportPanel.h>
#include <imgui.h>
#include <cmath>
#include <algorithm>

// TODO: Include your engine's rendering and camera headers
// #include <Rendering/RenderTarget.h>
// #include <Camera/CameraController.h>
// #include <Editor/SelectionSystem.h>
// #include <Editor/GizmoSystem.h>

namespace RiftCore::UI {

//=============================================================================
// CONFIGURATION CONSTANTS
//=============================================================================

namespace ViewportConfig {
    /** Minimum viewport size */
    constexpr float MIN_VIEWPORT_SIZE = 100.0f;
    
    /** Camera movement speed multiplier */
    constexpr float CAMERA_SPEED = 10.0f;
    
    /** Camera rotation sensitivity */
    constexpr float CAMERA_SENSITIVITY = 0.003f;
    
    /** Camera zoom speed */
    constexpr float ZOOM_SPEED = 1.0f;
    
    /** Grid fade distance */
    constexpr float GRID_FADE_DISTANCE = 1000.0f;
    
    /** Selection outline thickness */
    constexpr float SELECTION_OUTLINE_THICKNESS = 2.0f;
    
    /** Gizmo size in screen pixels */
    constexpr float GIZMO_SIZE = 75.0f;
}

//=============================================================================
// STATIC STATE (uses types from header)
//=============================================================================

static FViewportState s_State;

//=============================================================================
// FORWARD DECLARATIONS
//=============================================================================

static void DrawToolbar();
static void DrawGizmoModeButtons();
static void DrawTelemetryOverlay();
static void DrawViewportContextMenu();
static void DrawOrientationGizmo(const ImVec2& position, float size);
static void DrawGridOverlay();
static void DrawSelectionOutline();

static void HandleCameraInput();
static void HandleMousePicking(const ImVec2& mousePos);
static void HandleGizmoInteraction();
static void HandleKeyboardShortcuts();

static void UpdateRenderStats();
static ImVec4 GetRenderModeColor(ERenderMode mode);
static const char* GetRenderModeName(ERenderMode mode);
static const char* GetGizmoModeName(EGizmoMode mode);

//=============================================================================
// PUBLIC API IMPLEMENTATION
//=============================================================================

/**
 * @brief Main render function for the Viewport Panel
 * 
 * Called every frame by the UI system. Renders the 3D scene and handles
 * all viewport interaction.
 * 
 * @param sceneTextureID The GPU texture ID of the rendered scene
 * @param viewportSize The size of the rendered texture
 * 
 * Layout:
 * ┌─────────────────────────────────────────────────────────────┐
 * │ [W] [E] [R] [Space] │ Lit ▼ │ Show ▼ │ Realtime │ [...] │
 * ├─────────────────────────────────────────────────────────────┤
 * │                                              ┌─────────┐   │
 * │  FPS: 60.0                                   │   ◎     │   │
 * │  Frame: 16.67ms                              │ X Y Z   │   │
 * │  VRAM: 8.2/16 GB                             └─────────┘   │
 * │                                                            │
 * │                    [3D Scene Content]                      │
 * │                                                            │
 * │                         ═══╋═══                            │
 * │                            ║                               │
 * │                                                            │
 * └─────────────────────────────────────────────────────────────┘
 */
void ViewportPanel::OnUIRender(uint32_t sceneTextureID, const ImVec2& viewportSize) {
    // Re-sync ImGui context every frame — Editor.exe has its own GImGui copy
    // (separate static lib); SetCurrentContext routes all imgui calls to Renderer.dll's context.
    static ImGuiContext* s_Ctx = ImGui::GetCurrentContext();
    if (!s_Ctx) return;
    ImGui::SetCurrentContext(s_Ctx);

    // No padding for viewport to maximize scene area
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    if (s_State.bMaximized) {
        windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize;
    }
    
    bool windowOpen = ImGui::Begin("Viewport", nullptr, windowFlags);
    ImGui::PopStyleVar(); // always pop immediately after Begin, before any return
    
    if (!windowOpen) {
        ImGui::End();
        return;
    }
    
    // Track focus and hover state
    s_State.bIsFocused = ImGui::IsWindowFocused();
    s_State.bIsHovered = ImGui::IsWindowHovered();
    
    // Update viewport size
    ImVec2 contentSize = ImGui::GetContentRegionAvail();
    
    // Reserve space for toolbar if visible
    float toolbarHeight = s_State.bShowToolbar ? 30.0f : 0.0f;
    s_State.ViewportSize = ImVec2(
        std::max(contentSize.x, ViewportConfig::MIN_VIEWPORT_SIZE),
        std::max(contentSize.y - toolbarHeight, ViewportConfig::MIN_VIEWPORT_SIZE)
    );
    
    // Draw toolbar
    if (s_State.bShowToolbar) {
        DrawToolbar();
        ImGui::Separator();
    }
    
    // Store viewport position for mouse picking calculations
    s_State.ViewportPos = ImGui::GetCursorScreenPos();
    
    // Handle keyboard shortcuts
    HandleKeyboardShortcuts();
    
    // === Main Viewport Image ===
    // Flip UVs if necessary (OpenGL vs DirectX)
    ImGui::Image(
        (void*)(intptr_t)sceneTextureID, 
        s_State.ViewportSize, 
        ImVec2(0, 1),  // UV0 (flipped for OpenGL)
        ImVec2(1, 0)   // UV1 (flipped for OpenGL)
    );
    
    // Store image rect for interaction
    ImVec2 imageMin = ImGui::GetItemRectMin();
    ImVec2 imageMax = ImGui::GetItemRectMax();
    
    // Handle viewport interactions when image is hovered
    if (ImGui::IsItemHovered()) {
        // Camera control on right-click drag
        HandleCameraInput();
        
        // Mouse picking on left-click
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !s_State.bDraggingGizmo) {
            ImVec2 mousePos = ImGui::GetMousePos();
            HandleMousePicking(mousePos);
        }
    }
    
    // === Overlays (drawn on top of viewport) ===
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    
    // Telemetry overlay (top-left)
    if (s_State.bShowTelemetry) {
        UpdateRenderStats();
        DrawTelemetryOverlay();
    }
    
    // Orientation gizmo (top-right)
    float gizmoSize = 80.0f;
    ImVec2 gizmoPos = ImVec2(
        imageMax.x - gizmoSize - 10.0f,
        imageMin.y + 10.0f
    );
    DrawOrientationGizmo(gizmoPos, gizmoSize);
    
    // Gizmo mode buttons (left side)
    if (s_State.bShowGizmoControls) {
        DrawGizmoModeButtons();
    }
    
    // === Transform Gizmo ===
    // TODO: Enable once ImGuizmo is fully integrated
    // ImGuizmo::SetOrthographic(...);
    // ImGuizmo::SetDrawlist(drawList);
    // ImGuizmo::SetRect(...);
    HandleGizmoInteraction();
    
    // Context menu on right-click (when not camera controlling)
    if (ImGui::BeginPopupContextItem("ViewportContext")) {
        DrawViewportContextMenu();
        ImGui::EndPopup();
    }
    
    // Drag-drop target for assets
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_DRAG")) {
            uint64_t assetID = *(uint64_t*)payload->Data;
            // TODO: Spawn asset at cursor position
            // Scene::SpawnAssetAtScreenPosition(assetID, ImGui::GetMousePos());
            (void)assetID;
        }
        ImGui::EndDragDropTarget();
    }
    
    ImGui::End();
}

//=============================================================================
// INTERNAL IMPLEMENTATIONS
//=============================================================================

/**
 * @brief Draws the viewport toolbar
 */
static void DrawToolbar() {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
    
    // Gizmo mode buttons
    const char* modeIcons[] = { "W", "E", "R", "T" };
    const char* modeTooltips[] = { "Translate (W)", "Rotate (E)", "Scale (R)", "Bounds (T)" };
    EGizmoMode modes[] = { EGizmoMode::Translate, EGizmoMode::Rotate, EGizmoMode::Scale, EGizmoMode::Bounds };
    
    for (int i = 0; i < 4; ++i) {
        bool isSelected = s_State.GizmoMode == modes[i];
        
        if (isSelected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
        }
        
        if (ImGui::Button(modeIcons[i], ImVec2(25, 0))) {
            s_State.GizmoMode = modes[i];
        }
        
        if (isSelected) {
            ImGui::PopStyleColor();
        }
        
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", modeTooltips[i]);
        }
        
        ImGui::SameLine();
    }
    
    // World/Local space toggle
    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();
    
    const char* spaceLabel = s_State.GizmoSpace == EGizmoSpace::World ? "World" : "Local";
    if (ImGui::Button(spaceLabel, ImVec2(50, 0))) {
        s_State.GizmoSpace = (s_State.GizmoSpace == EGizmoSpace::World) 
            ? EGizmoSpace::Local 
            : EGizmoSpace::World;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Toggle World/Local space (G)");
    }
    
    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();
    
    // Snap toggle
    if (s_State.bGizmoSnap) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.4f, 0.2f, 1.0f));
    }
    if (ImGui::Button("Snap", ImVec2(40, 0))) {
        s_State.bGizmoSnap = !s_State.bGizmoSnap;
    }
    if (s_State.bGizmoSnap) {
        ImGui::PopStyleColor();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Toggle snapping (hold Ctrl while dragging)");
    }
    
    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();
    
    // Separator
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    
    // Render mode dropdown
    if (ImGui::BeginCombo("##RenderMode", GetRenderModeName(s_State.RenderMode), ImGuiComboFlags_WidthFitPreview)) {
        for (int i = 0; i < static_cast<int>(ERenderMode::COUNT); ++i) {
            ERenderMode mode = static_cast<ERenderMode>(i);
            bool isSelected = s_State.RenderMode == mode;
            
            if (ImGui::Selectable(GetRenderModeName(mode), isSelected)) {
                s_State.RenderMode = mode;
                // TODO: Apply render mode
                // Renderer::SetViewMode(mode);
            }
            
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    
    ImGui::SameLine();
    
    // Show flags dropdown
    if (ImGui::BeginCombo("##ShowFlags", "Show", ImGuiComboFlags_WidthFitPreview)) {
        struct ShowFlagItem { const char* name; EShowFlag flag; };
        ShowFlagItem items[] = {
            { "Grid", EShowFlag::Grid },
            { "Wireframe", EShowFlag::Wireframe },
            { "Bounds", EShowFlag::Bounds },
            { "Colliders", EShowFlag::Colliders },
            { "Lights", EShowFlag::Lights },
            { "Cameras", EShowFlag::Cameras },
            { "Particles", EShowFlag::Particles },
            { "Shadows", EShowFlag::Shadows },
            { "Post Processing", EShowFlag::PostProcess },
            { "Icons", EShowFlag::Icons },
            { "Statistics", EShowFlag::Stats }
        };
        
        for (const auto& item : items) {
            bool enabled = (s_State.ShowFlags & static_cast<uint32_t>(item.flag)) != 0;
            if (ImGui::Checkbox(item.name, &enabled)) {
                if (enabled) {
                    s_State.ShowFlags |= static_cast<uint32_t>(item.flag);
                } else {
                    s_State.ShowFlags &= ~static_cast<uint32_t>(item.flag);
                }
                // TODO: Apply show flags to renderer
            }
        }
        ImGui::EndCombo();
    }
    
    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();
    
    // Realtime toggle
    ImGui::PushStyleColor(ImGuiCol_Button, s_State.bRealtime 
        ? ImVec4(0.2f, 0.6f, 0.2f, 1.0f) 
        : ImVec4(0.4f, 0.2f, 0.2f, 1.0f));
    if (ImGui::Button(s_State.bRealtime ? "Realtime" : "Paused", ImVec2(70, 0))) {
        s_State.bRealtime = !s_State.bRealtime;
    }
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Toggle realtime rendering");
    }
    
    // Right-aligned camera info
    ImGui::SameLine(ImGui::GetWindowWidth() - 200.0f);
    
    // Camera mode
    const char* cameraModes[] = { "Perspective", "Ortho", "Top", "Bottom", "Left", "Right", "Front", "Back" };
    if (ImGui::BeginCombo("##CameraMode", cameraModes[static_cast<int>(s_State.Camera.Mode)], ImGuiComboFlags_WidthFitPreview)) {
        for (int i = 0; i < 8; ++i) {
            bool isSelected = static_cast<int>(s_State.Camera.Mode) == i;
            if (ImGui::Selectable(cameraModes[i], isSelected)) {
                s_State.Camera.Mode = static_cast<ECameraMode>(i);
                // TODO: Apply camera mode
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    
    ImGui::SameLine();
    
    // Maximize button
    const char* maxIcon = s_State.bMaximized ? "[-]" : "[+]";
    if (ImGui::Button(maxIcon, ImVec2(25, 0))) {
        s_State.bMaximized = !s_State.bMaximized;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(s_State.bMaximized ? "Restore" : "Maximize");
    }
    
    ImGui::PopStyleVar(2);
}

/**
 * @brief Draws gizmo mode selection buttons on the left side
 */
static void DrawGizmoModeButtons() {
    ImVec2 pos = ImVec2(s_State.ViewportPos.x + 10.0f, s_State.ViewportPos.y + 50.0f);
    
    ImGui::SetCursorScreenPos(pos);
    
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.1f, 0.1f, 0.1f, 0.7f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
    
    ImGui::BeginChild("GizmoButtons", ImVec2(35, 130), true, ImGuiWindowFlags_NoScrollbar);
    
    const char* icons[] = { "T", "R", "S", "-" };
    const char* tooltips[] = { "Translate", "Rotate", "Scale", "None" };
    EGizmoMode modes[] = { EGizmoMode::Translate, EGizmoMode::Rotate, EGizmoMode::Scale, EGizmoMode::Bounds };
    
    for (int i = 0; i < 4; ++i) {
        bool isSelected = s_State.GizmoMode == modes[i];
        
        ImVec4 btnColor = isSelected 
            ? ImVec4(0.3f, 0.5f, 0.8f, 1.0f) 
            : ImVec4(0.2f, 0.2f, 0.2f, 0.8f);
        
        ImGui::PushStyleColor(ImGuiCol_Button, btnColor);
        
        if (ImGui::Button(icons[i], ImVec2(25, 25))) {
            s_State.GizmoMode = modes[i];
        }
        
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", tooltips[i]);
        }
        
        ImGui::PopStyleColor();
    }
    
    ImGui::EndChild();
    
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

/**
 * @brief Draws the telemetry/stats overlay
 */
static void DrawTelemetryOverlay() {
    ImVec2 pos = ImVec2(s_State.ViewportPos.x + 10.0f, s_State.ViewportPos.y + 10.0f);
    
    ImGui::SetCursorScreenPos(pos);
    
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
    
    ImGui::BeginChild("Telemetry", ImVec2(160, 100), true, 
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs);
    
    // FPS with color coding
    ImVec4 fpsColor = ImVec4(0.3f, 0.9f, 0.3f, 1.0f); // Green
    if (s_State.Stats.FPS < 30.0f) {
        fpsColor = ImVec4(0.9f, 0.3f, 0.3f, 1.0f); // Red
    } else if (s_State.Stats.FPS < 60.0f) {
        fpsColor = ImVec4(0.9f, 0.9f, 0.3f, 1.0f); // Yellow
    }
    
    ImGui::TextColored(fpsColor, "FPS: %.1f", s_State.Stats.FPS);
    ImGui::TextColored(Colors::AccentCyan, "Frame: %.2f ms", s_State.Stats.FrameTimeMs);
    ImGui::TextColored(Colors::AccentCyan, "GPU: %.2f ms", s_State.Stats.GPUTimeMs);
    
    ImGui::Separator();
    
    ImGui::TextColored(Colors::AccentCyan, "VRAM: %llu / %llu MB", 
        (unsigned long long)s_State.Stats.VRAMUsedMB, 
        (unsigned long long)s_State.Stats.VRAMTotalMB);
    ImGui::TextDisabled("Draw Calls: %u", s_State.Stats.DrawCalls);
    ImGui::TextDisabled("Tris: %uk", s_State.Stats.Triangles / 1000);
    
    ImGui::EndChild();
    
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

/**
 * @brief Draws the 3D orientation gizmo (view cube alternative)
 * 
 * @param position Screen position to draw at
 * @param size Size of the gizmo
 */
static void DrawOrientationGizmo(const ImVec2& position, float size) {
    // TODO: This is a simplified placeholder
    // In production, use ImGuizmo::ViewManipulate or a custom 3D view cube
    
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    
    ImVec2 center = ImVec2(position.x + size * 0.5f, position.y + size * 0.5f);
    float axisLength = size * 0.4f;
    
    // Background circle
    drawList->AddCircleFilled(center, size * 0.5f, IM_COL32(30, 30, 30, 180));
    drawList->AddCircle(center, size * 0.5f, IM_COL32(60, 60, 60, 255), 32, 2.0f);
    
    // Simplified axis display (would need proper rotation from camera)
    // X axis (Red)
    drawList->AddLine(center, ImVec2(center.x + axisLength, center.y), IM_COL32(255, 100, 100, 255), 2.0f);
    drawList->AddText(ImVec2(center.x + axisLength + 2, center.y - 7), IM_COL32(255, 100, 100, 255), "X");
    
    // Y axis (Green)
    drawList->AddLine(center, ImVec2(center.x, center.y - axisLength), IM_COL32(100, 255, 100, 255), 2.0f);
    drawList->AddText(ImVec2(center.x - 4, center.y - axisLength - 14), IM_COL32(100, 255, 100, 255), "Y");
    
    // Z axis (Blue) - coming toward viewer
    drawList->AddCircleFilled(center, 4.0f, IM_COL32(100, 100, 255, 255));
    
    // Clickable areas for quick view changes would go here
    // TODO: Implement click handling for view snapping
}

/**
 * @brief Draws the viewport context menu
 */
static void DrawViewportContextMenu() {
    if (ImGui::BeginMenu("Create")) {
        if (ImGui::MenuItem("Empty Object")) { /* TODO */ }
        ImGui::Separator();
        if (ImGui::BeginMenu("3D Object")) {
            if (ImGui::MenuItem("Cube")) { /* TODO */ }
            if (ImGui::MenuItem("Sphere")) { /* TODO */ }
            if (ImGui::MenuItem("Plane")) { /* TODO */ }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Light")) {
            if (ImGui::MenuItem("Directional")) { /* TODO */ }
            if (ImGui::MenuItem("Point")) { /* TODO */ }
            if (ImGui::MenuItem("Spot")) { /* TODO */ }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Camera")) { /* TODO */ }
        ImGui::EndMenu();
    }
    
    ImGui::Separator();
    
    if (ImGui::MenuItem("Paste", "Ctrl+V")) { /* TODO */ }
    
    ImGui::Separator();
    
    if (ImGui::MenuItem("Focus Selection", "F")) {
        // TODO: Focus camera on selection
    }
    
    if (ImGui::MenuItem("Frame All", "Home")) {
        // TODO: Frame all objects
    }
    
    ImGui::Separator();
    
    if (ImGui::BeginMenu("Bookmark View")) {
        for (int i = 1; i <= 4; ++i) {
            char label[32];
            snprintf(label, sizeof(label), "Save View %d (Ctrl+%d)", i, i);
            if (ImGui::MenuItem(label)) {
                // TODO: Save camera bookmark
            }
        }
        ImGui::EndMenu();
    }
}

/**
 * @brief Handles camera input (WASD, mouse look, etc.)
 */
static void HandleCameraInput() {
    ImGuiIO& io = ImGui::GetIO();
    
    // Right mouse button for camera control
    if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
        if (!s_State.bCameraControlActive) {
            s_State.bCameraControlActive = true;
            s_State.LastMouseX = io.MousePos.x;
            s_State.LastMouseY = io.MousePos.y;
        }
        
        // Mouse delta for rotation
        float deltaX = io.MousePos.x - s_State.LastMouseX;
        float deltaY = io.MousePos.y - s_State.LastMouseY;
        s_State.LastMouseX = io.MousePos.x;
        s_State.LastMouseY = io.MousePos.y;
        
        // Apply rotation
        s_State.Camera.Rotation[1] += deltaX * ViewportConfig::CAMERA_SENSITIVITY * 100.0f;
        s_State.Camera.Rotation[0] += deltaY * ViewportConfig::CAMERA_SENSITIVITY * 100.0f;
        
        // Clamp pitch
        s_State.Camera.Rotation[0] = std::clamp(s_State.Camera.Rotation[0], -89.0f, 89.0f);
        
        // WASD movement
        float speed = ViewportConfig::CAMERA_SPEED * s_State.Camera.MoveSpeed * io.DeltaTime;
        if (io.KeyShift) speed *= 2.0f;
        
        // Calculate forward/right vectors (simplified, assumes Y-up)
        float yaw = s_State.Camera.Rotation[1] * 0.0174533f; // deg to rad
        float forward[3] = { sinf(yaw), 0.0f, cosf(yaw) };
        float right[3] = { cosf(yaw), 0.0f, -sinf(yaw) };
        
        if (ImGui::IsKeyDown(ImGuiKey_W)) {
            s_State.Camera.Position[0] += forward[0] * speed;
            s_State.Camera.Position[2] += forward[2] * speed;
        }
        if (ImGui::IsKeyDown(ImGuiKey_S)) {
            s_State.Camera.Position[0] -= forward[0] * speed;
            s_State.Camera.Position[2] -= forward[2] * speed;
        }
        if (ImGui::IsKeyDown(ImGuiKey_D)) {
            s_State.Camera.Position[0] += right[0] * speed;
            s_State.Camera.Position[2] += right[2] * speed;
        }
        if (ImGui::IsKeyDown(ImGuiKey_A)) {
            s_State.Camera.Position[0] -= right[0] * speed;
            s_State.Camera.Position[2] -= right[2] * speed;
        }
        if (ImGui::IsKeyDown(ImGuiKey_E) || ImGui::IsKeyDown(ImGuiKey_Space)) {
            s_State.Camera.Position[1] += speed;
        }
        if (ImGui::IsKeyDown(ImGuiKey_Q)) {
            s_State.Camera.Position[1] -= speed;
        }
        
        // TODO: Apply camera transform to actual camera
        // Camera::SetTransform(s_State.Camera.Position, s_State.Camera.Rotation);
    } else {
        s_State.bCameraControlActive = false;
    }
    
    // Mouse wheel for zoom/speed
    if (std::abs(io.MouseWheel) > 0.01f) {
        if (s_State.bCameraControlActive) {
            // Adjust camera speed
            s_State.Camera.MoveSpeed *= (1.0f + io.MouseWheel * 0.1f);
            s_State.Camera.MoveSpeed = std::clamp(s_State.Camera.MoveSpeed, 0.1f, 10.0f);
        } else {
            // Zoom camera
            float zoomDelta = io.MouseWheel * ViewportConfig::ZOOM_SPEED;
            float yaw = s_State.Camera.Rotation[1] * 0.0174533f;
            float pitch = s_State.Camera.Rotation[0] * 0.0174533f;
            
            s_State.Camera.Position[0] += sinf(yaw) * cosf(pitch) * zoomDelta;
            s_State.Camera.Position[1] += sinf(pitch) * zoomDelta;
            s_State.Camera.Position[2] += cosf(yaw) * cosf(pitch) * zoomDelta;
        }
    }
}

/**
 * @brief Handles mouse picking for object selection
 * 
 * @param mousePos Screen position of mouse click
 */
static void HandleMousePicking(const ImVec2& mousePos) {
    // Convert screen position to viewport-relative position
    ImVec2 localPos = ImVec2(
        mousePos.x - s_State.ViewportPos.x,
        mousePos.y - s_State.ViewportPos.y
    );
    
    // Normalize to 0-1 range
    ImVec2 normalizedPos = ImVec2(
        localPos.x / s_State.ViewportSize.x,
        localPos.y / s_State.ViewportSize.y
    );
    
    // TODO: Perform ray cast from camera through this screen point
    // Ray ray = Camera::ScreenToWorldRay(normalizedPos);
    // RaycastResult hit = Physics::Raycast(ray);
    // if (hit.bHit) {
    //     SelectionSystem::Select(hit.EntityID, !ImGui::GetIO().KeyCtrl);
    // }
    
    (void)normalizedPos;
}

/**
 * @brief Handles transform gizmo interaction
 */
static void HandleGizmoInteraction() {
    // TODO: Get selected object transform
    // if (!SelectionSystem::HasSelection()) return;
    // 
    // Transform selectedTransform = SelectionSystem::GetSelectedTransform();
    // float* viewMatrix = Camera::GetViewMatrix();
    // float* projMatrix = Camera::GetProjectionMatrix();
    // float* objectMatrix = selectedTransform.GetMatrix();
    // 
    // ImGuizmo::OPERATION operation;
    // switch (s_State.GizmoMode) {
    //     case EGizmoMode::Translate: operation = ImGuizmo::TRANSLATE; break;
    //     case EGizmoMode::Rotate: operation = ImGuizmo::ROTATE; break;
    //     case EGizmoMode::Scale: operation = ImGuizmo::SCALE; break;
    //     default: operation = ImGuizmo::TRANSLATE; break;
    // }
    // 
    // ImGuizmo::MODE mode = (s_State.GizmoSpace == EGizmoSpace::World) 
    //     ? ImGuizmo::WORLD 
    //     : ImGuizmo::LOCAL;
    // 
    // float* snap = nullptr;
    // float snapValues[3] = { s_State.TranslateSnap, s_State.TranslateSnap, s_State.TranslateSnap };
    // if (s_State.bGizmoSnap || ImGui::GetIO().KeyCtrl) {
    //     snap = snapValues;
    // }
    // 
    // ImGuizmo::Manipulate(viewMatrix, projMatrix, operation, mode, objectMatrix, nullptr, snap);
    // 
    // if (ImGuizmo::IsUsing()) {
    //     s_State.bDraggingGizmo = true;
    //     // Apply transform
    //     SelectionSystem::SetSelectedTransform(Transform::FromMatrix(objectMatrix));
    // } else {
    //     s_State.bDraggingGizmo = false;
    // }
    
    // TODO: s_State.bDraggingGizmo = ImGuizmo::IsUsing(); // re-enable when gizmo is integrated
    s_State.bDraggingGizmo = false;
}

/**
 * @brief Handles keyboard shortcuts for the viewport
 */
static void HandleKeyboardShortcuts() {
    if (!s_State.bIsFocused) return;
    
    // Don't process shortcuts when camera is active
    if (s_State.bCameraControlActive) return;
    
    // W - Translate mode
    if (ImGui::IsKeyPressed(ImGuiKey_W) && !ImGui::GetIO().KeyCtrl) {
        s_State.GizmoMode = EGizmoMode::Translate;
    }
    
    // E - Rotate mode
    if (ImGui::IsKeyPressed(ImGuiKey_E) && !ImGui::GetIO().KeyCtrl) {
        s_State.GizmoMode = EGizmoMode::Rotate;
    }
    
    // R - Scale mode
    if (ImGui::IsKeyPressed(ImGuiKey_R) && !ImGui::GetIO().KeyCtrl) {
        s_State.GizmoMode = EGizmoMode::Scale;
    }
    
    // G - Toggle world/local
    if (ImGui::IsKeyPressed(ImGuiKey_G)) {
        s_State.GizmoSpace = (s_State.GizmoSpace == EGizmoSpace::World) 
            ? EGizmoSpace::Local 
            : EGizmoSpace::World;
    }
    
    // F - Focus on selection
    if (ImGui::IsKeyPressed(ImGuiKey_F)) {
        // TODO: FocusOnSelection();
    }
    
    // Home - Frame all
    if (ImGui::IsKeyPressed(ImGuiKey_Home)) {
        // TODO: FrameAll();
    }
    
    // Delete - Delete selection
    if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        // TODO: DeleteSelection();
    }
}

/**
 * @brief Updates render statistics
 */
static void UpdateRenderStats() {
    // Get real FPS from ImGui
    s_State.Stats.FPS = ImGui::GetIO().Framerate;
    s_State.Stats.FrameTimeMs = 1000.0f / s_State.Stats.FPS;
    
    // TODO: Get actual stats from renderer
    // s_State.Stats.GPUTimeMs = Renderer::GetGPUTime();
    // s_State.Stats.DrawCalls = Renderer::GetDrawCallCount();
    // s_State.Stats.Triangles = Renderer::GetTriangleCount();
    // s_State.Stats.VRAMUsedMB = Renderer::GetVRAMUsage() / (1024 * 1024);
    // s_State.Stats.VRAMTotalMB = Renderer::GetVRAMTotal() / (1024 * 1024);
    
    // Placeholder values
    s_State.Stats.GPUTimeMs = s_State.Stats.FrameTimeMs * 0.8f;
    s_State.Stats.DrawCalls = 1234;
    s_State.Stats.Triangles = 567890;
    s_State.Stats.VRAMUsedMB = 8200;
    s_State.Stats.VRAMTotalMB = 16384;
}

/**
 * @brief Gets the display name for a render mode
 * 
 * @param mode The render mode
 * @return Human-readable name
 */
static const char* GetRenderModeName(ERenderMode mode) {
    switch (mode) {
        case ERenderMode::Lit:              return "Lit";
        case ERenderMode::Unlit:            return "Unlit";
        case ERenderMode::Wireframe:        return "Wireframe";
        case ERenderMode::WireframeOnShaded:return "Wireframe on Shaded";
        case ERenderMode::Normals:          return "Normals";
        case ERenderMode::UV:               return "UV";
        case ERenderMode::VertexColors:     return "Vertex Colors";
        case ERenderMode::Overdraw:         return "Overdraw";
        case ERenderMode::LightComplexity:  return "Light Complexity";
        case ERenderMode::ShaderComplexity: return "Shader Complexity";
        case ERenderMode::QuadOverdraw:     return "Quad Overdraw";
        case ERenderMode::LightmapDensity:  return "Lightmap Density";
        default:                            return "Unknown";
    }
}

/**
 * @brief Gets the display name for a gizmo mode
 * 
 * @param mode The gizmo mode
 * @return Human-readable name
 */
static const char* GetGizmoModeName(EGizmoMode mode) {
    switch (mode) {
        case EGizmoMode::Translate: return "Translate";
        case EGizmoMode::Rotate:    return "Rotate";
        case EGizmoMode::Scale:     return "Scale";
        case EGizmoMode::Bounds:    return "Bounds";
        default:                    return "Unknown";
    }
}

} // namespace RiftCore::UI
