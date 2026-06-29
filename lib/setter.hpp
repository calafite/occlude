#pragma once
#include "common.hpp"
#include "settings.hpp"

#include <array>
#include <concepts>
#include <cstdlib>
#include <cstdio>
#include <expected>
#include <string>


enum class CommandError : std::uint8_t { ExecutionFailed };

template<typename T>
concept CommandRunner = requires(T t, std::string const& cmd) {
  { t.run(cmd) } -> std::same_as<std::expected<void, CommandError>>;
  { t.runYieldOutput(cmd) } -> std::same_as<std::expected<std::string, CommandError>>;
};

struct SystemCommandRunner {
  [[nodiscard]] static std::expected<void, CommandError> run(std::string const& cmd) {
    int result = std::system(cmd.c_str());
    if(result == 0) {
      return {};
    }
    return std::unexpected(CommandError::ExecutionFailed);
  }

  [[nodiscard]] static std::expected<std::string, CommandError> runYieldOutput(std::string const& cmd) {
    const auto *const cCmd = cmd.c_str();
    FILE* pipe = popen(cCmd, "r");

    if(!static_cast<bool>(pipe)) {
      return std::unexpected(CommandError::ExecutionFailed);
    }

    std::string result;
    std::array<char, 128> buffer{};

    int bufferSize = static_cast<int>(buffer.size());

    while(fgets(buffer.data(), bufferSize, pipe) != nullptr) {
      result.append(buffer.data());
    }

    int returnValue = pclose(pipe);
    if(returnValue == 0) {
      bool resultEmpty = result.empty();
      bool isNewline = result.back() == '\n';
      if(!resultEmpty && isNewline) {
        result.pop_back();
      }
      return result;
    }
    return std::unexpected(CommandError::ExecutionFailed);
  }
};
static_assert(CommandRunner<SystemCommandRunner>, "SystemCommandRunner must satisfy CommandRunner");

template<CommandRunner Runner>
struct WallpaperSetter {
  WallpaperSetter(Runner& runnerRef, Settings const& settingsRef) : runner(runnerRef), settings(settingsRef) {}

  std::expected<void, CommandError> apply(FilePath const& absPath) {
    std::string cmd = settings.get().setterCommandTemplate;
    std::string const search = "{path}";

    std::size_t pos = cmd.find(search);
    if(pos != std::string::npos) {
      cmd.replace(pos, search.length(), absPath.string());
    } else {
      cmd += " " + absPath.string();
    }

    return runner.get().run(cmd);
  }

private:
  std::reference_wrapper<Runner> runner;
  std::reference_wrapper<const Settings> settings;
};
