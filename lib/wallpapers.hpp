#pragma once

#include "common.hpp"
#include "hash.hpp"
#include <cstdint>
#include <optional>

enum class Visibility : std::uint8_t {
  Safe,
  Unsafe,
  Unclassified,
};

struct Wallpaper {
  FilePath absPath;
  Hash hash;
  Timestamp createdAt;
  Visibility visibility = Visibility::Unclassified;
  std::optional<Timestamp> lastShown;

  Wallpaper(
    FilePath absPathV,
    Hash hashV,
    Timestamp createdAtV,
    Visibility visibilityV,
    std::optional<Timestamp> lastShownV
  ) : absPath(std::move(absPathV)),
    hash(hashV),
    createdAt(createdAtV),
    visibility(visibilityV),
    lastShown(lastShownV) {}
};
