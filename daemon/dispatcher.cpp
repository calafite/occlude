#include "dispatcher.hpp"

#include "../lib/fs.hpp"

#include <format>
#include <nlohmann/json.hpp>

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

    std::string hashHex = wallpaper.hash.toString();
    std::string visibilityString(toString(wallpaper.visibility));
    std::string visibilityColor;

    switch(wallpaper.visibility) {
    case Visibility::Safe:
      visibilityColor = "\033[32m";
      break;
    case Visibility::Unsafe:
      visibilityColor = "\033[31m";
      break;
    case Visibility::Unclassified:
      visibilityColor = "\033[33m";
      break;
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

  std::lock_guard<std::mutex> lock(engineMutex.get());
  const Hash hash = engine.get().wallpaperStore.ingest(path, visibility);
  engine.get().manifestStore.save(engine.get().manifest);

  engine.get().resolveActiveIngestion(hash, path, visibility);

  const bool isSafeType = visibility == Visibility::Safe;
  return isSafeType ? "OK \033[32m✔\033[0m Ingested safe wallpaper" : "OK \033[32m✔\033[0m Ingested unsafe wallpaper";
}

std::string CommandDispatcher::handleCurrent(const CommandMessage& /*message*/) {
  const auto currentResult = engine.get().runner.get().runYieldOutput(settings.get().getterCommandTemplate);
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

std::string CommandDispatcher::handleDump(const CommandMessage& /*message*/) {
  std::lock_guard<std::mutex> lock(engineMutex.get());

  nlohmann::json root;
  const bool isSafeMode = engine.get().manifest.state.stateMode == StateMode::Safe;
  root["state"]["mode"] = isSafeMode ? "Safe" : "Unsafe";
  root["state"]["publicCurrent"] = engine.get().manifest.state.publicCurrent.value_or("");
  root["state"]["privateCurrent"] = engine.get().manifest.state.privateCurrent.value_or("");

  auto wallpapersArray = nlohmann::json::array();
  const auto allWallpapers = engine.get().manifest.all();
  for(const auto& wallpaperRef : allWallpapers) {
    const auto& wallpaper = wallpaperRef.get();

    nlohmann::json wallpaperObj;
    wallpaperObj["hash"] = wallpaper.hash.toString();
    wallpaperObj["path"] = wallpaper.absPath.string();
    wallpaperObj["visibility"] = toString(wallpaper.visibility);
    wallpaperObj["createdAt"] = std::format("{:%Y-%m-%d %H:%M:%S}", wallpaper.createdAt);

    if(wallpaper.lastShown.has_value()) {
      wallpaperObj["lastShown"] = std::format("{:%Y-%m-%d %H:%M:%S}", *wallpaper.lastShown);
    } else {
      wallpaperObj["lastShown"] = nullptr;
    }

    wallpapersArray.push_back(wallpaperObj);
  }

  root["wallpapers"] = wallpapersArray;
  return "OK " + root.dump();
}

std::string CommandDispatcher::handleApply(const CommandMessage& message) {
  const bool emptyArgument = message.argument.empty();
  if(emptyArgument) {
    return "ERR Apply requires a <hash>";
  }

  std::lock_guard<std::mutex> lock(engineMutex.get());
  try {
    const Hash hash(message.argument);
    const bool success = engine.get().applyWallpaper(hash);
    if(success) {
      return "OK \033[32m✔\033[0m Wallpaper applied successfully";
    }
    return "ERR Failed to apply wallpaper";
  } catch(const std::exception& exception) {
    return std::format("ERR {}", exception.what());
  }
}

std::string CommandDispatcher::handleDelete(const CommandMessage& message) {
  const bool emptyArgument = message.argument.empty();
  if(emptyArgument) {
    return "ERR Delete requires a <hash>";
  }
  std::lock_guard<std::mutex> lock(engineMutex.get());
  try {
    const Hash hash(message.argument);
    engine.get().deleteWallpaper(hash);
    return "OK \033[32m✔\033[0m Wallpaper deleted successfully";
  } catch(const std::exception& exception) {
    return std::format("ERR {}", exception.what());
  }
}

std::string CommandDispatcher::handleRename(const CommandMessage& message) {
  const std::size_t spaceIndex = message.argument.find(' ');
  if(spaceIndex == std::string::npos) {
    return "ERR Rename requires <hash> <new_name>";
  }

  const std::string hashHex = message.argument.substr(0, spaceIndex);
  const std::string newName = message.argument.substr(spaceIndex + 1);

  std::lock_guard<std::mutex> lock(engineMutex.get());
  try {
    const Hash hash(hashHex);
    engine.get().renameWallpaper(hash, newName);
    return "OK \033[32m✔\033[0m Wallpaper renamed successfully";
  } catch(const std::exception& exception) {
    return std::format("ERR {}", exception.what());
  }
}

std::string CommandDispatcher::handleSetMode(const CommandMessage& message) {
  const bool emptyArgument = message.argument.empty();
  if(emptyArgument) {
    return "ERR Set mode requires <safe|unsafe>";
  }

  StateMode targetMode{};
  const bool isSafe = message.argument == "safe";
  const bool isUnsafe = message.argument == "unsafe";

  if(isSafe) {
    targetMode = StateMode::Safe;
  } else if(isUnsafe) {
    targetMode = StateMode::Unsafe;
  } else {
    return std::format("ERR Unknown mode '{}'", message.argument);
  }

  std::lock_guard<std::mutex> lock(engineMutex.get());
  try {
    engine.get().setMode(targetMode);
    const std::string modeString = isSafe ? "SAFE" : "UNSAFE";
    return std::format("OK \033[32m✔\033[0m Switched to {} mode", modeString);
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
  handlers["DUMP"] = [this](const CommandMessage& message) {
    return handleDump(message);
  };
  handlers["APPLY"] = [this](const CommandMessage& message) {
    return handleApply(message);
  };
  handlers["DELETE"] = [this](const CommandMessage& message) {
    return handleDelete(message);
  };
  handlers["RENAME"] = [this](const CommandMessage& message) {
    return handleRename(message);
  };
  handlers["SET_MODE"] = [this](const CommandMessage& message) {
    return handleSetMode(message);
  };

  handlers["INGEST_SAFE"] = [this](const CommandMessage& message) {
    return handleIngest(message, Visibility::Safe);
  };
  handlers["INGEST_UNSAFE"] = [this](const CommandMessage& message) {
    return handleIngest(message, Visibility::Unsafe);
  };
}
