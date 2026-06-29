#pragma once
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>

namespace cli {
  struct Parser {
    Parser() = delete;

    static void printHelp() {
      std::cout << "Usage: occlude <command> [args...]\n"
                << "Commands:\n"
                << "  cycle                 Move to the next wallpaper\n"
                << "  toggle                Toggle between Safe/Unsafe modes\n"
                << "  ingest_safe <path>    Add a new safe wallpaper\n"
                << "  ingest_unsafe <path>  Add a new unsafe wallpaper\n"
                << "  status                Print daemon status\n"
                << "  current               Get the current wallpaper\n"
                << "  list                  Show all wallpapers\n";
    }

    [[nodiscard]] static std::optional<std::string> parse(int argc, char** argv) {
      const bool hasCommand = argc >= 2;
      if(!hasCommand) {
        printHelp();
        return std::nullopt;
      }

      const std::string command = argv[1];

      const bool isCycle = command == "cycle";
      if(isCycle) {
        return "CYCLE";
      }
      const bool isToggle = command == "toggle";
      if(isToggle) {
        return "TOGGLE";
      }
      const bool isStatus = command == "status";
      if(isStatus) {
        return "STATUS";
      }
      const bool isList = command == "list";
      if(isList) {
        return "LIST";
      }

      const bool isIngestSafe = command == "ingest_safe";
      if(isIngestSafe) {
        const bool hasCorrectArgCount = argc == 3;
        if(!hasCorrectArgCount) {
          std::cerr << "Error: 'ingest_safe' requires exactly one argument: <path>\n";
          return std::nullopt;
        }

        std::error_code ec;
        const std::filesystem::path absPath = std::filesystem::absolute(argv[2], ec);
        if(ec) {
          std::cerr << "Error: Failed to resolve path '" << argv[2] << "': " << ec.message() << "\n";
          return std::nullopt;
        }

        return "INGEST_SAFE " + absPath.string();
      }

      const bool isIngestUnsafe = command == "ingest_unsafe";
      if(isIngestUnsafe) {
        const bool hasCorrectArgCount = argc == 3;
        if(!hasCorrectArgCount) {
          std::cerr << "Error: 'ingest_unsafe' requires exactly one argument: <path>\n";
          return std::nullopt;
        }

        std::error_code ec;
        const std::filesystem::path absPath = std::filesystem::absolute(argv[2], ec);
        if(ec) {
          std::cerr << "Error: Failed to resolve path '" << argv[2] << "': " << ec.message() << "\n";
          return std::nullopt;
        }

        return "INGEST_UNSAFE " + absPath.string();
      }

      const bool isCurrent = command == "current";
      if(isCurrent) {
        return "CURRENT";
      }

      std::cerr << "Error: Unknown command '" << command << "'\n";
      printHelp();
      return std::nullopt;
    }
  };
} // namespace cli
