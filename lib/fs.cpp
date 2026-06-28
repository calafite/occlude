#include "fs.hpp"

#include "common.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <stdexcept>
#include <unistd.h>

namespace {
  inline std::byte castByte(char character) {
    return static_cast<std::byte>(character);
  }
} // namespace

// RFS

bool RealFileSystem::exists(FilePath const& path) {
  return std::filesystem::exists(path);
}

std::vector<std::byte> RealFileSystem::read(FilePath const& path) {
  std::ifstream in(path, std::ios::binary);
  if(!static_cast<bool>(in)) {
    throw std::runtime_error("RealFileSystem::Read failed to open: " + path.string());
  }
  const auto istreambufF = std::istreambuf_iterator<char>(in);
  const auto istreambufL = std::istreambuf_iterator<char>();
  std::vector<char> raw((istreambufF), istreambufL);
  std::vector<std::byte> bytes(raw.size());
  std::ranges::transform(raw, bytes.begin(), castByte);
  return bytes;
}

std::vector<FilePath> RealFileSystem::listDirectory(FilePath const& directory) {
  std::vector<FilePath> result;
  const bool targetExists = std::filesystem::exists(directory);
  const bool isDirectory = std::filesystem::is_directory(directory);
  if(!targetExists || !isDirectory) {
    return result;
  }
  const auto directoryIterator = std::filesystem::directory_iterator(directory);
  for(auto const& entry : directoryIterator) {
    result.push_back(entry.path());
  }
  return result;
}

void RealFileSystem::write(FilePath const& path, ByteSpan bytes) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if(!out) {
    throw std::runtime_error("RealFileSystem::write: failed to open " + path.string());
  }
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  const auto* const castData = reinterpret_cast<const char*>(bytes.data());
  const auto streamSize = static_cast<std::streamsize>(bytes.size());
  out.write(castData, streamSize);
}

void RealFileSystem::sync(FilePath const& path) {
  const std::string pathStr = path.string();
  const auto* const cPath = pathStr.c_str();
  FILE* file = std::fopen(cPath, "rb"); // NOLINT(cppcoreguidelines-owning-memory)
  const bool fileOpen = static_cast<bool>(file);
  if(!fileOpen) {
    throw std::runtime_error("Failed to open file for fsync: " + pathStr);
  }
  int fileDescriptor = fileno(file);
  if(fileDescriptor == -1) {
    std::fclose(file); // NOLINT(cppcoreguidelines-owning-memory)
    throw std::runtime_error("fileno failed: " + pathStr);
  }
  int result = fsync(fileDescriptor);
  std::fclose(file); // NOLINT(cppcoreguidelines-owning-memory)
  if(result != 0) {
    throw std::runtime_error("fsync failed: " + pathStr);
  }
}

void RealFileSystem::move(MoveOperation& moveOperation) {
  FilePath& from = moveOperation.from;
  FilePath& to = moveOperation.to;

  std::error_code errorCode;
  std::filesystem::rename(from, to, errorCode);
  if(errorCode) {
    throw std::runtime_error(
        "RealFileSystem::move: " + errorCode.message() + " (" + from.string() + " -> " + to.string() + ")"
    );
  }
}

void RealFileSystem::remove(FilePath const& path) {
  std::error_code errorCode;
  std::filesystem::remove(path, errorCode);
  if(errorCode) {
    throw std::runtime_error("RealFileSystem::remove: " + errorCode.message() + " (" + path.string() + ")");
  }
}

// VFS

bool VirtualFileSystem::exists(FilePath const& path) const {
  return files.contains(path);
}

std::vector<std::byte> VirtualFileSystem::read(FilePath const& path) const {
  const auto iterator = files.find(path);
  const auto pathExists = iterator != files.end();
  if(!pathExists) {
    throw std::runtime_error("VirtualFileSystem::read: no such file: " + path.string());
  }
  return iterator->second;
}

std::vector<FilePath> VirtualFileSystem::listDirectory(FilePath const& directory) const {
  std::vector<FilePath> result;
  for(auto const& [path, _] : files) {
    if(path.parent_path() == directory) {
      result.push_back(path);
    }
  }
  return result;
}

void VirtualFileSystem::write(FilePath const& path, ByteSpan bytes) {
  files[path] = std::vector<std::byte>(bytes.begin(), bytes.end());
}

void VirtualFileSystem::sync(FilePath const& path) {
  // virtual file system is always synced
  // as it exists exclusively in memory
  (void) path;
}

void VirtualFileSystem::move(MoveOperation& moveOperation) {
  FilePath& from = moveOperation.from;
  FilePath& to = moveOperation.to;

  if(from == to) {
    return;
  }

  const auto iterator = files.find(from);
  const auto targetExists = iterator != files.end();
  if(!targetExists) {
    throw std::runtime_error("VirtualFileSystem::move: no such file " + from.string());
  }
  files[to] = std::move(iterator->second);
  files.erase(iterator);
}

void VirtualFileSystem::remove(FilePath const& path) {
  files.erase(path);
}

void VirtualFileSystem::unmount(FilePath const& root) {
  std::erase_if(files, [&](auto const& entry) {
    return std::ranges::starts_with(entry.first, root);
  });
}
