#include "components.hpp"

#include "ipcClient.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <regex>
#include <vector>

using namespace ftxui;

// NOLINTBEGIN(readability-identifier-naming)
namespace {
  void ExecuteCommand(AppState& state, const std::string& cmd) {
    std::string response = IpcClient::sendCommand(cmd);
    const std::regex ansiRegex("\033\\[[0-9;]*m");

    state.daemonLogs = std::regex_replace(response, ansiRegex, "");
    IpcClient::syncState(state);
  }

  std::string GetSortModeName(int index) {
    if(index == 0) {
      return "Name";
    }
    if(index == 1) {
      return "Date";
    }
    return "Visibility";
  }

  bool HandleGlobalDaemonKeys(AppState& state, const Event& event, const std::function<void()>& onQuit) {
    if(event == Event::Character('q') || event == Event::Character('Q')) {
      onQuit();
      return true;
    }

    if(event == Event::Character('t') || event == Event::Character('T')) {
      ExecuteCommand(state, "TOGGLE");
      return true;
    }

    if(event == Event::Character('c') || event == Event::Character('C')) {
      ExecuteCommand(state, "CYCLE");
      return true;
    }

    if(event == Event::Character('s')) {
      ExecuteCommand(state, "SCAN");
      return true;
    }

    if(event == Event::Character('S')) {
      state.sortModeIndex = (state.sortModeIndex + 1) % 3;
      IpcClient::applyFilterAndSort(state);
      return true;
    }

    return false;
  }

  bool HandleWallpaperActionKeys(AppState& state, const Event& event) {
    if(state.filteredWallpapers.empty()) {
      return false;
    }

    const auto& wallpaper = state.filteredWallpapers[state.selectedIndex];

    if(event == Event::Character('a') || event == Event::Character('A')) {
      ExecuteCommand(state, "APPLY " + wallpaper.hash);
      return true;
    }

    if(event == Event::Character('d') || event == Event::Character('D')) {
      ExecuteCommand(state, "DELETE " + wallpaper.hash);
      return true;
    }

    if(event == Event::Character('r') || event == Event::Character('R')) {
      state.renameInput = wallpaper.filename;
      state.showRenameModal = true;
      return true;
    }

    if(event == Event::Character('1')) {
      ExecuteCommand(state, "CLASSIFY " + wallpaper.hash + " safe");
      return true;
    }

    if(event == Event::Character('2')) {
      ExecuteCommand(state, "CLASSIFY " + wallpaper.hash + " unsafe");
      return true;
    }

    if(event == Event::Character('3')) {
      ExecuteCommand(state, "CLASSIFY " + wallpaper.hash + " unclassified");
      return true;
    }

    return false;
  }

  bool HandleMainLayerKeys(
      AppState& state, const Event& event, const Component& inputFilter, const Component& wallpaperMenu,
      const std::function<void()>& onQuit
  ) {
    if(state.showRenameModal) {
      return false;
    }

    if(inputFilter->Focused()) {
      if(event == Event::Escape) {
        wallpaperMenu->TakeFocus();
        return true;
      }
      return false;
    }

    if(event == Event::Character('/')) {
      inputFilter->TakeFocus();
      return true;
    }

    if(HandleGlobalDaemonKeys(state, event, onQuit)) {
      return true;
    }

    return HandleWallpaperActionKeys(state, event);
  }

  Element RenderWallpaperDetail(const AppState& state) {
    if(state.filteredWallpapers.empty()) {
      return text("No wallpaper selected.") | dim;
    }

    if(state.selectedIndex >= static_cast<int>(state.filteredWallpapers.size())) {
      return text("No wallpaper selected.") | dim;
    }

    const auto& wallpaper = state.filteredWallpapers[state.selectedIndex];

    return vbox(
        {text("WALLPAPER DETAIL") | bold,
         separator(),
         hbox({text("Hash:      ") | bold, text(wallpaper.hash) | color(Color::Cyan)}),
         hbox({text("File:      ") | bold, text(wallpaper.filename)}),
         hbox({text("Vis:       ") | bold, text(wallpaper.visibility)}),
         hbox({text("Created:   ") | bold, text(wallpaper.createdAt)}),
         hbox({text("Last Seen: ") | bold, text(wallpaper.lastShown)})}
    );
  }

  Element RenderHelpBar() {
    return hbox(
               {text(" [/] Search ") | dim | bold,
                text(" [Esc] Unfocus ") | dim,
                text(" [j/k] Nav ") | dim,
                text(" [a] Apply ") | dim | bold,
                text(" [1-3] Classify ") | dim,
                text(" [r] Rename ") | dim,
                text(" [d] Delete ") | dim,
                text(" [S] Sort Mode ") | dim,
                text(" [s] Scan ") | dim,
                text(" [c] Cycle ") | dim,
                text(" [t] Toggle ") | dim,
                text(" [q] Quit ") | dim}
           ) |
        center;
  }

  Element RenderLeftPanel(const AppState& state, const Component& filterWithEvents, const Component& wallpaperMenu) {
    std::string sortStr = GetSortModeName(state.sortModeIndex);

    return vbox(
               {hbox({text("Search (/): "), filterWithEvents->Render() | border}),
                hbox({text("Sort Mode (S): ") | dim, text(sortStr) | bold}),
                separator(),
                hbox({text("  HASH     │ VISIBILITY   │ FILENAME") | bold}),
                separator(),
                wallpaperMenu->Render() | vscroll_indicator | frame | flex}
           ) |
        border | flex;
  }

  Element RenderRightPanel(const AppState& state) {
    return RenderWallpaperDetail(state) | border | size(WIDTH, EQUAL, 50);
  }

  Element RenderMainView(const AppState& state, const Element& leftPanel, const Element& rightPanel) {
    Color modeColor = Color::Red;

    if(state.systemMode == "Safe") {
      modeColor = Color::Green;
    }

    auto header = hbox(
        {text("OCCLUDE TUI") | bold | color(Color::Blue),
         filler(),
         text(" MODE: " + state.systemMode + " ") | bold | bgcolor(modeColor) | color(Color::Black)}
    );

    bool isError = state.daemonLogs.starts_with("ERR");

    auto logs =
        vbox({text("Logs:") | bold, text(state.daemonLogs) | color(isError ? Color::Red : Color::White)}) | border;

    return vbox({header, separator(), hbox({leftPanel, rightPanel}) | flex, logs, RenderHelpBar()});
  }

  Element RenderRenameModal(const Component& renameInput) {
    return vbox(
               {text("Rename Wallpaper") | bold | center,
                text("Press [Enter] to confirm, [Esc] to cancel.") | dim | center,
                separator(),
                renameInput->Render() | border}
           ) |
        border | bgcolor(Color::Black) | center;
  }

} // namespace

Component CreateMainUI(AppState& state, const std::function<void()>& onQuit) {
  auto inputFilter = Input(&state.filterText, "Type to filter...");

  auto filterWithEvents = CatchEvent(inputFilter, [&state](const Event& event) {
    bool shouldUpdate = false;

    if(event.is_character()) {
      shouldUpdate = true;
    }
    if(event == Event::Backspace) {
      shouldUpdate = true;
    }
    if(event == Event::Delete) {
      shouldUpdate = true;
    }

    if(shouldUpdate) {
      IpcClient::applyFilterAndSort(state);
    }

    return false;
  });

  auto wallpaperMenu = Menu(&state.menuEntries, &state.selectedIndex);

  auto mainLayer = Container::Vertical({filterWithEvents, wallpaperMenu});

  auto keyRouter = CatchEvent(mainLayer, [&state, onQuit, inputFilter, wallpaperMenu](const Event& event) {
    return HandleMainLayerKeys(state, event, inputFilter, wallpaperMenu, onQuit);
  });

  auto layoutRenderer = Renderer(keyRouter, [&state, filterWithEvents, wallpaperMenu]() {
    Element leftPanel = RenderLeftPanel(state, filterWithEvents, wallpaperMenu);
    Element rightPanel = RenderRightPanel(state);

    return RenderMainView(state, leftPanel, rightPanel);
  });

  auto renameInput = Input(&state.renameInput, "New filename...");

  auto renameModalRouter = CatchEvent(renameInput, [&state, wallpaperMenu](const Event& event) {
    if(event == Event::Escape) {
      state.showRenameModal = false;
      wallpaperMenu->TakeFocus();
      return true;
    }

    if(event == Event::Return) {
      state.showRenameModal = false;

      if(!state.filteredWallpapers.empty()) {
        if(!state.renameInput.empty()) {
          ExecuteCommand(
              state,
              "RENAME " + state.filteredWallpapers[state.selectedIndex].hash + " " + state.renameInput
          );
        }
      }

      wallpaperMenu->TakeFocus();
      return true;
    }

    return false;
  });

  auto modalRenderer = Renderer(renameModalRouter, [renameInput]() {
    return RenderRenameModal(renameInput);
  });

  return Modal(layoutRenderer, modalRenderer, &state.showRenameModal);
}
// NOLINTEND(readability-identifier-naming)
