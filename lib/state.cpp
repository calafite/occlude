#include "state.hpp"
#include "wallpapers.hpp"
#include "hash.hpp"

#include <memory>
#include <optional>
#include <utility>

Manifest::Manifest(State currentState) : state(std::move(currentState)) {}

void Manifest::loadWallpaper(
  FilePath absPath,
  Hash hash,
  Timestamp createdAt,
  Visibility visibility,
  std::optional<Timestamp> lastShown
) {
  auto existing = byHash.find(hash);
  if (existing != byHash.end()) {
    std::size_t index = existing->second;
    wallpapers[index] = std::make_unique<Wallpaper>(
      std::move(absPath), hash, createdAt, visibility, lastShown
    );
    return;
  }
  auto wallpaper = std::make_unique<Wallpaper>(
    std::move(absPath), hash, createdAt, visibility, lastShown
  );
  byHash[hash] = wallpapers.size();
  wallpapers.push_back(std::move(wallpaper));
}

void Manifest::registerWallpaper(
    FilePath absPath,
    Hash hash,
    Timestamp createdAt,
    Visibility visibility
) {
  loadWallpaper(std::move(absPath), hash, createdAt, visibility, std::nullopt);
}

void Manifest::deleteWallpaper(Hash hash) {
  auto iterator = byHash.find(hash);
  bool exists = iterator != byHash.end();
  if (!exists) {
    return;
  }
  std::size_t index = iterator->second;
  bool withinBounds = index < wallpapers.size();
  if (!withinBounds) {
    byHash.erase(iterator);
    return;
  }
  std::size_t lastIndex = wallpapers.size() - 1;
  if (index != lastIndex) {
    Hash lastHash = wallpapers[lastIndex]->hash;
    wallpapers[index] = std::move(wallpapers[lastIndex]);
    byHash[lastHash] = index;
  }
  wallpapers.pop_back();
  byHash.erase(iterator);
}
