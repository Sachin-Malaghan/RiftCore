#include <Core/Logger.h>
#include <iostream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <iomanip>

// Undefine Windows FormatMessage macro if it exists
// This conflicts with our Logger::FormatMessage method name
#ifdef FormatMessage
    #undef FormatMessage
#endif

#ifdef RIFTCORE_PLATFORM_WINDOWS
    #include <Windows.h>
    #ifdef FormatMessage
        #undef FormatMessage
    #endif
#endif










namespace RiftCore {

    static void SetWindowsConsoleColor(LogLevel level) {
#ifdef RIFTCORE_PLATFORM_WINDOWS
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        WORD color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
        switch(level) {
            case LogLevel::Trace:
                color = FOREGROUND_INTENSITY;
                break;
            case LogLevel::Debug:
                color = FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
                break;
            case LogLevel::Info:
                color = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
                break;
            case LogLevel::Warning:
                color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
                break;
            case LogLevel::Error:
                color = FOREGROUND_RED | FOREGROUND_INTENSITY;
                break;
            case LogLevel::Critical:
                color = FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
                break;
            default: break;
        }
        SetConsoleTextAttribute(h, color);
#else
        RIFTCORE_UNUSED(level);
#endif
    }

    static void ResetWindowsConsoleColor() {
#ifdef RIFTCORE_PLATFORM_WINDOWS
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleTextAttribute(h,
            FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
#endif
    }

    // ConsoleLogSink
    void ConsoleLogSink::Write(LogLevel level, LogCategory category,
                               const String& message,
                               const char* file, int line) {
        RIFTCORE_UNUSED(category);
        RIFTCORE_UNUSED(file);
        RIFTCORE_UNUSED(line);
        SetWindowsConsoleColor(level);
        std::cout << message << "\n";
        ResetWindowsConsoleColor();
    }

    void ConsoleLogSink::Flush() {
        std::cout.flush();
    }

    // FileLogSink
    FileLogSink::FileLogSink(const String& filePath)
        : file_(filePath, std::ios::out | std::ios::app) {
        if (!file_.is_open()) {
            std::cerr << "[Logger] Failed to open: " << filePath << "\n";
        }
    }

    FileLogSink::~FileLogSink() {
        if (file_.is_open()) {
            file_.flush();
            file_.close();
        }
    }

    void FileLogSink::Write(LogLevel level, LogCategory category,
                            const String& message,
                            const char* file, int line) {
        RIFTCORE_UNUSED(level);
        RIFTCORE_UNUSED(category);
        RIFTCORE_UNUSED(file);
        RIFTCORE_UNUSED(line);
        std::lock_guard<std::mutex> lock(mutex_);
        if (file_.is_open()) {
            file_ << message << "\n";
        }
    }

    void FileLogSink::Flush() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (file_.is_open()) file_.flush();
    }

    // Logger
    Logger::Logger() : minLevel_(LogLevel::Debug) {
        AddSink(std::make_shared<ConsoleLogSink>());
    }

    Logger::~Logger() { Flush(); }

    void Logger::AddSink(std::shared_ptr<ILogSink> sink) {
        std::lock_guard<std::mutex> lock(mutex_);
        sinks_.push_back(std::move(sink));
    }

    void Logger::ClearSinks() {
        std::lock_guard<std::mutex> lock(mutex_);
        sinks_.clear();
    }

    void Logger::SetLevel(LogLevel level) { minLevel_ = level; }
    LogLevel Logger::GetLevel() const     { return minLevel_;  }

    void Logger::Log(LogLevel level, LogCategory category,
                     const String& message,
                     const char* file, int line) {
        if (level < minLevel_) return;
        String formatted = BuildMessage(level, category, message, file, line);
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& sink : sinks_) {
            sink->Write(level, category, formatted, file, line);
        }
    }

    void Logger::Flush() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& sink : sinks_) sink->Flush();
    }

    String Logger::GetTimestamp() {
        auto now  = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms   = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        std::ostringstream oss;
        struct tm tmInfo = {};
#ifdef RIFTCORE_PLATFORM_WINDOWS
        localtime_s(&tmInfo, &time);
#else
        localtime_r(&time, &tmInfo);
#endif
        oss << std::put_time(&tmInfo, "%H:%M:%S")
            << "." << std::setfill('0') << std::setw(3) << ms.count();
        return oss.str();
    }

    String Logger::BuildMessage(LogLevel level, LogCategory category,
                                 const String& message,
                                 const char* file, int line) {
        std::ostringstream oss;
        oss << "[" << GetTimestamp()              << "]"
            << "[" << LogLevelToString(level)     << "]"
            << "[" << (category ? category : "General") << "] "
            << message;
        if (file && level >= LogLevel::Error) {
            oss << "  (" << file << ":" << line << ")";
        }
        return oss.str();
    }

} // namespace RiftCore
