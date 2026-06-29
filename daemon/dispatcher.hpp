#pragma once

#include "../lib/engine.hpp"
#include "../lib/settings.hpp"
#include "parsing.hpp"

#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

struct CommandDispatcher {
  using Handler = std::function<std::string(const CommandMessage&)>;

  CommandDispatcher(
      Engine<
          RealFileSystem,                  //
          SystemCommandRunner>& engineRef, //
      std::mutex& engineMutexRef,          //
      const Settings& settingsRef          //
  );

  [[nodiscard]] std::string dispatch(const CommandMessage& msg);

private:
  void registerHandlers();
  std::string handleIngest(const CommandMessage& msg, Visibility visibility);
  std::reference_wrapper<Engine<RealFileSystem, SystemCommandRunner>> engine;
  std::reference_wrapper<std::mutex> engineMutex;
  std::reference_wrapper<const Settings> settings;
  std::unordered_map<std::string, Handler> handlers;
};
