// Copyright 2022 Yuri Wiitala. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

#include "pb/codec/iterable_util.h"
#include "pb/field_list.h"
#include "pb/integer_wrapper.h"

namespace pb::codec {

// Documentation:
// https://developers.google.com/protocol-buffers/docs/encoding#optional

// Returns true if the |Field| is represented in C++ code with a data type that:
//
//   1. Would serialize as a repeated field.
//   2. Could accept multiple values for the same field during a parse.
template <typename Field>
[[nodiscard]] constexpr bool IsRepeatedField() {
  using T = typename Field::Member;
  return (IsIterable<T>() && !(std::is_same_v<T, std::string> ||
                               std::is_same_v<T, std::string_view>));
}

// Returns true if the |Field| is represented in C++ code with a data type that:
//
//   1. Would serialize as a packed repeated field, a more-efficient encoding on
//      the wire for repeats of scalar numeric types.
//   2. Could parse from a packed repeated field encoding (though an unpacked
//      encoding could be used, and this would be acceptable).
template <typename Field>
[[nodiscard]] constexpr bool CanEncodeAsAPackedRepeatedField() {
  if constexpr (IsRepeatedField<Field>()) {
    using ValueType = IterableValueType<typename Field::Member>;
    return (std::is_integral_v<ValueType> || std::is_enum_v<ValueType> ||
            std::is_same_v<ValueType, double> ||
            std::is_same_v<ValueType, float> ||
            std::is_same_v<ValueType, ::pb::sint32_t> ||
            std::is_same_v<ValueType, ::pb::sint64_t> ||
            std::is_same_v<ValueType, ::pb::fixed64_t> ||
            std::is_same_v<ValueType, ::pb::sfixed64_t> ||
            std::is_same_v<ValueType, ::pb::fixed32_t> ||
            std::is_same_v<ValueType, ::pb::sfixed32_t>);
  }
  return false;
}

template <typename T>
[[nodiscard]] constexpr bool IsStoringOneValue(const std::optional<T>& opt) {
  return opt.has_value();
}

template <typename T>
[[nodiscard]] constexpr bool IsStoringOneValue(const std::unique_ptr<T>& ptr) {
  return !!ptr;
}

template <typename T>
[[nodiscard]] constexpr bool IsStoringOneValue(const T&) {
  return true;
}

template <>
[[nodiscard]] constexpr bool IsStoringOneValue<std::string_view>(
    const std::string_view& sv) {
  return sv.data() != nullptr;
}

template <typename T>
[[nodiscard]] constexpr const T& GetTheOneValue(const std::optional<T>& opt) {
  return *opt;
}

template <typename T>
[[nodiscard]] constexpr const T& GetTheOneValue(const std::unique_ptr<T>& ptr) {
  return *ptr;
}

template <typename T>
[[nodiscard]] constexpr const T& GetTheOneValue(const T& obj) {
  return obj;
}

}  // namespace pb::codec
