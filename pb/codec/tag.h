// Copyright 2022 Yuri Wiitala. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <cstdint>
#include <limits>

#include "pb/codec/field_rules.h"
#include "pb/codec/iterable_util.h"
#include "pb/codec/wire_type.h"
#include "pb/field_list.h"

namespace pb::codec {

using Tag = uint32_t;

constexpr int kNumWireTypeBits = 3;

// Returns a Tag (a.k.a. a "key" in Google's public Protocol Buffers
// documentation) containing the given |field_number| and |wire_type|.
[[nodiscard]] constexpr Tag MakeTag(int32_t field_number, WireType wire_type) {
  Tag tag = static_cast<Tag>(field_number) << kNumWireTypeBits;
  tag |= static_cast<uint8_t>(wire_type);
  return tag;
}

// Returns the Tag (a.k.a. a "key" in Google's public Protocol Buffers
// documentation) for a field to be serialized. This is the decoded value, not
// the varint encoding. A tag consists of 29 bits of field number followed by 3
// bits of wire type.
template <typename Field>
[[nodiscard]] constexpr Tag GetTagForSerialization() {
  static_assert((uint64_t{Field::GetFieldNumber()} << kNumWireTypeBits) <=
                std::numeric_limits<Tag>::max());
  if constexpr (CanEncodeAsAPackedRepeatedField<Field>()) {
    return MakeTag(Field::GetFieldNumber(), WireType::kLengthDelimited);
  } else if constexpr (IsRepeatedField<Field>()) {
    return MakeTag(Field::GetFieldNumber(),
                   GetWireType<IterableValueType<typename Field::Member>>());
  } else {
    return MakeTag(Field::GetFieldNumber(),
                   GetWireType<typename Field::Member>());
  }
}

// Extracts the wire type from the given |tag|.
[[nodiscard]] constexpr WireType GetWireTypeFromTag(Tag tag) {
  return static_cast<WireType>(tag & ((1 << kNumWireTypeBits) - 1));
}

// Extracts the field number from the given |tag|.
[[nodiscard]] constexpr int32_t GetFieldNumberFromTag(Tag tag) {
  return static_cast<int32_t>(tag >> kNumWireTypeBits);
}

}  // namespace pb::codec
