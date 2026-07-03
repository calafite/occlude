#include "cycler.hpp"

#include "../../lib/utils/log.hpp"

#include <chrono>
#include <condition_variable>

WallpaperCycler::WallpaperCycler(
    Engine<
        RealFileSystem,                  //
        SystemCommandRunner>& engineRef, //
    std::mutex& engineMutexRef,          //
    const Settings& settingsRef          //
)
    : engine(engineRef),           //
      engineMutex(engineMutexRef), //
      settings(settingsRef) {}

void WallpaperCycler::start() {
  const uint32_t interval = settings.get().cycleIntervalSeconds;
  if(interval == 0) {
    logging::info("Auto-cycling is disabled (cycleIntervalSeconds = 0).");
    return;
  }

  worker = std::jthread([this](const std::stop_token& stopToken) {
    run(stopToken);
  });
}

void WallpaperCycler::run(const std::stop_token& stopToken) {
  const uint32_t interval = settings.get().cycleIntervalSeconds;
  logging::info("Wallpaper cycler thread started (interval: {}s)", interval);

  std::condition_variable_any cv;
  std::mutex cvMtx;

  while(!stopToken.stop_requested()) {
    std::unique_lock<std::mutex> lock(cvMtx);
    const bool stopSignaled = cv.wait_for(lock, stopToken, std::chrono::seconds(interval), [&stopToken] {
      return stopToken.stop_requested();
    });

    if(!stopSignaled) {
      cycleNow();
    }
  }
}

void WallpaperCycler::cycleNow() {
  std::lock_guard<std::mutex> lock(engineMutex.get());
  try {
    logging::info("Auto-cycler: Triggering automatic wallpaper cycle.");
    engine.get().cycle();
  } catch(const std::exception& exception) {
    logging::error("Auto-cycler execution error: {}", exception.what());
  }
}
