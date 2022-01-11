// Copyright 2022 Yuri Wiitala. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <cstdint>

namespace pb::codec {

// These limits are the same as those in Google's open-source implementation.
// Quite reasonable.

// Maximum number of serialized bytes for an outermost message.
constexpr int32_t kMaxSerializedSize = 64 << 20;  // 64 megabytes.

// Maximum message nesting depth.
constexpr int kMaxMessageNestingDepth = 100;

// Returns true if |x| is within the range of valid field numbers.
[[nodiscard]] constexpr bool IsValidFieldNumber(int32_t x) {
  // According to Google's public documentation, field numbers must be in the
  // range 1 to 2^29 - 1, and also not in the range 19000 to 19999 (which is
  // reserved for the Protocol Buffers implementation for some unexplained
  // reason).
  return ((x >= 1) && (x < 19000 || x > 19999) && (x <= 536870911));
}

}  // namespace pb::codec
