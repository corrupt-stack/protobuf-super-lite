// Copyright 2022 Yuri Wiitala. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <string>
#include <type_traits>

#include "pb/integer_wrapper.h"

namespace pb::codec::internal {

template <typename T, typename Enable = void>
struct MapPairDetector {
  constexpr static bool kCouldBeAMapFieldEntry = false;
};

template <typename T>
struct MapPairDetector<
    T,
    typename std::enable_if_t<
        // "...any scalar except for floating point types..."
        // "Note that enum is not a valid key_type."
        // "The value_type can be any type..."
        std::is_integral_v<std::remove_cv_t<typename T::first_type>> ||
        std::is_same_v<std::remove_cv_t<typename T::first_type>,
                       ::pb::sint32_t> ||
        std::is_same_v<std::remove_cv_t<typename T::first_type>,
                       ::pb::sint64_t> ||
        std::is_same_v<std::remove_cv_t<typename T::first_type>,
                       ::pb::fixed32_t> ||
        std::is_same_v<std::remove_cv_t<typename T::first_type>,
                       ::pb::fixed64_t> ||
        std::is_same_v<std::remove_cv_t<typename T::first_type>,
                       ::pb::sfixed32_t> ||
        std::is_same_v<std::remove_cv_t<typename T::first_type>,
                       ::pb::sfixed64_t> ||
        std::is_same_v<std::remove_cv_t<typename T::first_type>,
                       std::string>>> {
  constexpr static bool kCouldBeAMapFieldEntry = true;
};

}  // namespace pb::codec::internal
