#pragma once
#include "state.hpp"
#include <ftxui/component/component.hpp>

ftxui::Component CreateRenameModal(AppState& state, ftxui::Component wallpaperMenu);
ftxui::Component CreateDeleteModal(AppState& state, ftxui::Component wallpaperMenu);
ftxui::Component CreateToggleModal(AppState& state, ftxui::Component wallpaperMenu);
ftxui::Component CreateClassifyModal(AppState& state, ftxui::Component wallpaperMenu);
