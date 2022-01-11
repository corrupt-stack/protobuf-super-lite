// Copyright 2022 Yuri Wiitala. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <cstdint>

#include "pb/integer_wrapper-internal.h"

namespace pb {

// Represents the .proto sint32/64 types.
//
// These are drop-in replacements for int32_t and int64_t data members where
// protobuf serialization and parsing should use "ZigZag" encoding.
//
// See Google's public language guide for when to use these:
// https://developers.google.com/protocol-buffers/docs/proto3#scalar
using sint32_t = internal::IntegerWrapper<int32_t, false>;
using sint64_t = internal::IntegerWrapper<int64_t, false>;

// Represents the .proto fixed32/64 and sfixed32/64 types.
//
// These are drop-in replacements for uint32_t, uint64_t, int32_t, or int64_t
// data members where protobuf serialization/parsing should just write/read the
// raw 4 or 8 bytes to/from the stream in little-endian byte order. In other
// words, this disables the varint encoding.
//
// See Google's public language guide for when to use these:
// https://developers.google.com/protocol-buffers/docs/proto3#scalar
using fixed32_t = internal::IntegerWrapper<uint32_t, true>;
using fixed64_t = internal::IntegerWrapper<uint64_t, true>;
using sfixed32_t = internal::IntegerWrapper<int32_t, true>;
using sfixed64_t = internal::IntegerWrapper<int64_t, true>;

}  // namespace pb
