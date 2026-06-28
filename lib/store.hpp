#pragma once

#include "common.hpp"
#include "fs.hpp"
#include "hash.hpp"
#include "state.hpp"

#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace detail {
  constexpr MagicBytes kMagic = {'O', 'C', 'C', 'L'};
  constexpr std::uint32_t kVersion = 1;

  template<typename Type>
  concept PodCapable = std::semiregular<Type> && std::is_trivially_copyable_v<Type>;

  struct BufferReader {
    ByteSpan buffer;
    std::size_t offset = 0;

    explicit BufferReader(ByteSpan b) : buffer(b) {}

    void readRaw(void* dest, std::size_t size) {
      if(offset + size > buffer.size()) {
        throw std::runtime_error("Manifest file truncated or unreadable");
      }
      std::memcpy(dest, buffer.data() + offset, size);
      offset += size;
    }

    template<PodCapable Type>
    Type readPod() {
      Type value{};
      readRaw(&value, sizeof(Type));
      return value;
    }

    std::string readString() {
      const auto size = readPod<std::uint32_t>();
      std::string str(size, '\0');
      if(size > 0) {
        readRaw(str.data(), size);
      }
      return str;
    }

    std::optional<std::string> readOptionalString() {
      const auto present = readPod<std::uint8_t>();
      if(static_cast<bool>(present)) {
        return readString();
      }
      return std::nullopt;
    }

    Timestamp readTimestamp() {
      const auto nanoseconds = readPod<std::int64_t>();
      return Timestamp(static_cast<NanosecondsType>(nanoseconds));
    }
  };

  struct BufferWriter {
    std::vector<std::byte> buffer;

    void writeRaw(const void* src, std::size_t size) {
      const auto* p = static_cast<const std::byte*>(src);
      buffer.insert(buffer.end(), p, p + size);
    }

    template<PodCapable Type>
    void writePod(Type const& val) {
      writeRaw(&val, sizeof(Type));
    }

    void writeString(std::string const& str) {
      const auto size = static_cast<std::uint32_t>(str.size());
      writePod(size);
      writeRaw(str.data(), str.size());
    }

    void writeOptionalString(std::optional<std::string> const& oStr) {
      std::uint8_t present = oStr.has_value() ? 1 : 0;
      writePod(present);
      if(static_cast<bool>(present)) {
        writeString(*oStr);
      }
    }

    void writeTimestamp(Timestamp const& timestamp) {
      const auto nanoseconds =
          static_cast<std::int64_t>(std::chrono::duration_cast<NanosecondsType>(timestamp.time_since_epoch()).count());
      writePod(nanoseconds);
    }

    [[nodiscard]] ByteSpan viewBytes() const {
      return buffer;
    }
  };
} // namespace detail

template<FileSystem FS>
struct ManifestStore {
  ManifestStore(FS& filesystem, FilePath manifestPath) : fs(filesystem), path(std::move(manifestPath)) {}

  [[nodiscard]] Manifest load() const {
    if(!fs.get().exists(path)) {
      return Manifest{};
    }

    auto data = fs.get().read(path);
    detail::BufferReader reader(data);

    auto magic = reader.readPod<MagicBytes>();
    if(magic != detail::kMagic) {
      throw std::runtime_error("Manifest has invalid magic bytes");
    }

    const auto version = reader.readPod<std::uint32_t>();
    if(version != detail::kVersion) {
      throw std::runtime_error("Manifest file has unsupported version: " + std::to_string(version));
    }

    State state;
    state.stateMode = static_cast<StateMode>(reader.readPod<std::uint8_t>());
    state.publicCurrent = reader.readOptionalString();
    state.privateCurrent = reader.readOptionalString();

    Manifest manifest(std::move(state));

    const auto count = reader.readPod<std::uint64_t>();
    manifest.wallpapers.reserve(count);

    for(std::uint64_t i = 0; i < count; ++i) {
      FilePath absPath = reader.readString();

      HashByteArray hashBytes{};
      reader.readRaw(hashBytes.data(), hashBytes.size());
      Hash hash(hashBytes);

      Timestamp createdAt = reader.readTimestamp();
      auto visibility = static_cast<Visibility>(reader.readPod<std::uint8_t>());

      auto lastShownTag = reader.readPod<std::uint8_t>();
      std::optional<Timestamp> lastShown;
      if(static_cast<bool>(lastShownTag)) {
        lastShown = reader.readTimestamp();
      }

      manifest.loadWallpaper(std::move(absPath), hash, createdAt, visibility, lastShown);
    }

    return manifest;
  }

  void save(Manifest const& manifest) const {
    FilePath tempPath = path;
    tempPath += ".tmp";

    detail::BufferWriter writer;

    writer.writePod(detail::kMagic);
    writer.writePod(detail::kVersion);

    const auto stateMode = static_cast<std::uint8_t>(manifest.state.stateMode);
    writer.writePod(stateMode);
    writer.writeOptionalString(manifest.state.publicCurrent);
    writer.writeOptionalString(manifest.state.privateCurrent);

    const auto wallpaperCount = static_cast<std::uint64_t>(manifest.wallpapers.size());
    writer.writePod(wallpaperCount);

    for(auto const& wallpaper : manifest.wallpapers) {
      writer.writeString(wallpaper->absPath.string());
      writer.writeRaw(wallpaper->hash.value.data(), wallpaper->hash.value.size());
      writer.writeTimestamp(wallpaper->createdAt);
      writer.writePod(static_cast<std::uint8_t>(wallpaper->visibility));

      std::uint8_t tag = wallpaper->lastShown.has_value() ? 1 : 0;
      writer.writePod(tag);
      if(static_cast<bool>(tag)) {
        writer.writeTimestamp(*wallpaper->lastShown);
      }
    }

    fs.get().write(tempPath, writer.viewBytes());
    fs.get().sync(tempPath);

    MoveOperation moveOp{.from = tempPath, .to = path};
    fs.get().move(moveOp);
  }

private:
  std::reference_wrapper<FS> fs;
  FilePath path;
};
