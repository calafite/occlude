#include "dispatcher.hpp"

#include "../lib/log.hpp"
#include "utils.hpp"

#include <format>

CommandDispatcher::CommandDispatcher(
    Engine<
        RealFileSystem,                  //
        SystemCommandRunner>& engineRef, //
    std::mutex& engineMutexRef,          //
    const Settings& settingsRef          //
)
    : engine(engineRef),           //
      engineMutex(engineMutexRef), //
      settings(settingsRef) {
  registerHandlers();
}

std::string CommandDispatcher::dispatch(const CommandMessage& msg) {
  auto it = handlers.find(msg.command);
  if(it != handlers.end()) {
    return it->second(msg);
  }
  return "ERR Unknown command";
}

std::string CommandDispatcher::handleIngest(const CommandMessage& msg, Visibility visibility) {
  if(msg.argument.empty()) {
    return "ERR Ingest requires a <path>";
  }
  const FilePath path = msg.argument;

  bool wasActive = false;
  auto currentResult = SystemCommandRunner::runYieldOutput(settings.get().getterCommandTemplate);
  if(currentResult) {
    const FilePath absPath = resolveTilde(path);
    const FilePath absCurrent = resolveTilde(*currentResult);
    if(absPath == absCurrent) {
      wasActive = true;
    }
  }

  std::lock_guard<std::mutex> lock(engineMutex.get());
  const Hash hash = engine.get().wallpaperStore.ingest(path, visibility);
  engine.get().manifestStore.save(engine.get().manifest);

  if(wasActive) {
    Visibility expectedVisibility =
        (engine.get().manifest.state.stateMode == StateMode::Safe) ? Visibility::Safe : Visibility::Unsafe;

    if(expectedVisibility == visibility) {
      engine.get().applyWallpaper(hash);
    } else {
      engine.get().cycle();
    }
  }

  return visibility == Visibility::Safe ? "OK Ingested safe wallpaper" : "OK Ingested unsafe wallpaper";
}

void CommandDispatcher::registerHandlers() {
  handlers["CYCLE"] = [this](const CommandMessage&) {
    std::lock_guard<std::mutex> lock(engineMutex.get());
    engine.get().cycle();
    return "OK Cycled to next wallpaper";
  };

  handlers["TOGGLE"] = [this](const CommandMessage&) {
    std::lock_guard<std::mutex> lock(engineMutex.get());
    engine.get().toggleMode();
    return "OK Toggled visibility state";
  };

  handlers["STATUS"] = [this](const CommandMessage&) {
    std::lock_guard<std::mutex> lock(engineMutex.get());
    const bool isSafe = engine.get().manifest.state.stateMode == StateMode::Safe;
    const std::string mode = isSafe ? "SAFE" : "UNSAFE";
    return "OK Current Mode: " + mode;
  };

  handlers["LIST"] = [this](const CommandMessage&) {
    std::lock_guard<std::mutex> lock(engineMutex.get());
    auto allWps = engine.get().manifest.all();
    std::string out = "OK ";

    if(allWps.empty()) {
      out += "No wallpapers found.";
      return out;
    }

    out += "Hash | Path | Visibility | Date\v";
    for(const auto& wpRef : allWps) {
      const auto& wp = wpRef.get();

      std::string hashHex;
      hashHex.reserve(HASH_SIZE * 2);
      for(std::byte b : wp.hash.value) {
        hashHex += std::format("{:02x}", static_cast<unsigned char>(b));
      }

      std::string visStr;
      switch(wp.visibility) {
      case Visibility::Safe:
        visStr = "Safe";
        break;
      case Visibility::Unsafe:
        visStr = "Unsafe";
        break;
      case Visibility::Unclassified:
        visStr = "Unclassified";
        break;
      }

      std::string dateStr = std::format("{:%Y-%m-%d %H:%M:%S}", wp.createdAt);
      out += std::format("{} | {} | {} | {}\v", hashHex.substr(0, 8) + "...", wp.absPath.string(), visStr, dateStr);
    }

    if(out.back() == '\v') {
      out.pop_back();
    }
    return out;
  };

  handlers["INGEST_SAFE"] = [this](const CommandMessage& msg) {
    return handleIngest(msg, Visibility::Safe);
  };

  handlers["INGEST_UNSAFE"] = [this](const CommandMessage& msg) {
    return handleIngest(msg, Visibility::Unsafe);
  };

  handlers["CURRENT"] = [this](const CommandMessage&) {
    auto out = SystemCommandRunner::runYieldOutput(settings.get().getterCommandTemplate);
    if(static_cast<bool>(out)) {
      return "OK " + *out;
    }
    return std::string("ERR Failed to get current wallpaper");
  };
}
