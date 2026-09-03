// ============================================================
// ILogger.h
// Logging interface. Core DLL implements this.
// All other DLLs get ILogger from EngineContext.
// ============================================================
#pragma once

#include "../Common/Platform.h"
#include "../Common/Types.h"







namespace RiftCore {

    // ── Log levels ────────────────────────────────────────────
    enum class LogLevel : u8 {
        Trace    = 0,    // Very detailed — function entry/exit
        Debug    = 1,    // Debug information
        Info     = 2,    // Normal operational messages
        Warning  = 3,    // Something unexpected but not fatal
        Error    = 4,    // Something went wrong — recoverable
        Critical = 5,    // Fatal error — engine should abort
        Off      = 6     // Silence all logging
    };

    // Convert LogLevel to string
    inline const char* LogLevelToString(LogLevel level) {
        switch(level) {
            case LogLevel::Trace:    return "TRACE";
            case LogLevel::Debug:    return "DEBUG";
            case LogLevel::Info:     return "INFO";
            case LogLevel::Warning:  return "WARNING";
            case LogLevel::Error:    return "ERROR";
            case LogLevel::Critical: return "CRITICAL";
            default:                 return "UNKNOWN";
        }
    }

    // ── Log category ──────────────────────────────────────────
    // Allows filtering by system: "Renderer", "Physics", etc.
    using LogCategory = const char*;

    // ── ILogger interface ─────────────────────────────────────
    class ILogger {
    public:
        virtual ~ILogger() = default;

        // ── Core log function ─────────────────────────────────
        virtual void Log(
            LogLevel     level,
            LogCategory  category,
            const String& message,
            const char*  file  = nullptr,
            int          line  = 0
        ) = 0;

        // ── Control ───────────────────────────────────────────
        virtual void SetLevel(LogLevel level) = 0;
        virtual LogLevel GetLevel() const     = 0;
        virtual void Flush()                  = 0;

        // ── Convenience wrappers ──────────────────────────────
        void Trace   (LogCategory cat, const String& msg) {
            Log(LogLevel::Trace,    cat, msg); }
        void Debug   (LogCategory cat, const String& msg) {
            Log(LogLevel::Debug,    cat, msg); }
        void Info    (LogCategory cat, const String& msg) {
            Log(LogLevel::Info,     cat, msg); }
        void Warning (LogCategory cat, const String& msg) {
            Log(LogLevel::Warning,  cat, msg); }
        void Error   (LogCategory cat, const String& msg) {
            Log(LogLevel::Error,    cat, msg); }
        void Critical(LogCategory cat, const String& msg) {
            Log(LogLevel::Critical, cat, msg); }
    };

    // ── Logging Macros ────────────────────────────────────────
    // These macros add file/line info automatically.
    // Usage: RIFT_LOG_INFO("Renderer", "Shader compiled");

    #define RIFT_LOG(logger, level, category, msg) \
        if(logger) (logger)->Log(level, category, msg, __FILE__, __LINE__)

    #define RIFT_LOG_TRACE(logger, cat, msg) \
        RIFT_LOG(logger, RiftCore::LogLevel::Trace,    cat, msg)
    #define RIFT_LOG_DEBUG(logger, cat, msg) \
        RIFT_LOG(logger, RiftCore::LogLevel::Debug,    cat, msg)
    #define RIFT_LOG_INFO(logger, cat, msg) \
        RIFT_LOG(logger, RiftCore::LogLevel::Info,     cat, msg)
    #define RIFT_LOG_WARN(logger, cat, msg) \
        RIFT_LOG(logger, RiftCore::LogLevel::Warning,  cat, msg)
    #define RIFT_LOG_ERROR(logger, cat, msg) \
        RIFT_LOG(logger, RiftCore::LogLevel::Error,    cat, msg)
    #define RIFT_LOG_CRITICAL(logger, cat, msg) \
        RIFT_LOG(logger, RiftCore::LogLevel::Critical, cat, msg)

} // namespace RiftCore
