// Copyright 2022 Yuri Wiitala. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "pb/codec/map_field_entry-internal.h"

namespace pb::codec {

// Returns true if |T| is a key+value pair type, and could be treated as
// equivalent to a "MapFieldEntry" message for the purposes of parsing and
// serialization. See map_field_entry_facade.h.
template <typename T>
[[nodiscard]] constexpr bool CouldBeAMapFieldEntry() {
  return internal::MapPairDetector<T>::kCouldBeAMapFieldEntry;
}

}  // namespace pb::codec
