#pragma once

#include "../lib/engine.hpp"
#include "../lib/settings.hpp"

#include <filesystem>
#include <mutex>
#include <thread>

struct WallpaperScanner {
  WallpaperScanner(
      Engine<
          RealFileSystem,         //
          SystemCommandRunner     //
          >& engineRef,           //
      std::mutex& engineMutexRef, //
      const Settings& settingsRef //
  );

  void start();
  void scanNow();

private:
  void run(const std::stop_token& stopToken);
  void processFile(const std::filesystem::directory_entry& entry);

  std::reference_wrapper<Engine<RealFileSystem, SystemCommandRunner>> engine;
  std::reference_wrapper<std::mutex> engineMutex;
  std::reference_wrapper<const Settings> settings;
  std::jthread worker;
};
