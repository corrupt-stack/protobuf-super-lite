// Copyright 2022 Yuri Wiitala. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <cassert>
#include <cstdint>
#include <type_traits>

#include "pb/codec/limits.h"
#include "pb/codec/serialize.h"

namespace pb {

// Computes the size of a serialized version of |message|, in bytes, while also
// validating the object graph. Returns -1 if the serialized size would be
// larger than the design limit.
//
// When nesting messages, it is the responsibility of the caller to ensure the
// message nesting depth does not exceed |pb::codec::kMaxMessageNestingDepth|,
// found in pb/codec/limits.h. This implementation; to remain small, simple, and
// efficient; does not sanity-check this. It will happily serialize the message
// structure, but other protobuf implementations will reject the wire bytes when
// parsed.
template <class Message,
          std::enable_if_t<std::is_class_v<typename Message::ProtobufFields>,
                           int> = 0>
[[nodiscard]] constexpr int32_t ComputeSerializedSize(const Message& message) {
  const auto size = codec::ComputeSerializedSizeOfFields(
      message, typename Message::ProtobufFields{});
  return (size <= codec::kMaxSerializedSize) ? size : -1;
}

// Serializes the given |message| into the given |buffer|. The |buffer| must be
// at least as large as the value returned by ComputeSerializedSize(message).
//
// WARNING: Very bad things will happen if ComputeSerializedSize(), above, would
// return -1. Note that all sanity-checking occurs there, and Serialize() is
// made efficient by assuming there are no possible error/failure cases.
template <class Message,
          std::enable_if_t<std::is_class_v<typename Message::ProtobufFields>,
                           int> = 0>
void Serialize(const Message& message, uint8_t* buffer) {
  assert(ComputeSerializedSize(message) >= 0);
  assert(buffer);
  [[maybe_unused]] auto* const buffer_end = codec::SerializeFields(
      message, typename Message::ProtobufFields{}, buffer);
  assert((buffer + ComputeSerializedSize(message)) == buffer_end);
}

}  // namespace pb
