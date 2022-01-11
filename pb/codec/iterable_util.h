// Copyright 2022 Yuri Wiitala. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "pb/codec/iterable_util-internal.h"

namespace pb::codec {

// Returns true if |T| is a type that is supported by std::begin() and
// std::end().
template <typename T>
[[nodiscard]] constexpr bool IsIterable() {
  return internal::IterableDetector<T>::kIsIterable;
}

// Declares the type of the values contained by an |Iterable|.
template <typename Iterable>
using IterableValueType =
    typename internal::IterableValueTypeDetector<Iterable>::Type;

}  // namespace pb::codec
