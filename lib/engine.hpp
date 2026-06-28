#pragma once
#include "common.hpp"
#include "hash.hpp"
#include "setter.hpp"
#include "settings.hpp"
#include "state.hpp"
#include "store.hpp"
#include "wallpapers.hpp"
#include "wallpaperStore.hpp"

#include <chrono>
#include <format>
#include <iostream>

namespace detail {
  inline std::string toHex(Hash const& hash) {
    std::string out;
    out.reserve(HASH_SIZE * 2);
    for (std::byte b : hash.value) {
      out += std::format("{:02x}", static_cast<unsigned char>(b));
    }
    return out;
  }
} // namespace detail

template<FileSystem FS, CommandRunner Runner>
struct Engine {
  Settings settings;
  ManifestStore<FS> manifestStore;
  Manifest manifest;
  WallpaperStore<FS> wallpaperStore;
  WallpaperSetter<Runner> setter;

  Engine(FS& fs, Runner& runner, Settings s)
      : settings(std::move(s)),
        manifestStore(fs, settings.manifestPath),
        manifest(manifestStore.load()),
        wallpaperStore(manifest, fs, settings.publicRoot, settings.privateRoot),
        setter(runner, settings) {}

  void cycle() {
    auto available = manifest.current();
    if (available.empty()) {
      std::cerr << "Engine: No wallpapers available for current state.\n";
      return;
    }

    auto oldest = std::ranges::min_element(available, [](auto const& a, auto const& b) {
      if (!a.get().lastShown) {
        return true;
      }  
      if (!b.get().lastShown) {
        return false;
      }
      return *a.get().lastShown < *b.get().lastShown;
    });

    applyWallpaper(oldest->get().hash);
  }

  // Toggles between SFW and NSFW modes
  void toggleMode() {
    if (manifest.state.stateMode == StateMode::Safe) {
      manifest.state.stateMode = StateMode::Unsafe;
      std::cout << "Engine: Switched to UNSAFE mode.\n";
    } else {
      manifest.state.stateMode = StateMode::Safe;
      std::cout << "Engine: Switched to SAFE mode.\n";
    }
    
    cycle(); 
  }

  void applyWallpaper(Hash const& hash) {
    auto resolvedPath = wallpaperStore.resolve(hash);
    
    if (!resolvedPath.has_value()) {
      std::cerr << "Engine: Failed to resolve wallpaper path.\n";
      return;
    }

    auto execution = setter.apply(*resolvedPath);
    if (!execution.has_value()) {
      std::cerr << "Engine: Setter command failed to execute.\n";
      return;
    }

    auto found = manifest.find(hash);
    if (found) {
        for (auto& wp : manifest.wallpapers) {
            if (wp->hash == hash) {
                wp->lastShown = std::chrono::system_clock::now();
                break;
            }
        }
    }

    std::string hashHex = detail::toHex(hash);
    if (manifest.state.stateMode == StateMode::Safe) {
        manifest.state.publicCurrent = hashHex;
    } else {
        manifest.state.privateCurrent = hashHex;
    }

    manifestStore.save(manifest);
    std::cout << "Engine: Successfully applied " << resolvedPath->filename().string() << "\n";
  }
};
