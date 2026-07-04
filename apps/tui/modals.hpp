#pragma once
#include "state.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>


// NOLINTBEGIN
ftxui::Component CreateRenameModal(AppState& state, ftxui::Component wallpaperMenu);
ftxui::Component CreateDeleteModal(AppState& state, ftxui::Component wallpaperMenu);
ftxui::Component CreateToggleModal(AppState& state, ftxui::Component wallpaperMenu);
ftxui::Component CreateClassifyModal(AppState& state, ftxui::Component wallpaperMenu);
ftxui::Component CreateHelpModal(AppState& state, ftxui::Component wallpaperMenu);
// NOLINTEND
