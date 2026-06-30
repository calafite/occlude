#pragma once
#include "common.hpp"

#include <cstdint>
#include <string>

struct Settings {
  FilePath publicRoot;
  FilePath privateRoot;
  FilePath unclassifiedRoot;
  FilePath manifestPath;
  std::string setterCommandTemplate = "noctalia msg wallpaper-set {path}";
  std::string getterCommandTemplate = "noctalia msg wallpaper-get";
  std::string defaultDownloadDirectory;
  std::uint32_t scanIntervalMinutes = 0;
  std::string defaultIngestionVisibility = "unclassified";
};
