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
    uint32_t        SourceLine;     ///< Source line number
    ELogVerbosity   Verbosity;      ///< Severity level
    ELogCategory    Category;       ///< System category
    ELogCategory    CategoryType;   ///< Category type (alias)
    double          Timestamp;      ///< Time since startup
    uint32_t        FrameNumber;    ///< Frame when logged
    uint32_t        RepeatCount;    ///< Consecutive repeat count
    bool            bIsCollapsed;   ///< Collapsed with repeats
    
    FLogEntry()
        : ID(0), SourceLine(0), Verbosity(ELogVerbosity::Info), Category(ELogCategory::Core),
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
    // Lifecycle
    //-------------------------------------------------------------------------
    
    /**
     * @brief Initializes the console panel
     * 
     * Registers built-in commands and sets up log capture.
     */
    void Initialize();
    
    /**
     * @brief Shuts down the panel
     */
    void Shutdown();
    
    //-------------------------------------------------------------------------
    // Rendering
    //-------------------------------------------------------------------------
    
    /**
     * @brief Main render function called every frame
     */
    void OnUIRender();
    
    //-------------------------------------------------------------------------
    // Logging API
    //-------------------------------------------------------------------------
    
    /**
     * @brief Adds a simple log message
     * @param msg Message text
     */
    void AddLog(const std::string& msg);
    
    /**
     * @brief Adds a log message with verbosity and category
     * @param msg Message text
     * @param verbosity Severity level
     * @param category Category name string
     */
    void AddLog(const std::string& msg, ELogVerbosity verbosity, const std::string& category);
    
    /**
     * @brief Adds a log message with full metadata
     * @param verbosity Severity level
     * @param category System category
     * @param message Message text
     * @param source Source identifier (optional)
     */
    void Log(ELogVerbosity verbosity, ELogCategory category,
             const std::string& message, const std::string& source = "");
    
    /** Convenience methods for different verbosity levels */
    void LogTrace(const std::string& msg, ELogCategory cat = ELogCategory::Core);
    void LogDebug(const std::string& msg, ELogCategory cat = ELogCategory::Core);
    void LogInfo(const std::string& msg, ELogCategory cat = ELogCategory::Core);
    void LogWarning(const std::string& msg, ELogCategory cat = ELogCategory::Core);
    void LogError(const std::string& msg, ELogCategory cat = ELogCategory::Core);
    void LogFatal(const std::string& msg, ELogCategory cat = ELogCategory::Core);
    
    /**
     * @brief Clears all log entries
     */
    void Clear();
    
    //-------------------------------------------------------------------------
    // Command System
    //-------------------------------------------------------------------------
    
    /**
     * @brief Registers a console command
     * @param name Command name (case-insensitive)
     * @param description Help text
     * @param handler Function to execute
     */
    void RegisterCommand(const std::string& name, const std::string& description,
                        std::function<void(const std::vector<std::string>&)> handler);
    
    /**
     * @brief Unregisters a console command
     * @param name Command name
     */
    void UnregisterCommand(const std::string& name);
    
    /**
     * @brief Executes a command string
     * @param commandLine Full command with arguments
     */
    void ExecuteCommand(const std::string& commandLine);
    
    //-------------------------------------------------------------------------
    // Configuration
    //-------------------------------------------------------------------------
    
    /**
     * @brief Gets the filter settings
     * @return Reference to filter struct
     */
    FConsoleFilter& GetFilter();
    
    /**
     * @brief Sets the maximum number of log entries to keep
     * @param maxEntries Max entries (0 = unlimited)
     */
    void SetMaxEntries(size_t maxEntries);
    
    /**
     * @brief Enables/disables auto-scroll
     * @param enabled Auto-scroll state
     */
    void SetAutoScroll(bool enabled);
    
    /**
     * @brief Enables/disables timestamp display
     * @param show Show timestamps
     */
    void SetShowTimestamps(bool show);
    
    /**
     * @brief Enables/disables collapsing repeated messages
     * @param collapse Collapse state
     */
    void SetCollapseRepeats(bool collapse);
    
    //-------------------------------------------------------------------------
    // Export
    //-------------------------------------------------------------------------
    
    /**
     * @brief Exports log to file
     * @param filepath Output file path
     * @param includeMetadata Include timestamps/categories
     * @return true on success
     */
    bool ExportToFile(const std::string& filepath, bool includeMetadata = true);
    
    /**
     * @brief Copies selected/all logs to clipboard
     * @param selectedOnly Only copy selected entries
     */
    void CopyToClipboard(bool selectedOnly = false);
    
    //-------------------------------------------------------------------------
    // Singleton Access
    //-------------------------------------------------------------------------
    
    /**
     * @brief Gets the global console instance
     * @return Reference to singleton
     */
    static ConsolePanel& Get();
    
private:
    //-------------------------------------------------------------------------
    // Internal State
    //-------------------------------------------------------------------------
    
    std::vector<FLogEntry>      m_Logs;
    std::vector<FLogEntry>      m_FilteredLogs;
    std::vector<FConsoleCommand> m_Commands;
    std::vector<std::string>    m_CommandHistory;
    
    FConsoleFilter              m_Filter;
    char                        m_InputBuffer[1024];
    char                        m_SearchBuffer[256];
    
    int                         m_HistoryIndex;
    size_t                      m_MaxEntries;
    uint64_t                    m_NextLogID;
    
    bool                        m_bAutoScroll;
    bool                        m_bScrollToBottom;
    bool                        m_bShowTimestamps;
    bool                        m_bShowCategories;
    bool                        m_bCollapseRepeats;
    
    mutable std::mutex          m_LogMutex;
    
    //-------------------------------------------------------------------------
    // Internal Methods
    //-------------------------------------------------------------------------
    
    void DrawToolbar();
    void DrawFilterPopup();
    void DrawLogArea();
    void DrawCommandInput();
    void DrawLogEntry(const FLogEntry& entry, int index);
    void ApplyFilters();
    void HandleKeyboardShortcuts();
    void RegisterBuiltInCommands();
    std::vector<std::string> GetAutocompleteSuggestions(const std::string& partial);
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
