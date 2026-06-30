#pragma once

#include "../lib/engine.hpp"
#include "../lib/settings.hpp"
#include "parsing.hpp"
#include "scanner.hpp"

#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

struct CommandDispatcher {
  using Handler = std::function<std::string(const CommandMessage&)>;

  CommandDispatcher(
      Engine<
          RealFileSystem,         //
          SystemCommandRunner     //
          >& engineRef,           //
      std::mutex& engineMutexRef, //
      const Settings& settingsRef,//
      WallpaperScanner& scannerRef//
  );

  [[nodiscard]] std::string dispatch(const CommandMessage& message);

private:
  void registerHandlers();
  
  [[nodiscard]] std::string handleCycle(const CommandMessage& message);
  [[nodiscard]] std::string handleToggle(const CommandMessage& message);
  [[nodiscard]] std::string handleStatus(const CommandMessage& message);
  [[nodiscard]] std::string handleList(const CommandMessage& message);
  [[nodiscard]] std::string handleIngest(const CommandMessage& message, Visibility visibility);
  [[nodiscard]] std::string handleCurrent(const CommandMessage& message);
  [[nodiscard]] std::string handleScan(const CommandMessage& message);
  [[nodiscard]] std::string handleClassify(const CommandMessage& message);

  std::reference_wrapper<Engine<RealFileSystem, SystemCommandRunner>> engine;
  std::reference_wrapper<std::mutex> engineMutex;
  std::reference_wrapper<const Settings> settings;
  std::reference_wrapper<WallpaperScanner> scanner;
  
  std::unordered_map<std::string, Handler> handlers;
};
