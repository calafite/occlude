#pragma once
#include "state.hpp"
#include <string>

struct IpcClient {
  static std::string sendCommand(const std::string& commandStr);
  static void syncState(AppState& state);
  static void applyFilterAndSort(AppState& state);
};
