#pragma once
#include <string>

struct CommandMessage {
  std::string command;
  std::string argument;
};

[[nodiscard]] inline CommandMessage parseIpcMessage(const std::string& msg) {
  const std::size_t firstSpace = msg.find(' ');
  const bool hasArgument = firstSpace != std::string::npos;
  if(!hasArgument) {
    return CommandMessage{.command=msg, .argument=""};
  }
  return CommandMessage{
    .command=msg.substr(0, firstSpace),
    .argument=msg.substr(firstSpace + 1)
  };
}
