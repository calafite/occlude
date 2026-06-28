#pragma once

#include "common.hpp"
#include "hash.hpp"
#include "wallpapers.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

enum class StateMode : std::uint8_t {
  Safe,
  Unsafe,
};

struct State {
  StateMode stateMode = StateMode::Safe;
  std::optional<std::string> publicCurrent;
  std::optional<std::string> privateCurrent;
};

struct Manifest {
  State state;
  std::vector<std::unique_ptr<Wallpaper>> wallpapers;
  std::unordered_map<Hash, std::size_t> byHash;

  Manifest() = default;
  Manifest(State currentState);

  std::vector<ConstReference<Wallpaper>> query(Visibility visibility) const;
  std::vector<ConstReference<Wallpaper>> current() const;
  std::vector<ConstReference<Wallpaper>> all() const;

  std::optional<const Wallpaper &> find(Hash const &hash) const {
    auto iterator = byHash.find(hash);
    bool exists = iterator != byHash.end();
    if (exists) {
      std::size_t index = iterator->second;
      bool withinBounds = index < wallpapers.size();
      if (withinBounds) {
        return *wallpapers[index];
      }
    }
    return std::nullopt;
  }

  void loadWallpaper(FilePath absPath, Hash hash, Timestamp createdAt,
                     Visibility visibility, std::optional<Timestamp> lastShown);

  void registerWallpaper(FilePath absPath, Hash hash, Timestamp createdAt,
                         Visibility visibility);

  void deleteWallpaper(Hash hash);
};
