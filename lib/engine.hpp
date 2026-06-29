#pragma once
#include "common.hpp"
#include "hash.hpp"
#include "log.hpp"
#include "setter.hpp"
#include "settings.hpp"
#include "state.hpp"
#include "store.hpp"
#include "wallpaperStore.hpp"
#include "wallpapers.hpp"

#include <chrono>
#include <exception>
#include <format>

namespace detail {
  inline std::string toHex(Hash const& hash) {
    std::string out;
    out.reserve(HASH_SIZE * 2);
    for(std::byte b : hash.value) {
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
      : settings(std::move(s)),                                                  //
        manifestStore(fs, settings.manifestPath),                                //
        manifest(manifestStore.load()),                                          //
        wallpaperStore(manifest, fs, settings.publicRoot, settings.privateRoot), //
        setter(runner, settings) {}                                              //

  void cycle() {
    auto available = manifest.current();
    if(available.empty()) {
      logging::error("Engine: No wallpapers available for current state.");
      return;
    }

    auto oldest = std::ranges::min_element(available, [](auto const& a, auto const& b) {
      if(!a.get().lastShown) {
        return true;
      }
      if(!b.get().lastShown) {
        return false;
      }
      return *a.get().lastShown < *b.get().lastShown;
    });

    applyWallpaper(oldest->get().hash);
  }

  void toggleMode() {
    std::optional<std::string> targetHashHex;
    if(manifest.state.stateMode == StateMode::Safe) {
      manifest.state.stateMode = StateMode::Unsafe;
      targetHashHex = manifest.state.privateCurrent;
      logging::info("Engine: Switched to UNSAFE mode.");
    } else {
      manifest.state.stateMode = StateMode::Safe;
      targetHashHex = manifest.state.publicCurrent;
      logging::info("Engine: Switched to SAFE mode.");
    }

    bool hasTarget = targetHashHex.has_value();
    if(hasTarget) {
      try {
        Hash targetHash(*targetHashHex);
        auto found = manifest.find(targetHash);
        auto foundVisibility = found.value().visibility;
        Visibility expected = state_helper::fromState(manifest.state.stateMode);
        if(found && foundVisibility == expected) {
          applyWallpaper(targetHash);
          return;
        }
      } catch(std::exception const&) {
        cycle();
      }
    }
  }

  void applyWallpaper(Hash const& hash) {
    auto resolvedPath = wallpaperStore.resolve(hash);

    if(!resolvedPath.has_value()) {
      logging::error("Engine: Failed to resolve wallpaper path.");
      return;
    }

    auto execution = setter.apply(*resolvedPath);
    if(!execution.has_value()) {
      logging::error("Engine: Setter command failed to execute.");
      return;
    }

    auto found = manifest.find(hash);
    if(found) {
      for(auto& wp : manifest.wallpapers) {
        if(wp->hash == hash) {
          wp->lastShown = std::chrono::system_clock::now();
          break;
        }
      }
    }

    std::string hashHex = detail::toHex(hash);
    if(manifest.state.stateMode == StateMode::Safe) {
      manifest.state.publicCurrent = hashHex;
    } else {
      manifest.state.privateCurrent = hashHex;
    }

    manifestStore.save(manifest);
    logging::info("Engine: Successfully applied {}", resolvedPath->filename().string());
  }
};
