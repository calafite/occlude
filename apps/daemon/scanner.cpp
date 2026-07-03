#include "scanner.hpp"

#include "../../lib/io/fs.hpp"
#include "../../lib/utils/log.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <mutex>

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
  const uint32_t interval = settings.get().scanIntervalSeconds;
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
  const uint32_t interval = settings.get().scanIntervalSeconds;
  const std::string& directoryPath = settings.get().defaultDownloadDirectory;

  logging::info("Scanner thread started for directory: {} (interval: {}s)", directoryPath, interval);

  std::condition_variable_any cv;
  std::mutex cvMtx;

  while(!stopToken.stop_requested()) {
    scanNow();

    std::unique_lock<std::mutex> lock(cvMtx);
    cv.wait_for(lock, stopToken, std::chrono::seconds(interval), [&stopToken] {
      return stopToken.stop_requested();
    });
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

    const Hash hash = engine.get().wallpaperStore.ingest(path, currentVisibility);

    engine.get().resolveActiveIngestion(hash, path, currentVisibility);

    engine.get().manifestStore.save(engine.get().manifest);

    std::string visibilityName(toString(currentVisibility));
    std::ranges::transform(visibilityName, visibilityName.begin(), [](unsigned char c) {
      return std::toupper(c);
    });

    logging::info(
        "Scanner discovered and automatically ingested new wallpaper: {} as {}",
        path.string(),
        visibilityName
    );
  } catch(const std::exception& exception) {
    logging::error("Scanner failed to ingest {}: {}", entry.path().string(), exception.what());
  }
}
