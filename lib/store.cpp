#include "store.hpp"
#include "common.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <ios>
#include <ostream>
#include <stdexcept>
#include <type_traits>
#include <fstream>
#include <unistd.h>

namespace {
  constexpr MagicBytes  kMagic = {'O', 'C', 'C', 'L'};
  constexpr std::uint32_t kVersion = 1;

  template<typename Type>
  concept PodCapable = std::semiregular<Type> && std::is_trivially_copyable_v<Type>;

  inline FilePath temporaryPath(FilePath path) {
    path += ".tmp";
    return path;
  }

  void writeRaw(std::ostream& out, const void* data, std::size_t size) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto *const reData = reinterpret_cast<const char*>(data);
    const auto streamSize = static_cast<std::streamsize>(size);
    out.write(reData, streamSize);   
  }

  template<PodCapable Type>
  void writePod(std::ostream& out, Type const& value) {
    const std::size_t typeSize = sizeof(Type);
    writeRaw(out, &value, typeSize);
  }

  void writeString(std::ostream& out, std::string const& str) {
    const auto size = static_cast<std::uint32_t>(str.size());
    writePod(out, size);
    writeRaw(out, str.data(), str.size());
  }

  void writeOptionalString(std::ostream& out, std::optional<std::string> const& oStr) {
    std::uint8_t present = oStr.has_value() ? 1 : 0;
    writePod(out, present);
    if(static_cast<bool>(present)) {
      writeString(out, *oStr);
    }
  }

  void writeTimestamp(std::ostream& out, Timestamp const& timestamp) {
    const auto nanoseconds = static_cast<std::int64_t>(
      duration_cast<nanosecondsType>(timestamp.time_since_epoch()).count()
    );
    writePod(out, nanoseconds);
  }

  void readRaw(std::ifstream& in, void* data, std::size_t size) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    auto* reData = reinterpret_cast<char*>(data);
    const auto streamSize = static_cast<std::streamsize>(size);
    in.read(reData, streamSize);
    if (!in) {
      throw std::runtime_error("Manifest file truncated or unreadable");
    }
  }

  template<PodCapable Type>
  Type readPod(std::ifstream& in) {
    Type value{};
    const auto typeSize = sizeof(Type);
    readRaw(in, &value, typeSize);
    return value; 
  }

  std::string readString(std::ifstream& in) {
    const auto size = readPod<std::uint32_t>(in);
    std::string str(size, '\0');
    if (size > 0) {
      readRaw(in, str.data(), size);
    }
    return str;
  }

  std::optional<std::string> readOptionalString(std::ifstream& in) {
    const auto present = readPod<std::uint8_t>(in);
    if(static_cast<bool>(present)) {
      return readString(in);
    }
    return std::nullopt;
  }

  Timestamp readTimestamp(std::ifstream& in) {
    const auto nanoseconds = readPod<std::int64_t>(in);
    return Timestamp(static_cast<nanosecondsType>(nanoseconds));
  }

  void writeToTemp(FilePath const& tempPath, Manifest const& manifest) {
    std::ofstream out(tempPath, std::ios::binary | std::ios::trunc);

    if(!out) {
      throw std::runtime_error("Failed to open temporary manifest file: " + tempPath.string());
    }

    writeRaw(out, kMagic.data(), kMagic.size());
    writePod(out, kVersion);

    const auto stateMode = static_cast<std::uint8_t>(manifest.state.stateMode);
    writePod(out, stateMode);
    writeOptionalString(out, manifest.state.publicCurrent);
    writeOptionalString(out, manifest.state.privateCurrent);

    const auto wallpaperCount = manifest.wallpapers.size();
    writePod(out, wallpaperCount);

    for(auto const& wallpaper : manifest.wallpapers) {
      writeString(out, wallpaper->absPath.string());
      writeRaw(out, wallpaper->hash.value.data(), wallpaper->hash.value.size());
      writeTimestamp(out, wallpaper->createdAt);
      writePod(out, static_cast<std::uint8_t>(wallpaper->visibility));

      std::uint8_t tag = wallpaper->lastShown.has_value() ? 1 : 0;
      writePod(out, tag);
      if(static_cast<bool>(tag)) {
        writeTimestamp(out, *wallpaper->lastShown);
      }
    }

    out.flush();
    if(!out) {
      throw std::runtime_error("Failed writing temporary manifest file: " + tempPath.string());
    }
  }

  void fsyncFile(FilePath const& tempPath) {
    FILE* file = std::fopen(tempPath.string().c_str(), "rb");

    if(!static_cast<bool>(file)) {
      throw std::runtime_error("Failed to reopen temporary manifest file for fsync.");
    }

    int fileDescriptor = fileno(file);
    
    if(fileDescriptor == -1) {
      std::fclose(file); // NOLINT(cppcoreguidelines-owning-memory)        
      throw std::runtime_error("fileno failed on temporary manifest file: " + tempPath.string());
    }
     
    int result = fsync(fileDescriptor);
    std::fclose(file); // NOLINT(cppcoreguidelines-owning-memory)

    if(result != 0)  {
      throw std::runtime_error("fsync failed on temporary manifest file: " + tempPath.string());
    } 
  }
};

ManifestStore::ManifestStore(FilePath manifestPath) : path(std::move(manifestPath)) {}

Manifest ManifestStore::load() const {
  bool pathExists = std::filesystem::exists(path);

  if(!pathExists) {
    return Manifest{};
  }

  std::ifstream in(path, std::ios::binary);

  if (!in) {
    throw std::runtime_error("Failed to open manifest file: " + path.string());
  }

  MagicBytes magic{};
  readRaw(in, magic.data(), magic.size());

  if(magic != kMagic) {
    throw std::runtime_error("Manifest has invalid magic bytes");
  }

  const auto version = readPod<std::uint32_t>(in);

  if(version != kVersion) {
    throw std::runtime_error("Manifest file has unsupported version: " + std::to_string(version));
  }

  State state;
  state.stateMode = static_cast<StateMode>(readPod<std::uint8_t>(in));
  state.publicCurrent = readOptionalString(in);
  state.privateCurrent = readOptionalString(in);

  Manifest manifest(std::move(state));

  const auto count = readPod<std::uint64_t>(in);
  manifest.wallpapers.reserve(count);

  for (std::uint64_t i = 0; i < count; ++i) {
    FilePath absPath = readString(in);

    HashByteArray hashBytes{};
    readRaw(in, hashBytes.data(), hashBytes.size());
    Hash hash(hashBytes);

    Timestamp createdAt = readTimestamp(in);
    auto visibility = static_cast<Visibility>(readPod<std::uint8_t>(in));

    auto lastShownTag = readPod<std::uint8_t>(in);
    std::optional<Timestamp> lastShown;
    if (static_cast<bool>(lastShownTag)) {
      lastShown = readTimestamp(in);
    }

    manifest.loadWallpaper(absPath, hash, createdAt, visibility, lastShown);
  }

  return manifest;
}

void ManifestStore::save(Manifest const& manifest) const {
  auto tempPath = temporaryPath(path);
  writeToTemp(tempPath, manifest);
  fsyncFile(tempPath);

  std::error_code errorCode;
  std::filesystem::rename(tempPath, path, errorCode);

  if(static_cast<bool>(errorCode)) {
    throw std::runtime_error("Failed to atomically replace manifest file: " + errorCode.message());
  }
}
