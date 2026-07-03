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

  const std::vector<std::string> g_sortEntries = {"Name", "Date", "Visibility"};

  void ExecuteCommand(AppState& state, const std::string& cmd) {
    std::string response = IpcClient::sendCommand(cmd);
    const std::regex ansiRegex("\033\\[[0-9;]*m");

    state.daemonLogs = std::regex_replace(response, ansiRegex, "");
    IpcClient::syncState(state);
  }

  bool HandleFilterEvent(AppState& state, const Event& event) {
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
  }

  Component CreateFilterComponent(AppState& state) {
    auto inputFilter = Input(&state.filterText, "Search...");

    return CatchEvent(std::move(inputFilter), [&state](const Event& event) {
      return HandleFilterEvent(state, event);
    });
  }

  bool HandleSortEvent(AppState& state, const Event& /*event*/) {
    IpcClient::applyFilterAndSort(state);
    return false;
  }

  Component CreateSortComponent(AppState& state) {
    auto radioSort = Radiobox(&g_sortEntries, &state.sortModeIndex);

    return CatchEvent(std::move(radioSort), [&state](const Event& event) {
      return HandleSortEvent(state, event);
    });
  }

  Component CreateClassButtons(AppState& state) {
    auto btnSafe = Button("Safe", [&state]() {
      if(!state.filteredWallpapers.empty()) {
        ExecuteCommand(state, "CLASSIFY " + state.filteredWallpapers[state.selectedIndex].hash + " safe");
      }
    });

    auto btnUnsafe = Button("Unsafe", [&state]() {
      if(!state.filteredWallpapers.empty()) {
        ExecuteCommand(state, "CLASSIFY " + state.filteredWallpapers[state.selectedIndex].hash + " unsafe");
      }
    });

    auto btnUnclassed = Button("Unclass", [&state]() {
      if(!state.filteredWallpapers.empty()) {
        ExecuteCommand(state, "CLASSIFY " + state.filteredWallpapers[state.selectedIndex].hash + " unclassified");
      }
    });

    return Container::Horizontal({std::move(btnSafe), std::move(btnUnsafe), std::move(btnUnclassed)});
  }

  Component CreateActionButtons(AppState& state) {
    auto btnApply = Button("Set Active", [&state]() {
      if(!state.filteredWallpapers.empty()) {
        ExecuteCommand(state, "APPLY " + state.filteredWallpapers[state.selectedIndex].hash);
      }
    });

    auto btnRename = Button("Rename", [&state]() {
      if(!state.filteredWallpapers.empty()) {
        state.renameInput = state.filteredWallpapers[state.selectedIndex].filename;
        state.showRenameModal = true;
      }
    });

    auto btnDelete = Button("Delete", [&state]() {
      if(!state.filteredWallpapers.empty()) {
        ExecuteCommand(state, "DELETE " + state.filteredWallpapers[state.selectedIndex].hash);
      }
    });

    return Container::Horizontal({std::move(btnApply), std::move(btnRename), std::move(btnDelete)});
  }

  Component CreateGlobalControls(AppState& state, std::function<void()> onQuit) {
    auto btnToggle = Button("Toggle Mode (T)", [&state]() {
      ExecuteCommand(state, "TOGGLE");
    });

    auto btnCycle = Button("Cycle WP (C)", [&state]() {
      ExecuteCommand(state, "CYCLE");
    });

    auto btnScan = Button("Force Scan (S)", [&state]() {
      ExecuteCommand(state, "SCAN");
    });

    auto btnQuit = Button("Quit TUI (Q)", std::move(onQuit));

    return Container::Horizontal({std::move(btnToggle), std::move(btnCycle), std::move(btnScan), std::move(btnQuit)});
  }

  Component CreateRenameConfirmButton(AppState& state) {
    return Button("Confirm", [&state]() {
      state.showRenameModal = false;

      if(!state.filteredWallpapers.empty()) {
        if(!state.renameInput.empty()) {
          ExecuteCommand(
              state,
              "RENAME " + state.filteredWallpapers[state.selectedIndex].hash + " " + state.renameInput
          );
        }
      }
    });
  }

  Component CreateRenameCancelButton(AppState& state) {
    return Button("Cancel", [&state]() {
      state.showRenameModal = false;
    });
  }

  bool HandleGlobalKeys(AppState& state, const std::function<void()>& onQuit, const Event& event) {
    if(state.showRenameModal) {
      return false;
    }

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

    if(event == Event::Character('s') || event == Event::Character('S')) {
      ExecuteCommand(state, "SCAN");
      return true;
    }

    return false;
  }

  Element RenderWallpaperDetail(const AppState& state, const Component& actionButtons, const Component& classButtons) {
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
         hbox({text("Last Seen: ") | bold, text(wallpaper.lastShown)}),
         separator(),
         text("Actions:") | bold,
         actionButtons->Render(),
         classButtons->Render()}
    );
  }

  Element RenderLeftPanel(const Component& inputFilter, const Component& radioSort, const Component& wallpaperMenu) {
    return vbox(
               {hbox({text("Filter: "), inputFilter->Render() | border}),
                hbox({text("Sort:   "), radioSort->Render()}),
                separator(),
                hbox({text("  HASH     │ VISIBILITY   │ FILENAME") | bold}),
                separator(),
                wallpaperMenu->Render() | vscroll_indicator | frame | flex}
           ) |
        border | flex;
  }

  Element RenderRightPanel(const AppState& state, const Component& actionButtons, const Component& classButtons) {
    return RenderWallpaperDetail(state, actionButtons, classButtons) | border | size(WIDTH, EQUAL, 50);
  }

  Element RenderMainView(
      const AppState& state, const Element& leftPanel, const Element& rightPanel, const Component& globalControls
  ) {
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

    return vbox({header, separator(), hbox({leftPanel, rightPanel}) | flex, logs, globalControls->Render()});
  }

  Element RenderRenameModal(
      const Component& renameInput, const Component& renameConfirm, const Component& renameCancel
  ) {
    return vbox(
               {text("Rename Wallpaper") | bold | center,
                separator(),
                renameInput->Render() | border,
                hbox({renameConfirm->Render(), renameCancel->Render()}) | center}
           ) |
        border | bgcolor(Color::Black) | center;
  }

} // namespace

Component CreateMainUI(AppState& state, const std::function<void()>& onQuit) {
  auto inputFilter = CreateFilterComponent(state);
  auto radioSort = CreateSortComponent(state);
  auto wallpaperMenu = Menu(&state.menuEntries, &state.selectedIndex);

  auto classButtons = CreateClassButtons(state);
  auto actionButtons = CreateActionButtons(state);

  auto globalControls = CreateGlobalControls(state, onQuit);

  auto leftPanelBase = Container::Vertical({std::move(inputFilter), std::move(radioSort), std::move(wallpaperMenu)});

  auto rightPanelBase = Container::Vertical({std::move(classButtons), std::move(actionButtons)});

  auto mainLayer = Container::Vertical(
      {Container::Horizontal({std::move(leftPanelBase), std::move(rightPanelBase)}), std::move(globalControls)}
  );

  auto keyRouter = CatchEvent(std::move(mainLayer), [&state, onQuit](const Event& event) {
    return HandleGlobalKeys(state, onQuit, event);
  });

  auto layoutRenderer = Renderer(
      std::move(keyRouter),
      [&state, inputFilter, radioSort, wallpaperMenu, classButtons, actionButtons, globalControls]() {
        Element leftPanel = RenderLeftPanel(inputFilter, radioSort, wallpaperMenu);
        Element rightPanel = RenderRightPanel(state, actionButtons, classButtons);

        return RenderMainView(state, leftPanel, rightPanel, globalControls);
      }
  );

  auto renameInput = Input(&state.renameInput, "New filename...");
  auto renameConfirm = CreateRenameConfirmButton(state);
  auto renameCancel = CreateRenameCancelButton(state);

  auto renameModalContainer = Container::Vertical(
      {std::move(renameInput), Container::Horizontal({std::move(renameConfirm), std::move(renameCancel)})}
  );

  auto modalRenderer = Renderer(std::move(renameModalContainer), [renameInput, renameConfirm, renameCancel]() {
    return RenderRenameModal(renameInput, renameConfirm, renameCancel);
  });

  return Modal(std::move(layoutRenderer), std::move(modalRenderer), &state.showRenameModal);
}
// NOLINTEND(readability-identifier-naming)
