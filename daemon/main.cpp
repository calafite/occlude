#include "../lib/common.hpp"
#include "../lib/config.hpp"
#include "../lib/engine.hpp"
#include "../lib/ipc.hpp"
#include "../lib/log.hpp"
#include "../lib/setter.hpp"
#include "parsing.hpp"

#include <filesystem>
#include <string>

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
int main() {
  const char* const home = std::getenv("HOME");
  const bool hasHomeEnv = home != nullptr;
  const std::string homeDir = hasHomeEnv ? std::string(home) : "/tmp";
  const std::string logPath = homeDir + "/.config/occlude/daemon.log";

  logging::Logger::get().init(logPath);
  logging::info("Starting occlude daemon...");

  const Settings settings = ConfigManager::loadOrInit();

  std::filesystem::create_directories(settings.publicRoot);
  std::filesystem::create_directories(settings.privateRoot);

  RealFileSystem rfs;
  SystemCommandRunner runner;
  Engine<RealFileSystem, SystemCommandRunner> engine(rfs, runner, settings);

  auto serverResult = IPC::Server::create();
  const bool serverCreated = serverResult.has_value();
  if(!serverCreated) {
    logging::error("Fatal: Failed to create IPC socket.");
    return 1;
  }
  logging::info("Listening for IPC on {}", serverResult->path);

  while(true) {
    auto connResult = serverResult->accept();
    const bool connAccepted = connResult.has_value();
    if(!connAccepted) {
      continue;
    }

    auto& conn = *connResult;
    auto msgResult = conn.receive();
    const bool msgReceived = msgResult.has_value();
    if(!msgReceived) {
      continue;
    }

    const CommandMessage msg = parseIpcMessage(*msgResult);
    const bool hasCommand = !msg.command.empty();
    if(!hasCommand) {
      continue;
    }

    const std::string cmd = msg.command;
    logging::info("Received command: {}", cmd);

    try {
      const bool isCycle = cmd == "CYCLE";
      if(isCycle) {
        engine.cycle();
        const auto _ = conn.send("OK Cycled to next wallpaper");
        continue;
      }

      const bool isToggle = cmd == "TOGGLE";
      if(isToggle) {
        engine.toggleMode();
        const auto _ = conn.send("OK Toggled visibility state");
        continue;
      }

      const bool isStatus = cmd == "STATUS";
      if(isStatus) {
        const bool isSafe = engine.manifest.state.stateMode == StateMode::Safe;
        const std::string mode = isSafe ? "SAFE" : "UNSAFE";
        const auto _ = conn.send("OK Current Mode: " + mode);
        continue;
      }

      const bool isIngestSafe = cmd == "INGEST_SAFE";
      if(isIngestSafe) {
        const bool hasPathArg = !msg.argument.empty();
        if(!hasPathArg) {
          const auto _ = conn.send("ERR Ingest safe requires a <path>");
          continue;
        }
        const FilePath path = msg.argument;
        engine.wallpaperStore.ingest(path, Visibility::Safe);
        engine.manifestStore.save(engine.manifest);
        const auto _ = conn.send("OK Ingested safe wallpaper");
        continue;
      }

      const bool isIngestUnsafe = cmd == "INGEST_UNSAFE";
      if(isIngestUnsafe) {
        const bool hasPathArg = !msg.argument.empty();
        if(!hasPathArg) {
          const auto _ = conn.send("ERR Ingest unsafe requires a <path>");
          continue;
        }
        const FilePath path = msg.argument;
        engine.wallpaperStore.ingest(path, Visibility::Unsafe);
        engine.manifestStore.save(engine.manifest);
        const auto _ = conn.send("OK Ingested unsafe wallpaper");
        continue;
      }

      const auto _ = conn.send("ERR Unknown command");
    } catch(const std::exception& ex) {
      logging::error("Exception caught while executing command '{}': {}", cmd, ex.what());
      const auto _ = conn.send(std::format("ERR {}", ex.what()));
    }
  }

  return 0;
}
