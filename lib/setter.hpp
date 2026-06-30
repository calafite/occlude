#pragma once
#include "common.hpp"
#include "settings.hpp"

#include <array>
#include <cerrno>
#include <concepts>
#include <cstdio>
#include <cstdlib>
#include <expected>
#include <fcntl.h>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

enum class CommandError : std::uint8_t { ExecutionFailed };

template<typename T>
concept CommandRunner = requires(T t, std::string const& cmd, std::string const& path) {
  { t.run(cmd, path) } -> std::same_as<std::expected<void, CommandError>>;
  { t.runYieldOutput(cmd) } -> std::same_as<std::expected<std::string, CommandError>>;
};

// this is to silence the similar arguments warning
// i do not want to add another struct just for this
// NOLINTNEXTLINE
inline std::vector<std::string> splitCommandArgs(const std::string& cmd, const std::string& pathReplacement) {
  std::vector<std::string> args;
  std::istringstream iss(cmd);
  std::string arg;
  bool replaced = false;

  while(iss >> arg) {
    size_t pos = arg.find("{path}");
    if(pos != std::string::npos) {
      arg.replace(pos, 6, pathReplacement);
      replaced = true;
    }
    args.push_back(arg);
  }

  if(!pathReplacement.empty() && !replaced) {
    args.push_back(pathReplacement);
  }

  return args;
}

struct SystemCommandRunner {
  [[nodiscard]] static std::expected<void, CommandError> run(std::string const& cmdTemplate, std::string const& path) {
    std::vector<std::string> argsStr = splitCommandArgs(cmdTemplate, path);
    if(argsStr.empty()) {
      return std::unexpected(CommandError::ExecutionFailed);
    }

    std::vector<char*> args;
    args.reserve(argsStr.size() + 1);
    for(auto& s : argsStr) {
      args.push_back(s.data());
    }
    args.push_back(nullptr);

    pid_t pid = fork();
    if(pid < 0) {
      return std::unexpected(CommandError::ExecutionFailed);
    }

    if(pid == 0) {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
      int devNull = open("/dev/null", O_WRONLY);
      if(devNull >= 0) {
        dup2(devNull, STDOUT_FILENO);
        dup2(devNull, STDERR_FILENO);
        close(devNull);
      }
      execvp(args[0], args.data());
      _exit(127);
    }

    int status = 0;
    if(waitpid(pid, &status, 0) < 0) {
      return std::unexpected(CommandError::ExecutionFailed);
    }

    if(WIFEXITED(status) && WEXITSTATUS(status) == 0) {
      return {};
    }
    return std::unexpected(CommandError::ExecutionFailed);
  }

  [[nodiscard]] static std::expected<std::string, CommandError> runYieldOutput(std::string const& cmd) {
    std::vector<std::string> argsStr = splitCommandArgs(cmd, "");
    if(argsStr.empty()) {
      return std::unexpected(CommandError::ExecutionFailed);
    }

    std::vector<char*> args;
    args.reserve(argsStr.size() + 1);
    for(auto& s : argsStr) {
      args.push_back(s.data());
    }
    args.push_back(nullptr);

    std::array<int, 2> pipeFd{};
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    if(pipe(pipeFd.data()) < 0) {
      return std::unexpected(CommandError::ExecutionFailed);
    }

    pid_t pid = fork();
    if(pid < 0) {
      close(pipeFd[0]);
      close(pipeFd[1]);
      return std::unexpected(CommandError::ExecutionFailed);
    }

    if(pid == 0) {
      close(pipeFd[0]);
      dup2(pipeFd[1], STDOUT_FILENO);
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
      int devNull = open("/dev/null", O_WRONLY);
      if(devNull >= 0) {
        dup2(devNull, STDERR_FILENO);
        close(devNull);
      }
      close(pipeFd[1]);

      execvp(args[0], args.data());
      _exit(127);
    }

    close(pipeFd[1]);

    std::string result;
    std::array<char, 128> buffer{};
    while(true) {
      ssize_t n = read(pipeFd[0], buffer.data(), buffer.size());
      if(n < 0) {
        if(errno == EINTR) {
          continue;
        }
        close(pipeFd[0]);
        waitpid(pid, nullptr, 0);
        return std::unexpected(CommandError::ExecutionFailed);
      }
      if(n == 0) {
        break;
      }
      result.append(buffer.data(), static_cast<std::size_t>(n));
    }
    close(pipeFd[0]);

    int status = 0;
    if(waitpid(pid, &status, 0) < 0) {
      return std::unexpected(CommandError::ExecutionFailed);
    }

    if(WIFEXITED(status) && WEXITSTATUS(status) == 0) {
      if(!result.empty() && result.back() == '\n') {
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
    return runner.get().run(settings.get().setterCommandTemplate, absPath.string());
  }

private:
  std::reference_wrapper<Runner> runner;
  std::reference_wrapper<const Settings> settings;
};
