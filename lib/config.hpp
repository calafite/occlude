#pragma once
#include "common.hpp"
#include "log.hpp"
#include "settings.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

[[nodiscard]] inline Settings getDefaultSettings() {
  const char* const home = std::getenv("HOME");
  const bool hasHomeEnv = home != nullptr;
  const std::string homeDir = hasHomeEnv ? std::string(home) : "/tmp";
  const FilePath configDir = FilePath(homeDir) / ".config" / "occlude";
  return Settings{
      .publicRoot = configDir / "public",
      .privateRoot = configDir / "private",
      .manifestPath = configDir / "manifest.bin",
      .setterCommandTemplate = "noctalia msg wallpaper-set {path}",
      .getterCommandTemplate = "noctalia msg wallpaper-get"
  };
}

// NOLINTBEGIN
inline void to_json(nlohmann::json& j, Settings const& settings) {
  j = nlohmann::json{
      {"publicRoot", settings.publicRoot.string()},
      {"privateRoot", settings.privateRoot.string()},
      {"manifestPath", settings.manifestPath.string()},
      {"setterCommandTemplate", settings.setterCommandTemplate},
      {"getterCommandTemplate", settings.getterCommandTemplate}
  };
}

inline void from_json(nlohmann::json const& j, Settings& settings) {
  Settings defaults = getDefaultSettings();
  settings.publicRoot = j.value("publicRoot", defaults.publicRoot.string());
  settings.privateRoot = j.value("privateRoot", defaults.privateRoot.string());
  settings.manifestPath = j.value("manifestPath", defaults.manifestPath.string());
  settings.setterCommandTemplate = j.value("setterCommandTemplate", defaults.setterCommandTemplate);
  settings.getterCommandTemplate = j.value("getterCommandTemplate", defaults.getterCommandTemplate);
}
// NOLINTEND

struct ConfigManager {
  ConfigManager() = delete;

  [[nodiscard]] static Settings loadOrInit() {
    const char* const home = std::getenv("HOME");
    const bool hasHomeEnv = home != nullptr;
    const std::string homeDir = hasHomeEnv ? std::string(home) : "/tmp";
    const FilePath configDir = FilePath(homeDir) / ".config" / "occlude";
    const FilePath configPath = configDir / "config.json";
    std::filesystem::create_directories(configDir);

    const bool configExists = std::filesystem::exists(configPath);
    if(!configExists) {
      logging::warn("Config file not found at {}. Creating default configuration.", configPath.string());
      Settings defaultSettings = getDefaultSettings();
      saveConfig(configPath, defaultSettings);
      return defaultSettings;
    }
    return parseConfig(configPath);
  }

private:
  [[nodiscard]] static Settings parseConfig(FilePath const& path) {
    std::ifstream file(path);
    const bool fileOpen = file.is_open();
    if(!fileOpen) {
      logging::warn("Failed to open config file at {}. Falling back to defaults.", path.string());
      return getDefaultSettings();
    }

    Settings settings;
    try {
      const nlohmann::json parsed = nlohmann::json::parse(file);
      settings = parsed.get<Settings>();
    } catch(nlohmann::json::exception const& ex) {
      logging::warn("Failed to parse config at {}: {}. Falling back to defaults.", path.string(), ex.what());
      return getDefaultSettings();
    }

    logging::info("Loaded configuration from {}", path.string());
    return settings;
  }

  static void saveConfig(FilePath const& path, Settings const& settings) {
    std::ofstream file(path);
    const nlohmann::json j = settings;
    file << j.dump(2) << "\n";
    logging::info("Saved default configuration to {}", path.string());
  }
};
