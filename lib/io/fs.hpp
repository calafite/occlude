#pragma once
#include "../utils/common.hpp"
#include "../utils/hash.hpp"

#include <concepts>
#include <cstddef>
#include <filesystem>
#include <string>
#include <system_error>
#include <unordered_map>
#include <vector>

[[nodiscard]] inline FilePath resolveTilde(const FilePath& path) {
  FilePath resolved = path;
  const std::string pathStr = path.string();
  const bool startsWithTilde = pathStr.starts_with("~/");

  if(startsWithTilde) {
    resolved = FilePath(getHomeDirectory()) / pathStr.substr(2);
  }

  std::error_code ec;
  return std::filesystem::absolute(resolved, ec);
}

struct MoveOperation {
  FilePath from;
  FilePath to;
};

template<typename FS>
concept FileSystem = requires(FS& fs, FilePath path, MoveOperation& moveOp, ByteSpan bytes) {
  { fs.exists(path) } -> std::same_as<bool>;
  { fs.read(path) } -> std::same_as<std::vector<std::byte>>;
  { fs.listDirectory(path) } -> std::same_as<std::vector<FilePath>>;
  { fs.write(path, bytes) } -> std::same_as<void>;
  { fs.sync(path) } -> std::same_as<void>;
  { fs.move(moveOp) } -> std::same_as<void>;
  { fs.remove(path) } -> std::same_as<void>;
  { fs.hashFile(path) } -> std::same_as<Hash>;
};

struct RealFileSystem {
  [[nodiscard]] static bool exists(FilePath const& path);
  [[nodiscard]] static std::vector<std::byte> read(FilePath const& path);
  [[nodiscard]] static std::vector<FilePath> listDirectory(FilePath const& directory);
  static void write(FilePath const& path, ByteSpan bytes);
  static void sync(FilePath const& path);
  static void move(MoveOperation& moveOperation);
  static void remove(FilePath const& path);
  [[nodiscard]] static Hash hashFile(FilePath const& path);
};
static_assert(FileSystem<RealFileSystem>, "RealFileSystem must satisfy FileSystem");

struct VirtualFileSystem {
  [[nodiscard]] bool exists(FilePath const& path) const;
  [[nodiscard]] std::vector<std::byte> read(FilePath const& path) const;
  [[nodiscard]] std::vector<FilePath> listDirectory(FilePath const& directory) const;
  static void sync(FilePath const& path);
  void write(FilePath const& path, ByteSpan bytes);
  void move(MoveOperation& moveOperation);
  void remove(FilePath const& path);
  void unmount(FilePath const& root);
  [[nodiscard]] Hash hashFile(FilePath const& path) const;

private:
  std::unordered_map<FilePath, std::vector<std::byte>> files;
};
static_assert(FileSystem<VirtualFileSystem>, "VirtualFileSystem must satisfy FileSystem");
