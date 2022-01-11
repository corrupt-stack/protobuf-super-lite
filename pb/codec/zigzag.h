// Copyright 2022 Yuri Wiitala. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <limits>
#include <type_traits>

namespace pb::codec {

// Documentation:
// https://developers.google.com/protocol-buffers/docs/encoding#signed_integers

// Encodes the given signed integer |n| to its ZigZag representation. Always
// returns an unsigned integer of the same number of bits as |n|.
//
// Example (32-bit signed integer):
//   0 → 0
//   -1 → 1
//   1 → 2
//   -2 → 3
//   2 → 4
//   ...
//   2147483647 → 4294967294
//   -2147483648 → 4294967295
template <typename SignedInteger,
          std::enable_if_t<std::is_integral_v<SignedInteger> &&
                               std::is_signed_v<SignedInteger>,
                           int> = 0>
[[nodiscard]] constexpr auto EncodeZigZag(SignedInteger n) {
  using Bits = std::make_unsigned_t<SignedInteger>;
  // Note: The right-shift in the expression below is a sign-extending shift.
  const auto all_bits_set_if_negative_value =
      static_cast<Bits>(n >> std::numeric_limits<decltype(n)>::digits);
  return (static_cast<Bits>(n) << 1) ^ all_bits_set_if_negative_value;
}

// Decodes the given ZigZag-encoded |bits| to the native signed integer.
template <typename Bits,
          std::enable_if_t<std::is_integral_v<Bits> && std::is_unsigned_v<Bits>,
                           int> = 0>
[[nodiscard]] constexpr auto DecodeZigZag(Bits bits) {
  using SignedInteger = std::make_signed_t<Bits>;
  return static_cast<SignedInteger>((bits >> 1) ^ (~(bits & 1) + 1));
}

}  // namespace pb::codec
