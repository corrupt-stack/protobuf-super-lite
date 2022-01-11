// Copyright 2022 Yuri Wiitala. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pb/codec/zigzag.h"

#include <cstdint>
#include <limits>

namespace {

using pb::codec::EncodeZigZag;

static_assert(EncodeZigZag(int32_t{0}) == uint32_t{0});
static_assert(EncodeZigZag(int32_t{-1}) == uint32_t{1});
static_assert(EncodeZigZag(int32_t{1}) == uint32_t{2});
static_assert(EncodeZigZag(int32_t{-2}) == uint32_t{3});
static_assert(EncodeZigZag(int32_t{2}) == uint32_t{4});
static_assert(EncodeZigZag(int32_t{std::numeric_limits<int32_t>::max()}) ==
              (std::numeric_limits<uint32_t>::max() - 1));
static_assert(EncodeZigZag(int32_t{std::numeric_limits<int32_t>::min()}) ==
              std::numeric_limits<uint32_t>::max());

static_assert(EncodeZigZag(int64_t{0}) == uint64_t{0});
static_assert(EncodeZigZag(int64_t{-1}) == uint64_t{1});
static_assert(EncodeZigZag(int64_t{1}) == uint64_t{2});
static_assert(EncodeZigZag(int64_t{-2}) == uint64_t{3});
static_assert(EncodeZigZag(int64_t{2}) == uint64_t{4});
static_assert(EncodeZigZag(int64_t{std::numeric_limits<int64_t>::max()}) ==
              (std::numeric_limits<uint64_t>::max() - 1));
static_assert(EncodeZigZag(int64_t{std::numeric_limits<int64_t>::min()}) ==
              std::numeric_limits<uint64_t>::max());

using pb::codec::DecodeZigZag;

static_assert(DecodeZigZag(uint32_t{0}) == int32_t{0});
static_assert(DecodeZigZag(uint32_t{1}) == int32_t{-1});
static_assert(DecodeZigZag(uint32_t{2}) == int32_t{1});
static_assert(DecodeZigZag(uint32_t{3}) == int32_t{-2});
static_assert(DecodeZigZag(uint32_t{4}) == int32_t{2});
static_assert(DecodeZigZag(std::numeric_limits<uint32_t>::max() - 1) ==
              int32_t{std::numeric_limits<int32_t>::max()});
static_assert(DecodeZigZag(std::numeric_limits<uint32_t>::max()) ==
              int32_t{std::numeric_limits<int32_t>::min()});

static_assert(DecodeZigZag(uint64_t{0}) == int64_t{0});
static_assert(DecodeZigZag(uint64_t{1}) == int64_t{-1});
static_assert(DecodeZigZag(uint64_t{2}) == int64_t{1});
static_assert(DecodeZigZag(uint64_t{3}) == int64_t{-2});
static_assert(DecodeZigZag(uint64_t{4}) == int64_t{2});
static_assert(DecodeZigZag(std::numeric_limits<uint64_t>::max() - 1) ==
              int64_t{std::numeric_limits<int64_t>::max()});
static_assert(DecodeZigZag(std::numeric_limits<uint64_t>::max()) ==
              int64_t{std::numeric_limits<int64_t>::min()});

}  // namespace
