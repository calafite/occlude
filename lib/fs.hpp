#pragma once
#include "common.hpp"

#include <vector>
#include <concepts>
#include <cstddef>
#include <unordered_map>

struct MoveOperation {
  FilePath from;
  FilePath to;
};

template<typename FS>
concept FileSystem = requires(
    FS& fs,        
    FilePath path, 
    MoveOperation& moveOp, 
    ByteSpan bytes 
) {
  { fs.exists(path) } -> std::same_as<bool>;
  { fs.read(path) } -> std::same_as<std::vector<std::byte>>;
  { fs.listDirectory(path) } -> std::same_as<std::vector<FilePath>>;
  { fs.write(path, bytes) } -> std::same_as<void>;
  { fs.sync(path) } -> std::same_as<void>; 
  { fs.move(moveOp) } -> std::same_as<void>;
  { fs.remove(path) } -> std::same_as<void>;
};

struct RealFileSystem {
  [[nodiscard]] static bool exists(FilePath const& path);
  [[nodiscard]] static std::vector<std::byte> read(FilePath const& path);
  [[nodiscard]] static std::vector<FilePath> listDirectory(FilePath const& directory);
  static void write(FilePath const& path, ByteSpan bytes);
  static void sync(FilePath const& path);
  static void move(MoveOperation& moveOperation);
  static void remove(FilePath const& path);
};
static_assert(FileSystem<RealFileSystem>, "RealFileSystem must satisfy FileSystem");


struct VirtualFileSystem {
  [[nodiscard]] bool exists(FilePath const& path) const;
  [[nodiscard]] std::vector<std::byte> read(FilePath const& path) const;
  [[nodiscard]] std::vector<FilePath> listDirectory(FilePath const& directory) const;
  void write(FilePath const& path, ByteSpan bytes);
  void sync(FilePath const& path);
  void move(MoveOperation&  moveOperation);
  void remove(FilePath const& path);
  void unmount(FilePath const& root);
  private:
    std::unordered_map<FilePath, std::vector<std::byte>> files;  
};
static_assert(FileSystem<VirtualFileSystem>, "VirtualFileSystem must satisfy FileSystem");
