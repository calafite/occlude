#include "components.hpp"
#include "ipcClient.hpp"
#include "modals.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <vector>

using namespace ftxui;

// NOLINTBEGIN(readability-identifier-naming)

namespace {

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
      state.showToggleModal = true;
      return true;
    }
    if(event == Event::Character('c') || event == Event::Character('C')) {
      IpcClient::executeCommand(state, "CYCLE");
      return true;
    }
    if(event == Event::Character('s')) {
      IpcClient::executeCommand(state, "SCAN");
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
      if(state.systemMode == "Safe" && wallpaper.visibility != "Safe") {
        state.daemonLogs = "ERR Blocked: Cannot apply non-safe wallpaper in Safe mode.";
        return true;
      }
      if(state.systemMode == "Unsafe" && wallpaper.visibility == "Unclassified") {
        state.daemonLogs = "ERR Blocked: Cannot apply unclassified wallpaper.";
        return true;
      }
      IpcClient::executeCommand(state, "APPLY " + wallpaper.hash);
      return true;
    }

    if(event == Event::Character('d') || event == Event::Character('D')) {
      state.showDeleteModal = true;
      return true;
    }

    if(event == Event::Character('r') || event == Event::Character('R')) {
      state.renameInput = wallpaper.filename;
      state.showRenameModal = true;
      return true;
    }

    if(event == Event::Character('v') || event == Event::Character('V')) {
      if(wallpaper.visibility == "Safe") {
        state.classifyIndex = 0;
      } else if(wallpaper.visibility == "Unsafe") {
        state.classifyIndex = 1;
      } else {
        state.classifyIndex = 2;
      }
      state.showClassifyModal = true;
      return true;
    }

    return false;
  }

  bool HandleMainLayerKeys(
      AppState& state, const Event& event, const Component& inputFilter, const Component& wallpaperMenu,
      const std::function<void()>& onQuit
  ) {
    if(state.showRenameModal || state.showDeleteModal || state.showToggleModal || state.showClassifyModal) {
      return false;
    }

    if(event == Event::Custom) {
      IpcClient::syncState(state);
      return true;
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

  Element RenderHelpBar() {
    return hbox(
               {text(" [/] Search ") | dim | bold,
                text(" [Esc] Unfocus ") | dim,
                text(" [j/k] Nav ") | dim,
                text(" [a] Apply ") | dim | bold,
                text(" [v] Classify ") | dim,
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

  Element RenderWallpaperDetail(const AppState& state) {
    if(state.filteredWallpapers.empty()) {
      return text("No wallpaper selected.") | dim;
    }
    if(state.selectedIndex >= static_cast<int>(state.filteredWallpapers.size())) {
      return text("No wallpaper selected.") | dim;
    }

    const auto& wallpaper = state.filteredWallpapers[state.selectedIndex];
    const std::string shortHash = wallpaper.hash.substr(0, 12);

    return vbox(
        {text("WALLPAPER DETAIL") | bold,
         separator(),
         hbox({text("Hash:      ") | bold, text(shortHash) | color(Color::Cyan)}),
         hbox({text("File:      ") | bold, text(wallpaper.filename)}),
         hbox({text("Vis:       ") | bold, text(wallpaper.visibility)}),
         hbox({text("Created:   ") | bold, text(wallpaper.createdAt)}),
         hbox({text("Last Seen: ") | bold, text(wallpaper.lastShown)})}
    );
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

} // namespace

Component CreateMainUI(AppState& state, const std::function<void()>& onQuit) {
  auto inputFilter = Input(&state.filterText, "Type to filter...");
  auto filterWithEvents = CatchEvent(inputFilter, [&state](const Event& event) {
    if(event.is_character() || event == Event::Backspace || event == Event::Delete) {
      IpcClient::applyFilterAndSort(state);
    }
    return false;
  });

  auto wallpaperMenu = Menu(&state.menuEntries, &state.selectedIndex);

  auto mainLayer = Container::Vertical({filterWithEvents, wallpaperMenu});

  auto keyRouter = CatchEvent(mainLayer, [&state, onQuit, inputFilter, wallpaperMenu](const Event& event) {
    return HandleMainLayerKeys(state, event, inputFilter, wallpaperMenu, onQuit);
  });

  auto mainLayoutRenderer = Renderer(keyRouter, [&state, filterWithEvents, wallpaperMenu]() {
    Element leftPanel = RenderLeftPanel(state, filterWithEvents, wallpaperMenu);
    Element rightPanel = RenderWallpaperDetail(state) | border | size(WIDTH, EQUAL, 50);
    return RenderMainView(state, leftPanel, rightPanel);
  });

  auto renameModal = CreateRenameModal(state, wallpaperMenu);
  auto renameRenderer = Renderer(renameModal, [&state, renameModal]() {
    return vbox({text("Rename Wallpaper") | bold | center, separator(), renameModal->Render() | center}) | border |
        bgcolor(Color::Black) | center;
  });

  auto deleteModal = CreateDeleteModal(state, wallpaperMenu);
  auto deleteRenderer = Renderer(deleteModal, [&state, deleteModal]() {
    return vbox(
               {text("Delete Wallpaper?") | bold | center,
                text("Are you sure you want to permanently delete this?") | dim | center,
                separator(),
                deleteModal->Render() | center}
           ) |
        border | bgcolor(Color::Black) | center;
  });

  auto toggleModal = CreateToggleModal(state, wallpaperMenu);
  auto toggleRenderer = Renderer(toggleModal, [&state, toggleModal]() {
    return vbox(
               {text("Toggle System Mode") | bold | center,
                text("Switch between Safe and Unsafe mode?") | dim | center,
                separator(),
                toggleModal->Render() | center}
           ) |
        border | bgcolor(Color::Black) | center;
  });

  auto classifyModal = CreateClassifyModal(state, wallpaperMenu);
  auto classifyRenderer = Renderer(classifyModal, [&state, classifyModal]() {
    return vbox({text("Classify Wallpaper") | bold | center, separator(), classifyModal->Render() | center}) | border |
        bgcolor(Color::Black) | center;
  });

  auto ui = Modal(mainLayoutRenderer, renameRenderer, &state.showRenameModal);
  ui = Modal(ui, deleteRenderer, &state.showDeleteModal);
  ui = Modal(ui, toggleRenderer, &state.showToggleModal);
  ui = Modal(ui, classifyRenderer, &state.showClassifyModal);

  return ui;
}

// NOLINTEND(readability-identifier-naming)
