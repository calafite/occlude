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

#include <algorithm>
#include <chrono>
#include <exception>
#include <format>
#include <functional>

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
  std::reference_wrapper<FS> fs;
  Settings settings;
  ManifestStore<FS> manifestStore;
  Manifest manifest;
  WallpaperStore<FS> wallpaperStore;
  WallpaperSetter<Runner> setter;

  Engine(FS& fsRef, Runner& runner, Settings s)
      : fs(fsRef),                                                                                             //
        settings(std::move(s)),                                                                                //
        manifestStore(fsRef, settings.manifestPath),                                                           //
        manifest(manifestStore.load()),                                                                        //
        wallpaperStore(manifest, fsRef, settings.publicRoot, settings.privateRoot, settings.unclassifiedRoot), //
        setter(runner, settings) {}                                                                            //

  void cycle() {
    while(true) {
      auto available = manifest.current();
      if(available.empty()) {
        logging::error("Engine: No wallpapers available for current state.");
        return;
      }

      std::string activePathStr;
      auto activeOut = SystemCommandRunner::runYieldOutput(settings.getterCommandTemplate);
      if(activeOut) {
        activePathStr = *activeOut;
      }

      std::string currentHashHex = (manifest.state.stateMode == StateMode::Safe)
          ? manifest.state.publicCurrent.value_or("")
          : manifest.state.privateCurrent.value_or("");

      if(available.size() > 1) {
        std::erase_if(available, [&](const auto& wpRef) {
          bool isEngineCurrent = (detail::toHex(wpRef.get().hash) == currentHashHex);
          bool isVisuallyCurrent =
              (!activePathStr.empty() &&
               (wpRef.get().absPath.string() == activePathStr ||
                activePathStr.find(wpRef.get().absPath.filename().string()) != std::string::npos));
          return isEngineCurrent || isVisuallyCurrent;
        });
      }

      auto oldest = std::ranges::min_element(available, [](auto const& a, auto const& b) {
        if(!a.get().lastShown && !b.get().lastShown) {
          return false;
        }
        if(!a.get().lastShown) {
          return true;
        }
        if(!b.get().lastShown) {
          return false;
        }
        return *a.get().lastShown < *b.get().lastShown;
      });

      Hash oldestHash = oldest->get().hash;
      bool success = applyWallpaper(oldestHash);
      if(success) {
        break;
      }
      logging::warn("Engine: Cycle skipped a broken/missing wallpaper. Retrying...");
    }
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

        if(found) {
          auto foundVisibility = found.value().visibility;
          const bool isSafeMode = manifest.state.stateMode == StateMode::Safe;

          bool isValid = false;
          if(isSafeMode) {
            isValid = foundVisibility == Visibility::Safe;
          } else {
            isValid = foundVisibility == Visibility::Safe || foundVisibility == Visibility::Unsafe;
          }

          if(isValid) {
            bool success = applyWallpaper(targetHash);
            if(success) {
              return;
            }
          }
        }
      } catch(std::exception const&) {
        cycle();
        return;
      }
    }

    cycle();
  }

  bool applyWallpaper(Hash const& hash) {
    auto resolvedPath = wallpaperStore.resolve(hash);

    if(!resolvedPath.has_value()) {
      if(resolvedPath.error() == ResolveError::FileMissing) {
        logging::warn("Engine: Wallpaper file missing from disk. Removing from manifest.");
        manifest.deleteWallpaper(hash);
        manifestStore.save(manifest);
      } else {
        logging::error("Engine: Failed to resolve wallpaper path.");
      }
      return false;
    }

    auto execution = setter.apply(*resolvedPath);
    if(!execution.has_value()) {
      logging::error("Engine: Setter command failed to execute.");
      return false;
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
    return true;
  }

  void classify(Hash const& hash, Visibility newVisibility) {
    auto found = manifest.find(hash);
    if(!found) {
      throw std::runtime_error("Wallpaper not found in manifest");
    }

    Visibility oldVisibility = found.value().visibility;
    if(oldVisibility == newVisibility) {
      return;
    }

    FilePath currentPath = found.value().absPath;

    FilePath targetRoot;
    if(newVisibility == Visibility::Unsafe) {
      targetRoot = settings.privateRoot;
    } else if(newVisibility == Visibility::Safe) {
      targetRoot = settings.publicRoot;
    } else {
      targetRoot = settings.unclassifiedRoot;
    }

    FilePath newPath = targetRoot / currentPath.filename();

    if(currentPath != newPath) {
      MoveOperation moveOp{.from = currentPath, .to = newPath};
      fs.get().move(moveOp);
    }

    for(auto& wp : manifest.wallpapers) {
      if(wp->hash == hash) {
        wp->visibility = newVisibility;
        wp->absPath = newPath;
        break;
      }
    }

    manifestStore.save(manifest);
  }
};
