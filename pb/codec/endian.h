// Copyright 2022 Yuri Wiitala. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <cstdint>
#include <cstring>

namespace pb::codec {

// Returns true if this platform stores integers in little-endian byte order.
//
// Implementation note: In C++20, this function could simply be a constant
// expression:
//
//   std::endian::native == std::endian::little.
//
// Before C++20, there is no way to write a constexpr expression or function to
// detect the platform's endianness. Thus, the following inline function carries
// out the explicit steps for detection. Luckily, all the popular optimizing
// compilers will reduce this to a boolean constant.
[[nodiscard]] inline bool IsLittleEndianArchitecture() {
  const uint32_t kTestWord = 0x01234567;
  uint8_t bytes[sizeof(kTestWord)];
  std::memcpy(bytes, &kTestWord, sizeof(bytes));
  return bytes[0] == 0x67;
}

// Reverses the ordering of the storage bytes of the given 64-bit |value|
// (a.k.a. "byte swap"). Note that all recent GCC and Clang compiler optimizers
// will reduce this code to a single bswap instruction (tested: x86 and ARM).
[[nodiscard]] constexpr uint64_t ReverseBytes64(uint64_t value) {
  uint64_t result{};
  result |= ((value >> 0) & 0xff) << 56;
  result |= ((value >> 8) & 0xff) << 48;
  result |= ((value >> 16) & 0xff) << 40;
  result |= ((value >> 24) & 0xff) << 32;
  result |= ((value >> 32) & 0xff) << 24;
  result |= ((value >> 40) & 0xff) << 16;
  result |= ((value >> 48) & 0xff) << 8;
  result |= ((value >> 56) & 0xff) << 0;
  return result;
}

// Reverses the ordering of the storage bytes of the given 32-bit |value|
// (a.k.a. "byte swap"). Note that all recent GCC and Clang compiler optimizers
// will reduce this code to a single bswap instruction (tested: x86 and ARM).
[[nodiscard]] constexpr uint32_t ReverseBytes32(uint32_t value) {
  uint32_t result{};
  result |= ((value >> 0) & 0xff) << 24;
  result |= ((value >> 8) & 0xff) << 16;
  result |= ((value >> 16) & 0xff) << 8;
  result |= ((value >> 24) & 0xff) << 0;
  return result;
}

}  // namespace pb::codec
