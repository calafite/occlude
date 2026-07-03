#pragma once
#include "state.hpp"
#include <ftxui/component/component.hpp>
#include <functional>

ftxui::Component CreateMainUI(AppState& state, const std::function<void()>& onQuit);
