#pragma once
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>

namespace cli {
  namespace detail {

    inline std::optional<std::string> handleClassify(int argc, char** argv) {
      if(argc != 4) {
        std::cerr << "Error: 'classify' requires exactly two arguments: <hash> and <safe|unsafe|unclassified>\n";
        return std::nullopt;
      }
      return std::string("CLASSIFY ") + argv[2] + " " + argv[3];
    }

    inline std::optional<std::string> handleIngest(const std::string& cmdPrefix, int argc, char** argv) {
      if(argc != 3) {
        std::cerr << "Error: '" << cmdPrefix << "' requires exactly one argument: <path>\n";
        return std::nullopt;
      }

      std::error_code ec;
      const std::filesystem::path absPath = std::filesystem::absolute(argv[2], ec);
      if(ec) {
        std::cerr << "Error: Failed to resolve path '" << argv[2] << "': " << ec.message() << "\n";
        return std::nullopt;
      }

      std::string action = "INGEST_SAFE ";
      if(cmdPrefix == "ingest_unsafe") {
        action = "INGEST_UNSAFE ";
      }
      return action + absPath.string();
    }

    inline std::optional<std::string> handleSetMode(int argc, char** argv) {
      if(argc != 3) {
        std::cerr << "Error: 'set_mode' requires exactly one argument: <safe|unsafe>\n";
        return std::nullopt;
      }
      std::string arg = argv[2];
      if(arg != "safe" && arg != "unsafe") {
        std::cerr << "Error: 'set_mode' argument must be 'safe' or 'unsafe'\n";
        return std::nullopt;
      }
      return std::string("SET_MODE ") + arg;
    }

  } // namespace detail

  struct Parser {
    Parser() = delete;

    static void printHelp() {
      std::cout << "Usage: occlude <command> [args...]\n"
                << "Commands:\n"
                << "  cycle                 Move to the next wallpaper\n"
                << "  toggle                Toggle between Safe/Unsafe modes\n"
                << "  set_mode <mode>       Set mode explicitly (safe/unsafe)\n"
                << "  ingest_safe <path>    Add a new safe wallpaper\n"
                << "  ingest_unsafe <path>  Add a new unsafe wallpaper\n"
                << "  classify <hash> <vis> Change visibility of a wallpaper (safe/unsafe/unclassified)\n"
                << "  status                Print daemon status\n"
                << "  current               Get the current wallpaper\n"
                << "  list                  Show all wallpapers\n"
                << "  scan                  Force manual scan of downloads directory\n";
    }

    [[nodiscard]] static std::optional<std::string> parse(int argc, char** argv) {
      const bool hasCommand = argc >= 2;
      if(!hasCommand) {
        printHelp();
        return std::nullopt;
      }

      const std::string command = argv[1];

      if(command == "cycle") {
        return "CYCLE";
      }
      if(command == "toggle") {
        return "TOGGLE";
      }
      if(command == "status") {
        return "STATUS";
      }
      if(command == "list") {
        return "LIST";
      }
      if(command == "scan" || command == "force_ingest") {
        return "SCAN";
      }
      if(command == "current") {
        return "CURRENT";
      }
      if(command == "classify") {
        return detail::handleClassify(argc, argv);
      }
      if(command == "ingest_safe" || command == "ingest_unsafe") {
        return detail::handleIngest(command, argc, argv);
      }
      if(command == "set_mode") {
        return detail::handleSetMode(argc, argv);
      }

      std::cerr << "Error: Unknown command '" << command << "'\n";
      printHelp();
      return std::nullopt;
    }
  };
} // namespace cli
