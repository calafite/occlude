#include "components.hpp"
#include "ipcClient.hpp"
#include "state.hpp"
#include <ftxui/component/screen_interactive.hpp>

int main() {
  auto screen = ftxui::ScreenInteractive::Fullscreen();
  AppState state;
  IpcClient::syncState(state);
  auto mainUI = CreateMainUI(state, screen.ExitLoopClosure());
  screen.Loop(mainUI);
  return 0;
}
