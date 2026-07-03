#pragma once

#include "../../lib/core/engine.hpp"
#include "../../lib/core/settings.hpp"

#include <mutex>
#include <thread>

struct WallpaperCycler {
  WallpaperCycler(
      Engine<RealFileSystem, SystemCommandRunner>& engineRef,
      std::mutex& engineMutexRef,
      const Settings& settingsRef
  );

  void start();
  void cycleNow();

private:
  void run(const std::stop_token& stopToken);

  std::reference_wrapper<Engine<RealFileSystem, SystemCommandRunner>> engine;
  std::reference_wrapper<std::mutex> engineMutex;
  std::reference_wrapper<const Settings> settings;
  std::jthread worker;
};
