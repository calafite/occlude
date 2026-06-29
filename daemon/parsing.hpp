#pragma once

#include "../lib/common.hpp"
#include <string>

struct CommandMessage {
  std::string command;
  std::string argument;
};

[[nodiscard]] inline CommandMessage parseIpcMessage(const std::string& msg) {
  const std::size_t firstSpace = msg.find(' ');
  const bool hasArgument = firstSpace != std::string::npos;
  if(!hasArgument) {
    return CommandMessage{.command = msg, .argument = ""};
  }
  return CommandMessage{.command = msg.substr(0, firstSpace), .argument = msg.substr(firstSpace + 1)};
}

[[nodiscard]] inline FilePath resolveTilde(const FilePath& path) {
  FilePath resolved = path;
  std::string pathStr = path.string();
  if(pathStr.starts_with("~/")) {
    const char* const home = std::getenv("HOME");
    if(home != nullptr) {
      resolved = FilePath(home) / pathStr.substr(2);
    }
  }
  std::error_code ec;
  return std::filesystem::absolute(resolved, ec);
}
