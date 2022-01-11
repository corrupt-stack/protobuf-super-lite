// Copyright 2022 Yuri Wiitala. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <cstdint>

#include "pb/codec/limits.h"

namespace pb {

template <auto member_pointer, int32_t field_number>
struct Field {
  template <typename T, class C>
  static C a_hypothetical_function(T C::*);
  using Clazz = decltype(a_hypothetical_function(member_pointer));

  template <typename T, class C>
  static T another_hypothetical_function(T C::*);
  using Member = decltype(another_hypothetical_function(member_pointer));

  [[nodiscard]] static constexpr const Member& GetMemberReferenceIn(
      const Clazz& instance) {
    return instance.*member_pointer;
  }

  [[nodiscard]] static constexpr Member& GetMutableMemberReferenceIn(
      Clazz& instance) {
    return instance.*member_pointer;
  }

  [[nodiscard]] static constexpr int32_t GetFieldNumber() {
    static_assert(codec::IsValidFieldNumber(field_number));
    return field_number;
  }
};

// Base-case template: Matches only for empty FieldList's.
template <typename... Fields>
struct FieldList {
  using FirstField = void;
  using RemainingFields = void;

  template <std::size_t index>
  using FieldAt = void;

  static constexpr std::size_t kFieldCount = 0;

  static constexpr bool AreFieldNumbersMonotonicallyIncreasing() {
    return true;
  }
};

// FieldList template for non-empty FieldList's.
template <typename First, typename... Rest>
struct FieldList<First, Rest...> {
  using FirstField = First;
  using RemainingFields = FieldList<Rest...>;

  template <std::size_t index>
  using FieldAt =
      std::conditional_t<index == 0,
                         FirstField,
                         typename RemainingFields::template FieldAt<index - 1>>;

  static constexpr std::size_t kFieldCount = 1 + sizeof...(Rest);

  static constexpr bool AreFieldNumbersMonotonicallyIncreasing() {
    if constexpr (RemainingFields::kFieldCount == 0) {
      return true;
    } else if constexpr (FirstField::GetFieldNumber() >=
                         RemainingFields::FirstField::GetFieldNumber()) {
      return false;
    } else {
      return RemainingFields::AreFieldNumbersMonotonicallyIncreasing();
    }
  }
};

}  // namespace pb
