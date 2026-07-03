#include "components.hpp"
#include "ipcClient.hpp"
#include "state.hpp"

#include <chrono>
#include <condition_variable>
#include <ftxui/component/screen_interactive.hpp>
#include <mutex>
#include <thread>

int main() {
  auto screen = ftxui::ScreenInteractive::Fullscreen();
  AppState state;
  IpcClient::syncState(state);
  std::mutex cvMutex;
  std::condition_variable_any cv;
  std::jthread refreshThread([&](std::stop_token stopToken) {
    while(!stopToken.stop_requested()) {
      std::unique_lock<std::mutex> lock(cvMutex);
      const bool stopped = cv.wait_for(lock, stopToken, std::chrono::seconds(5), [&stopToken]() {
        return stopToken.stop_requested();
      });

      if(!stopped) {
        screen.PostEvent(ftxui::Event::Custom);
      }
    }
  });
  auto mainUI = CreateMainUI(state, [&]() {
    refreshThread.request_stop();
    cv.notify_all();
    screen.ExitLoopClosure()();
  });
  screen.Loop(mainUI);
  return 0;
}
