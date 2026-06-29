#include "../lib/common.hpp"

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
