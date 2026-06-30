#include "scanner.hpp"

#include "../lib/log.hpp"
#include "utils.hpp"

#include <chrono>

WallpaperScanner::WallpaperScanner(
    Engine<
        RealFileSystem,         //
        SystemCommandRunner     //
        >& engineRef,           //
    std::mutex& engineMutexRef, //
    const Settings& settingsRef //
)
    : engine(engineRef),           //
      engineMutex(engineMutexRef), //
      settings(settingsRef) {}

void WallpaperScanner::start() {
  const uint32_t interval = settings.get().scanIntervalMinutes;
  const std::string& directoryPath = settings.get().defaultDownloadDirectory;

  const bool hasNoDirectory = directoryPath.empty();
  const bool hasNoInterval = interval == 0;
  if(hasNoDirectory || hasNoInterval) {
    return;
  }

  worker = std::jthread([this](const std::stop_token& stopToken) {
    run(stopToken);
  });
}

void WallpaperScanner::run(const std::stop_token& stopToken) {
  const uint32_t interval = settings.get().scanIntervalMinutes;
  const std::string& directoryPath = settings.get().defaultDownloadDirectory;

  logging::info("Scanner thread started for directory: {} (interval: {}m)", directoryPath, interval);

  while(!stopToken.stop_requested()) {
    scanNow();

    for(uint32_t i = 0; i < interval * 60; ++i) {
      const bool stopRequested = stopToken.stop_requested();
      if(stopRequested) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }
}

void WallpaperScanner::scanNow() {
  const std::string& directoryPath = settings.get().defaultDownloadDirectory;
  const bool hasNoDirectory = directoryPath.empty();
  if(hasNoDirectory) {
    return;
  }

  try {
    const FilePath directory = resolveTilde(directoryPath);
    const bool exists = std::filesystem::exists(directory);
    const bool isDirectory = std::filesystem::is_directory(directory);

    if(exists && isDirectory) {
      for(const auto& entry : std::filesystem::directory_iterator(directory)) {
        const bool isRegular = entry.is_regular_file();
        if(!isRegular) {
          continue;
        }

        const std::string filename = entry.path().filename().string();
        const std::string extension = entry.path().extension().string();

        const bool isHidden = filename.starts_with(".");
        const bool isTemp = extension == ".crdownload" || //
            extension == ".part" ||                       //
            extension == ".tmp";

        if(isHidden || isTemp) {
          continue;
        }

        processFile(entry);
      }
    }
  } catch(const std::exception& exception) {
    logging::error("Scanner execution error: {}", exception.what());
  }
}

void WallpaperScanner::processFile(const std::filesystem::directory_entry& entry) {
  std::lock_guard<std::mutex> lock(engineMutex.get());
  try {
    const FilePath& path = entry.path();

    Visibility currentVisibility = Visibility::Unclassified;
    const std::string& defaultVis = settings.get().defaultIngestionVisibility;
    if(defaultVis == "safe") {
      currentVisibility = Visibility::Safe;
    } else if(defaultVis == "unsafe") {
      currentVisibility = Visibility::Unsafe;
    } else if(defaultVis == "current") {
      const bool isSafeMode = engine.get().manifest.state.stateMode == StateMode::Safe;
      currentVisibility = isSafeMode ? Visibility::Safe : Visibility::Unsafe;
    }

    bool wasActive = false;
    auto activeOut = SystemCommandRunner::runYieldOutput(settings.get().getterCommandTemplate);
    if(activeOut) {
      FilePath absPath = resolveTilde(path);
      FilePath absCurrent = resolveTilde(*activeOut);
      if(absPath == absCurrent) {
        wasActive = true;
      }
    }

    const Hash hash = engine.get().wallpaperStore.ingest(path, currentVisibility);

    if(wasActive && currentVisibility != Visibility::Unclassified) {
      const Visibility expectedVisibility =
          (engine.get().manifest.state.stateMode == StateMode::Safe) ? Visibility::Safe : Visibility::Unsafe;

      if(currentVisibility == expectedVisibility) {
        engine.get().applyWallpaper(hash);
      } else {
        engine.get().cycle();
      }
    } else if(wasActive && currentVisibility == Visibility::Unclassified) {
      engine.get().cycle();
    }

    engine.get().manifestStore.save(engine.get().manifest);

    std::string visibilityName = "UNCLASSIFIED";
    if(currentVisibility == Visibility::Safe) {
      visibilityName = "SAFE";
    } else if(currentVisibility == Visibility::Unsafe) {
      visibilityName = "UNSAFE";
    }

    logging::info(
        "Scanner discovered and automatically ingested new wallpaper: {} as {}",
        path.string(),
        visibilityName
    );
  } catch(const std::exception& exception) {
    logging::error("Scanner failed to ingest {}: {}", entry.path().string(), exception.what());
  }
}
