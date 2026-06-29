#include "dispatcher.hpp"

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

  return visibility == Visibility::Safe ? "OK \033[32m✔\033[0m Ingested safe wallpaper"
                                        : "OK \033[32m✔\033[0m Ingested unsafe wallpaper";
}

void CommandDispatcher::registerHandlers() {
  handlers["CYCLE"] = [this](const CommandMessage&) {
    std::lock_guard<std::mutex> lock(engineMutex.get());
    engine.get().cycle();
    return "OK \033[32m✔\033[0m Cycled to next wallpaper";
  };

  handlers["TOGGLE"] = [this](const CommandMessage&) {
    std::lock_guard<std::mutex> lock(engineMutex.get());
    engine.get().toggleMode();
    return "OK \033[32m✔\033[0m Toggled visibility state";
  };

  handlers["STATUS"] = [this](const CommandMessage&) {
    std::lock_guard<std::mutex> lock(engineMutex.get());
    const bool isSafe = engine.get().manifest.state.stateMode == StateMode::Safe;
    const std::string mode = isSafe ? "\033[32mSAFE\033[0m" : "\033[31mUNSAFE\033[0m";
    return "OK \033[34mℹ\033[0m  Current Mode: " + mode;
  };

  handlers["LIST"] = [this](const CommandMessage&) {
    std::lock_guard<std::mutex> lock(engineMutex.get());
    auto allWps = engine.get().manifest.all();
    std::string out = "OK ";

    if(allWps.empty()) {
      out += "No wallpapers found.";
      return out;
    }

    out += std::format("\033[1m{:<8} │ {:<12} │ {:<19} │ {}\033[0m\v", "HASH", "VISIBILITY", "DATE", "PATH");
    out += "─────────┼──────────────┼─────────────────────┼────────────────────────────────────────\v";

    for(const auto& wpRef : allWps) {
      const auto& wp = wpRef.get();

      std::string hashHex;
      hashHex.reserve(HASH_SIZE * 2);
      for(std::byte b : wp.hash.value) {
        hashHex += std::format("{:02x}", static_cast<unsigned char>(b));
      }

      std::string visStr;
      std::string visColor;
      switch(wp.visibility) {
      case Visibility::Safe:
        visStr = "Safe";
        visColor = "\033[32m"; // GREEN
        break;
      case Visibility::Unsafe:
        visStr = "Unsafe";
        visColor = "\033[31m"; // RED
        break;
      case Visibility::Unclassified:
        visStr = "Unclassified";
        visColor = "\033[33m"; // YELLOW
        break;
      }

      std::string dateStr = std::format("{:%Y-%m-%d %H:%M:%S}", wp.createdAt);
      std::string shortHash = hashHex.substr(0, 8);

      out += std::format("{} │ {}{:<12}\033[0m │ {} │ {}\v", shortHash, visColor, visStr, dateStr, wp.absPath.string());
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
