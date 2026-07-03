#pragma once

#include "../utils/common.hpp"
#include "../utils/hash.hpp"

#include <cstdint>
#include <optional>

enum class Visibility : std::uint8_t {
  Safe,
  Unsafe,
  Unclassified,
};

constexpr std::string_view toString(Visibility visibility) {
  switch(visibility) {
    case Visibility::Safe: return "Safe";
    case Visibility::Unsafe: return "Unsafe";
    case Visibility::Unclassified: return "Unclassified";
  }
  return "Unknown";
}

struct Wallpaper {
  FilePath absPath;
  Hash hash;
  Timestamp createdAt;
  Visibility visibility = Visibility::Unclassified;
  std::optional<Timestamp> lastShown;

  Wallpaper(
      FilePath absPathV,                  //
      Hash hashV,                         //
      Timestamp createdAtV,               //
      Visibility visibilityV,             //
      std::optional<Timestamp> lastShownV //
  )
      : absPath(std::move(absPathV)), //
        hash(hashV),                  //
        createdAt(createdAtV),        //
        visibility(visibilityV),      //
        lastShown(lastShownV) {}      //
};

