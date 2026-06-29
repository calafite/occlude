#pragma once
#include "common.hpp"
#include "fs.hpp"
#include "hash.hpp"
#include "state.hpp"
#include "wallpapers.hpp"

#include <chrono>
#include <expected>

enum class ResolveError : std::uint8_t { NotFound, Quarantined, Unmounted, FileMissing };

template<FileSystem FS>
struct WallpaperStore {
  WallpaperStore(
      Manifest& manifestRef, //
      FS& filesystem,        //
      FilePath publicRootV,  //
      FilePath privateRootV  //
  )
      : manifest(manifestRef),              //
        fs(filesystem),                     //
        publicRoot(std::move(publicRootV)), //
        privateRoot(std::move(privateRootV)) {}

  [[nodiscard]] std::expected<FilePath, ResolveError> resolve(Hash const& hash) const {
    auto found = manifest.get().find(hash);
    if(!found) {
      return std::unexpected(ResolveError::NotFound);
    }
    Wallpaper const& wallpaper = found.value();

    if(wallpaper.visibility == Visibility::Unclassified) {
      return std::unexpected(ResolveError::Quarantined);
    }

    FilePath const& root = wallpaper.visibility == Visibility::Unsafe ? privateRoot : publicRoot;
    if(!fs.get().exists(root)) {
      return std::unexpected(ResolveError::Unmounted);
    }

    if(!fs.get().exists(wallpaper.absPath)) {
      return std::unexpected(ResolveError::FileMissing);
    }

    return wallpaper.absPath;
  }

  Hash ingest(FilePath const& sourcePath, Visibility visibility) {
    auto bytes = fs.get().read(sourcePath);
    Hash hash = Hash::fromBytes(bytes);

    FilePath const& root = visibility == Visibility::Unsafe ? privateRoot : publicRoot;
    FilePath destination = root / sourcePath.filename();

    MoveOperation moveOp{.from = sourcePath, .to = destination};
    fs.get().move(moveOp);

    manifest.get().registerWallpaper(destination, hash, std::chrono::system_clock::now(), visibility);
    return hash;
  }

private:
  std::reference_wrapper<Manifest> manifest;
  std::reference_wrapper<FS> fs;
  FilePath publicRoot;
  FilePath privateRoot;
};
