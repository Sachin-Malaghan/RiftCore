#include "EditorStyle.h"

#include <imgui_internal.h>
#include <cstdio>
#include <filesystem>

namespace RiftCore::Ed {

    static Fonts g_fonts;

    const Fonts& GetFonts() { return g_fonts; }

    static std::string FindSystemFont(const char* file) {
        std::error_code ec;
        std::filesystem::path dirs[] = {
#ifdef _WIN32
            std::filesystem::path(std::getenv("WINDIR") ? std::getenv("WINDIR") : "C:\\Windows") / "Fonts",
#endif
            "Assets/Fonts",
        };
        for (auto& d : dirs) {
            auto p = d / file;
            if (std::filesystem::exists(p, ec)) return p.string();
        }
        return {};
    }

    // Adds a text font with the icon font merged in.
    static ImFont* AddFont(const std::string& text, const std::string& icons,
                           float size, float iconSize, float iconOffsetY)
    {
        ImGuiIO& io = ImGui::GetIO();
        ImFont* font = nullptr;
        if (!text.empty()) font = io.Fonts->AddFontFromFileTTF(text.c_str(), size);
        if (!font) {
            ImFontConfig cfg;
            cfg.SizePixels = size;
            font = io.Fonts->AddFontDefault(&cfg);
        }
        if (!icons.empty()) {
            ImFontConfig cfg;
            cfg.MergeMode        = true;
            cfg.PixelSnapH       = true;
            cfg.GlyphOffset      = ImVec2(0.0f, iconOffsetY);
            cfg.GlyphMinAdvanceX = iconSize;
            io.Fonts->AddFontFromFileTTF(icons.c_str(), iconSize, &cfg);
        }
        return font;
    }

    void SetupStyle() {
        std::string ui    = FindSystemFont("segoeui.ttf");
        std::string semi  = FindSystemFont("seguisb.ttf");
        std::string mono  = FindSystemFont("consola.ttf");
        std::string icons = FindSystemFont("segmdl2.ttf");

        g_fonts.icons = !icons.empty();
        g_fonts.ui    = AddFont(ui, icons, 17.0f, 15.0f, 3.0f);
        g_fonts.bold  = AddFont(semi.empty() ? ui : semi, icons, 17.0f, 15.0f, 3.0f);
        g_fonts.mono  = AddFont(mono, "", 15.0f, 0.0f, 0.0f);
        g_fonts.large = AddFont(ui, icons, 17.0f, 26.0f, 6.0f);
        ImGui::GetIO().FontDefault = g_fonts.ui;

        ImGuiStyle& s = ImGui::GetStyle();
        s.WindowPadding     = ImVec2(10, 10);
        s.FramePadding      = ImVec2(8, 5);
        s.ItemSpacing       = ImVec2(8, 6);
        s.ItemInnerSpacing  = ImVec2(6, 4);
        s.IndentSpacing     = 18.0f;
        s.ScrollbarSize     = 12.0f;
        s.GrabMinSize       = 10.0f;
        s.WindowRounding    = 0.0f;
        s.ChildRounding     = 6.0f;
        s.FrameRounding     = 5.0f;
        s.PopupRounding     = 6.0f;
        s.ScrollbarRounding = 6.0f;
        s.GrabRounding      = 4.0f;
        s.TabRounding       = 5.0f;
        s.WindowBorderSize  = 0.0f;
        s.ChildBorderSize   = 1.0f;
        s.PopupBorderSize   = 1.0f;
        s.FrameBorderSize   = 0.0f;
        s.TabBarBorderSize  = 1.0f;
        s.SeparatorTextBorderSize = 1.0f;

        ImVec4* c = s.Colors;
        c[ImGuiCol_Text]                 = Col::Text;
        c[ImGuiCol_TextDisabled]         = Col::TextDim;
        c[ImGuiCol_WindowBg]             = Col::Bg;
        c[ImGuiCol_ChildBg]              = Col::Panel;
        c[ImGuiCol_PopupBg]              = ImVec4(0.11f, 0.115f, 0.13f, 0.99f);
        c[ImGuiCol_Border]               = Col::Border;
        c[ImGuiCol_BorderShadow]         = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_FrameBg]              = Col::Frame;
        c[ImGuiCol_FrameBgHovered]       = Col::FrameHot;
        c[ImGuiCol_FrameBgActive]        = Col::FrameHot;
        c[ImGuiCol_TitleBg]              = Col::Bg;
        c[ImGuiCol_TitleBgActive]        = Col::Panel;
        c[ImGuiCol_TitleBgCollapsed]     = Col::Bg;
        c[ImGuiCol_MenuBarBg]            = Col::Bg;
        c[ImGuiCol_ScrollbarBg]          = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_ScrollbarGrab]        = Col::Frame;
        c[ImGuiCol_ScrollbarGrabHovered] = Col::FrameHot;
        c[ImGuiCol_ScrollbarGrabActive]  = Col::Accent;
        c[ImGuiCol_CheckMark]            = Col::AccentHot;
        c[ImGuiCol_SliderGrab]           = Col::Accent;
        c[ImGuiCol_SliderGrabActive]     = Col::AccentHot;
        c[ImGuiCol_Button]               = Col::Frame;
        c[ImGuiCol_ButtonHovered]        = Col::FrameHot;
        c[ImGuiCol_ButtonActive]         = Col::Accent;
        c[ImGuiCol_Header]               = Col::AccentDim;
        c[ImGuiCol_HeaderHovered]        = ImVec4(0.22f, 0.54f, 0.96f, 0.22f);
        c[ImGuiCol_HeaderActive]         = ImVec4(0.22f, 0.54f, 0.96f, 0.50f);
        c[ImGuiCol_Separator]            = Col::Border;
        c[ImGuiCol_SeparatorHovered]     = Col::Accent;
        c[ImGuiCol_SeparatorActive]      = Col::AccentHot;
        c[ImGuiCol_ResizeGrip]           = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_ResizeGripHovered]    = Col::AccentDim;
        c[ImGuiCol_ResizeGripActive]     = Col::Accent;
        c[ImGuiCol_Tab]                  = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_TabHovered]           = Col::FrameHot;
        c[ImGuiCol_TabSelected]          = Col::PanelAlt;
        c[ImGuiCol_TabSelectedOverline]  = Col::Accent;
        c[ImGuiCol_TabDimmed]            = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_TabDimmedSelected]    = Col::PanelAlt;
        c[ImGuiCol_PlotLines]            = Col::Accent;
        c[ImGuiCol_PlotHistogram]        = Col::Accent;
        c[ImGuiCol_TableHeaderBg]        = Col::PanelAlt;
        c[ImGuiCol_TableBorderStrong]    = Col::Border;
        c[ImGuiCol_TableBorderLight]     = ImVec4(0.24f, 0.25f, 0.28f, 0.5f);
        c[ImGuiCol_TableRowBgAlt]        = ImVec4(1, 1, 1, 0.025f);
        c[ImGuiCol_TextSelectedBg]       = Col::AccentDim;
        c[ImGuiCol_DragDropTarget]       = Col::AccentHot;
        c[ImGuiCol_NavCursor]            = Col::Accent;
        c[ImGuiCol_ModalWindowDimBg]     = ImVec4(0, 0, 0, 0.55f);
    }

    const char* L(const char* icon, const char* label) {
        static char buf[8][192];
        static int  next = 0;
        char* out = buf[next];
        next = (next + 1) % 8;
        if (g_fonts.icons && icon && *icon) std::snprintf(out, 192, "%s  %s", icon, label);
        else                                std::snprintf(out, 192, "%s", label);
        return out;
    }

    const char* I(const char* icon, const char* fallback) {
        return g_fonts.icons ? icon : fallback;
    }

    void Tooltip(const char* text) {
        if (text && *text && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort |
                                                  ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("%s", text);
        }
    }

    bool ToolButton(const char* icon, const char* fallback, const char* tooltip,
                    bool active, const ImVec4* tint, bool enabled)
    {
        ImGui::PushID(tooltip);
        ImGui::BeginDisabled(!enabled);
        ImGui::PushStyleColor(ImGuiCol_Button, active ? Col::AccentDim : ImVec4(0, 0, 0, 0));
        if (tint) ImGui::PushStyleColor(ImGuiCol_Text, *tint);
        const float h = 32.0f;
        float w = g_fonts.icons ? h : ImGui::CalcTextSize(fallback).x + 16.0f;
        bool clicked = ImGui::Button(I(icon, fallback), ImVec2(w, h));
        if (tint) ImGui::PopStyleColor();
        ImGui::PopStyleColor();
        if (active) {
            ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImVec2(a.x + 6, b.y - 3), ImVec2(b.x - 6, b.y - 1),
                ImGui::GetColorU32(Col::Accent), 2.0f);
        }
        ImGui::EndDisabled();
        Tooltip(tooltip);
        ImGui::PopID();
        ImGui::SameLine(0, 2);
        return clicked;
    }

    void ToolSeparator() {
        ImGui::SameLine(0, 6);
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddLine(ImVec2(p.x, p.y + 6), ImVec2(p.x, p.y + 26),
                                            ImGui::GetColorU32(Col::Border));
        ImGui::Dummy(ImVec2(1, 32));
        ImGui::SameLine(0, 8);
    }

    bool Section(const char* icon, const char* label, bool defaultOpen) {
        ImGui::PushFont(g_fonts.bold);
        ImGui::PushStyleColor(ImGuiCol_Header,        Col::PanelAlt);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, Col::Frame);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,  Col::Frame);
        bool open = ImGui::CollapsingHeader(L(icon, label),
            defaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0);
        ImGui::PopStyleColor(3);
        ImGui::PopFont();
        return open;
    }

    void PropertyLabel(const char* label) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(Col::TextDim, "%s", label);
        ImGui::SameLine(ImGui::GetContentRegionAvail().x > 260.0f ? 110.0f : 92.0f);
        ImGui::SetNextItemWidth(-1.0f);
    }

    bool DragVec3(const char* id, float v[3], float speed, float resetTo) {
        static const ImVec4 axis[3] = { Col::AxisX, Col::AxisY, Col::AxisZ };
        static const char*  name[3] = { "X", "Y", "Z" };
        bool changed = false;
        ImGui::PushID(id);
        float spacing = 4.0f;
        float full = ImGui::GetContentRegionAvail().x;
        float tagW = 20.0f;
        float boxW = (full - spacing * 2.0f) / 3.0f - tagW;
        if (boxW < 30.0f) boxW = 30.0f;
        for (int i = 0; i < 3; i++) {
            ImGui::PushID(i);
            if (i > 0) ImGui::SameLine(0, spacing);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(axis[i].x, axis[i].y, axis[i].z, 0.85f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, axis[i]);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  axis[i]);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 5));
            if (ImGui::Button(name[i], ImVec2(tagW, 0))) { v[i] = resetTo; changed = true; }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);
            Tooltip("Click to reset");
            ImGui::SameLine(0, 0);
            ImGui::SetNextItemWidth(boxW);
            if (ImGui::DragFloat("##v", &v[i], speed, 0.0f, 0.0f, "%.3f")) changed = true;
            ImGui::PopID();
        }
        ImGui::PopID();
        return changed;
    }

} // namespace RiftCore::Ed
