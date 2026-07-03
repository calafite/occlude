#include "../../lib/utils/common.hpp"
#include "../../lib/core/config.hpp"
#include "../../lib/core/engine.hpp"
#include "../../lib/ipc/ipc.hpp"
#include "../../lib/utils/log.hpp"
#include "../../lib/io/setter.hpp"
#include "cycler.hpp"
#include "dispatcher.hpp"
#include "parsing.hpp"
#include "scanner.hpp"

#include <csignal>
#include <filesystem>
#include <mutex>
#include <string>

int main() {
  std::signal(SIGPIPE, SIG_IGN);
  const std::string logPath = getHomeDirectory() + "/.config/occlude/daemon.log";
  logging::Logger::get().init(logPath);
  logging::info("Starting occlude daemon...");
  
  logging::Logger::get().init(logPath);
  logging::info("Starting occlude daemon...");

  const Settings settings = ConfigManager::loadOrInit();

  std::filesystem::create_directories(settings.publicRoot);
  std::filesystem::create_directories(settings.privateRoot);
  std::filesystem::create_directories(settings.unclassifiedRoot);

  RealFileSystem rfs;
  SystemCommandRunner runner;
  Engine<RealFileSystem, SystemCommandRunner> engine(rfs, runner, settings);

  std::mutex engineMutex;

  WallpaperScanner scanner(engine, engineMutex, settings);
  scanner.start();

  WallpaperCycler cycler(engine, engineMutex, settings);
  cycler.start();

  CommandDispatcher dispatcher(engine, engineMutex, settings, scanner);

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

    logging::info("Received command: {}", msg.command);

    try {
      std::string response = dispatcher.dispatch(msg);
      const auto _ = conn.send(response);
    } catch(const std::exception& ex) {
      logging::error("Exception caught while executing command '{}': {}", msg.command, ex.what());
      const auto _ = conn.send(std::string("ERR ") + ex.what());
    }
  }

  return 0;
}
