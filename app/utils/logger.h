#pragma once

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>

namespace Emulator
{

enum class LogLevel : int
{
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3,
};

class Logger
{
  public:
    static Logger& instance()
    {
        static Logger inst;
        return inst;
    }

    LogLevel level() const { return level_; }

    void log(LogLevel lvl, std::string_view msg)
    {
        if (lvl < level_)
        {
            return;
        }

        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

        std::lock_guard<std::mutex> lock(mutex_);
        std::cerr << "[" << level_str(lvl) << "] " << "[" << format_time(t, ms.count()) << "] " << msg << "\n";
    }

  private:
    Logger()
    {
        const char* env = std::getenv("CUEMU_LOG_LEVEL");
        if (env != nullptr)
        {
            std::string_view s(env);
            if (s == "DEBUG")
            {
                level_ = LogLevel::DEBUG;
            }
            else if (s == "INFO")
            {
                level_ = LogLevel::INFO;
            }
            else if (s == "WARNING")
            {
                level_ = LogLevel::WARNING;
            }
            else if (s == "ERROR")
            {
                level_ = LogLevel::ERROR;
            }
        }
    }

    static std::string_view level_str(LogLevel lvl)
    {
        switch (lvl)
        {
            case LogLevel::DEBUG:
                return "DEBUG  ";
            case LogLevel::INFO:
                return "INFO   ";
            case LogLevel::WARNING:
                return "WARNING";
            case LogLevel::ERROR:
                return "ERROR  ";
        }
        return "UNKNOWN";
    }

    static std::string format_time(std::time_t t, long ms_part)
    {
        std::tm tm_info{};
        localtime_r(&t, &tm_info);
        char buf[16];
        std::strftime(buf, sizeof(buf), "%H:%M:%S", &tm_info);
        std::ostringstream oss;
        oss << buf << "." << std::setfill('0') << std::setw(3) << ms_part;
        return oss.str();
    }

    LogLevel level_ = LogLevel::INFO;
    std::mutex mutex_;
};

inline void log(LogLevel lvl, std::string_view msg)
{
    Logger::instance().log(lvl, msg);
}

} // namespace Emulator

// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define LOG_DEBUG(msg)   ::Emulator::log(::Emulator::LogLevel::DEBUG, (msg))
#define LOG_INFO(msg)    ::Emulator::log(::Emulator::LogLevel::INFO, (msg))
#define LOG_WARNING(msg) ::Emulator::log(::Emulator::LogLevel::WARNING, (msg))
#define LOG_ERROR(msg)   ::Emulator::log(::Emulator::LogLevel::ERROR, (msg))
// NOLINTEND(cppcoreguidelines-macro-usage)
