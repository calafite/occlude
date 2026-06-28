#include "hash.hpp"

#include "common.hpp"
#include "picosha2.h"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace {
  inline std::byte castByte(unsigned char character) {
    return static_cast<std::byte>(character);
  }

  inline unsigned char castChar(std::byte byte) {
    return static_cast<unsigned char>(byte);
  }
} // namespace

Hash Hash::fromFile(const FilePath& path) {
  std::ifstream in(path, std::ios::binary);

  if(!static_cast<bool>(in)) {
    throw std::runtime_error("Failed to open file for hashing: " + path.string());
  }

  std::vector<unsigned char> digest(picosha2::k_digest_size);
  picosha2::hash256(
      std::istreambuf_iterator<char>(in), //
      std::istreambuf_iterator<char>(),   //
      digest.begin(),                     //
      digest.end()                        //
  );

  bool correctSize = digest.size() == HASH_SIZE;
  if(!correctSize) {
    throw std::runtime_error("Unexpected digest size from hash function");
  }

  HashByteArray rBytes{};
  std::ranges::transform(digest, rBytes.begin(), castByte);
  return Hash(rBytes);
}

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
