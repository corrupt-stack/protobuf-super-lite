// Copyright 2022 Yuri Wiitala. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

#include "pb/codec/map_field_entry.h"
#include "pb/integer_wrapper.h"

namespace pb::codec {

enum class WireType : uint8_t {
  kVarint = 0,
  kFixed64Bit = 1,
  kLengthDelimited = 2,
  kStartGroup = 3,  // Deprecated according to documentation.
  kEndGroup = 4,    // Deprecated according to documentation.
  kFixed32Bit = 5,
  kReserved1 = 6,  // Reserved for future use.
  kReserved2 = 7,  // Reserved for future use.
};

template <
    typename Integral,
    std::enable_if_t<std::is_integral_v<Integral> || std::is_enum_v<Integral> ||
                         std::is_same_v<Integral, ::pb::sint32_t> ||
                         std::is_same_v<Integral, ::pb::sint64_t>,
                     int> = 0>
[[nodiscard]] constexpr WireType GetWireType() {
  return WireType::kVarint;
}

template <typename Fixed64,
          std::enable_if_t<std::is_same_v<Fixed64, double> ||
                               std::is_same_v<Fixed64, ::pb::fixed64_t> ||
                               std::is_same_v<Fixed64, ::pb::sfixed64_t>,
                           int> = 0>
[[nodiscard]] constexpr WireType GetWireType() {
  return WireType::kFixed64Bit;
}

template <typename Fixed32,
          std::enable_if_t<std::is_same_v<Fixed32, float> ||
                               std::is_same_v<Fixed32, ::pb::fixed32_t> ||
                               std::is_same_v<Fixed32, ::pb::sfixed32_t>,
                           int> = 0>
[[nodiscard]] constexpr WireType GetWireType() {
  return WireType::kFixed32Bit;
}

template <typename String,
          std::enable_if_t<std::is_same_v<String, std::string> ||
                               std::is_same_v<String, std::string_view>,
                           int> = 0>
[[nodiscard]] constexpr WireType GetWireType() {
  return WireType::kLengthDelimited;
}

template <typename Message,
          std::enable_if_t<std::is_class_v<typename Message::ProtobufFields>,
                           int> = 0>
[[nodiscard]] constexpr WireType GetWireType() {
  return WireType::kLengthDelimited;
}

template <
    typename T,
    std::enable_if_t<std::is_same_v<T, std::optional<typename T::value_type>>,
                     int> = 0>
[[nodiscard]] constexpr WireType GetWireType() {
  return GetWireType<typename T::value_type>();
}

template <typename T,
          std::enable_if_t<
              std::is_same_v<T, std::unique_ptr<typename T::element_type>>,
              int> = 0>
[[nodiscard]] constexpr WireType GetWireType() {
  return GetWireType<typename T::element_type>();
}

template <typename Pair,
          std::enable_if_t<CouldBeAMapFieldEntry<Pair>(), int> = 0>
[[nodiscard]] constexpr WireType GetWireType() {
  return WireType::kLengthDelimited;
}

}  // namespace pb::codec
