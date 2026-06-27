#pragma once

#include "common.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <stdexcept>
#include <ranges> 

struct Hash {
  ByteArray value{};
  auto operator<=>(Hash const&) const = default;
  
  constexpr Hash() noexcept = default;
  constexpr explicit Hash(ByteArray const& valueV) noexcept : value(valueV) {}
  constexpr explicit Hash(ByteSpan const& sourceV) noexcept {
    std::ranges::copy(sourceV, value.begin());
  }
  
  constexpr explicit Hash(std::string_view hexValue) {
    bool rightSize = hexValue.size() == 64;

    if(!rightSize) {
      throw std::invalid_argument("Invalid size for Hex string; must be 64 characters long");
    }

    for(std::size_t i = 0; i < HASH_SIZE; ++i) {
      std::uint8_t high = parseChar(hexValue[2*i]);
      std::uint8_t low = parseChar(hexValue[(2*i)+1]);
      value.at(i) = static_cast<std::byte>((high << 4) | low);
    }
  }

  private:
    static std::uint8_t parseChar(char character) {
      if(character >= '0' && character <= '9') {
        return character - '0';
      }
      if(character >= 'a' && character <= 'f') {
        return 10 + (character - 'a');
      }
      if(character >= 'A' && character <= 'F') {
        return 10 + (character - 'A');
      }
      throw std::invalid_argument("Invalid Hex character");
    }
};


template<>
struct std::hash<Hash> {
  std::size_t operator()(Hash const& hash) const noexcept {
    HashChunk chunk{};
    std::ranges::copy(hash.value | std::views::take(UINT_SIZE), chunk.begin());
    return std::bit_cast<std::size_t>(chunk);
  }
};
