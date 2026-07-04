#include "components.hpp"

#include "ipcClient.hpp"
#include "modals.hpp"

#include <fcntl.h>
#include <format>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <functional>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

using namespace ftxui;

// NOLINTBEGIN(readability-identifier-naming)
// NOLINTBEGIN
namespace {
  void launchExternalPreview(const std::string& path) {
    pid_t pid = fork();

    if(pid < 0) {
      return;
    }

    if(pid == 0) {
      pid_t grandchild = fork();
      if(grandchild < 0) {
        _exit(1);
      }
      if(grandchild > 0) {
        _exit(0);
      }

      int devNull = open("/dev/null", O_WRONLY);
      if(devNull >= 0) {
        dup2(devNull, STDOUT_FILENO);
        dup2(devNull, STDERR_FILENO);
        close(devNull);
      }

#if __APPLE__
      const char* launcher = "open";
#else
      const char* launcher = "xdg-open";
#endif

      auto* launcherV = const_cast<char*>(launcher);
      auto* cPath = const_cast<char*>(path.c_str());
      char* args[] = {launcherV, cPath, nullptr};
      execvp(launcher, args);
      _exit(127);
    }

    int status = 0;
    waitpid(pid, &status, 0);
  }
  // NOLINTEND
  std::string GetSortModeName(int index) {
    if(index == 0) {
      return "Name";
    }
    if(index == 1) {
      return "Date";
    }
    return "Visibility";
  }

  void ToggleSingleSelection(AppState& state, const WallpaperItem& wallpaper) {
    if(state.selectedHashes.contains(wallpaper.hash)) {
      state.selectedHashes.erase(wallpaper.hash);
    } else {
      state.selectedHashes.insert(wallpaper.hash);
    }
    IpcClient::updateMenuEntries(state);
  }

  void ToggleAllSelection(AppState& state) {
    bool allSelected = true;
    for(const auto& wp : state.filteredWallpapers) {
      if(!state.selectedHashes.contains(wp.hash)) {
        allSelected = false;
        break;
      }
    }

    if(allSelected) {
      for(const auto& wp : state.filteredWallpapers) {
        state.selectedHashes.erase(wp.hash);
      }
    } else {
      for(const auto& wp : state.filteredWallpapers) {
        state.selectedHashes.insert(wp.hash);
      }
    }
    IpcClient::updateMenuEntries(state);
  }

  bool HandleRenameAction(AppState& state, const WallpaperItem& wallpaper) {
    if(!state.selectedHashes.empty()) {
      state.daemonLogs = "ERR Blocked: Cannot rename while multiple wallpapers are selected.";
      return true;
    }
    state.renameInput = wallpaper.filename;
    state.showRenameModal = true;
    return true;
  }

  bool HandleClassifyAction(AppState& state, const WallpaperItem& wallpaper) {
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

  bool HandlePreviewAction(AppState& state, const WallpaperItem& wallpaper) {
    launchExternalPreview(wallpaper.path);
    state.daemonLogs = "Previewing file: " + wallpaper.filename;
    return true;
  }

  bool HandleApplyAction(AppState& state, const WallpaperItem& wallpaper) {
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

    if(event == Event::Character(' ')) {
      ToggleSingleSelection(state, wallpaper);
      return true;
    }
    if(event == Event::Character('*')) {
      ToggleAllSelection(state);
      return true;
    }
    if(event == Event::Character('r') || event == Event::Character('R')) {
      return HandleRenameAction(state, wallpaper);
    }
    if(event == Event::Character('v') || event == Event::Character('V')) {
      return HandleClassifyAction(state, wallpaper);
    }
    if(event == Event::Character('p') || event == Event::Character('P')) {
      return HandlePreviewAction(state, wallpaper);
    }
    if(event == Event::Character('a') || event == Event::Character('A')) {
      return HandleApplyAction(state, wallpaper);
    }
    if(event == Event::Character('d') || event == Event::Character('D')) {
      state.showDeleteModal = true;
      return true;
    }

    return false;
  }

  // NOLINTBEGIN(bugprone-easily-swappable-parameters)
  bool HandleMainLayerKeys(
      AppState& state,                    //
      const Event& event,                 //
      const Component& inputFilter,       //
      const Component& wallpaperMenu,     //
      const std::function<void()>& onQuit //
  ) {
    if(
        state.showRenameModal      //
        || state.showDeleteModal   //
        || state.showToggleModal   //
        || state.showClassifyModal //
        || state.showHelpModal     //
    ) {
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

    if(event == Event::Character('?')) {
      state.showHelpModal = true;
      return true;
    }

    if(HandleGlobalDaemonKeys(state, event, onQuit)) {
      return true;
    }

    return HandleWallpaperActionKeys(state, event);
  }
  // NOLINTEND(bugprone-easily-swappable-parameters)

  Element RenderHelpBar() {
    return hbox(
               {text(" [/] Search ") | dim | bold,
                text(" [?] Help ") | dim | bold,
                text(" [Space] Select ") | dim,
                text(" [j/k] Nav ") | dim,
                text(" [a] Apply ") | dim,
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
                hbox({text(" SEL │ HASH     │ VISIBILITY   │ FILENAME") | bold}),
                wallpaperMenu->Render() | vscroll_indicator | frame | flex}
           ) |
        border | flex;
  }

  Element RenderMainView(const AppState& state, const Element& leftPanel, const Element& rightPanel) {
    Color modeColor = Color::Red;
    if(state.systemMode == "Safe") {
      modeColor = Color::Green;
    }

    bool isSafe = state.systemMode == "Safe";
    std::string currentFilename;
    if(isSafe) {
      currentFilename = state.publicCurrentFilename;
    } else {
      currentFilename = state.privateCurrentFilename;
    }

    if(currentFilename.empty()) {
      currentFilename = "None";
    }

    constexpr int maxLen = 24;
    if(currentFilename.size() > maxLen) {
      currentFilename = std::format("{}{}", currentFilename.substr(0, maxLen - 3), "...");
    }

    auto header = hbox(
        {text("OCCLUDE TUI") | bold | color(Color::Blue),
         filler(),
         text(" CURRENT: " + currentFilename + " ") | bold | bgcolor(Color::Blue) | color(Color::Black),
         text(" "),
         text(" MODE: " + state.systemMode + " ") | bold | bgcolor(modeColor) | color(Color::Black)}
    );

    bool isError = state.daemonLogs.starts_with("ERR");
    auto logs =
        vbox({text("Logs:") | bold, text(state.daemonLogs) | color(isError ? Color::Red : Color::White)}) | border;

    return vbox({header, separator(), hbox({leftPanel, rightPanel}) | flex, logs, RenderHelpBar()});
  }

  Element RenderRenameModalUI(const Component& renameModal) {
    return vbox({text("Rename Wallpaper") | bold | center, separator(), renameModal->Render() | center}) | border |
        bgcolor(Color::Black) | center;
  }

  Element RenderDeleteModalUI(const AppState& state, const Component& deleteModal) {
    std::string title = state.selectedHashes.empty() ? "Delete Wallpaper?" : "Bulk Delete Wallpapers?";
    const bool promptC = state.selectedHashes.empty();
    std::string prompt;

    if(promptC) {
      prompt = "Are you sure you want to permanently delete this?";
    } else {
      prompt = std::format(
          "Are you sure you want to permanently delete {} selected wallpapers?",
          state.selectedHashes.size()
      );
    }

    return vbox({
               text(title)                    //
                   | bold | center,           //
               text(prompt)                   //
                   | dim | center,            //
               separator(),                   //
               deleteModal->Render() | center //
           })                                 //
        | border                              //
        | bgcolor(Color::Black)               //
        | center;                             //
  }

  Element RenderToggleModalUI(const Component& toggleModal) {
    return vbox(
               {text("Toggle System Mode") | bold | center,
                text("Switch between Safe and Unsafe mode?") | dim | center,
                separator(),
                toggleModal->Render() | center}
           ) |
        border | bgcolor(Color::Black) | center;
  }

  Element RenderClassifyModalUI(const AppState& state, const Component& classifyModal) {
    std::string title = state.selectedHashes.empty()
        ? "Classify Wallpaper"
        : std::format("Bulk Classify {} Wallpapers", state.selectedHashes.size());
    return vbox({text(title) | bold | center, separator(), classifyModal->Render() | center}) | border |
        bgcolor(Color::Black) | center;
  }

  Element RenderHelpModalUI(const Component& helpModal) {
    return vbox(
               {text(" KEYBINDS & HELP ") | bold | center,
                separator(),
                vbox(
                    {text(""),
                     text(" Navigation & UI") | bold | color(Color::Cyan),
                     text("   [/]          Focus Search"),
                     text("   [Escape]     Unfocus Search / Close Modals"),
                     text("   [j/k, ↑/↓]   Navigate list"),
                     text("   [S]          Cycle Sort Mode (Name/Date/Visibility)"),
                     text("   [?]          Toggle this Help Screen"),
                     text("   [q]          Quit"),
                     text(""),
                     text(" Selection & Actions") | bold | color(Color::Cyan),
                     text("   [Space]      Toggle single selection"),
                     text("   [*]          Toggle all in filtered list"),
                     text("   [a]          Apply selected/highlighted wallpaper"),
                     text("   [p]          Open image in external viewer"),
                     text("   [v]          Classify (Safe/Unsafe/Unclassified)"),
                     text("   [r]          Rename highlighted wallpaper"),
                     text("   [d]          Delete wallpaper from disk & DB"),
                     text(""),
                     text(" Daemon Commands") | bold | color(Color::Cyan),
                     text("   [s]          Force daemon to scan downloads"),
                     text("   [c]          Cycle active wallpaper"),
                     text("   [t]          Toggle system Mode (Safe <-> Unsafe)"),
                     text("")}
                ),
                separator(),
                helpModal->Render() | center}
           ) |
        border | bgcolor(Color::Black) | center;
  }

  struct FilterEventHandler {
    std::reference_wrapper<AppState> state;

    bool operator()(const Event& event) const {
      if(event.is_character() || event == Event::Backspace || event == Event::Delete) {
        IpcClient::applyFilterAndSort(state.get());
      }
      return false;
    }
  };

  struct MainLayerEventHandler {
    std::reference_wrapper<AppState> state;
    std::function<void()> onQuit;
    Component inputFilter;
    Component wallpaperMenu;

    bool operator()(const Event& event) const {
      return HandleMainLayerKeys(state.get(), event, inputFilter, wallpaperMenu, onQuit);
    }
  };

  struct MainLayoutRendererImpl {
    std::reference_wrapper<AppState> state;
    Component filterWithEvents;
    Component wallpaperMenu;

    Element operator()() const {
      Element leftPanel = RenderLeftPanel(state.get(), filterWithEvents, wallpaperMenu);
      Element rightPanel = RenderWallpaperDetail(state.get()) | border | size(WIDTH, EQUAL, 50);
      return RenderMainView(state.get(), leftPanel, rightPanel);
    }
  };

  struct RenameRendererImpl {
    Component renameModal;
    Element operator()() const {
      return RenderRenameModalUI(renameModal);
    }
  };

  struct DeleteRendererImpl {
    std::reference_wrapper<AppState> state;
    Component deleteModal;

    Element operator()() const {
      return RenderDeleteModalUI(state.get(), deleteModal);
    }
  };

  struct ToggleRendererImpl {
    Component toggleModal;
    Element operator()() const {
      return RenderToggleModalUI(toggleModal);
    }
  };

  struct ClassifyRendererImpl {
    std::reference_wrapper<AppState> state;
    Component classifyModal;

    Element operator()() const {
      return RenderClassifyModalUI(state.get(), classifyModal);
    }
  };

  struct HelpRendererImpl {
    Component helpModal;
    Element operator()() const {
      return RenderHelpModalUI(helpModal);
    }
  };

} // namespace

Component CreateMainUI(AppState& state, const std::function<void()>& onQuit) {
  auto inputFilter = Input(&state.filterText, "Type to filter...");

  FilterEventHandler filterHandler{.state = std::ref(state)};
  auto filterWithEvents = CatchEvent(inputFilter, filterHandler);

  auto wallpaperMenu = Menu(&state.menuEntries, &state.selectedIndex);

  auto mainLayer = Container::Vertical({filterWithEvents, wallpaperMenu});

  MainLayerEventHandler mainLayerHandler{
      .state = std::ref(state),
      .onQuit = onQuit,
      .inputFilter = inputFilter,
      .wallpaperMenu = wallpaperMenu
  };
  auto keyRouter = CatchEvent(mainLayer, mainLayerHandler);

  MainLayoutRendererImpl mainLayoutImpl{
      .state = std::ref(state),
      .filterWithEvents = filterWithEvents,
      .wallpaperMenu = wallpaperMenu
  };
  auto mainLayoutRenderer = Renderer(keyRouter, mainLayoutImpl);

  auto renameModal = CreateRenameModal(state, wallpaperMenu);
  RenameRendererImpl renameImpl{.renameModal = renameModal};
  auto renameRenderer = Renderer(renameModal, renameImpl);

  auto deleteModal = CreateDeleteModal(state, wallpaperMenu);
  DeleteRendererImpl deleteImpl{.state = std::ref(state), .deleteModal = deleteModal};
  auto deleteRenderer = Renderer(deleteModal, deleteImpl);

  auto toggleModal = CreateToggleModal(state, wallpaperMenu);
  ToggleRendererImpl toggleImpl{.toggleModal = toggleModal};
  auto toggleRenderer = Renderer(toggleModal, toggleImpl);

  auto classifyModal = CreateClassifyModal(state, wallpaperMenu);
  ClassifyRendererImpl classifyImpl{.state = std::ref(state), .classifyModal = classifyModal};
  auto classifyRenderer = Renderer(classifyModal, classifyImpl);

  auto helpModal = CreateHelpModal(state, wallpaperMenu);
  HelpRendererImpl helpImpl{.helpModal = helpModal};
  auto helpRenderer = Renderer(helpModal, helpImpl);

  auto ui = Modal(mainLayoutRenderer, renameRenderer, &state.showRenameModal);
  ui = Modal(ui, deleteRenderer, &state.showDeleteModal);
  ui = Modal(ui, toggleRenderer, &state.showToggleModal);
  ui = Modal(ui, classifyRenderer, &state.showClassifyModal);
  ui = Modal(ui, helpRenderer, &state.showHelpModal);

  return ui;
}
// NOLINTEND(readability-identifier-naming)
