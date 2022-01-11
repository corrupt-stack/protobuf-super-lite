// Copyright 2022 Yuri Wiitala. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

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
//
// All the ParseValue...() functions return the following:
//
//   If the parse succeeds: Pointer to the first byte just after the region
//   "consumed" by the parse.
//
//   If the parse fails: nullptr (and NOTE that the output argument may or may
//   not have been modified!).

// Varints: This template covers (un)signed char, short, int, etc.; but NOT
// bool. Tags are also encoded as varints.
template <typename Integral,
          std::enable_if_t<std::is_integral_v<Integral> &&
                               !std::is_same_v<Integral, bool>,
                           int> = 0>
[[nodiscard]] const uint8_t* ParseValue(const uint8_t* buffer,
                                        const uint8_t* buffer_end,
                                        int /* nesting_level */,
                                        Integral& result) {
  using UnsignedIntegral = std::make_unsigned_t<Integral>;
  static constexpr auto kMaxPossibleSize =
      (std::numeric_limits<UnsignedIntegral>::digits + 6) / 7;

  // Implementation note: Google's open-source implementation includes an
  // optimization for varint parsing. Whenever at least 10 bytes are left in the
  // buffer, the bytes are processed without checking for |buffer_end| in each
  // iteration.
  //
  // While this sounds like it should provide a speed-up (fewer instructions,
  // and one fewer branch), this did not at all pan-out in my own benchmarking
  // tests on modern hardware. In fact, in half of the scenarios I tested, the
  // execution time was surprisingly 10-25% worse! Also, I won't claim that I
  // thoroughly tested a wide sample of architectures/platforms, or generations
  // of hardware. However, it seems reasonable that, in an age where even
  // 50-cent microcontrollers use superscalar processor architectures and branch
  // prediction, this "optimization" is nowadays moot.
  //
  // So, I've kept it simple here, and the compiler optimizer happily unrolls
  // this loop:
  UnsignedIntegral bits{};
  for (int i = 0; i < kMaxPossibleSize; ++i) {
    if (&buffer[i] >= buffer_end) {
      return nullptr;  // Unexpected end-of-buffer reached.
    }
    bits |= static_cast<UnsignedIntegral>(buffer[i] & 0b01111111) << (i * 7);
    if (!(buffer[i] & 0b10000000)) {
      result = static_cast<Integral>(bits);
      return &buffer[i + 1];
    }
  }

  // If this point is reached, skip over the remainder of the encoded varint.
  // According to Google's public documentation, a varint parsed from the wire,
  // that does not fit the type on the receiving end, should have a parse as if
  // a C++ truncation type-cast occurred.
  for (buffer += kMaxPossibleSize; buffer != buffer_end; ++buffer) {
    if (!(*buffer & 0b10000000)) {
      result = static_cast<Integral>(bits);
      return buffer + 1;
    }
  }

  return nullptr;  // Unexpected end-of-buffer reached.
}

// Bools: Encoded as varint. While it seems obvious that these should only ever
// be a 1-byte varint (to store a 0 or 1 value), Google's public documentation
// explicitly states that, for int32/int64/uint32/uint64/bool, parsers must be
// fully forwards- and backwards-compatible. Thus, this code assumes zero is
// false and a varint of any valid length that is non-zero is true.
template <typename Bool, std::enable_if_t<std::is_same_v<Bool, bool>, int> = 0>
[[nodiscard]] const uint8_t* ParseValue(const uint8_t* buffer,
                                        const uint8_t* buffer_end,
                                        int nesting_level,
                                        Bool& result) {
  uint64_t value{};
  buffer = ParseValue(buffer, buffer_end, nesting_level, value);
  // If the above returned nullptr (a parse error), that will propagate to the
  // caller of this function regardless of the post-processing below.
  result = !!value;
  return buffer;
}

// Enums: Parse as a varint into their underlying integral type.
template <typename Enum, std::enable_if_t<std::is_enum_v<Enum>, int> = 0>
[[nodiscard]] const uint8_t* ParseValue(const uint8_t* buffer,
                                        const uint8_t* buffer_end,
                                        int nesting_level,
                                        Enum& result) {
  std::underlying_type_t<Enum> value{};
  buffer = ParseValue(buffer, buffer_end, nesting_level, value);
  // If the above returned nullptr (a parse error), that will propagate to the
  // caller of this function regardless of the post-processing below.
  result = static_cast<Enum>(value);
  return buffer;
}

// ZigZag-encoded signed integers.
template <
    typename SignedIntegerWrapper,
    std::enable_if_t<std::is_same_v<SignedIntegerWrapper, ::pb::sint32_t> ||
                         std::is_same_v<SignedIntegerWrapper, ::pb::sint64_t>,
                     int> = 0>
[[nodiscard]] const uint8_t* ParseValue(const uint8_t* buffer,
                                        const uint8_t* buffer_end,
                                        int nesting_level,
                                        SignedIntegerWrapper& result) {
  using Bits = std::make_unsigned_t<decltype(result.value())>;
  Bits bits{};
  buffer = ParseValue(buffer, buffer_end, nesting_level, bits);
  // If the above returned nullptr (a parse error), that will propagate to the
  // caller of this function regardless of the post-processing below.
  result = DecodeZigZag(bits);
  return buffer;
}

// Copies the bytes from |buffer| to |result| with byte-order conversion for
// fixed-size 64-bit values.
template <typename Fixed64,
          std::enable_if_t<std::is_same_v<Fixed64, double> ||
                               std::is_same_v<Fixed64, ::pb::fixed64_t> ||
                               std::is_same_v<Fixed64, ::pb::sfixed64_t>,
                           int> = 0>
void ParseFixedValueNoBoundsCheck(const uint8_t* buffer, Fixed64& result) {
  // Implementation note: On little-endian architectures, the compiler will
  // optimize all of the following to a simple 8-byte copy, often a single
  // instruction.
  uint64_t bits;
  std::memcpy(&bits, buffer, sizeof(uint64_t));
  if (!IsLittleEndianArchitecture()) {
    bits = ReverseBytes64(bits);
  }
  if constexpr (std::is_same_v<Fixed64, double>) {
    static_assert(sizeof(double) == sizeof(uint64_t));
    static_assert(std::numeric_limits<double>::is_iec559);
    std::memcpy(&result, &bits, sizeof(uint64_t));
  } else {
    result = static_cast<decltype(result.value())>(bits);
  }
}

// Copies the bytes from |buffer| to |result| with byte-order conversion for
// fixed-size 32-bit values.
template <typename Fixed32,
          std::enable_if_t<std::is_same_v<Fixed32, float> ||
                               std::is_same_v<Fixed32, ::pb::fixed32_t> ||
                               std::is_same_v<Fixed32, ::pb::sfixed32_t>,
                           int> = 0>
void ParseFixedValueNoBoundsCheck(const uint8_t* buffer, Fixed32& result) {
  // Implementation note: On little-endian architectures, the compiler will
  // optimize all of the following to a simple 4-byte copy, often a single
  // instruction.
  uint32_t bits;
  std::memcpy(&bits, buffer, sizeof(uint32_t));
  if (!IsLittleEndianArchitecture()) {
    bits = ReverseBytes32(bits);
  }
  if constexpr (std::is_same_v<Fixed32, float>) {
    static_assert(sizeof(float) == sizeof(uint32_t));
    static_assert(std::numeric_limits<float>::is_iec559);
    std::memcpy(&result, &bits, sizeof(uint32_t));
  } else {
    result = static_cast<decltype(result.value())>(bits);
  }
}

// Doubles or floats, as little-endian bytes on the wire.
template <typename Float,
          std::enable_if_t<std::is_same_v<Float, double> ||
                               std::is_same_v<Float, float>,
                           int> = 0>
[[nodiscard]] const uint8_t* ParseValue(const uint8_t* buffer,
                                        const uint8_t* buffer_end,
                                        int /* nesting_level */,
                                        Float& result) {
  constexpr std::ptrdiff_t kFixedSize = sizeof(Float);
  if ((buffer_end - buffer) < kFixedSize) {
    return nullptr;
  }
  ParseFixedValueNoBoundsCheck(buffer, result);
  return buffer + kFixedSize;
}

// Fixed-size 64- or 32-bit integers, as little-endian bytes on the wire.
template <typename Fixed,
          std::enable_if_t<std::is_same_v<Fixed, ::pb::fixed64_t> ||
                               std::is_same_v<Fixed, ::pb::sfixed64_t> ||
                               std::is_same_v<Fixed, ::pb::fixed32_t> ||
                               std::is_same_v<Fixed, ::pb::sfixed32_t>,
                           int> = 0>
[[nodiscard]] const uint8_t* ParseValue(const uint8_t* buffer,
                                        const uint8_t* buffer_end,
                                        int /* nesting_level */,
                                        Fixed& result) {
  constexpr std::ptrdiff_t kFixedSize = sizeof(result.value());
  if ((buffer_end - buffer) < kFixedSize) {
    return nullptr;
  }
  ParseFixedValueNoBoundsCheck(buffer, result);
  return buffer + kFixedSize;
}

// Returns true if |buffer| is non-null, |byte_count| bytes can be read from it,
// and |byte_count| does not violate the maximum payload size of a
// length-delimited field.
[[nodiscard]] inline bool IsParsedByteCountValid(const uint8_t* buffer,
                                                 const uint8_t* buffer_end,
                                                 uint32_t byte_count) {
  // Determine (at compile time) the largest possible size of the preceeding
  // varint that encoded the |byte_count|.
  constexpr int kBitsPerVarintByte = 7;
  constexpr auto kMaxBitsToRepresentSize =
      // Note: In C++20, this could be just:
      //   32 - std::countl_zero(kMaxSerializedSize)
      []() {
        static_assert(sizeof(kMaxSerializedSize) == sizeof(uint32_t));
        uint32_t bits = kMaxSerializedSize;
        int count = 0;
        while (bits != 0) {
          bits >>= 1;
          ++count;
        }
        return count;
      }();
  constexpr auto kBiggestVarintSize =
      // CEIL(kMaxBitsToRepresentSize / kBitsPerVarintByte).
      (kMaxBitsToRepresentSize + (kBitsPerVarintByte - 1)) / kBitsPerVarintByte;

  // The maximum valid payload size is the maximum serialized size minus what
  // was already used by the |byte_count| varint.
  constexpr auto kMaxValidPayloadSize =
      static_cast<uint32_t>(kMaxSerializedSize - kBiggestVarintSize);

  return (buffer && (byte_count <= kMaxValidPayloadSize) &&
          ((buffer + byte_count) <= buffer_end));
}

// Strings: Encoded as a length varint followed by the bytes.
template <typename String,
          std::enable_if_t<std::is_same_v<String, std::string> ||
                               std::is_same_v<String, std::string_view>,
                           int> = 0>
[[nodiscard]] const uint8_t* ParseValue(const uint8_t* buffer,
                                        const uint8_t* buffer_end,
                                        int nesting_level,
                                        String& result) {
  uint32_t byte_count;
  buffer = ParseValue(buffer, buffer_end, nesting_level, byte_count);
  if (!IsParsedByteCountValid(buffer, buffer_end, byte_count)) {
    return nullptr;
  }

  static_assert(sizeof(typename String::value_type) == 1);
  buffer_end = buffer + byte_count;
  if constexpr (std::is_same_v<String, std::string>) {
    result.assign(buffer, buffer_end);
  } else if constexpr (std::is_same_v<String, std::string_view>) {
    result = String(reinterpret_cast<const char*>(buffer), byte_count);
  }

  return buffer_end;
}

// Forward declaration of ParseValue(<nested message>).
template <typename Message,
          std::enable_if_t<std::is_class_v<typename Message::ProtobufFields>,
                           int> = 0>
[[nodiscard]] const uint8_t* ParseValue(const uint8_t* buffer,
                                        const uint8_t* buffer_end,
                                        int nesting_level,
                                        Message& message);

// Pairs: Parse into the given |pair| as a MapFieldEntry message. This provides
// the support for parsing maps.
template <typename Pair,
          std::enable_if_t<CouldBeAMapFieldEntry<Pair>(), int> = 0>
[[nodiscard]] const uint8_t* ParseValue(const uint8_t* buffer,
                                        const uint8_t* buffer_end,
                                        int nesting_level,
                                        Pair& pair) {
  return ParseValue(buffer, buffer_end, nesting_level,
                    AsMutableMapFieldEntryFacade(pair));
}

// Adapter for optional fields.
template <typename T>
[[nodiscard]] const uint8_t* ParseValue(const uint8_t* buffer,
                                        const uint8_t* buffer_end,
                                        int nesting_level,
                                        std::optional<T>& result) {
  if (!result) {
    result.emplace();
  }
  return ParseValue(buffer, buffer_end, nesting_level, *result);
}

// Adapter for unique_ptr fields.
template <typename T>
[[nodiscard]] const uint8_t* ParseValue(const uint8_t* buffer,
                                        const uint8_t* buffer_end,
                                        int nesting_level,
                                        std::unique_ptr<T>& result) {
  if (!result) {
    result = std::make_unique<T>();
  }
  return ParseValue(buffer, buffer_end, nesting_level, *result);
}

// Adapter for parsing one element when using the "unpacked repeated" encoding.
// This supports all Containers that have:
//
//   iterator insert(const_iterator pos, value_type&&... args);
//
// This works with all of the STL SequenceContainers (e.g., vector, deque, list)
// as well as the AssociativeContainers (e.g., set, unordered_set).
template <typename Container,
          std::enable_if_t<!(std::is_same_v<Container, std::string> ||
                             std::is_same_v<Container, std::string_view>),
                           int> = 0>
[[nodiscard]] auto ParseValue(const uint8_t* buffer,
                              const uint8_t* buffer_end,
                              int nesting_level,
                              Container& result)
    -> decltype(result.insert(std::end(result), std::move(*std::begin(result))),
                static_cast<const uint8_t*>(nullptr)) {
  IterableValueType<Container> element{};
  buffer = ParseValue(buffer, buffer_end, nesting_level, element);
  // If the above returned nullptr (a parse error), the element being inserted
  // into |result| will be invalid. However, the nullptr will still propagate to
  // the caller of this function to indicate error.
  result.insert(std::end(result), std::move(element));
  return buffer;
}

// Called from ParsePackedRepeatedValues() to parse all the varints in the range
// |begin| to |end| and append them to |result|.
template <typename Container>
[[nodiscard]] const uint8_t* ParsePackedRepeatedVarintHelper(
    const uint8_t* begin,
    const uint8_t* end,
    Container& result) {
  IterableValueType<Container> element{};
  while (begin != end) {
    begin = ParseValue(begin, end, -1, element);
    if (!begin) {
      break;
    }
    result.insert(std::end(result), std::move(element));
  }
  return begin;
}

// Called from ParsePackedRepeatedValues() to parse all the fixed-sized scalars
// in the range |buffer| to |buffer + byte_count| and append them to |result|.
template <uint32_t kBytesPerElement, typename Container>
[[nodiscard]] const uint8_t* ParsePackedRepeatedFixedHelper(
    const uint8_t* buffer,
    uint32_t byte_count,
    Container& result) {
  if ((byte_count % kBytesPerElement) != 0) {
    return nullptr;
  }
  IterableValueType<Container> element{};
  uint32_t i;
  for (i = 0; i != byte_count; i += kBytesPerElement) {
    ParseFixedValueNoBoundsCheck(&buffer[i], element);
    result.insert(std::end(result), std::move(element));
  }
  return &buffer[i];
}

// Parses zero or more elements when using the "packed repeated" encoding. This
// supports the same Containers as ParseValue<Container>() above.
template <WireType kElementWireType, typename Container>
[[nodiscard]] auto ParsePackedRepeatedValues(const uint8_t* buffer,
                                             const uint8_t* buffer_end,
                                             Container& result)
    -> decltype(result.insert(std::end(result), std::move(*std::begin(result))),
                static_cast<const uint8_t*>(nullptr)) {
  uint32_t byte_count;
  buffer = ParseValue(buffer, buffer_end, -1, byte_count);
  if (!IsParsedByteCountValid(buffer, buffer_end, byte_count)) {
    return nullptr;
  }

  if constexpr (kElementWireType == WireType::kVarint) {
    buffer =
        ParsePackedRepeatedVarintHelper(buffer, buffer + byte_count, result);
  } else if constexpr (kElementWireType == WireType::kFixed64Bit) {
    buffer = ParsePackedRepeatedFixedHelper<sizeof(uint64_t)>(
        buffer, byte_count, result);
  } else if constexpr (kElementWireType == WireType::kFixed32Bit) {
    buffer = ParsePackedRepeatedFixedHelper<sizeof(uint32_t)>(
        buffer, byte_count, result);
  } else {
    static_assert(kElementWireType == WireType::kVarint ||
                  kElementWireType == WireType::kFixed64Bit ||
                  kElementWireType == WireType::kFixed32Bit);
  }

  return buffer;
}

// Skips-over the bytes comprising a value of the given |wire_type| in |buffer|,
// and returns a pointer to the position just after the value. This is called
// when a tag+value is encountered in the wire data for an unknown field (e.g.,
// when the originator is newer software with additional message fields
// defined).
[[nodiscard]] inline const uint8_t* SkipValueAfterTag(const uint8_t* buffer,
                                                      const uint8_t* buffer_end,
                                                      int nesting_level,
                                                      WireType wire_type) {
  switch (wire_type) {
    case WireType::kVarint: {
      uint64_t garbage;
      return ParseValue(buffer, buffer_end, nesting_level, garbage);
    }

    case WireType::kFixed64Bit:
      buffer += sizeof(uint64_t);
      return ((buffer_end - buffer) >= 0) ? buffer : nullptr;

    case WireType::kLengthDelimited: {
      uint32_t byte_count;
      buffer = ParseValue(buffer, buffer_end, nesting_level, byte_count);
      if (!IsParsedByteCountValid(buffer, buffer_end, byte_count)) {
        return nullptr;
      }
      return buffer + byte_count;
    }

    case WireType::kFixed32Bit:
      buffer += sizeof(uint32_t);
      return ((buffer_end - buffer) >= 0) ? buffer : nullptr;

    case WireType::kStartGroup:
    case WireType::kEndGroup:
    case WireType::kReserved1:
    case WireType::kReserved2:
    default:
      // Don't know how to skip-over wire types that are not documented.
      break;
  }

  return nullptr;  // Unimplemented WireType breaks the parse.
}

// Parses the value in |buffer|, where |buffer| points just after a tag,
// assuming the value is for |TheField|. This function also determines whether
// the value is one element of a repeated field, multiple elements of a repeated
// field, or [the usual] single field value.
template <typename Message, typename TheField>
[[nodiscard]] const uint8_t* ParseValueAfterTagForField(
    const uint8_t* buffer,
    const uint8_t* buffer_end,
    int nesting_level,
    WireType wire_type_from_tag,
    int32_t,
    Message& message) {
  if constexpr (IsRepeatedField<TheField>()) {
    constexpr auto kElementWireType =
        GetWireType<IterableValueType<typename TheField::Member>>();
    if (wire_type_from_tag == kElementWireType) {
      // Parse one element of an unpacked repeated field.
      return ParseValue(buffer, buffer_end, nesting_level,
                        TheField::GetMutableMemberReferenceIn(message));
    } else if constexpr (CanEncodeAsAPackedRepeatedField<TheField>()) {
      if (wire_type_from_tag == WireType::kLengthDelimited) {
        return ParsePackedRepeatedValues<kElementWireType>(
            buffer, buffer_end, TheField::GetMutableMemberReferenceIn(message));
      }
    }
  } else if (wire_type_from_tag == GetWireType<typename TheField::Member>()) {
    return ParseValue(buffer, buffer_end, nesting_level,
                      TheField::GetMutableMemberReferenceIn(message));
  }

  // If this point is reached, the WireType was wrong.
  return nullptr;
}

// Searches for a message field having the given |field_number| and then calls
// ParseValueAfterTagForField(). If the |Message| does not have such a field,
// SkipValueAfterTag() will be called instead to skip-over the unknown field's
// value.
//
// This template effects a recursive binary search algorithm to be
// code-generated by the compiler, ensuring O(lg N) look-up complexity for the
// desired field at runtime. This is mainly for the benefit of messages with
// lots of fields.
template <typename Message,
          std::size_t kBeginIndex = 0,
          std::size_t kEndIndex = Message::ProtobufFields::kFieldCount>
[[nodiscard]] const uint8_t* ParseValueAfterTag(const uint8_t* buffer,
                                                const uint8_t* buffer_end,
                                                int nesting_level,
                                                WireType wire_type,
                                                int32_t field_number,
                                                Message& message) {
  static_assert(
      Message::ProtobufFields::AreFieldNumbersMonotonicallyIncreasing());

  if constexpr (kBeginIndex < kEndIndex) {
    constexpr auto kPivotIndex = kBeginIndex + (kEndIndex - kBeginIndex) / 2;
    using PivotField =
        typename Message::ProtobufFields::template FieldAt<kPivotIndex>;

    if (field_number == PivotField::GetFieldNumber()) {
      return ParseValueAfterTagForField<Message, PivotField>(
          buffer, buffer_end, nesting_level, wire_type, field_number, message);
    } else if (field_number < PivotField::GetFieldNumber()) {
      return ParseValueAfterTag<Message, kBeginIndex, kPivotIndex>(
          buffer, buffer_end, nesting_level, wire_type, field_number, message);
    } else /* if (field_number > PivotField::GetFieldNumber()) */ {
      return ParseValueAfterTag<Message, kPivotIndex + 1, kEndIndex>(
          buffer, buffer_end, nesting_level, wire_type, field_number, message);
    }
  } else {
    return SkipValueAfterTag(buffer, buffer_end, nesting_level, wire_type);
  }
}

// Scans the |buffer| for encoded tag+value pairs, populating the data members
// in |message| when matches are made (according to Message::ProtobufFields).
template <typename Message>
[[nodiscard]] const uint8_t* ParseFields(const uint8_t* buffer,
                                         const uint8_t* buffer_end,
                                         int nesting_level,
                                         Message& message) {
  while (buffer != buffer_end) {
    Tag tag;
    buffer = ParseValue(buffer, buffer_end, nesting_level, tag);
    if (!buffer) {
      return nullptr;
    }
    buffer = ParseValueAfterTag(buffer, buffer_end, nesting_level,
                                GetWireTypeFromTag(tag),
                                GetFieldNumberFromTag(tag), message);
    if (!buffer) {
      return nullptr;
    }
  }
  return buffer;
}

// Nested Messages: Encoded as a length varint followed by the encoded tag+value
// pairs.
template <
    typename Message,
    std::enable_if_t<std::is_class_v<typename Message::ProtobufFields>, int>>
[[nodiscard]] const uint8_t* ParseValue(const uint8_t* buffer,
                                        const uint8_t* buffer_end,
                                        int nesting_level,
                                        Message& result) {
  uint32_t byte_count;
  buffer = ParseValue(buffer, buffer_end, nesting_level, byte_count);
  if (!IsParsedByteCountValid(buffer, buffer_end, byte_count)) {
    return nullptr;
  }

  if (nesting_level >= kMaxMessageNestingDepth) {
    return nullptr;
  }
  ++nesting_level;

  return ParseFields(buffer, buffer + byte_count, nesting_level, result);
}

}  // namespace pb::codec
