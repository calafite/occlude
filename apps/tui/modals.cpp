#include "modals.hpp"

#include "ipcClient.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <utility>

using namespace ftxui;

// NOLINTBEGIN(readability-identifier-naming)
ftxui::Component CreateRenameModal(AppState& state, ftxui::Component wallpaperMenu) {
  auto input = Input(&state.renameInput, "New filename...");

  auto btnConfirm = Button("Confirm", [&state, wallpaperMenu]() {
    state.showRenameModal = false;

    if(!state.filteredWallpapers.empty()) {
      if(!state.renameInput.empty()) {
        IpcClient::executeCommand(
            state,
            "RENAME " + state.filteredWallpapers[state.selectedIndex].hash + " " + state.renameInput
        );
      }
    }
    wallpaperMenu->TakeFocus();
  });

  auto btnCancel = Button("Cancel", [&state, wallpaperMenu]() {
    state.showRenameModal = false;
    wallpaperMenu->TakeFocus();
  });

  auto layout =
      Container::Vertical({std::move(input), Container::Horizontal({std::move(btnConfirm), std::move(btnCancel)})});

  return CatchEvent(std::move(layout), [&state, wallpaperMenu = std::move(wallpaperMenu)](const Event& event) {
    if(event == Event::Escape) {
      state.showRenameModal = false;
      wallpaperMenu->TakeFocus();
      return true;
    }
    return false;
  });
}

ftxui::Component CreateDeleteModal(AppState& state, ftxui::Component wallpaperMenu) {
  auto btnDelete = Button("Delete", [&state, wallpaperMenu]() {
    state.showDeleteModal = false;

    if(!state.selectedHashes.empty()) {
      for(const auto& hash : state.selectedHashes) {
        IpcClient::executeCommand(state, "DELETE " + hash, true);
      }
      state.selectedHashes.clear();
      IpcClient::syncState(state);
    } else if(!state.filteredWallpapers.empty()) {
      IpcClient::executeCommand(state, "DELETE " + state.filteredWallpapers[state.selectedIndex].hash);
    }
    wallpaperMenu->TakeFocus();
  });

  auto btnCancel = Button("Cancel", [&state, wallpaperMenu]() {
    state.showDeleteModal = false;
    wallpaperMenu->TakeFocus();
  });

  auto layout = Container::Horizontal({std::move(btnDelete), std::move(btnCancel)});

  return CatchEvent(std::move(layout), [&state, wallpaperMenu = std::move(wallpaperMenu)](const Event& event) {
    if(event == Event::Escape) {
      state.showDeleteModal = false;
      wallpaperMenu->TakeFocus();
      return true;
    }
    return false;
  });
}

ftxui::Component CreateToggleModal(AppState& state, ftxui::Component wallpaperMenu) {
  auto btnToggle = Button("Toggle", [&state, wallpaperMenu]() {
    state.showToggleModal = false;
    IpcClient::executeCommand(state, "TOGGLE");
    wallpaperMenu->TakeFocus();
  });

  auto btnCancel = Button("Cancel", [&state, wallpaperMenu]() {
    state.showToggleModal = false;
    wallpaperMenu->TakeFocus();
  });

  auto layout = Container::Horizontal({std::move(btnToggle), std::move(btnCancel)});

  return CatchEvent(std::move(layout), [&state, wallpaperMenu = std::move(wallpaperMenu)](const Event& event) {
    if(event == Event::Escape) {
      state.showToggleModal = false;
      wallpaperMenu->TakeFocus();
      return true;
    }
    return false;
  });
}

ftxui::Component CreateClassifyModal(AppState& state, ftxui::Component wallpaperMenu) {
  auto radio = Radiobox(&state.classifyEntries, &state.classifyIndex);

  auto btnOk = Button("OK", [&state, wallpaperMenu]() {
    state.showClassifyModal = false;

    std::string target = "unclassified";
    if(state.classifyIndex == 0) {
      target = "safe";
    }
    if(state.classifyIndex == 1) {
      target = "unsafe";
    }

    if(!state.selectedHashes.empty()) {
      for(const auto& hash : state.selectedHashes) {
        IpcClient::executeCommand(state, std::format("CLASSIFY {} {}", hash, target), true);
      }
      state.selectedHashes.clear();
      IpcClient::syncState(state); 
    } else if(!state.filteredWallpapers.empty()) {
      const auto& hash = state.filteredWallpapers[state.selectedIndex].hash;
      IpcClient::executeCommand(state, std::format("CLASSIFY {} {}", hash, target)); 
    }
    wallpaperMenu->TakeFocus();
  });

  auto btnCancel = Button("Cancel", [&state, wallpaperMenu]() {
    state.showClassifyModal = false;
    wallpaperMenu->TakeFocus();
  });

  auto layout =
      Container::Vertical({std::move(radio), Container::Horizontal({std::move(btnOk), std::move(btnCancel)})});

  return CatchEvent(std::move(layout), [&state, wallpaperMenu = std::move(wallpaperMenu)](const Event& event) {
    if(event == Event::Escape) {
      state.showClassifyModal = false;
      wallpaperMenu->TakeFocus();
      return true;
    }
    return false;
  });
}
// NOLINTEND(readability-identifier-naming)
