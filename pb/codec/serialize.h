// Copyright 2022 Yuri Wiitala. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <cstdint>
#include <cstring>
#include <iterator>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>

#include "pb/codec/endian.h"
#include "pb/codec/field_rules.h"
#include "pb/codec/iterable_util.h"
#include "pb/codec/limits.h"
#include "pb/codec/map_field_entry.h"
#include "pb/codec/map_field_entry_facade.h"
#include "pb/codec/tag.h"
#include "pb/codec/wire_type.h"
#include "pb/codec/zigzag.h"
#include "pb/field_list.h"
#include "pb/integer_wrapper.h"

namespace pb::codec {

// Source: https://developers.google.com/protocol-buffers/docs/encoding

// Return value when ComputeSerializedValueSize() detects a field that would
// serialize as too many bytes.
//
// The value of this constant is chosen specifically to be both larger than
// |kMaxSerializedSize| but not overflow an int32_t if added to itself several
// times before a size range check occurs in the implementation.
constexpr int32_t kWouldSerializeTooManyBytes = kMaxSerializedSize + 1;

// Return value when ComputeSerializedValueSize() detects the maximum nested
// message depth has been reached (e.g., perhaps due to circular pointers?).
//
// The value of this constant is chosen specifically to be both larger than
// |kMaxSerializedSize| but not overflow an int32_t if added to itself several
// times before a size range check occurs in the implementation.
constexpr int32_t kWouldSerializeTooDeeply = kMaxSerializedSize + 2;

// ------------------------------------------------

// Varints (unsigned): This template covers unsigned char, short, int, etc.
template <typename UnsignedIntegral,
          std::enable_if_t<std::is_integral_v<UnsignedIntegral> &&
                               std::is_unsigned_v<UnsignedIntegral> &&
                               !std::is_same_v<UnsignedIntegral, bool>,
                           int> = 0>
[[nodiscard]] constexpr int32_t ComputeSerializedValueSize(
    UnsignedIntegral value) {
  // Since varints must be 10 bytes or less, only support 64-bit integers or
  // smaller.
  static_assert(sizeof(UnsignedIntegral) <= 8);

  // TODO: For C++20, this should be replaced with:
  //   CEIL(((sizeof(value) * 8) - std::countl_zero(value)) / 7)

  int32_t count = 1;
  while (value >= 128) {
    value >>= 7;
    ++count;
  }
  return count;
}

// Varints (signed): This template covers signed char, short, int, etc.
template <typename SignedIntegral,
          std::enable_if_t<std::is_integral_v<SignedIntegral> &&
                               std::is_signed_v<SignedIntegral>,
                           int> = 0>
[[nodiscard]] constexpr int32_t ComputeSerializedValueSize(
    SignedIntegral value) {
  // Since varints must be 10 bytes or less, only support 64-bit integers or
  // smaller.
  static_assert(sizeof(SignedIntegral) <= 8);

  if (value < 0) {
    // Signed integers are implicitly sign-extended to 64-bits before the varint
    // encoding takes place. Thus, all negative values will be 10 bytes long.
    return 10;
  }
  return ComputeSerializedValueSize(
      static_cast<std::make_unsigned_t<SignedIntegral>>(value));
}

// Bools: Always serialized as one byte.
template <typename Bool, std::enable_if_t<std::is_same_v<Bool, bool>, int> = 0>
[[nodiscard]] constexpr int32_t ComputeSerializedValueSize(Bool) {
  return 1;
}

// Enums: Simply cast to their underlying integral type and encode as a varint.
template <typename Enum, std::enable_if_t<std::is_enum_v<Enum>, int> = 0>
[[nodiscard]] constexpr int32_t ComputeSerializedValueSize(Enum enum_value) {
  // According to Google's public documentation, enum values must be an integer,
  // and within the range of a 32-bit integer.
  static_assert(std::is_integral_v<std::underlying_type_t<Enum>>);
  static_assert(sizeof(std::underlying_type_t<Enum>) <= sizeof(int32_t));
  return ComputeSerializedValueSize(
      static_cast<std::underlying_type_t<Enum>>(enum_value));
}

// ZigZag-encoded signed integers.
template <
    typename SignedIntegerWrapper,
    std::enable_if_t<std::is_same_v<SignedIntegerWrapper, ::pb::sint32_t> ||
                         std::is_same_v<SignedIntegerWrapper, ::pb::sint64_t>,
                     int> = 0>
[[nodiscard]] constexpr int32_t ComputeSerializedValueSize(
    SignedIntegerWrapper wrapped_value) {
  return ComputeSerializedValueSize(EncodeZigZag(wrapped_value.value()));
}

// Fixed64 and Fixed32's: Floating point or integer types as 8 or 4
// little-endian bytes on the wire.
template <typename Fixed,
          std::enable_if_t<std::is_same_v<Fixed, double> ||
                               std::is_same_v<Fixed, float> ||
                               std::is_same_v<Fixed, ::pb::fixed64_t> ||
                               std::is_same_v<Fixed, ::pb::fixed32_t> ||
                               std::is_same_v<Fixed, ::pb::sfixed64_t> ||
                               std::is_same_v<Fixed, ::pb::sfixed32_t>,
                           int> = 0>
[[nodiscard]] constexpr int32_t ComputeSerializedValueSize(Fixed) {
  if constexpr (std::is_same_v<Fixed, double>) {
    static_assert(sizeof(double) == sizeof(uint64_t));
    return sizeof(uint64_t);
  } else if constexpr (std::is_same_v<Fixed, float>) {
    static_assert(sizeof(float) == sizeof(uint32_t));
    return sizeof(uint32_t);
  } else {
    return sizeof(Fixed{}.value());
  }
}

// Strings: Encoded as a length varint followed by the bytes.
template <typename String,
          std::enable_if_t<std::is_same_v<String, std::string> ||
                               std::is_same_v<String, std::string_view>,
                           int> = 0>
[[nodiscard]] constexpr int32_t ComputeSerializedValueSize(
    const String& string) {
  const int32_t varint_byte_count = ComputeSerializedValueSize(string.size());
  // Do a max-encoded-size check here because the cast from String::size_type to
  // int32_t may truncate-away the upper bits on some platforms.
  if (string.size() > static_cast<typename String::size_type>(
                          kMaxSerializedSize - varint_byte_count)) {
    return kWouldSerializeTooManyBytes;
  }
  return varint_byte_count + static_cast<int32_t>(string.size());
}

// Forward declaration of ComputeSerializedValueSize(<nested message>).
template <typename Message,
          std::enable_if_t<std::is_class_v<typename Message::ProtobufFields>,
                           int> = 0>
[[nodiscard]] constexpr int32_t ComputeSerializedValueSize(
    const Message& message);

// Pairs: Serialize the given |pair| as a MapFieldEntry message. This provides
// the support for serializing maps.
template <typename Pair,
          std::enable_if_t<CouldBeAMapFieldEntry<Pair>(), int> = 0>
[[nodiscard]] constexpr int32_t ComputeSerializedValueSize(const Pair& pair) {
  return ComputeSerializedValueSize(AsMapFieldEntryFacade(pair));
}

// Computes the number of bytes needed to encode the elements in a packed
// repeated field.
template <typename ValueType, typename ConstIterator>
[[nodiscard]] int64_t ComputePackedFieldPayloadSizeHelper(ConstIterator begin,
                                                          ConstIterator end) {
  int64_t payload_size;
  if constexpr (GetWireType<ValueType>() == WireType::kVarint) {
    payload_size = 0;
    for (auto it = begin; it != end; ++it) {
      // Note: The static_cast<ValueType&>(*it) below is necessary to adapt
      // iterators that use proxy references (e.g.,
      // std::vector<bool>::const_iterator).
      payload_size +=
          ComputeSerializedValueSize(static_cast<const ValueType&>(*it));
    }
  } else if constexpr (GetWireType<ValueType>() == WireType::kFixed64Bit ||
                       GetWireType<ValueType>() == WireType::kFixed32Bit) {
    constexpr auto kBytesPerElement = ComputeSerializedValueSize(ValueType{});
    payload_size =
        static_cast<int64_t>(std::distance(begin, end)) * kBytesPerElement;
  } else {
    static_assert(GetWireType<ValueType>() == WireType::kVarint ||
                  GetWireType<ValueType>() == WireType::kFixed64Bit ||
                  GetWireType<ValueType>() == WireType::kFixed32Bit);
    // The following is an abuse of an error flag, and is just here to be
    // super-pedantic; though, this code point shouldn't be reachable to begin
    // with!
    payload_size = kWouldSerializeTooManyBytes;
  }
  return payload_size;
}

// See comments for ComputeSerializedSizeOfFields<Message, ...>() below.
//
// This is the base case of the type-system-tail-recursive algorithm, where
// there are no fields left to add to the total byte count. The recursion
// terminates from this point by simply returning |byte_count_so_far| as the
// final total.
template <typename Message>
[[nodiscard]] constexpr int32_t ComputeSerializedSizeOfFields(
    const Message&,
    FieldList<>,
    int64_t byte_count_so_far = 0) {
  // Note: The max-size check is forced here since the 64-bit
  // |byte_count_so_far| is being truncated to a 32-bit return value.
  if (byte_count_so_far > kMaxSerializedSize) {
    return kWouldSerializeTooManyBytes;
  }
  return static_cast<int32_t>(byte_count_so_far);
}

// This type-system-tail-recursive function walks the fields of |message|,
// computing the encoded size of each field's tag+value and accumulating it into
// |byte_count_so_far|.
//
// Optimization note: Throughout the execution, |byte_count_so_far| could easily
// exceed |kMaxSerializedSize| at any point. While it seems sound to check for
// this case frequently and do an early-return to avoid extra work, that would
// be coding to what should be an extremely uncommon case. In other words, the
// user shouldn't be designing their application in such a way that serializing
// frequently causes the maximum encoding size to be breached. Thus, this check
// only occurs at the very end, in the base-case specialization of this function
// (above). This philosophy also makes the code here a lot cleaner.
template <typename Message, typename FirstField, typename... TheRemainingFields>
[[nodiscard]] constexpr int32_t ComputeSerializedSizeOfFields(
    const Message& message,
    FieldList<FirstField, TheRemainingFields...>,
    int64_t byte_count_so_far = 0) {
  constexpr Tag kTag = GetTagForSerialization<FirstField>();
  constexpr int32_t kTagSize = ComputeSerializedValueSize(kTag);

  if constexpr (CanEncodeAsAPackedRepeatedField<FirstField>()) {
    const auto& container = FirstField::GetMemberReferenceIn(message);
    const auto end = std::end(container);
    auto it = std::begin(container);
    if (it != end) {
      byte_count_so_far += kTagSize;
      using ValueType = IterableValueType<typename FirstField::Member>;
      const int64_t payload_size =
          ComputePackedFieldPayloadSizeHelper<ValueType>(it, end);
      byte_count_so_far +=
          // Optimization: |payload_size| is positive. However, if it's too
          // positive, where truncating it to 32-bits makes it encode as a
          // smaller varint, that error won't matter at the end:
          // |byte_count_so_far| will still end up exceeding
          // |kMaxSerializedSize|, triggering the final max-encoded-size check.
          ComputeSerializedValueSize(static_cast<uint32_t>(payload_size));
      byte_count_so_far += payload_size;
    }
  } else if constexpr (IsRepeatedField<FirstField>()) {
    for (const auto& member : FirstField::GetMemberReferenceIn(message)) {
      if (IsStoringOneValue(member)) {
        byte_count_so_far += kTagSize;
        byte_count_so_far += ComputeSerializedValueSize(GetTheOneValue(member));
      }
    }
  } else {
    const auto& member = FirstField::GetMemberReferenceIn(message);
    if (IsStoringOneValue(member)) {
      byte_count_so_far += kTagSize;
      byte_count_so_far += ComputeSerializedValueSize(GetTheOneValue(member));
    }
  }

  return ComputeSerializedSizeOfFields(
      message, FieldList<TheRemainingFields...>{}, byte_count_so_far);
}

// Nested Messages: Encoded as a length varint followed by the encoding of the
// fields from the FieldsList walkers (above).
template <
    typename Message,
    std::enable_if_t<std::is_class_v<typename Message::ProtobufFields>, int>>
[[nodiscard]] constexpr int32_t ComputeSerializedValueSize(
    const Message& message) {
  const int32_t payload_size = ComputeSerializedSizeOfFields(
      message, typename Message::ProtobufFields{});
  return (ComputeSerializedValueSize(
              // Optimization: Skip <0 check since |payload_size| is >= 0.
              static_cast<uint32_t>(payload_size)) +
          payload_size);
}

// ------------------------------------------------

// All the SerializeValue...() functions return a pointer to the byte just after
// the last byte output to the buffer. There is no failure case (i.e., these
// functions never return nullptr).

// Varints (unsigned): This template covers unsigned char, short, int, etc.
template <typename UnsignedIntegral,
          std::enable_if_t<std::is_integral_v<UnsignedIntegral> &&
                               std::is_unsigned_v<UnsignedIntegral> &&
                               !std::is_same_v<UnsignedIntegral, bool>,
                           int> = 0>
[[nodiscard]] uint8_t* SerializeValue(UnsignedIntegral value, uint8_t* buffer) {
  while (value >= 128) {
    *buffer = 0b10000000 | static_cast<uint8_t>(value);
    ++buffer;
    value >>= 7;
  }
  // The higest bit in the last byte being output to the buffer must be zero to
  // terminate the byte sequence. This is guaranteed if the number of bits in an
  // UnsignedIntegral is not divisible by 7.
  static_assert((std::numeric_limits<UnsignedIntegral>::digits % 7) != 0);
  *buffer = static_cast<uint8_t>(value);
  return buffer + 1;
}

// Varints (signed): This template covers signed char, short, int, etc.
template <typename SignedIntegral,
          std::enable_if_t<std::is_integral_v<SignedIntegral> &&
                               std::is_signed_v<SignedIntegral>,
                           int> = 0>
[[nodiscard]] uint8_t* SerializeValue(SignedIntegral value, uint8_t* buffer) {
  // Signed integers must be sign-extended to 64-bits before the varint encoding
  // takes place. This is a requirement of the wire format, to ensure
  // backwards-compatibility when integral message field types are changed to
  // use more bits (e.g., serializing as int32_t, but parsing as int64_t).
  //
  // If |value| is non-negative, it does not need to be sign-extended. However,
  // it would be more expensive to branch on that condition. So, just do it
  // unconditionally.
  const int64_t sign_extended_value = value;
  return SerializeValue(static_cast<uint64_t>(sign_extended_value), buffer);
}

// Bools: Always one byte.
template <typename Bool, std::enable_if_t<std::is_same_v<Bool, bool>, int> = 0>
[[nodiscard]] uint8_t* SerializeValue(Bool value, uint8_t* buffer) {
  *buffer = static_cast<uint8_t>(value);
  return buffer + 1;
}

// Enums: Simply cast to their underlying integral type and encode as a varint.
template <typename Enum, std::enable_if_t<std::is_enum_v<Enum>, int> = 0>
[[nodiscard]] uint8_t* SerializeValue(Enum enum_value, uint8_t* buffer) {
  return SerializeValue(static_cast<std::underlying_type_t<Enum>>(enum_value),
                        buffer);
}

// ZigZag-encoded signed integers.
template <
    typename SignedIntegerWrapper,
    std::enable_if_t<std::is_same_v<SignedIntegerWrapper, ::pb::sint32_t> ||
                         std::is_same_v<SignedIntegerWrapper, ::pb::sint64_t>,
                     int> = 0>
[[nodiscard]] uint8_t* SerializeValue(SignedIntegerWrapper wrapped_value,
                                      uint8_t* buffer) {
  return SerializeValue(EncodeZigZag(wrapped_value.value()), buffer);
}

// Fixed64's: Doubles, or fixed-size 64-bit integers (as 8 little-endian bytes
// on the wire).
template <typename Fixed64,
          std::enable_if_t<std::is_same_v<Fixed64, double> ||
                               std::is_same_v<Fixed64, ::pb::fixed64_t> ||
                               std::is_same_v<Fixed64, ::pb::sfixed64_t>,
                           int> = 0>
[[nodiscard]] uint8_t* SerializeValue(Fixed64 value, uint8_t* buffer) {
  // Implementation note: On little-endian architectures, the compiler will
  // optimize all of the following to a simple 8-byte copy, often a single
  // instruction.
  uint64_t bits;
  if constexpr (std::is_same_v<Fixed64, double>) {
    static_assert(sizeof(double) == sizeof(uint64_t));
    static_assert(std::numeric_limits<double>::is_iec559);
    std::memcpy(&bits, &value, sizeof(uint64_t));
  } else {
    bits = static_cast<uint64_t>(value.value());
  }
  if (!IsLittleEndianArchitecture()) {
    bits = ReverseBytes64(bits);
  }
  std::memcpy(buffer, &bits, sizeof(uint64_t));

  return buffer + sizeof(uint64_t);
}

// Fixed32's: Floats or fixed-size 32-bit integers (as 4 little-endian bytes on
// the wire).
template <typename Fixed32,
          std::enable_if_t<std::is_same_v<Fixed32, float> ||
                               std::is_same_v<Fixed32, ::pb::fixed32_t> ||
                               std::is_same_v<Fixed32, ::pb::sfixed32_t>,
                           int> = 0>
[[nodiscard]] uint8_t* SerializeValue(Fixed32 value, uint8_t* buffer) {
  // Implementation note: On little-endian architectures, the compiler will
  // optimize all of the following to a simple 4-byte copy, often a single
  // instruction.
  uint32_t bits;
  if constexpr (std::is_same_v<Fixed32, float>) {
    static_assert(sizeof(float) == sizeof(uint32_t));
    static_assert(std::numeric_limits<float>::is_iec559);
    std::memcpy(&bits, &value, sizeof(uint32_t));
  } else {
    bits = static_cast<uint32_t>(value.value());
  }
  if (!IsLittleEndianArchitecture()) {
    bits = ReverseBytes32(bits);
  }
  std::memcpy(buffer, &bits, sizeof(uint32_t));

  return buffer + sizeof(uint32_t);
}

// Strings: Encoded as a length varint followed by the bytes.
template <typename String,
          std::enable_if_t<std::is_same_v<String, std::string> ||
                               std::is_same_v<String, std::string_view>,
                           int> = 0>
[[nodiscard]] uint8_t* SerializeValue(const String& string, uint8_t* buffer) {
  static_assert(sizeof(typename String::value_type) == 1);
  buffer = SerializeValue(string.size(), buffer);
  std::memcpy(buffer, string.data(), string.size());
  return buffer + string.size();
}

// Forward declaration of SerializeValue(<nested message>).
template <typename Message,
          std::enable_if_t<std::is_class_v<typename Message::ProtobufFields>,
                           int> = 0>
[[nodiscard]] uint8_t* SerializeValue(const Message& message, uint8_t* buffer);

// Pairs: Serialize as a MapFieldEntry, to support serializing maps.
template <typename Pair,
          std::enable_if_t<CouldBeAMapFieldEntry<Pair>(), int> = 0>
[[nodiscard]] uint8_t* SerializeValue(const Pair& pair, uint8_t* buffer) {
  return SerializeValue(AsMapFieldEntryFacade(pair), buffer);
}

// See comments for SerializeFields<Message, ...>() below.
//
// This is the base case of the type-system-tail-recursive algorithm, where
// there are no fields left to be serialized. Nothing is appended to |buffer|
// and the recursion terminates from this point.
template <typename Message>
uint8_t* SerializeFields(const Message&, FieldList<>, uint8_t* buffer) {
  return buffer;
}

// This type-system-tail-recursive function walks the fields of |message|,
// encoding each field's tag+value and appending it to |buffer|.
template <typename Message, typename FirstField, typename... TheRemainingFields>
uint8_t* SerializeFields(const Message& message,
                         FieldList<FirstField, TheRemainingFields...>,
                         uint8_t* buffer) {
  constexpr Tag kTag = GetTagForSerialization<FirstField>();

  if constexpr (CanEncodeAsAPackedRepeatedField<FirstField>()) {
    const auto& container = FirstField::GetMemberReferenceIn(message);
    const auto end = std::end(container);
    auto it = std::begin(container);
    if (it != end) {
      buffer = SerializeValue(kTag, buffer);

      using ValueType = IterableValueType<typename FirstField::Member>;
      const int64_t payload_size =
          ComputePackedFieldPayloadSizeHelper<ValueType>(it, end);
      buffer = SerializeValue(static_cast<uint32_t>(payload_size), buffer);

      // Note: The static_cast<ValueType&>(*it) below is necessary to adapt
      // iterators that use proxy references (e.g.,
      // std::vector<bool>::const_iterator).
      do {
        buffer = SerializeValue(static_cast<const ValueType&>(*it), buffer);
        ++it;
      } while (it != end);
    }
  } else if constexpr (IsRepeatedField<FirstField>()) {
    for (const auto& element_in_container :
         FirstField::GetMemberReferenceIn(message)) {
      if (IsStoringOneValue(element_in_container)) {
        buffer = SerializeValue(kTag, buffer);
        buffer = SerializeValue(GetTheOneValue(element_in_container), buffer);
      }
    }
  } else {
    const auto& member = FirstField::GetMemberReferenceIn(message);
    if (IsStoringOneValue(member)) {
      buffer = SerializeValue(kTag, buffer);
      buffer = SerializeValue(GetTheOneValue(member), buffer);
    }
  }

  return SerializeFields(message, FieldList<TheRemainingFields...>{}, buffer);
}

// Nested Messages: Encoded as a length varint followed by the encoding of the
// fields from the FieldsList walkers (above).
template <
    typename Message,
    std::enable_if_t<std::is_class_v<typename Message::ProtobufFields>, int>>
[[nodiscard]] uint8_t* SerializeValue(const Message& message, uint8_t* buffer) {
  const int32_t payload_size = ComputeSerializedSizeOfFields(
      message, typename Message::ProtobufFields{});
  buffer = SerializeValue(
      // Optimization: Skip <0 check since |payload_size| is >= 0.
      static_cast<uint32_t>(payload_size), buffer);
  return SerializeFields(message, typename Message::ProtobufFields{}, buffer);
}

}  // namespace pb::codec
