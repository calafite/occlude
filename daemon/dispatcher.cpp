#include "dispatcher.hpp"

#include "utils.hpp"

#include <format>

CommandDispatcher::CommandDispatcher(
    Engine<
        RealFileSystem,          //
        SystemCommandRunner      //
        >& engineRef,            //
    std::mutex& engineMutexRef,  //
    const Settings& settingsRef, //
    WallpaperScanner& scannerRef //
)
    : engine(engineRef),           //
      engineMutex(engineMutexRef), //
      settings(settingsRef),       //
      scanner(scannerRef) {        //
  registerHandlers();
}

std::string CommandDispatcher::dispatch(const CommandMessage& message) {
  auto iterator = handlers.find(message.command);
  const bool commandFound = iterator != handlers.end();
  if(commandFound) {
    return iterator->second(message);
  }
  return "ERR Unknown command";
}

std::string CommandDispatcher::handleCycle(const CommandMessage& /*message*/) {
  std::lock_guard<std::mutex> lock(engineMutex.get());
  engine.get().cycle();
  return "OK \033[32m✔\033[0m Cycled to next wallpaper";
}

std::string CommandDispatcher::handleToggle(const CommandMessage& /*message*/) {
  std::lock_guard<std::mutex> lock(engineMutex.get());
  engine.get().toggleMode();
  return "OK \033[32m✔\033[0m Toggled visibility state";
}

std::string CommandDispatcher::handleStatus(const CommandMessage& /*message*/) {
  std::lock_guard<std::mutex> lock(engineMutex.get());
  const bool isSafeMode = engine.get().manifest.state.stateMode == StateMode::Safe;
  const std::string modeString = isSafeMode ? "\033[32mSAFE\033[0m" : "\033[31mUNSAFE\033[0m";
  return "OK \033[34mℹ\033[0m Current Mode: " + modeString;
}

std::string CommandDispatcher::handleList(const CommandMessage& /*message*/) {
  std::lock_guard<std::mutex> lock(engineMutex.get());
  const auto allWallpapers = engine.get().manifest.all();

  const bool hasNoWallpapers = allWallpapers.empty();
  if(hasNoWallpapers) {
    return "OK No wallpapers found.";
  }

  std::string output = "OK ";
  output += std::format("\033[1m{:<8} │ {:<12} │ {:<19} │ {}\033[0m\v", "HASH", "VISIBILITY", "DATE", "PATH");
  output += "─────────┼──────────────┼─────────────────────┼────────────────────────────────────────\v";

  for(const auto& wallpaperRef : allWallpapers) {
    const auto& wallpaper = wallpaperRef.get();

    std::string hashHex;
    hashHex.reserve(HASH_SIZE * 2);
    for(std::byte byte : wallpaper.hash.value) {
      hashHex += std::format("{:02x}", static_cast<unsigned char>(byte));
    }

    std::string visibilityString;
    std::string visibilityColor;
    switch(wallpaper.visibility) {
    case Visibility::Safe: {
      visibilityString = "Safe";
      visibilityColor = "\033[32m";
      break;
    }
    case Visibility::Unsafe: {
      visibilityString = "Unsafe";
      visibilityColor = "\033[31m";
      break;
    }
    case Visibility::Unclassified: {
      visibilityString = "Unclassified";
      visibilityColor = "\033[33m";
      break;
    }
    }

    const std::string dateString = std::format("{:%Y-%m-%d %H:%M:%S}", wallpaper.createdAt);
    const std::string shortHash = hashHex.substr(0, 8);

    output += std::format(
        "{} │ {}{:<12}\033[0m │ {} │ {}\v", //
        shortHash,                          //
        visibilityColor,                    //
        visibilityString,                   //
        dateString,                         //
        wallpaper.absPath.string()          //
    );
  }

  const bool trailingTab = output.back() == '\v';
  if(trailingTab) {
    output.pop_back();
  }
  return output;
}

std::string CommandDispatcher::handleIngest(const CommandMessage& message, Visibility visibility) {
  const bool emptyArgument = message.argument.empty();
  if(emptyArgument) {
    return "ERR Ingest requires a <path>";
  }
  const FilePath path = message.argument;

  bool wasActive = false;
  const auto currentResult = SystemCommandRunner::runYieldOutput(settings.get().getterCommandTemplate);
  const bool hasCurrentResult = currentResult.has_value();
  if(hasCurrentResult) {
    const FilePath absolutePath = resolveTilde(path);
    const FilePath absoluteCurrent = resolveTilde(*currentResult);
    const bool pathsEqual = absolutePath == absoluteCurrent;
    if(pathsEqual) {
      wasActive = true;
    }
  }

  std::lock_guard<std::mutex> lock(engineMutex.get());
  const Hash hash = engine.get().wallpaperStore.ingest(path, visibility);
  engine.get().manifestStore.save(engine.get().manifest);

  if(wasActive) {
    const bool isSafeMode = engine.get().manifest.state.stateMode == StateMode::Safe;
    const Visibility expectedVisibility = isSafeMode ? Visibility::Safe : Visibility::Unsafe;
    const bool visibilityMatches = expectedVisibility == visibility;

    if(visibilityMatches) {
      engine.get().applyWallpaper(hash);
    } else {
      engine.get().cycle();
    }
  }

  const bool isSafeType = visibility == Visibility::Safe;
  return isSafeType ? "OK \033[32m✔\033[0m Ingested safe wallpaper" : "OK \033[32m✔\033[0m Ingested unsafe wallpaper";
}

std::string CommandDispatcher::handleCurrent(const CommandMessage& /*message*/) {
  const auto currentResult = SystemCommandRunner::runYieldOutput(settings.get().getterCommandTemplate);
  const bool hasResult = currentResult.has_value();
  if(hasResult) {
    return "OK " + *currentResult;
  }
  return "ERR Failed to get current wallpaper";
}

std::string CommandDispatcher::handleScan(const CommandMessage& /*message*/) {
  scanner.get().scanNow();
  return "OK \033[32m✔\033[0m Forced wallpaper ingestion of the download directory";
}

std::string CommandDispatcher::handleClassify(const CommandMessage& message) {
  const bool emptyArgument = message.argument.empty();
  if(emptyArgument) {
    return "ERR Classify requires <hash> <safe|unsafe|unclassified>";
  }

  const std::size_t spaceIndex = message.argument.find(' ');
  const bool invalidFormat = spaceIndex == std::string::npos;
  if(invalidFormat) {
    return "ERR Invalid arguments";
  }

  const std::string hashHex = message.argument.substr(0, spaceIndex);
  const std::string visibilityString = message.argument.substr(spaceIndex + 1);

  Visibility visibility{};
  const bool isSafe = visibilityString == "safe";
  const bool isUnsafe = visibilityString == "unsafe";
  const bool isUnclassified = visibilityString == "unclassified";

  if(isSafe) {
    visibility = Visibility::Safe;
  } else if(isUnsafe) {
    visibility = Visibility::Unsafe;
  } else if(isUnclassified) {
    visibility = Visibility::Unclassified;
  } else {
    return std::format("ERR Unknown visibility '{}'", visibilityString);
  }

  std::lock_guard<std::mutex> lock(engineMutex.get());
  try {
    const Hash hash(hashHex);
    engine.get().classify(hash, visibility);
    return std::format("OK \033[32m✔\033[0m Wallpaper classified as {}", visibilityString);
  } catch(const std::exception& exception) {
    return std::format("ERR {}", exception.what());
  }
}

void CommandDispatcher::registerHandlers() {
  handlers["CYCLE"] = [this](const CommandMessage& message) {
    return handleCycle(message);
  };
  handlers["TOGGLE"] = [this](const CommandMessage& message) {
    return handleToggle(message);
  };
  handlers["STATUS"] = [this](const CommandMessage& message) {
    return handleStatus(message);
  };
  handlers["LIST"] = [this](const CommandMessage& message) {
    return handleList(message);
  };
  handlers["CURRENT"] = [this](const CommandMessage& message) {
    return handleCurrent(message);
  };
  handlers["SCAN"] = [this](const CommandMessage& message) {
    return handleScan(message);
  };
  handlers["CLASSIFY"] = [this](const CommandMessage& message) {
    return handleClassify(message);
  };

  handlers["INGEST_SAFE"] = [this](const CommandMessage& message) {
    return handleIngest(message, Visibility::Safe);
  };
  handlers["INGEST_UNSAFE"] = [this](const CommandMessage& message) {
    return handleIngest(message, Visibility::Unsafe);
  };
}
