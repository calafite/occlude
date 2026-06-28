#include "hash.hpp"

#include "common.hpp"
#include "picosha2.h"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace {
  inline std::byte castByte(unsigned char character) {
    return static_cast<std::byte>(character);
  }

  inline unsigned char castChar(std::byte byte) {
    return static_cast<unsigned char>(byte);
  }
} // namespace

Hash Hash::fromBytes(ByteSpan bytes) {
  const auto inputSize = bytes.size();
  std::vector<unsigned char> input(inputSize);
  std::ranges::transform(bytes, input.begin(), castChar);

  std::vector<unsigned char> digest(picosha2::k_digest_size);
  picosha2::hash256(
      input.begin(),  //
      input.end(),    //
      digest.begin(), //
      digest.end()    //
  );

  HashByteArray rBytes{};
  std::ranges::transform(digest, rBytes.begin(), castByte);
  return Hash(rBytes);
}
