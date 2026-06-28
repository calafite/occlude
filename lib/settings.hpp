#pragma once
#include "common.hpp"

#include <string>

struct Settings {
  FilePath publicRoot;
  FilePath privateRoot;
  FilePath manifestPath;
  std::string setterCommandTemplate = "noctalia msg wallpaper-set {path}";
};
