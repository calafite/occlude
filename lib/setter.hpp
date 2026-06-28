#pragma once
#include "common.hpp"
#include "settings.hpp"
#include <concepts>
#include <cstdlib>
#include <expected>
#include <string>

enum class CommandError : std::uint8_t {
  ExecutionFailed
};

template<typename T>
concept CommandRunner = requires(T t, std::string const& cmd) {
  { t.run(cmd) } -> std::same_as<std::expected<void, CommandError>>;
};

struct SystemCommandRunner {
  [[nodiscard]] static std::expected<void, CommandError> run(std::string const& cmd) {
    int result = std::system(cmd.c_str());
    if (result == 0) {
      return {};
    }
    return std::unexpected(CommandError::ExecutionFailed);
  }
};
static_assert(CommandRunner<SystemCommandRunner>, "SystemCommandRunner must satisfy CommandRunner");

template<CommandRunner Runner>
struct WallpaperSetter {
  WallpaperSetter(Runner& runnerRef, Settings const& settingsRef)
      : runner(runnerRef), settings(settingsRef) {}

  std::expected<void, CommandError> apply(FilePath const& absPath) {
    std::string cmd = settings.get().setterCommandTemplate;
    std::string const search = "{path}";
    
    std::size_t pos = cmd.find(search);
    if (pos != std::string::npos) {
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
