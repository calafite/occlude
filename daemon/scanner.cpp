#include "scanner.hpp"

#include "../lib/log.hpp"
#include "utils.hpp"

#include <chrono>

WallpaperScanner::WallpaperScanner(
    Engine<RealFileSystem, SystemCommandRunner>& engineRef, std::mutex& engineMutexRef, const Settings& settingsRef
)
    : engine(engineRef), engineMutex(engineMutexRef), settings(settingsRef) {}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void WallpaperScanner::start() {
  const uint32_t interval = settings.get().scanIntervalMinutes;
  const std::string& dirPath = settings.get().defaultDownloadDirectory;

  if(dirPath.empty() || interval == 0) {
    return;
  }

  // NOLINTNEXTLINE(readability-function-cognitive-complexity)
  worker = std::jthread([this, interval, dirPath](std::stop_token const& stoken) {
    logging::info("Scanner thread started for directory: {} (interval: {}m)", dirPath, interval);

    while(!stoken.stop_requested()) {
      try {
        FilePath dir = resolveTilde(dirPath);

        if(std::filesystem::exists(dir) && std::filesystem::is_directory(dir)) {
          for(const auto& entry : std::filesystem::directory_iterator(dir)) {
            if(entry.is_regular_file()) {
              std::string filename = entry.path().filename().string();
              std::string ext = entry.path().extension().string();

              if(filename.starts_with(".") || ext == ".crdownload" || ext == ".part" || ext == ".tmp") {
                continue;
              }

              std::lock_guard<std::mutex> lock(engineMutex.get());
              try {
                const FilePath& path = entry.path();
                engine.get().wallpaperStore.ingest(path, Visibility::Safe);
                engine.get().manifestStore.save(engine.get().manifest);
                logging::info("Scanner discovered and automatically ingested new wallpaper: {}", path.string());
              } catch(const std::exception& e) {
                logging::error("Scanner failed to ingest {}: {}", entry.path().string(), e.what());
              }
            }
          }
        }
      } catch(const std::exception& e) {
        logging::error("Scanner thread error: {}", e.what());
      }

      for(uint32_t i = 0; i < interval * 60; ++i) {
        if(stoken.stop_requested()) {
          break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
      }
    }
  });
}
