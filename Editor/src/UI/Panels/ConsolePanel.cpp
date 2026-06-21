/**
 * @file ConsolePanel.cpp
 * @brief Production-grade Console/Output Log Panel for RiftCore Engine
 * 
 * This panel provides a comprehensive logging and command console similar to
 * Unreal Engine's Output Log, with filtering, search, copy, and command execution.
 * 
 * @author RiftCore Team
 * @version 2.0.0
 * @date 2026-04-21
 * 
 * @note Architecture inspired by Unreal Engine's SOutputLog
 * 
 * ============================================================================
 * EXTERNAL DEPENDENCIES (TODO: Implement these interfaces)
 * ============================================================================
 * - ILogSystem: Engine logging system for receiving log messages
 * - ICommandProcessor: Console command execution system
 * - IAutoComplete: Command auto-completion provider
 * - EditorPreferences: For saving console settings
 * ============================================================================
 */


#include <UI/Panels/ConsolePanel.h>
#include <imgui.h>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <cstring>
#include <deque>
#include <mutex>
#include <regex>
#include <unordered_set>

// TODO: Include your engine's logging system
// #include <Core/Log.h>
// #include <Core/CommandProcessor.h>

namespace RiftCore::UI {

//=============================================================================
// CONFIGURATION CONSTANTS
//=============================================================================

namespace ConsoleConfig {
    /** Maximum number of log entries to keep in memory */
    constexpr size_t MAX_LOG_ENTRIES = 10000;
    
    /** Maximum number of command history entries */
    constexpr size_t MAX_COMMAND_HISTORY = 100;
    
    /** Maximum length of a single log message */
    constexpr size_t MAX_MESSAGE_LENGTH = 4096;
    
    /** Auto-scroll threshold (scroll to bottom if within this many pixels) */
    constexpr float AUTO_SCROLL_THRESHOLD = 50.0f;
    
    /** Default font scale for console text */
    constexpr float DEFAULT_FONT_SCALE = 1.0f;
    
    /** Time format for log timestamps */
    constexpr const char* TIMESTAMP_FORMAT = "[%H:%M:%S]";
}

//=============================================================================
// STATIC STATE (uses types from header)
//=============================================================================

static FConsoleState s_State;
static std::deque<FLogEntry> s_LogEntries;
static std::vector<FLogEntry*> s_FilteredEntries;
static std::mutex s_LogMutex;
static uint64_t s_NextEntryID = 1;

//=============================================================================
// FORWARD DECLARATIONS
//=============================================================================

static void DrawToolbar();
static void DrawLogArea();
static void DrawInputArea();
static void DrawFilterPanel();
static void DrawSettingsPopup();
static void DrawLogEntry(const FLogEntry& entry);
static void ApplyFilters();
static void ExecuteCommand(const char* command);
static void ClearLog();
static void CopyToClipboard(bool selectedOnly = false);
static void SaveLogToFile();
static bool PassesFilter(const FLogEntry& entry);
static ImVec4 GetVerbosityColor(ELogVerbosity verbosity);
static const char* GetVerbosityName(ELogVerbosity verbosity);
static const char* GetVerbosityIcon(ELogVerbosity verbosity);
static const char* GetCategoryName(ELogCategory category);
static std::string FormatTimestamp(uint64_t timestamp);

//=============================================================================
// PUBLIC API IMPLEMENTATION
//=============================================================================

/**
 * @brief Adds a log message to the console
 * 
 * Thread-safe. Can be called from any thread.
 * 
 * @param msg The message text to log
 * 
 * TODO: Integrate with your engine's logging system via callbacks:
 * @code
 * // In your logging system initialization:
 * Log::SetCallback([](const LogMessage& msg) {
 *     ConsolePanel::AddLog(msg.Text, msg.Verbosity, msg.Category);
 * });
 * @endcode
 */
void ConsolePanel::AddLog(const std::string& msg) {
    AddLog(msg, ELogVerbosity::Info, "Log");
}

/**
 * @brief Adds a log message with verbosity and category
 * 
 * Thread-safe. Can be called from any thread.
 * 
 * @param msg The message text
 * @param verbosity Message severity level
 * @param category Source category name
 */
void ConsolePanel::AddLog(const std::string& msg, ELogVerbosity verbosity, const std::string& category) {
    std::lock_guard<std::mutex> lock(s_LogMutex);
    
    FLogEntry entry;
    entry.ID = s_NextEntryID++;
    entry.Message = msg.length() > ConsoleConfig::MAX_MESSAGE_LENGTH 
        ? msg.substr(0, ConsoleConfig::MAX_MESSAGE_LENGTH) + "..." 
        : msg;
    entry.Verbosity = verbosity;
    entry.Category = category;
    entry.CategoryType = ELogCategory::Core; // TODO: Parse category string
    
    // Get current timestamp
    auto now = std::chrono::system_clock::now();
    entry.Timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    // TODO: Get frame number from engine
    // entry.FrameNumber = Engine::GetFrameNumber();
    entry.FrameNumber = 0;
    
    // Update statistics
    s_State.TotalMessages++;
    if (verbosity == ELogVerbosity::Error || verbosity == ELogVerbosity::Fatal)
        s_State.ErrorCount++;
    else if (verbosity == ELogVerbosity::Warning)
        s_State.WarningCount++;
    
    // Add to log (FIFO with max size)
    s_LogEntries.push_back(std::move(entry));
    if (s_LogEntries.size() > ConsoleConfig::MAX_LOG_ENTRIES) {
        // Decrement stats if removing error/warning
        const auto& removed = s_LogEntries.front();
        if (removed.Verbosity == ELogVerbosity::Error || removed.Verbosity == ELogVerbosity::Fatal)
            s_State.ErrorCount--;
        else if (removed.Verbosity == ELogVerbosity::Warning)
            s_State.WarningCount--;
        
        s_LogEntries.pop_front();
    }
    
    s_State.bNeedsRefilter = true;
    
    if (s_State.bAutoScroll) {
        s_State.bScrollToBottom = true;
    }
}

/**
 * @brief Main render function for the Console Panel
 * 
 * Called every frame by the UI system.
 * 
 * Layout:
 * ┌─────────────────────────────────────────────────────────────┐
 * │ Toolbar (Clear, Copy, Save, Filter toggle, Settings)       │
 * ├──────────┬──────────────────────────────────────────────────┤
 * │ Filters  │                                                  │
 * │ (toggle) │              Log Message Area                    │
 * │          │              (scrollable)                        │
 * ├──────────┴──────────────────────────────────────────────────┤
 * │ Command Input                                        [Send] │
 * └─────────────────────────────────────────────────────────────┘
 */
// Free-function aliases to force file-scope static lookup, bypassing any
// same-named member declarations that may exist in older header revisions.
namespace { namespace ConsoleFn {
    inline void ClearLog()                    { ::RiftCore::UI::ClearLog(); }
    inline void CopyToClipboard(bool sel)     { ::RiftCore::UI::CopyToClipboard(sel); }
    inline void SaveLogToFile()               { ::RiftCore::UI::SaveLogToFile(); }
    inline void ApplyFilters()                { ::RiftCore::UI::ApplyFilters(); }
    inline void DrawToolbar()                 { ::RiftCore::UI::DrawToolbar(); }
    inline void DrawFilterPanel()             { ::RiftCore::UI::DrawFilterPanel(); }
    inline void DrawLogArea()                 { ::RiftCore::UI::DrawLogArea(); }
    inline void DrawInputArea()               { ::RiftCore::UI::DrawInputArea(); }
    inline void DrawSettingsPopup()           { ::RiftCore::UI::DrawSettingsPopup(); }
}}

void ConsolePanel::OnUIRender() {
    static ImGuiContext* s_Ctx = ImGui::GetCurrentContext();
    if (!s_Ctx) return;
    ImGui::SetCurrentContext(s_Ctx);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    bool windowOpen = ImGui::Begin("Console", nullptr, ImGuiWindowFlags_MenuBar);
    ImGui::PopStyleVar();
    if (!windowOpen) {
        ImGui::End();
        return;
    }
    
    // Menu bar
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Console")) {
            if (ImGui::MenuItem("Clear", "Ctrl+Shift+C")) ConsoleFn::ClearLog();
            if (ImGui::MenuItem("Copy All", "Ctrl+C")) ConsoleFn::CopyToClipboard(false);
            if (ImGui::MenuItem("Save to File...")) ConsoleFn::SaveLogToFile();
            ImGui::Separator();
            ImGui::MenuItem("Auto-Scroll", nullptr, &s_State.bAutoScroll);
            ImGui::MenuItem("Word Wrap", nullptr, &s_State.bWrapText);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Show Timestamps", nullptr, &s_State.bShowTimestamps);
            ImGui::MenuItem("Show Categories", nullptr, &s_State.bShowCategories);
            ImGui::MenuItem("Show Frame Numbers", nullptr, &s_State.bShowFrameNumbers);
            ImGui::MenuItem("Show Source Location", nullptr, &s_State.bShowSourceLocation);
            ImGui::Separator();
            ImGui::MenuItem("Filter Panel", nullptr, &s_State.bShowFilters);
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
    
    // Apply filters if needed
    if (s_State.bNeedsRefilter) {
        ConsoleFn::ApplyFilters();
        s_State.bNeedsRefilter = false;
    }
    
    // Toolbar
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 4));
    ConsoleFn::DrawToolbar();
    ImGui::PopStyleVar();
    
    ImGui::Separator();
    
    // Main content area
    float inputAreaHeight = 30.0f;
    float contentHeight = ImGui::GetContentRegionAvail().y - inputAreaHeight;
    
    if (s_State.bShowFilters) {
        // Filter panel on left
        ImGui::BeginChild("FilterPanel", ImVec2(s_State.FilterPanelWidth, contentHeight), true);
        ConsoleFn::DrawFilterPanel();
        ImGui::EndChild();
        
        ImGui::SameLine();
    }
    
    // Log area
    ImGui::BeginChild("LogArea", ImVec2(0, contentHeight), true, 
        s_State.bWrapText ? 0 : ImGuiWindowFlags_HorizontalScrollbar);
    ConsoleFn::DrawLogArea();
    ImGui::EndChild();
    
    // Command input area
    ConsoleFn::DrawInputArea();
    
    // Settings popup
    if (s_State.bShowSettings) {
        ConsoleFn::DrawSettingsPopup();
    }
    
    ImGui::End();
}

//=============================================================================
// INTERNAL IMPLEMENTATIONS
//=============================================================================

/**
 * @brief Draws the toolbar with quick actions
 */
static void DrawToolbar() {
    // Clear button
    if (ImGui::Button("Clear")) {
        ClearLog();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Clear all log entries (Ctrl+Shift+C)");
    
    ImGui::SameLine();
    
    // Copy button
    if (ImGui::Button("Copy")) {
        CopyToClipboard(false);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Copy all visible logs to clipboard");
    
    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();
    
    // Quick filter buttons
    ImGui::TextDisabled("Filter:");
    ImGui::SameLine();
    
    ImVec4 errorColor = GetVerbosityColor(ELogVerbosity::Error);
    ImVec4 warnColor = GetVerbosityColor(ELogVerbosity::Warning);
    
    ImGui::PushStyleColor(ImGuiCol_Text, s_State.VerbosityFilters[static_cast<int>(ELogVerbosity::Error)] 
        ? errorColor : ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
    if (ImGui::SmallButton("Errors")) {
        s_State.VerbosityFilters[static_cast<int>(ELogVerbosity::Error)] = 
            !s_State.VerbosityFilters[static_cast<int>(ELogVerbosity::Error)];
        s_State.VerbosityFilters[static_cast<int>(ELogVerbosity::Fatal)] = 
            s_State.VerbosityFilters[static_cast<int>(ELogVerbosity::Error)];
        s_State.bNeedsRefilter = true;
    }
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle errors (%u)", s_State.ErrorCount);
    
    ImGui::SameLine();
    
    ImGui::PushStyleColor(ImGuiCol_Text, s_State.VerbosityFilters[static_cast<int>(ELogVerbosity::Warning)] 
        ? warnColor : ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
    if (ImGui::SmallButton("Warnings")) {
        s_State.VerbosityFilters[static_cast<int>(ELogVerbosity::Warning)] = 
            !s_State.VerbosityFilters[static_cast<int>(ELogVerbosity::Warning)];
        s_State.bNeedsRefilter = true;
    }
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle warnings (%u)", s_State.WarningCount);
    
    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();
    
    // Search box
    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::InputTextWithHint("##Search", "Search logs...", 
        s_State.SearchBuffer, sizeof(s_State.SearchBuffer))) {
        s_State.bNeedsRefilter = true;
    }
    
    ImGui::SameLine();
    
    // Filter panel toggle
    if (ImGui::Button(s_State.bShowFilters ? "<<" : ">>")) {
        s_State.bShowFilters = !s_State.bShowFilters;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle filter panel");
    
    // Right-aligned stats
    ImGui::SameLine(ImGui::GetWindowWidth() - 200.0f);
    ImGui::TextDisabled("%zu / %zu messages", s_State.FilteredCount, s_LogEntries.size());
}

/**
 * @brief Draws the scrollable log message area
 */
static void DrawLogArea() {
    std::lock_guard<std::mutex> lock(s_LogMutex);
    
    // Use clipper for efficient rendering of large lists
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(s_FilteredEntries.size()));
    
    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
            if (i >= 0 && i < static_cast<int>(s_FilteredEntries.size())) {
                DrawLogEntry(*s_FilteredEntries[i]);
            }
        }
    }
    
    clipper.End();
    
    // Auto-scroll handling
    if (s_State.bScrollToBottom) {
        ImGui::SetScrollHereY(1.0f);
        s_State.bScrollToBottom = false;
    }
}

/**
 * @brief Draws a single log entry with formatting
 * @param entry The log entry to draw
 */
static void DrawLogEntry(const FLogEntry& entry) {
    ImVec4 color = GetVerbosityColor(entry.Verbosity);
    
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    
    std::string line;
    
    // Timestamp
    if (s_State.bShowTimestamps) {
        line += FormatTimestamp(entry.Timestamp) + " ";
    }
    
    // Frame number
    if (s_State.bShowFrameNumbers) {
        char frameBuf[16];
        snprintf(frameBuf, sizeof(frameBuf), "[F%u] ", entry.FrameNumber);
        line += frameBuf;
    }
    
    // Verbosity icon
    line += GetVerbosityIcon(entry.Verbosity);
    line += " ";
    
    // Category
    if (s_State.bShowCategories && !entry.Category.empty()) {
        line += "[" + entry.Category + "] ";
    }
    
    // Message
    line += entry.Message;
    
    // Source location
    if (s_State.bShowSourceLocation && !entry.SourceFile.empty()) {
        char locBuf[128];
        snprintf(locBuf, sizeof(locBuf), " (%s:%d)", entry.SourceFile.c_str(), entry.SourceLine);
        line += locBuf;
    }
    
    // Selectable text (allows copy)
    ImGui::PushID(static_cast<int>(entry.ID));
    
    if (s_State.bWrapText) {
        ImGui::TextWrapped("%s", line.c_str());
    } else {
        ImGui::TextUnformatted(line.c_str());
    }
    
    // Context menu for individual entry
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Copy")) {
            ImGui::SetClipboardText(line.c_str());
        }
        if (ImGui::MenuItem("Copy Message Only")) {
            ImGui::SetClipboardText(entry.Message.c_str());
        }
        if (!entry.SourceFile.empty() && ImGui::MenuItem("Go to Source")) {
            // TODO: Open source file at line
            // Editor::OpenFile(entry.SourceFile, entry.SourceLine);
        }
        ImGui::EndPopup();
    }
    
    ImGui::PopID();
    ImGui::PopStyleColor();
}

/**
 * @brief Draws the command input area at the bottom
 */
static void DrawInputArea() {
    ImGui::Separator();
    
    // Input text
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 60.0f);
    
    ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_EnterReturnsTrue | 
                                     ImGuiInputTextFlags_CallbackHistory |
                                     ImGuiInputTextFlags_CallbackCompletion;
    
    bool reclaim_focus = false;
    
    auto InputCallback = [](ImGuiInputTextCallbackData* data) -> int {
        FConsoleState* state = static_cast<FConsoleState*>(data->UserData);
        
        if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
            // Command history navigation
            const int prev_history_pos = state->HistoryIndex;
            
            if (data->EventKey == ImGuiKey_UpArrow) {
                if (state->HistoryIndex == -1)
                    state->HistoryIndex = static_cast<int>(state->CommandHistory.size()) - 1;
                else if (state->HistoryIndex > 0)
                    state->HistoryIndex--;
            } else if (data->EventKey == ImGuiKey_DownArrow) {
                if (state->HistoryIndex != -1) {
                    if (++state->HistoryIndex >= static_cast<int>(state->CommandHistory.size()))
                        state->HistoryIndex = -1;
                }
            }
            
            if (prev_history_pos != state->HistoryIndex) {
                const char* history_str = (state->HistoryIndex >= 0) 
                    ? state->CommandHistory[state->HistoryIndex].c_str() 
                    : "";
                data->DeleteChars(0, data->BufTextLen);
                data->InsertChars(0, history_str);
            }
        } else if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion) {
            // TODO: Auto-completion
            // auto suggestions = CommandProcessor::GetCompletions(data->Buf);
        }
        
        return 0;
    };
    
    if (ImGui::InputText("##Input", s_State.InputBuffer, sizeof(s_State.InputBuffer), 
        inputFlags, InputCallback, &s_State)) {
        if (s_State.InputBuffer[0] != '\0') {
            ExecuteCommand(s_State.InputBuffer);
            s_State.InputBuffer[0] = '\0';
        }
        reclaim_focus = true;
    }
    
    // Keep focus on input
    ImGui::SetItemDefaultFocus();
    if (reclaim_focus) {
        ImGui::SetKeyboardFocusHere(-1);
    }
    
    ImGui::SameLine();
    
    if (ImGui::Button("Send", ImVec2(50, 0))) {
        if (s_State.InputBuffer[0] != '\0') {
            ExecuteCommand(s_State.InputBuffer);
            s_State.InputBuffer[0] = '\0';
        }
    }
}

/**
 * @brief Draws the filter panel
 */
static void DrawFilterPanel() {
    ImGui::Text("Verbosity");
    ImGui::Separator();
    
    for (int i = 0; i < static_cast<int>(ELogVerbosity::COUNT); ++i) {
        ImVec4 color = GetVerbosityColor(static_cast<ELogVerbosity>(i));
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        if (ImGui::Checkbox(GetVerbosityName(static_cast<ELogVerbosity>(i)), &s_State.VerbosityFilters[i])) {
            s_State.bNeedsRefilter = true;
        }
        ImGui::PopStyleColor();
    }
    
    ImGui::Spacing();
    ImGui::Text("Categories");
    ImGui::Separator();
    
    for (int i = 0; i < static_cast<int>(ELogCategory::COUNT); ++i) {
        if (ImGui::Checkbox(GetCategoryName(static_cast<ELogCategory>(i)), &s_State.CategoryFilters[i])) {
            s_State.bNeedsRefilter = true;
        }
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    
    if (ImGui::SmallButton("Enable All")) {
        for (int i = 0; i < static_cast<int>(ELogVerbosity::COUNT); ++i)
            s_State.VerbosityFilters[i] = true;
        for (int i = 0; i < static_cast<int>(ELogCategory::COUNT); ++i)
            s_State.CategoryFilters[i] = true;
        s_State.bNeedsRefilter = true;
    }
    
    ImGui::SameLine();
    
    if (ImGui::SmallButton("Disable All")) {
        for (int i = 0; i < static_cast<int>(ELogVerbosity::COUNT); ++i)
            s_State.VerbosityFilters[i] = false;
        for (int i = 0; i < static_cast<int>(ELogCategory::COUNT); ++i)
            s_State.CategoryFilters[i] = false;
        s_State.bNeedsRefilter = true;
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Search Options");
    
    if (ImGui::Checkbox("Regex", &s_State.bUseRegex)) {
        s_State.bNeedsRefilter = true;
    }
    if (ImGui::Checkbox("Case Sensitive", &s_State.bCaseSensitive)) {
        s_State.bNeedsRefilter = true;
    }
}

/**
 * @brief Draws the settings popup
 */
static void DrawSettingsPopup() {
    if (ImGui::BeginPopup("ConsoleSettings")) {
        ImGui::Text("Console Settings");
        ImGui::Separator();
        
        ImGui::SliderFloat("Font Scale", &s_State.FontScale, 0.5f, 2.0f, "%.1f");
        
        ImGui::EndPopup();
    }
}

/**
 * @brief Applies current filters to the log entries
 */
static void ApplyFilters() {
    std::lock_guard<std::mutex> lock(s_LogMutex);
    
    s_FilteredEntries.clear();
    s_FilteredEntries.reserve(s_LogEntries.size());
    
    for (auto& entry : s_LogEntries) {
        if (PassesFilter(entry)) {
            s_FilteredEntries.push_back(&entry);
        }
    }
    
    s_State.FilteredCount = s_FilteredEntries.size();
}

/**
 * @brief Checks if a log entry passes current filters
 * @param entry The entry to check
 * @return true if the entry should be displayed
 */
static bool PassesFilter(const FLogEntry& entry) {
    // Verbosity filter
    if (!s_State.VerbosityFilters[static_cast<int>(entry.Verbosity)]) {
        return false;
    }
    
    // Category filter
    if (!s_State.CategoryFilters[static_cast<int>(entry.CategoryType)]) {
        return false;
    }
    
    // Text search filter
    if (s_State.SearchBuffer[0] != '\0') {
        std::string searchStr = s_State.SearchBuffer;
        std::string messageStr = entry.Message;
        
        if (!s_State.bCaseSensitive) {
            std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), ::tolower);
            std::transform(messageStr.begin(), messageStr.end(), messageStr.begin(), ::tolower);
        }
        
        if (s_State.bUseRegex) {
            try {
                std::regex pattern(searchStr, s_State.bCaseSensitive 
                    ? std::regex_constants::ECMAScript 
                    : std::regex_constants::icase);
                if (!std::regex_search(entry.Message, pattern)) {
                    return false;
                }
            } catch (...) {
                // Invalid regex, fall back to string search
                if (messageStr.find(searchStr) == std::string::npos) {
                    return false;
                }
            }
        } else {
            if (messageStr.find(searchStr) == std::string::npos) {
                return false;
            }
        }
    }
    
    return true;
}

/**
 * @brief Executes a console command
 * @param command The command string to execute
 */
static void ExecuteCommand(const char* command) {
    // Add to history
    if (s_State.CommandHistory.empty() || s_State.CommandHistory.back() != command) {
        s_State.CommandHistory.push_back(command);
        if (s_State.CommandHistory.size() > ConsoleConfig::MAX_COMMAND_HISTORY) {
            s_State.CommandHistory.erase(s_State.CommandHistory.begin());
        }
    }
    s_State.HistoryIndex = -1;
    
    // Echo command
    ConsolePanel::AddLog(std::string("> ") + command, ELogVerbosity::Info, "Console");
    
    // Parse and execute
    std::string cmd = command;
    
    // Built-in commands
    if (cmd == "clear" || cmd == "cls") {
        ClearLog();
        return;
    }
    
    if (cmd == "help") {
        ConsolePanel::AddLog("Available commands:", ELogVerbosity::Info, "Console");
        ConsolePanel::AddLog("  clear/cls - Clear console", ELogVerbosity::Info, "Console");
        ConsolePanel::AddLog("  help - Show this help", ELogVerbosity::Info, "Console");
        ConsolePanel::AddLog("  stat fps - Show FPS stats", ELogVerbosity::Info, "Console");
        // TODO: List registered commands
        return;
    }
    
    // TODO: Forward to engine command processor
    // bool handled = CommandProcessor::Execute(command);
    // if (!handled) {
    //     ConsolePanel::AddLog("Unknown command: " + cmd, ELogVerbosity::Warning, "Console");
    // }
    
    ConsolePanel::AddLog("Command not implemented: " + cmd, ELogVerbosity::Warning, "Console");
}

/**
 * @brief Clears all log entries
 */
static void ClearLog() {
    std::lock_guard<std::mutex> lock(s_LogMutex);
    s_LogEntries.clear();
    s_FilteredEntries.clear();
    s_State.TotalMessages = 0;
    s_State.FilteredCount = 0;
    s_State.ErrorCount = 0;
    s_State.WarningCount = 0;
}

/**
 * @brief Copies log entries to clipboard
 * @param selectedOnly If true, only copy selected entries (not implemented)
 */
static void CopyToClipboard(bool selectedOnly) {
    std::lock_guard<std::mutex> lock(s_LogMutex);
    (void)selectedOnly;
    
    std::string text;
    text.reserve(s_FilteredEntries.size() * 100);
    
    for (const FLogEntry* entry : s_FilteredEntries) {
        if (s_State.bShowTimestamps) {
            text += FormatTimestamp(entry->Timestamp) + " ";
        }
        text += "[" + std::string(GetVerbosityName(entry->Verbosity)) + "] ";
        if (!entry->Category.empty()) {
            text += "[" + entry->Category + "] ";
        }
        text += entry->Message + "\n";
    }
    
    ImGui::SetClipboardText(text.c_str());
}

/**
 * @brief Saves log entries to a file
 * 
 * TODO: Implement file dialog and actual file writing
 */
static void SaveLogToFile() {
    // TODO: Open save file dialog
    // std::string path = FileDialog::SaveFile("Save Log", "log files (*.log)|*.log");
    // if (!path.empty()) {
    //     std::ofstream file(path);
    //     for (const auto& entry : s_LogEntries) {
    //         file << FormatTimestamp(entry.Timestamp) << " " << entry.Message << "\n";
    //     }
    // }
    
    ConsolePanel::AddLog("Save to file not implemented", ELogVerbosity::Warning, "Console");
}

/**
 * @brief Gets the display color for a verbosity level
 * @param verbosity The verbosity level
 * @return ImGui color vector
 */
static ImVec4 GetVerbosityColor(ELogVerbosity verbosity) {
    switch (verbosity) {
        case ELogVerbosity::Trace:   return ImVec4(0.5f, 0.5f, 0.5f, 1.0f);  // Gray
        case ELogVerbosity::Debug:   return ImVec4(0.7f, 0.7f, 0.7f, 1.0f);  // Light gray
        case ELogVerbosity::Info:    return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);  // White
        case ELogVerbosity::Warning: return ImVec4(1.0f, 0.8f, 0.0f, 1.0f);  // Yellow
        case ELogVerbosity::Error:   return ImVec4(1.0f, 0.3f, 0.3f, 1.0f);  // Red
        case ELogVerbosity::Fatal:   return ImVec4(1.0f, 0.0f, 0.5f, 1.0f);  // Magenta
        default:                     return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    }
}

/**
 * @brief Gets the name of a verbosity level
 * @param verbosity The verbosity level
 * @return Human-readable name
 */
static const char* GetVerbosityName(ELogVerbosity verbosity) {
    switch (verbosity) {
        case ELogVerbosity::Trace:   return "Trace";
        case ELogVerbosity::Debug:   return "Debug";
        case ELogVerbosity::Info:    return "Info";
        case ELogVerbosity::Warning: return "Warning";
        case ELogVerbosity::Error:   return "Error";
        case ELogVerbosity::Fatal:   return "Fatal";
        default:                     return "Unknown";
    }
}

/**
 * @brief Gets an icon for a verbosity level
 * @param verbosity The verbosity level
 * @return Icon text (TODO: Replace with FontAwesome glyphs)
 */
static const char* GetVerbosityIcon(ELogVerbosity verbosity) {
    switch (verbosity) {
        case ELogVerbosity::Trace:   return "[T]";
        case ELogVerbosity::Debug:   return "[D]";
        case ELogVerbosity::Info:    return "[I]";
        case ELogVerbosity::Warning: return "[W]";
        case ELogVerbosity::Error:   return "[E]";
        case ELogVerbosity::Fatal:   return "[!]";
        default:                     return "[?]";
    }
}

/**
 * @brief Gets the name of a log category
 * @param category The category
 * @return Human-readable name
 */
static const char* GetCategoryName(ELogCategory category) {
    switch (category) {
        case ELogCategory::Core:      return "Core";
        case ELogCategory::Rendering: return "Rendering";
        case ELogCategory::Physics:   return "Physics";
        case ELogCategory::Audio:     return "Audio";
        case ELogCategory::Scripting: return "Scripting";
        case ELogCategory::Network:   return "Network";
        case ELogCategory::Input:     return "Input";
        case ELogCategory::Editor:    return "Editor";
        case ELogCategory::Game:      return "Game";
        case ELogCategory::Asset:     return "Asset";
        case ELogCategory::Custom:    return "Custom";
        default:                      return "Unknown";
    }
}

/**
 * @brief Formats a timestamp into human-readable string
 * @param timestamp Unix timestamp in milliseconds
 * @return Formatted time string
 */
static std::string FormatTimestamp(uint64_t timestamp) {
    std::time_t seconds = static_cast<std::time_t>(timestamp / 1000);
    std::tm* tm = std::localtime(&seconds);
    
    if (!tm) return "[??:??:??]";
    
    char buffer[16];
    std::strftime(buffer, sizeof(buffer), ConsoleConfig::TIMESTAMP_FORMAT, tm);
    return buffer;
}

} // namespace RiftCore::UI
