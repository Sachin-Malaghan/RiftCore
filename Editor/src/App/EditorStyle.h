#pragma once
// Editor look: fonts, icon set, colours and a few shared widgets.
//
// Text uses Segoe UI and icons use Segoe MDL2 Assets, both of which ship
// with Windows 10/11, so nothing has to be bundled. On a system without
// them the editor falls back to ImGui's built-in font and plain labels.

#include <imgui.h>
#include <string>

namespace RiftCore::Ed {

    // ── Icons (Segoe MDL2 Assets code points) ──────────────
    namespace Icon {
        inline const char* New        = u8"\uE7C3";
        inline const char* Open       = u8"\uE8E5";
        inline const char* Save       = u8"\uE74E";
        inline const char* SaveAs     = u8"\uE792";
        inline const char* Undo       = u8"\uE7A7";
        inline const char* Redo       = u8"\uE7A6";
        inline const char* Select     = u8"\uE8B0";
        inline const char* Move       = u8"\uE7C2";
        inline const char* Rotate     = u8"\uE7AD";
        inline const char* Scale      = u8"\uE740";
        inline const char* Play       = u8"\uE768";
        inline const char* Pause      = u8"\uE769";
        inline const char* Stop       = u8"\uE71A";
        inline const char* Step       = u8"\uE893";
        inline const char* Add        = u8"\uE710";
        inline const char* Delete     = u8"\uE74D";
        inline const char* Duplicate  = u8"\uE8C8";
        inline const char* Rename     = u8"\uE8AC";
        inline const char* Search     = u8"\uE721";
        inline const char* Settings   = u8"\uE713";
        inline const char* Folder     = u8"\uE8B7";
        inline const char* FolderOpen = u8"\uE838";
        inline const char* File       = u8"\uE7C3";
        inline const char* Up         = u8"\uE74A";
        inline const char* Refresh    = u8"\uE72C";
        inline const char* Eye        = u8"\uE890";
        inline const char* EyeOff     = u8"\uED1A";
        inline const char* Outliner   = u8"\uE8FD";
        inline const char* Details    = u8"\uE946";
        inline const char* World      = u8"\uE774";
        inline const char* Stats      = u8"\uE9D9";
        inline const char* Content    = u8"\uE8F1";
        inline const char* Console    = u8"\uE756";
        inline const char* Code       = u8"\uE943";
        inline const char* Viewport   = u8"\uE7F4";
        inline const char* Place      = u8"\uE8FC";
        inline const char* Cube       = u8"\uF158";
        inline const char* Shapes     = u8"\uE739";
        inline const char* Prop       = u8"\uE7B8";
        inline const char* Light      = u8"\uEA80";
        inline const char* Camera     = u8"\uE722";
        inline const char* Physics    = u8"\uE945";
        inline const char* Transform  = u8"\uE8AB";
        inline const char* Material   = u8"\uE790";
        inline const char* Scene      = u8"\uE81E";
        inline const char* Audio      = u8"\uE767";
        inline const char* Image      = u8"\uEB9F";
        inline const char* Info       = u8"\uE946";
        inline const char* Warning    = u8"\uE7BA";
        inline const char* Error      = u8"\uEA39";
        inline const char* Clear      = u8"\uE894";
        inline const char* Focus      = u8"\uE71E";
        inline const char* Grid       = u8"\uE80A";
        inline const char* Wireframe  = u8"\uE8A9";
        inline const char* Snap       = u8"\uE81C";
        inline const char* Help       = u8"\uE897";
        inline const char* Run        = u8"\uE768";
        inline const char* Close      = u8"\uE711";
        inline const char* Check      = u8"\uE73E";
        inline const char* Empty      = u8"\uEA3A";
        inline const char* Gravity    = u8"\uE74B";
        inline const char* Sun        = u8"\uE706";
        inline const char* Exit       = u8"\uE7E8";
        inline const char* Model      = u8"\uE7B8";
    }

    // ── Palette ────────────────────────────────────────────
    namespace Col {
        inline const ImVec4 Bg        {0.086f, 0.090f, 0.102f, 1.0f};
        inline const ImVec4 Panel     {0.125f, 0.129f, 0.145f, 1.0f};
        inline const ImVec4 PanelAlt  {0.153f, 0.157f, 0.176f, 1.0f};
        inline const ImVec4 Frame     {0.192f, 0.200f, 0.224f, 1.0f};
        inline const ImVec4 FrameHot  {0.243f, 0.255f, 0.286f, 1.0f};
        inline const ImVec4 Border    {0.240f, 0.247f, 0.275f, 1.0f};
        inline const ImVec4 Text      {0.902f, 0.910f, 0.925f, 1.0f};
        inline const ImVec4 TextDim   {0.560f, 0.580f, 0.620f, 1.0f};
        inline const ImVec4 Accent    {0.220f, 0.540f, 0.960f, 1.0f};
        inline const ImVec4 AccentHot {0.330f, 0.630f, 1.000f, 1.0f};
        inline const ImVec4 AccentDim {0.220f, 0.540f, 0.960f, 0.35f};
        inline const ImVec4 Green     {0.240f, 0.720f, 0.400f, 1.0f};
        inline const ImVec4 Red       {0.900f, 0.330f, 0.310f, 1.0f};
        inline const ImVec4 Yellow    {0.950f, 0.760f, 0.260f, 1.0f};
        inline const ImVec4 AxisX     {0.860f, 0.300f, 0.300f, 1.0f};
        inline const ImVec4 AxisY     {0.420f, 0.740f, 0.250f, 1.0f};
        inline const ImVec4 AxisZ     {0.260f, 0.520f, 0.930f, 1.0f};
    }

    struct Fonts {
        ImFont* ui    = nullptr;   // Segoe UI + icons
        ImFont* bold  = nullptr;   // Segoe UI Semibold + icons
        ImFont* mono  = nullptr;   // Consolas (console, code)
        ImFont* large = nullptr;   // big icons (place panel, content tiles)
        bool    icons = false;     // icon font available
    };

    // Loads fonts and applies the theme. Call once after ImGui::CreateContext.
    void         SetupStyle();
    const Fonts& GetFonts();

    // "icon  label" (or just the label when the icon font is missing).
    // The returned pointer is valid until a few more calls are made.
    const char* L(const char* icon, const char* label);
    // Icon only, or `fallback` text without the icon font.
    const char* I(const char* icon, const char* fallback);

    // Square toolbar button with a tooltip. `active` draws it highlighted.
    bool ToolButton(const char* icon, const char* fallback, const char* tooltip,
                    bool active = false, const ImVec4* tint = nullptr, bool enabled = true);
    void ToolSeparator();

    // Section header inside a panel; returns true while open.
    bool Section(const char* icon, const char* label, bool defaultOpen = true);

    // Label on the left, widget on the right (call before the widget).
    void PropertyLabel(const char* label);

    // X/Y/Z drag row with coloured axis tags. Returns true when edited.
    bool DragVec3(const char* id, float v[3], float speed = 0.05f, float resetTo = 0.0f);

    void Tooltip(const char* text);

} // namespace RiftCore::Ed
