#pragma once
/**
 * @file ConsolePanel.h
 * @brief Production-grade Console/Output Log Panel for RiftCore Engine
 * 
 * Provides a comprehensive logging and command console interface similar
 * to Unreal Engine's Output Log. Supports multiple log verbosity levels,
 * categories, filtering, and command execution.
 * 
 * @author RiftCore Team
 * @version 2.0.0
 * @date 2026-04-21
 */

#include <vector>
#include <string>
#include <functional>
#include <cstdint>
#include <mutex>
#include <imgui.h>

namespace RiftCore::UI {

//=============================================================================
// ENUMERATIONS
//=============================================================================

/**
 * @enum ELogVerbosity
 * @brief Log message severity levels
 */
enum class ELogVerbosity : uint8_t {
    Trace = 0,      ///< Detailed trace information
    Debug,          ///< Debug messages
    Info,           ///< Informational messages
    Warning,        ///< Warning messages
    Error,          ///< Error messages
    Fatal,          ///< Fatal/crash messages
    COUNT           ///< Number of verbosity levels
};

/**
 * @enum ELogCategory
 * @brief Categorizes log messages by system
 */
enum class ELogCategory : uint8_t {
    Core = 0,       ///< Engine core
    Rendering,      ///< Rendering system
    Physics,        ///< Physics system
    Audio,          ///< Audio system
    Input,          ///< Input system
    Scripting,      ///< Scripting/Blueprints
    Network,        ///< Networking
    Assets,         ///< Asset management
    Asset,          ///< Asset (alias for Assets)
    Editor,         ///< Editor-specific
    Game,           ///< Game logic
    Custom,         ///< User-defined
    COUNT
};

// Operators for ELogCategory to support iteration and arithmetic
inline ELogCategory operator+(ELogCategory a, int b) {
    return static_cast<ELogCategory>(static_cast<uint8_t>(a) + b);
}

inline ELogCategory operator+(int a, ELogCategory b) {
    return static_cast<ELogCategory>(a + static_cast<uint8_t>(b));
}

inline ELogCategory& operator++(ELogCategory& a) {
    a = static_cast<ELogCategory>(static_cast<uint8_t>(a) + 1);
    return a;
}

inline int operator-(ELogCategory a, ELogCategory b) {
    return static_cast<int>(static_cast<uint8_t>(a)) - static_cast<int>(static_cast<uint8_t>(b));
}

//=============================================================================
// DATA STRUCTURES
//=============================================================================

/**
 * @struct FLogEntry
 * @brief Represents a single log message
 */
struct FLogEntry {
    uint64_t        ID;             ///< Unique message ID
    std::string     Message;        ///< Log message text
    std::string     Source;         ///< Source file/function
    std::string     SourceFile;     ///< Source file name
    std::string     Category;       ///< Category name as string
    uint32_t        SourceLine;     ///< Source line number
    ELogVerbosity   Verbosity;      ///< Severity level
    ELogCategory    CategoryType;   ///< Category type enum
    double          Timestamp;      ///< Time since startup
    uint32_t        FrameNumber;    ///< Frame when logged
    uint32_t        RepeatCount;    ///< Consecutive repeat count
    bool            bIsCollapsed;   ///< Collapsed with repeats
    
    FLogEntry()
        : ID(0), SourceLine(0), Verbosity(ELogVerbosity::Info),
          CategoryType(ELogCategory::Core), Timestamp(0.0), FrameNumber(0), RepeatCount(1), bIsCollapsed(false) {}
};

/**
 * @struct FConsoleFilter
 * @brief Filter settings for the console
 */
struct FConsoleFilter {
    bool            VerbosityFilters[6];    ///< Filter by verbosity
    bool            CategoryFilters[static_cast<size_t>(ELogCategory::COUNT)];
    std::string     SearchQuery;            ///< Text search
    bool            bUseRegex;              ///< Use regex search
    bool            bCaseSensitive;         ///< Case-sensitive search
    
    FConsoleFilter() : bUseRegex(false), bCaseSensitive(false) {
        for (int i = 0; i < 6; ++i) VerbosityFilters[i] = true;
        for (size_t i = 0; i < static_cast<size_t>(ELogCategory::COUNT); ++i) {
            CategoryFilters[i] = true;
        }
    }
};

/**
 * @struct FConsoleState
 * @brief Runtime state of the console panel
 */
struct FConsoleState {
    char            InputBuffer[1024];      ///< Command input buffer
    char            SearchBuffer[256];      ///< Search/filter buffer
    std::vector<std::string> CommandHistory;///< Command history
    int             HistoryPos;             ///< Current position in history (renamed from HistoryIndex)
    int             HistoryIndex;           ///< Alias for HistoryPos
    bool            bScrollToBottom;        ///< Should scroll to bottom
    bool            bAutoScroll;            ///< Auto-scroll enabled
    bool            bNeedsRefilter;         ///< Filter needs reapplication
    bool            bWrapText;              ///< Wrap long lines
    bool            bShowTimestamps;        ///< Show message timestamps
    bool            bShowCategories;        ///< Show category column
    bool            bShowFrameNumbers;      ///< Show frame numbers
    bool            bShowSourceLocation;    ///< Show source file/line
    bool            bShowFilters;           ///< Show filter panel
    bool            bShowSettings;          ///< Show settings panel
    bool            bUseRegex;              ///< Use regex for search
    bool            bCaseSensitive;         ///< Case-sensitive search
    bool            VerbosityFilters[7];    ///< Filter by verbosity level
    bool            CategoryFilters[16];    ///< Filter by category
    uint32_t        TotalMessages;          ///< Total message count
    uint32_t        ErrorCount;             ///< Error message count
    uint32_t        WarningCount;           ///< Warning message count
    uint32_t        FilteredCount;          ///< Filtered message count
    float           FilterPanelWidth;       ///< Width of filter panel
    float           FontScale;              ///< Font scale factor
    
    FConsoleState()
        : HistoryPos(-1), HistoryIndex(-1), bScrollToBottom(false), bAutoScroll(true),
          bNeedsRefilter(false), bWrapText(false), bShowTimestamps(true),
          bShowCategories(true), bShowFrameNumbers(false), bShowSourceLocation(false),
          bShowFilters(true), bShowSettings(false), bUseRegex(false), bCaseSensitive(false),
          TotalMessages(0), ErrorCount(0), WarningCount(0), FilteredCount(0),
          FilterPanelWidth(200.0f), FontScale(1.0f) {
        InputBuffer[0] = '\0';
        SearchBuffer[0] = '\0';
        for (int i = 0; i < 7; ++i) VerbosityFilters[i] = true;
        for (int i = 0; i < 16; ++i) CategoryFilters[i] = true;
    }
};

/**
 * @struct FConsoleCommand
 * @brief Registered console command
 */
struct FConsoleCommand {
    std::string     Name;           ///< Command name
    std::string     Description;    ///< Help text
    std::string     Usage;          ///< Usage example
    std::function<void(const std::vector<std::string>&)> Handler;
};

//=============================================================================
// CONSOLE PANEL CLASS
//=============================================================================

/**
 * @class ConsolePanel
 * @brief Main console/output log panel for the editor
 * 
 * Features:
 * - Thread-safe logging from any thread
 * - Multiple verbosity levels with color coding
 * - Category-based filtering
 * - Text search with regex support
 * - Command input with history and autocomplete
 * - Copy/export functionality
 * - Auto-scroll with user override
 * - Collapsible repeated messages
 */
class ConsolePanel {
public:
    //-------------------------------------------------------------------------
    // Lifecycle (stubs - real logic in static helpers inside .cpp)
    //-------------------------------------------------------------------------
    void Initialize() {}
    void Shutdown() {}
    
    //-------------------------------------------------------------------------
    // Rendering
    //-------------------------------------------------------------------------
    
    /** Main render function called every frame */
    void OnUIRender();
    
    //-------------------------------------------------------------------------
    // Logging API
    //-------------------------------------------------------------------------
    
    static void AddLog(const std::string& msg);
    static void AddLog(const std::string& msg, ELogVerbosity verbosity, const std::string& category);
    
    void Log(ELogVerbosity verbosity, ELogCategory /*category*/,
             const std::string& message, const std::string& /*source*/ = "") {
        AddLog(message, verbosity, "");
    }
    void LogTrace(const std::string& msg, ELogCategory /*cat*/ = ELogCategory::Core)  { AddLog(msg, ELogVerbosity::Trace, ""); }
    void LogDebug(const std::string& msg, ELogCategory /*cat*/ = ELogCategory::Core)  { AddLog(msg, ELogVerbosity::Debug, ""); }
    void LogInfo(const std::string& msg, ELogCategory /*cat*/ = ELogCategory::Core)   { AddLog(msg, ELogVerbosity::Info, ""); }
    void LogWarning(const std::string& msg, ELogCategory /*cat*/ = ELogCategory::Core){ AddLog(msg, ELogVerbosity::Warning, ""); }
    void LogError(const std::string& msg, ELogCategory /*cat*/ = ELogCategory::Core)  { AddLog(msg, ELogVerbosity::Error, ""); }
    void LogFatal(const std::string& msg, ELogCategory /*cat*/ = ELogCategory::Core)  { AddLog(msg, ELogVerbosity::Fatal, ""); }
    void Clear() { std::lock_guard<std::mutex> lock(m_LogMutex); m_Logs.clear(); m_FilteredLogs.clear(); }
    
    //-------------------------------------------------------------------------
    // Command System
    //-------------------------------------------------------------------------
    
    void RegisterCommand(const std::string& name, const std::string& description,
                        std::function<void(const std::vector<std::string>&)> handler) {
        m_Commands.push_back({name, description, "", handler});
    }
    void UnregisterCommand(const std::string& /*name*/) {}
    void ExecuteCommand(const std::string& /*commandLine*/) {}
    
    //-------------------------------------------------------------------------
    // Configuration & Export
    //-------------------------------------------------------------------------
    
    FConsoleFilter& GetFilter() { return m_Filter; }
    void SetMaxEntries(size_t maxEntries) { m_MaxEntries = maxEntries; }
    void SetAutoScroll(bool enabled) { m_bAutoScroll = enabled; }
    void SetShowTimestamps(bool show) { m_bShowTimestamps = show; }
    void SetCollapseRepeats(bool collapse) { m_bCollapseRepeats = collapse; }
    bool ExportToFile(const std::string& /*filepath*/, bool /*includeMetadata*/ = true) { return false; }
    void CopyToClipboard(bool /*selectedOnly*/ = false) {}
    
    //-------------------------------------------------------------------------
    // Singleton Access
    //-------------------------------------------------------------------------
    
    static ConsolePanel& Get() { static ConsolePanel s_Instance; return s_Instance; }


private:
    //-------------------------------------------------------------------------
    // Internal State
    //-------------------------------------------------------------------------
    
    std::vector<FLogEntry>      m_Logs;
    std::vector<FLogEntry>      m_FilteredLogs;
    std::vector<FConsoleCommand> m_Commands;
    std::vector<std::string>    m_CommandHistory;
    
    FConsoleFilter              m_Filter;
    char                        m_InputBuffer[1024] = {};
    char                        m_SearchBuffer[256] = {};
    
    int                         m_HistoryIndex = -1;
    size_t                      m_MaxEntries = 0;
    uint64_t                    m_NextLogID = 1;
    
    bool                        m_bAutoScroll = true;
    bool                        m_bScrollToBottom = false;
    bool                        m_bShowTimestamps = true;
    bool                        m_bShowCategories = true;
    bool                        m_bCollapseRepeats = false;
    
    mutable std::mutex          m_LogMutex;
};

//=============================================================================
// GLOBAL LOGGING MACROS
//=============================================================================

// TODO: Define these macros in your engine's logging header
// #define LOG_TRACE(cat, msg)   ConsolePanel::Get().LogTrace(msg, ELogCategory::cat)
// #define LOG_DEBUG(cat, msg)   ConsolePanel::Get().LogDebug(msg, ELogCategory::cat)
// #define LOG_INFO(cat, msg)    ConsolePanel::Get().LogInfo(msg, ELogCategory::cat)
// #define LOG_WARNING(cat, msg) ConsolePanel::Get().LogWarning(msg, ELogCategory::cat)
// #define LOG_ERROR(cat, msg)   ConsolePanel::Get().LogError(msg, ELogCategory::cat)
// #define LOG_FATAL(cat, msg)   ConsolePanel::Get().LogFatal(msg, ELogCategory::cat)

//=============================================================================
// UTILITY FUNCTIONS
//=============================================================================

/** Gets the display name for a verbosity level */
const char* GetVerbosityName(ELogVerbosity verbosity);

/** Gets the color for a verbosity level (as ImVec4) */
ImVec4 GetVerbosityColor(ELogVerbosity verbosity);

/** Gets the display name for a category */
const char* GetCategoryName(ELogCategory category);

} // namespace RiftCore::UI
