#pragma once
#include <chrono>
#include <filesystem>
#include <array>
#include <span>

using Timestamp = std::chrono::system_clock::time_point;
using FilePath = std::filesystem::path;

using nanosecondsType = std::chrono::nanoseconds;
using std::chrono::duration_cast;

const size_t HASH_SIZE = 32;
const size_t UINT_SIZE = sizeof(std::size_t);

using HashByteArray = std::array<std::byte, HASH_SIZE>;
using ByteArray = std::array<std::byte, HASH_SIZE>;
using HashByteSpan = std::span<const std::byte, HASH_SIZE>;
using ByteSpan = std::span<const std::byte>;
using HashChunk = std::array<std::byte, UINT_SIZE>;
using MagicBytes = std::array<char, 4>;
