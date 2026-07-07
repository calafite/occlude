#include "ipcClient.hpp"

#include "../../lib/ipc/ipc.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <format>
#include <nlohmann/json.hpp>
#include <regex>

static std::string stripAnsi(const std::string& input) {
  static const std::regex ansiRegex("\033\\[[0-9;]*m");
  return std::regex_replace(input, ansiRegex, "");
}

std::string IpcClient::sendCommand(const std::string& commandStr) {
  auto connectionResult = IPC::connect();
  if(!connectionResult) {
    return "ERR Connection lost. Is 'occluded' active?";
  }

  auto& connection = *connectionResult;
  if(!connection.send(commandStr)) {
    return "ERR Failed transmission on IPC layer.";
  }

  auto responseResult = connection.receive();
  if(!responseResult) {
    return "ERR Reading reply timeout.";
  }

  return *responseResult;
}

void IpcClient::syncState(AppState& state) {
  std::string rawDump = sendCommand("DUMP");
  if(rawDump.starts_with("ERR ")) {
    state.daemonLogs = stripAnsi(rawDump);
    return;
  }

  if(rawDump.starts_with("OK ")) {
    rawDump = rawDump.substr(3);
  }

  try {
    auto json = nlohmann::json::parse(rawDump);
    state.systemMode = json["state"]["mode"].get<std::string>();
    state.publicCurrent = json["state"]["publicCurrent"].get<std::string>();
    state.privateCurrent = json["state"]["privateCurrent"].get<std::string>();
    state.publicCurrentFilename = json["state"].value("publicCurrentFilename", "");
    state.privateCurrentFilename = json["state"].value("privateCurrentFilename", "");

    state.allWallpapers.clear();
    for(const auto& item : json["wallpapers"]) {
      WallpaperItem wp;
      wp.hash = item["hash"].get<std::string>();
      wp.path = item["path"].get<std::string>();
      wp.filename = std::filesystem::path(wp.path).filename().string();
      wp.visibility = item["visibility"].get<std::string>();
      wp.createdAt = item["createdAt"].get<std::string>();

      if(item["lastShown"].is_string()) {
        wp.lastShown = item["lastShown"].get<std::string>();
      } else {
        wp.lastShown = "Never";
      }

      state.allWallpapers.push_back(wp);
    }

    applyFilterAndSort(state);
  } catch(const std::exception& ex) {
    state.daemonLogs = "ERR Exception parsing dump payload: " + std::string(ex.what());
  }
}

void IpcClient::updateMenuEntries(AppState& state) {
  state.menuEntries.clear();
  for(const auto& wp : state.filteredWallpapers) {
    std::string checkbox = state.selectedHashes.contains(wp.hash) ? "[x]" : "[ ]";
    state.menuEntries.push_back(
        std::format("{} │ {:<8} │ {:<12} │ {}", checkbox, wp.hash.substr(0, 8), wp.visibility, wp.filename)
    );
  }
}

void IpcClient::applyFilterAndSort(AppState& state) {
  std::string activeHash;
  if(!state.filteredWallpapers.empty() && state.selectedIndex < static_cast<int>(state.filteredWallpapers.size())) {
    activeHash = state.filteredWallpapers[state.selectedIndex].hash;
  }

  std::string filterLower = state.filterText;
  std::ranges::transform(filterLower, filterLower.begin(), ::tolower);

  state.filteredWallpapers.clear();
  for(const auto& wp : state.allWallpapers) {
    std::string nameLower = wp.filename;
    std::ranges::transform(nameLower, nameLower.begin(), ::tolower);
    if(filterLower.empty() || nameLower.contains(filterLower)) {
      state.filteredWallpapers.push_back(wp);
    }
  }

  auto sortMode = static_cast<SortMode>(state.sortModeIndex);
  std::ranges::sort(state.filteredWallpapers, [sortMode](const auto& a, const auto& b) {
    if(sortMode == SortMode::Name) {
      return a.filename < b.filename;
    }
    if(sortMode == SortMode::Date) {
      return a.createdAt > b.createdAt;
    }
    return a.visibility < b.visibility;
  });

  updateMenuEntries(state);

  bool selectionRestored = false;
  if(!activeHash.empty()) {
    for(int i = 0; i < static_cast<int>(state.filteredWallpapers.size()); ++i) {
      if(state.filteredWallpapers[i].hash == activeHash) {
        state.selectedIndex = i;
        selectionRestored = true;
        break;
      }
    }
  }

  if(!selectionRestored) {
    if(state.selectedIndex >= static_cast<int>(state.filteredWallpapers.size())) {
      state.selectedIndex = std::max(0, static_cast<int>(state.filteredWallpapers.size()) - 1);
    }
  }
}

void IpcClient::executeCommand(AppState& state, const std::string& cmd, bool skipSync) {
  std::string response = IpcClient::sendCommand(cmd);
  state.daemonLogs = stripAnsi(response);
  if(!skipSync) {
    IpcClient::syncState(state);
  }
}
