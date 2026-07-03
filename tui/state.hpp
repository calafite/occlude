#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct WallpaperItem {
  std::string hash;
  std::string path;
  std::string filename;
  std::string visibility;
  std::string createdAt;
  std::string lastShown;
};

enum class SortMode : std::uint8_t { Name, Date, Visibility };

struct AppState {
  std::string systemMode = "Safe";
  std::string publicCurrent;
  std::string privateCurrent;
  std::string daemonLogs = "Started TUI. Connected to socket.";
  std::vector<WallpaperItem> allWallpapers;
  std::vector<WallpaperItem> filteredWallpapers;
  std::vector<std::string> menuEntries;
  int selectedIndex = 0;
  std::string filterText;
  int sortModeIndex = 0;
  bool showRenameModal = false;
  std::string renameInput;
  bool showDeleteModal = false;
  bool showToggleModal = false;
  bool showClassifyModal = false;
  int classifyIndex = 0;
  std::vector<std::string> classifyEntries = {"Safe", "Unsafe", "Unclassified"};
};
