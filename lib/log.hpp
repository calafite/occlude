#pragma once
#include "common.hpp"
#include <chrono>
#include <cstdint>
#include <format>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

namespace logging {
  enum class Level : std::uint8_t {
    Info, // 
    Warn, // 
    Error, // 
    Debug // 
  };

  struct Logger {
    [[nodiscard]] static Logger& get() {
      static Logger instance;
      return instance;
    }

    // compliance with special member functions rule
    // singleton: no copy, no move
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

    void init(FilePath const& logFilePath) {
      logFile.open(logFilePath, std::ios::app);
    }

    template <typename... Args>
    void info(std::format_string<Args...> fmt, Args&&... args) {
      logOut(Level::Info, std::format(fmt, std::forward<Args>(args)...));
    }
    template <typename... Args>
    void warn(std::format_string<Args...> fmt, Args&&... args) {
      logOut(Level::Warn, std::format(fmt, std::forward<Args>(args)...));
    }
    template <typename... Args>
    void error(std::format_string<Args...> fmt, Args&&... args) {
      logOut(Level::Error, std::format(fmt, std::forward<Args>(args)...));
    }

  private:
    Logger() = default;
    ~Logger() = default;

    std::ofstream logFile;

    void logOut(Level level, std::string_view message) {
      const auto now = std::chrono::system_clock::now();
      const std::string timestamp = std::format("{:%Y-%m-%d %H:%M:%S}", now);
      std::string levelStr;
      std::string colorCode;

      switch(level) {
        case Level::Info: {
          levelStr = "INFO ";
          colorCode = "\033[32m"; // GREEN
          break;
        }
        case Level::Warn: {
          levelStr = "WARN ";
          colorCode = "\033[33m"; // YELLOW
          break;
        }
        case Level::Error: {
          levelStr = "ERROR";
          colorCode = "\033[31m"; // RED
          break;
        }
        case Level::Debug: {
          levelStr = "DEBUG";
          colorCode = "\033[36m"; // CYAN
          break;
        }
      }

      const bool isErrorLevel = level == Level::Error;
      std::ostream& console = isErrorLevel ? std::cerr : std::cout;
      console << std::format("[{}] {}{}\033[0m: {}\n", timestamp, colorCode, levelStr, message);

      const bool logFileOpen = logFile.is_open();
      if(logFileOpen) {
        logFile << std::format("[{}] {}: {}\n", timestamp, levelStr, message);
        logFile.flush();
      }
    }
  };

  template <typename... Args>
  inline void info(std::format_string<Args...> fmt, Args&&... args) {
    Logger::get().info(fmt, std::forward<Args>(args)...);
  }
  template <typename... Args>
  inline void warn(std::format_string<Args...> fmt, Args&&... args) {
    Logger::get().warn(fmt, std::forward<Args>(args)...);
  }
  template <typename... Args>
  inline void error(std::format_string<Args...> fmt, Args&&... args) {
    Logger::get().error(fmt, std::forward<Args>(args)...);
  }
} // namespace log
