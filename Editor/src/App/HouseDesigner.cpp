// House AI: prompt-driven house design inside the editor.
//
// The design itself is done by the Python package Assets/Scripts/housegen
// (brief -> building model -> drawings, schedules, 3D scene). This file is
// the editor side: the prompt panel and a viewer for the generated 2D
// sheets, which are read from the design.json the package writes.

#define NOMINMAX
#include "EditorApp.h"
#include "EditorStyle.h"

#include <RiftCore/Scripting/IScripting.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <json.hpp>

#ifdef _WIN32
    #include <windows.h>
    #include <shellapi.h>
#endif

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace RiftCore {

    using namespace Ed;

    namespace {

        struct LayerStyle { ImU32 line; ImU32 fill; float width; };

        // CAD-style colours on a dark sheet.
        LayerStyle StyleFor(const std::string& layer) {
            static const std::unordered_map<std::string, LayerStyle> k = {
                { "WALL",      { IM_COL32(235, 235, 235, 255), IM_COL32(150, 152, 160, 255), 1.5f } },
                { "CONCRETE",  { IM_COL32(220, 220, 220, 255), IM_COL32(110, 112, 120, 255), 1.5f } },
                { "COLUMN",    { IM_COL32(255, 255, 255, 255), IM_COL32(245, 245, 245, 255), 1.0f } },
                { "DOOR",      { IM_COL32(240, 170,  80, 255), 0, 1.2f } },
                { "WINDOW",    { IM_COL32( 90, 180, 255, 255), 0, 1.2f } },
                { "STAIR",     { IM_COL32(170, 175, 185, 255), 0, 1.0f } },
                { "DIM",       { IM_COL32(240, 110, 110, 255), 0, 1.0f } },
                { "TAG",       { IM_COL32(110, 200, 255, 255), 0, 1.0f } },
                { "TEXT",      { IM_COL32(205, 210, 220, 255), 0, 1.0f } },
                { "ROOM",      { IM_COL32(255, 255, 255, 255), 0, 1.0f } },
                { "TITLE",     { IM_COL32(255, 255, 255, 255), IM_COL32(255, 255, 255, 255), 1.5f } },
                { "GROUND",    { IM_COL32(190, 150, 100, 255), 0, 2.5f } },
                { "ROOF",      { IM_COL32(235, 120,  95, 255), 0, 1.5f } },
                { "ROOFHATCH", { IM_COL32(150,  90,  80, 255), 0, 1.0f } },
                { "SLAB",      { IM_COL32(150, 150, 160, 255), 0, 1.0f } },
                { "HIDDEN",    { IM_COL32(125, 130, 140, 255), 0, 1.0f } },
                { "GRID",      { IM_COL32( 90, 200, 140, 255), 0, 1.0f } },
                { "FOOTING",   { IM_COL32(230, 160,  60, 255), 0, 1.0f } },
                { "BEAM",      { IM_COL32(140, 140, 150, 255), 0, 1.0f } },
                { "PLOT",      { IM_COL32(255, 255, 255, 255), 0, 2.0f } },
                { "SECTION",   { IM_COL32(255, 110, 110, 255), 0, 1.5f } },
            };
            auto it = k.find(layer);
            return it != k.end() ? it->second : LayerStyle{ IM_COL32(200, 200, 200, 255), 0, 1.0f };
        }

        std::string ReadFile(const std::string& path) {
            std::ifstream f(path, std::ios::binary);
            std::stringstream ss;
            ss << f.rdbuf();
            return ss.str();
        }

    } // namespace

    // ============================================================
    //  Generate / load
    // ============================================================

    void EditorApp::GenerateHouse() {
        if (!scripting_ || !scripting_->IsAvailable()) {
            Log(LogLevel::Error, "House AI needs Python scripting, which is not available in this build.");
            return;
        }
        if (housePrompt_.empty()) return;
        if (playState_ != PlayState::Edit) Stop();

        std::error_code ec;
        fs::create_directories("Output", ec);
        {
            std::ofstream f("Output/prompt.txt", std::ios::binary);
            f << housePrompt_;
        }
        Log(LogLevel::Info, "House AI: designing \"" + housePrompt_ + "\"");
        RunCode("import housegen\nhousegen.generate_from_file('Output/prompt.txt')\n");

        scenePath_.clear();
        selected_ = INVALID_NODE;
        houseRoof_ = houseUpper_ = true;
        if (LoadHouseDesign()) {
            SetHouseView(0);
            showDrawings_ = true;
        }
        UpdateTitle();
    }

    void EditorApp::SetHouseView(int preset) {
        if (houseSheets_.empty()) return;
        bool py = scripting_ && scripting_->IsAvailable();
        float w = houseW_ / 1000.0f, d = houseD_ / 1000.0f, top = houseTop_ / 1000.0f;
        houseRoof_ = houseUpper_ = (preset != 1);
        if (py) {
            scripting_->ExecuteString(preset == 1
                ? "import housegen\nhousegen.set_view(roof=False, upper_floors=False)\n"
                : "import housegen\nhousegen.set_view(roof=True, upper_floors=True)\n");
            PumpScriptOutput();
        }
        if (preset == 0) {            // from the road, front corner
            camera_.SetPosition({ w * 0.5f + 5.0f, top * 0.75f + 2.0f, d * 0.5f + w + 5.0f });
            camera_.SetTarget({ 0.0f, top * 0.42f, 0.0f });
        } else if (preset == 1) {     // looking down into the ground floor
            float h = std::max(w, d) * 1.05f + 3.0f;
            camera_.SetPosition({ 0.0f, h, d * 0.5f + 5.0f });
            camera_.SetTarget({ 0.0f, 0.5f, -0.6f });
        } else {                      // standing in the living room, eye level
            camera_.SetPosition({ houseLivingX_ - 1.2f, 0.45f + 1.6f, houseLivingZ_ + 1.0f });
            camera_.SetTarget({ houseLivingX_ + 1.5f, 1.45f, houseLivingZ_ - 3.0f });
        }
    }

    bool EditorApp::LoadHouseDesign() {
        std::string path = ReadFile("Output/latest.txt");
        while (!path.empty() && (path.back() == '\n' || path.back() == '\r' || path.back() == ' ')) path.pop_back();
        if (path.empty()) return false;

        json root = json::parse(ReadFile(path), nullptr, false);
        if (root.is_discarded() || !root.is_object()) {
            Log(LogLevel::Error, "House AI: could not read " + path);
            return false;
        }
        try {
            houseSheets_.clear();
            houseInfo_.clear();
            houseDir_ = fs::path(path).parent_path().generic_string();
            houseW_   = root.value("W", 0.0f);
            houseD_   = root.value("D", 0.0f);
            houseTop_ = root.value("top_level", 0.0f);

            houseLivingX_ = houseLivingZ_ = 0.0f;
            for (auto& r : root["rooms"]) {
                if (r.value("kind", "") == "living" && r.value("floor", 0) == 0) {
                    houseLivingX_ = (r.value("x", 0.0f) + r.value("w", 0.0f) * 0.5f - houseW_ * 0.5f) / 1000.0f;
                    houseLivingZ_ = (houseD_ * 0.5f - (r.value("y", 0.0f) + r.value("d", 0.0f) * 0.5f)) / 1000.0f;
                }
            }
            const json& b = root["brief"];
            houseInfo_.push_back({ "Project", b.value("name", "House") });
            houseInfo_.push_back({ "Size", std::to_string(root.value("W", 0)) + " x " +
                                           std::to_string(root.value("D", 0)) + " mm" });
            houseInfo_.push_back({ "Storeys", std::to_string(b.value("floors", 1)) });
            houseInfo_.push_back({ "Bedrooms / baths", std::to_string(b.value("bedrooms", 0)) + " / " +
                                                       std::to_string(b.value("bathrooms", 0)) });
            houseInfo_.push_back({ "Facing", b.value("facing", "north") +
                                             std::string(b.value("vastu", false) ? "  (Vastu)" : "") });
            houseInfo_.push_back({ "Roof", b.value("style", "modern") == "traditional" ? "gable" : "flat + parapet" });
            for (auto& q : root["quantities"]) {
                if (q.size() >= 3 && q[2].get<std::string>() == "sq.m" && houseInfo_.size() < 9) {
                    char buf[48];
                    std::snprintf(buf, sizeof(buf), "%.1f sq.m", q[1].get<double>());
                    std::string label = q[0].get<std::string>();
                    size_t cut = label.find_first_of("(,");
                    if (cut != std::string::npos) label = label.substr(0, cut);
                    houseInfo_.push_back({ label, buf });
                }
            }
            houseVastu_.clear();
            for (auto& v : root["vastu"]) {
                houseVastu_.push_back({ v[0].get<std::string>() + ": " + v[1].get<std::string>(),
                                        v[2].get<std::string>() });
            }

            for (auto& sj : root["sheets"]) {
                HouseSheet sheet;
                sheet.title = sj.value("title", "Sheet");
                for (int i = 0; i < 4; i++) sheet.bounds[i] = sj["bounds"][i].get<float>();
                for (auto& p : sj["prims"]) {
                    HousePrim prim;
                    std::string kind = p[0].get<std::string>();
                    if (kind == "line") {
                        prim.kind = 0;
                        for (int i = 0; i < 4; i++) prim.v[i] = p[1 + i].get<float>();
                        prim.layer = p[5].get<std::string>();
                    } else if (kind == "rect") {
                        prim.kind = 1;
                        for (int i = 0; i < 4; i++) prim.v[i] = p[1 + i].get<float>();
                        prim.layer = p[5].get<std::string>();
                        prim.fill  = p[6].get<bool>();
                    } else if (kind == "poly") {
                        prim.kind = 2;
                        for (auto& c : p[1]) prim.pts.push_back(c.get<float>());
                        prim.layer = p[2].get<std::string>();
                        prim.fill  = p[3].get<bool>();
                    } else if (kind == "arc") {
                        prim.kind = 3;
                        for (int i = 0; i < 5; i++) prim.v[i] = p[1 + i].get<float>();
                        prim.layer = p[6].get<std::string>();
                    } else if (kind == "text") {
                        prim.kind = 4;
                        prim.v[0] = p[1].get<float>();
                        prim.v[1] = p[2].get<float>();
                        prim.text = p[3].get<std::string>();
                        prim.v[2] = p[4].get<float>();
                        prim.layer = p[5].get<std::string>();
                        std::string a = p[6].get<std::string>();
                        prim.anchor = a == "l" ? 0 : a == "r" ? 2 : 1;
                        prim.v[3] = p[7].get<float>();
                    } else {
                        continue;
                    }
                    sheet.prims.push_back(std::move(prim));
                }
                houseSheets_.push_back(std::move(sheet));
            }
        } catch (const json::exception& e) {
            Log(LogLevel::Error, std::string("House AI: bad design file: ") + e.what());
            return false;
        }
        houseSheet_ = 0;
        houseZoom_ = 1.0f;
        housePanX_ = housePanY_ = 0.0f;
        return !houseSheets_.empty();
    }

    // ============================================================
    //  Prompt panel (left tab)
    // ============================================================

    void EditorApp::DrawHouseDesigner() {
        bool py = scripting_ && scripting_->IsAvailable();
        ImGui::BeginChild("##HouseScroll", ImVec2(0, 0));

        ImGui::PushFont(GetFonts().bold);
        ImGui::TextUnformatted(L(Icon::Home, "Describe the house"));
        ImGui::PopFont();
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextColored(Col::TextDim,
            "Bedrooms, bathrooms, storeys, plot size, facing, vastu, garage, study, pooja, roof style, area.");
        ImGui::PopTextWrapPos();
        ImGui::InputTextMultiline("##housePrompt", &housePrompt_, ImVec2(-1.0f, 96.0f));

        ImGui::BeginDisabled(!py || housePrompt_.empty());
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.45f, 0.85f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Col::AccentHot);
        if (ImGui::Button(L(Icon::Run, "Generate design"), ImVec2(-1.0f, 36.0f))) GenerateHouse();
        ImGui::PopStyleColor(2);
        ImGui::EndDisabled();
        if (!py) ImGui::TextColored(Col::Yellow, "Python is not available in this build.");

        if (Section(Icon::Details, "Examples")) {
            static const char* examples[] = {
                "2BHK house with 2 floors, vastu compliant, north facing",
                "Modern 3 bedroom 2 bath two storey house on a 12 x 18 m plot with garage",
                "Traditional 4 bedroom 3 bath duplex with study and pooja room, east facing vastu",
                "Single storey 2 bedroom cottage with gable roof, 30 x 40 ft plot",
                "3BHK south facing vastu house, 1200 sq ft per floor, G+1, car parking",
            };
            for (const char* ex : examples) {
                ImGui::PushTextWrapPos(0.0f);
                if (ImGui::Selectable(ex, false, 0, ImVec2(0, ImGui::CalcTextSize(ex, nullptr, false,
                        ImGui::GetContentRegionAvail().x).y))) {
                    housePrompt_ = ex;
                }
                ImGui::PopTextWrapPos();
            }
            ImGui::Spacing();
        }

        if (!houseSheets_.empty()) {
            if (Section(Icon::Scene, "Design")) {
                for (auto& row : houseInfo_) {
                    ImGui::TextColored(Col::TextDim, "%s", row.first.c_str());
                    ImGui::SameLine(128.0f);
                    ImGui::PushTextWrapPos(0.0f);
                    ImGui::TextUnformatted(row.second.c_str());
                    ImGui::PopTextWrapPos();
                }
                ImGui::Spacing();
            }
            if (!houseVastu_.empty() && Section(Icon::Check, "Vastu check")) {
                for (auto& row : houseVastu_) {
                    bool ok = row.second == "OK";
                    ImGui::TextColored(ok ? Col::Green : Col::Yellow, "%s", ok ? "OK  " : "NOTE");
                    ImGui::SameLine(52.0f);
                    ImGui::PushTextWrapPos(0.0f);
                    ImGui::TextUnformatted(row.first.c_str());
                    ImGui::PopTextWrapPos();
                }
                ImGui::Spacing();
            }
            if (Section(Icon::Viewport, "3D view")) {
                float bw = (ImGui::GetContentRegionAvail().x - 12.0f) / 3.0f;
                if (ImGui::Button("Exterior", ImVec2(bw, 0)))  SetHouseView(0);
                ImGui::SameLine(0, 6);
                if (ImGui::Button("Dollhouse", ImVec2(bw, 0))) SetHouseView(1);
                ImGui::SameLine(0, 6);
                if (ImGui::Button("Inside", ImVec2(bw, 0)))    SetHouseView(2);
                bool changed = ImGui::Checkbox("Show roof", &houseRoof_);
                changed |= ImGui::Checkbox("Show upper floors", &houseUpper_);
                if (changed && py) {
                    std::string code = "import housegen\nhousegen.set_view(roof=";
                    code += houseRoof_ ? "True" : "False";
                    code += ", upper_floors=";
                    code += houseUpper_ ? "True" : "False";
                    code += ")\n";
                    scripting_->ExecuteString(code.c_str());
                    PumpScriptOutput();
                }
                ImGui::TextColored(Col::TextDim, "Hide them to look into the plan in 3D.");
                ImGui::Spacing();
            }
            if (Section(Icon::Folder, "Deliverables")) {
                ImGui::PushTextWrapPos(0.0f);
                ImGui::TextColored(Col::TextDim, "%s", houseDir_.c_str());
                ImGui::TextUnformatted("drawings.dxf  (AutoCAD / Revit import)\n*.svg  one per sheet\n"
                                       "report.md  schedules and quantities\ndesign.json  the building model");
                ImGui::PopTextWrapPos();
#ifdef _WIN32
                if (ImGui::Button(L(Icon::FolderOpen, "Open output folder"), ImVec2(-1.0f, 0))) {
                    std::error_code ec;
                    std::string abs = fs::absolute(houseDir_, ec).make_preferred().string();
                    ShellExecuteA(nullptr, "open", abs.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                }
#endif
                if (ImGui::Button(L(Icon::Image, "Show 2D drawings"), ImVec2(-1.0f, 0))) showDrawings_ = true;
            }
        }
        ImGui::EndChild();
    }

    // ============================================================
    //  2D drawing viewer (centre tab)
    // ============================================================

    void EditorApp::DrawDrawings() {
        if (houseSheets_.empty()) {
            ImGui::Dummy(ImVec2(0, 30));
            ImGui::SetCursorPosX(30.0f);
            ImGui::TextColored(Col::TextDim,
                "No drawings yet.\n\nOpen the House AI tab on the left, describe a house and press Generate design.");
            return;
        }
        houseSheet_ = std::clamp(houseSheet_, 0, static_cast<int>(houseSheets_.size()) - 1);

        // Sheet selector.
        ImGui::SetCursorPos(ImVec2(8, ImGui::GetCursorPosY() + 6));
        for (int i = 0; i < static_cast<int>(houseSheets_.size()); i++) {
            if (i > 0) ImGui::SameLine(0, 4);
            bool on = i == houseSheet_;
            ImGui::PushStyleColor(ImGuiCol_Button, on ? Col::Accent : Col::Frame);
            if (ImGui::SmallButton(houseSheets_[i].title.c_str())) {
                houseSheet_ = i;
                houseZoom_ = 1.0f;
                housePanX_ = housePanY_ = 0.0f;
            }
            ImGui::PopStyleColor();
        }

        const HouseSheet& sheet = houseSheets_[houseSheet_];
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        p0.y += 4.0f;
        ImVec2 size = ImGui::GetContentRegionAvail();
        size.y -= 4.0f;
        if (size.x < 50.0f || size.y < 50.0f) return;
        ImVec2 p1(p0.x + size.x, p0.y + size.y);

        ImGui::SetCursorScreenPos(p0);
        ImGui::InvisibleButton("##sheet", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        ImGuiIO& io = ImGui::GetIO();
        if (ImGui::IsItemActive()) {
            housePanX_ += io.MouseDelta.x;
            housePanY_ += io.MouseDelta.y;
        }
        if (ImGui::IsItemHovered()) {
            if (io.MouseWheel != 0.0f) {
                float factor = io.MouseWheel > 0 ? 1.15f : 1.0f / 1.15f;
                // Zoom about the cursor.
                ImVec2 c((p0.x + p1.x) * 0.5f + housePanX_, (p0.y + p1.y) * 0.5f + housePanY_);
                housePanX_ += (io.MousePos.x - c.x) * (1.0f - factor);
                housePanY_ += (io.MousePos.y - c.y) * (1.0f - factor);
                houseZoom_ = std::clamp(houseZoom_ * factor, 0.2f, 40.0f);
            }
            if (ImGui::IsMouseDoubleClicked(0)) { houseZoom_ = 1.0f; housePanX_ = housePanY_ = 0.0f; }
        }

        float bw = std::max(sheet.bounds[2] - sheet.bounds[0], 1.0f);
        float bh = std::max(sheet.bounds[3] - sheet.bounds[1], 1.0f);
        float s  = std::min(size.x / bw, size.y / bh) * 0.94f * houseZoom_;
        float mx = (sheet.bounds[0] + sheet.bounds[2]) * 0.5f, my = (sheet.bounds[1] + sheet.bounds[3]) * 0.5f;
        float cx = (p0.x + p1.x) * 0.5f + housePanX_, cy = (p0.y + p1.y) * 0.5f + housePanY_;
        auto X = [&](float x) { return cx + (x - mx) * s; };
        auto Y = [&](float y) { return cy - (y - my) * s; };

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(p0, p1, IM_COL32(18, 20, 26, 255), 4.0f);
        dl->PushClipRect(p0, p1, true);
        ImFont* font = GetFonts().ui;

        for (const HousePrim& p : sheet.prims) {
            LayerStyle st = StyleFor(p.layer);
            switch (p.kind) {
            case 0:
                dl->AddLine(ImVec2(X(p.v[0]), Y(p.v[1])), ImVec2(X(p.v[2]), Y(p.v[3])), st.line, st.width);
                break;
            case 1: {
                ImVec2 a(X(p.v[0]), Y(p.v[1] + p.v[3])), b(X(p.v[0] + p.v[2]), Y(p.v[1]));
                if (p.fill && st.fill) dl->AddRectFilled(a, b, st.fill);
                else                   dl->AddRect(a, b, st.line, 0.0f, 0, st.width);
                break;
            }
            case 2: {
                ImVec2 pts[16];
                int n = std::min(static_cast<int>(p.pts.size() / 2), 16);
                for (int i = 0; i < n; i++) pts[i] = ImVec2(X(p.pts[i * 2]), Y(p.pts[i * 2 + 1]));
                if (p.fill) dl->AddConvexPolyFilled(pts, n, st.fill ? st.fill : st.line);
                else        dl->AddPolyline(pts, n, st.line, ImDrawFlags_Closed, st.width);
                break;
            }
            case 3: {
                const float d2r = 3.14159265f / 180.0f;
                dl->PathArcTo(ImVec2(X(p.v[0]), Y(p.v[1])), p.v[2] * s, -p.v[4] * d2r, -p.v[3] * d2r, 24);
                dl->PathStroke(st.line, 0, st.width);
                break;
            }
            case 4: {
                float px = p.v[2] * s * 1.3f;
                if (px < 5.0f) break;                       // too small to read at this zoom
                px = std::min(px, 96.0f);
                ImVec2 ts = font->CalcTextSizeA(px, FLT_MAX, 0.0f, p.text.c_str());
                float ax = p.anchor == 0 ? 0.0f : p.anchor == 2 ? ts.x : ts.x * 0.5f;
                ImVec2 at(X(p.v[0]), Y(p.v[1]));
                int first = dl->VtxBuffer.Size;
                dl->AddText(font, px, ImVec2(at.x - ax, at.y - px * 0.92f), st.line, p.text.c_str());
                if (p.v[3] != 0.0f) {                       // vertical text: rotate about the anchor
                    for (int i = first; i < dl->VtxBuffer.Size; i++) {
                        ImVec2& v = dl->VtxBuffer[i].pos;
                        float dx = v.x - at.x, dy = v.y - at.y;
                        v = ImVec2(at.x + dy, at.y - dx);
                    }
                }
                break;
            }
            }
        }
        dl->PopClipRect();
        dl->AddText(ImVec2(p0.x + 10, p1.y - 24), IM_COL32(255, 255, 255, 110),
                    "Drag to pan    Wheel to zoom    Double-click to fit");
    }

} // namespace RiftCore
