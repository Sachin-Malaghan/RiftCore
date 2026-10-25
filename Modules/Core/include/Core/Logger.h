#pragma once

#pragma warning(push)
#pragma warning(disable: 4251 4275)

#include <RiftCore/Common/Platform.h>
#include <RiftCore/Common/Types.h>
#include <RiftCore/Core/ILogger.h>

#include <fstream>
#include <mutex>
#include <vector>
#include <memory>
#include <sstream>
#include <chrono>
#include <ctime>
#include <iomanip>

#ifdef CORE_EXPORTS
    #define CORE_API RIFTCORE_EXPORT
#else
    #define CORE_API RIFTCORE_IMPORT
#endif







namespace RiftCore {

    class ILogSink {
    public:
        virtual ~ILogSink() = default;
        virtual void Write(
            LogLevel       level,
            LogCategory    category,
            const String&  message,
            const char*    file,
            int            line
        ) = 0;
        virtual void Flush() = 0;
    };

    class CORE_API ConsoleLogSink : public ILogSink {
    public:
        void Write(LogLevel level, LogCategory category,
                   const String& message,
                   const char* file, int line) override;
        void Flush() override;
    };

    class CORE_API FileLogSink : public ILogSink {
    public:
        explicit FileLogSink(const String& filePath);
        ~FileLogSink();
        void Write(LogLevel level, LogCategory category,
                   const String& message,
                   const char* file, int line) override;
        void Flush() override;
    private:
        std::ofstream  file_;
        std::mutex     mutex_;
    };

    class CORE_API Logger : public ILogger {
    public:
        Logger();
        ~Logger();

        void Log(LogLevel level, LogCategory category,
                 const String& message,
                 const char* file, int line) override;

        void     SetLevel(LogLevel level) override;
        LogLevel GetLevel() const override;
        void     Flush() override;

        void AddSink(std::shared_ptr<ILogSink> sink);
        void ClearSinks();

    private:
        LogLevel                               minLevel_;
        std::vector<std::shared_ptr<ILogSink>> sinks_;
        std::mutex                             mutex_;

        String BuildMessage(
            LogLevel level, LogCategory category,
            const String& message,
            const char* file, int line
        );
        String GetTimestamp();
    };

} // namespace RiftCore

#pragma warning(pop)
