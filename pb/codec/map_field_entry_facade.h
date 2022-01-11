// Copyright 2022 Yuri Wiitala. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "pb/field_list.h"

namespace pb::codec {

// An adapter that allows a Pair (e.g., std::pair), to become a MapFieldEntry
// message:
//
//   message MapFieldEntry {
//     key_type key = 1;
//     value_type value = 2;
//   }
//
// This provides support for serializing and parsing maps (e.g., std::map or
// std::unordered_map).
template <typename Pair>
struct MapFieldEntryFacade : public Pair {
  using ProtobufFields = ::pb::FieldList<::pb::Field<&Pair::first, 1>,
                                         ::pb::Field<&Pair::second, 2>>;

  // Prevent template deduction problems at compile time.
  using first_type = void;
  using second_type = void;
};

template <typename Pair>
[[nodiscard]] constexpr const MapFieldEntryFacade<Pair>& AsMapFieldEntryFacade(
    const Pair& pair) {
  return static_cast<const MapFieldEntryFacade<Pair>&>(pair);
}

template <typename Pair>
[[nodiscard]] constexpr MapFieldEntryFacade<Pair>& AsMutableMapFieldEntryFacade(
    Pair& pair) {
  return static_cast<MapFieldEntryFacade<Pair>&>(pair);
}

}  // namespace pb::codec
