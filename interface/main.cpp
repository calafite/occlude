#include "cli.hpp"
#include "../lib/ipc.hpp"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
  const auto payloadResult = cli::Parser::parse(argc, argv);
  const bool payloadParsed = payloadResult.has_value();
  if(!payloadParsed) {
    return 1;
  }

  auto connResult = IPC::connect();
  const bool connConnected = connResult.has_value();
  if(!connConnected) {
    std::cerr << "\033[31mError:\033[0m Could not connect to daemon. Is 'occluded' running?\n";
    return 1;
  }

  auto& conn = *connResult;
  const auto sendResult = conn.send(*payloadResult);
  const bool payloadSent = sendResult.has_value();
  if(!payloadSent) {
    std::cerr << "\033[31mError:\033[0m Failed to send command to daemon.\n";
    return 1;
  }

  const auto responseResult = conn.receive();
  const bool responseReceived = responseResult.has_value();
  if(!responseReceived) {
    std::cerr << "\033[31mError:\033[0m Failed to read response from daemon.\n";
    return 1;
  }

  const std::string& response = *responseResult;
  const bool isSuccess = response.starts_with("OK ");
  if(isSuccess) {
    std::cout << "\033[32mSuccess:\033[0m " << response.substr(3) << "\n";
    return 0;
  }

  const bool isError = response.starts_with("ERR ");
  if(isError) {
    std::cerr << "\033[31mError:\033[0m " << response.substr(4) << "\n";
    return 1;
  }

  std::cout << response << "\n";
  return 0;
}
