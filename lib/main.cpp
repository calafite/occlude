#include "common.hpp"
#include "engine.hpp"
#include "fs.hpp"
#include "setter.hpp"
#include "settings.hpp"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
  if(argc < 2) {
    std::cerr << "Usage: occlude [cycle|toggle|ingest_safe <path>|ingest_unsafe <path>]\n";
    return 1;
  }

  std::string command = argv[1];

  RealFileSystem rfs;
  SystemCommandRunner runner;

  Settings settings{
      .publicRoot = "/home/walt/.config/wallpapers",
      .privateRoot = "/home/walt/.local/share/occlude/wallpapers",
      .manifestPath = "/home/walt/.config/occlude/manifest.bin",
      .setterCommandTemplate = "noctalia msg wallpaper-set {path}"
  };

  Engine<RealFileSystem, SystemCommandRunner> engine(rfs, runner, settings);

  if(command == "cycle") {
    engine.cycle();
  } else if(command == "toggle") {
    engine.toggleMode();
  } else if(command == "ingest_safe" && argc == 3) {
    FilePath source = argv[2];
    engine.wallpaperStore.ingest(source, Visibility::Safe);
    engine.manifestStore.save(engine.manifest);
    std::cout << "Ingested Safe Wallpaper: " << source << "\n";
  } else if(command == "ingest_unsafe" && argc == 3) {
    FilePath source = argv[2];
    engine.wallpaperStore.ingest(source, Visibility::Unsafe);
    engine.manifestStore.save(engine.manifest);
    std::cout << "Ingested Unsafe Wallpaper: " << source << "\n";
  } else {
    std::cerr << "Unknown command.\n";
  }

  return 0;
}
