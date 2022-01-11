// Copyright 2022 Yuri Wiitala. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pb/codec/iterable_util.h"

#include <cstdint>
#include <deque>
#include <list>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <type_traits>
#include <vector>

#include "pb/integer_wrapper.h"

namespace {

using pb::codec::IsIterable;

static_assert(!IsIterable<int>());
static_assert(!IsIterable<bool>());
static_assert(!IsIterable<pb::sint32_t>());
static_assert(!IsIterable<pb::fixed32_t>());

class SomeClass {
 public:
  int member = 42;
};
static_assert(!IsIterable<SomeClass>());

static_assert(IsIterable<std::deque<int>>());
static_assert(IsIterable<std::list<int>>());
static_assert(IsIterable<std::set<int>>());
static_assert(IsIterable<std::vector<int>>());

static_assert(IsIterable<std::deque<bool>>());
static_assert(IsIterable<std::list<bool>>());
static_assert(IsIterable<std::set<bool>>());
static_assert(IsIterable<std::vector<bool>>());

static_assert(IsIterable<std::deque<pb::sint32_t>>());
static_assert(IsIterable<std::list<pb::sint32_t>>());
static_assert(IsIterable<std::set<pb::sint32_t>>());
static_assert(IsIterable<std::vector<pb::sint32_t>>());

static_assert(IsIterable<std::deque<pb::fixed32_t>>());
static_assert(IsIterable<std::list<pb::fixed32_t>>());
static_assert(IsIterable<std::set<pb::fixed32_t>>());
static_assert(IsIterable<std::vector<pb::fixed32_t>>());

static_assert(IsIterable<std::deque<SomeClass>>());
static_assert(IsIterable<std::list<SomeClass>>());
static_assert(IsIterable<std::set<SomeClass>>());
static_assert(IsIterable<std::vector<SomeClass>>());

static_assert(IsIterable<std::deque<std::string>>());
static_assert(IsIterable<std::list<std::string>>());
static_assert(IsIterable<std::set<std::string>>());
static_assert(IsIterable<std::vector<std::string>>());

using pb::codec::IterableValueType;

static_assert(std::is_same_v<int, IterableValueType<std::deque<int>>>);
static_assert(std::is_same_v<int, IterableValueType<std::list<int>>>);
static_assert(std::is_same_v<int, IterableValueType<std::set<int>>>);
static_assert(std::is_same_v<int, IterableValueType<std::vector<int>>>);

static_assert(std::is_same_v<bool, IterableValueType<std::deque<bool>>>);
static_assert(std::is_same_v<bool, IterableValueType<std::list<bool>>>);
static_assert(std::is_same_v<bool, IterableValueType<std::set<bool>>>);
static_assert(std::is_same_v<bool, IterableValueType<std::vector<bool>>>);

static_assert(
    std::is_same_v<pb::sint32_t, IterableValueType<std::deque<pb::sint32_t>>>);
static_assert(
    std::is_same_v<pb::sint32_t, IterableValueType<std::list<pb::sint32_t>>>);
static_assert(
    std::is_same_v<pb::sint32_t, IterableValueType<std::set<pb::sint32_t>>>);
static_assert(
    std::is_same_v<pb::sint32_t, IterableValueType<std::vector<pb::sint32_t>>>);

static_assert(std::is_same_v<pb::fixed32_t,
                             IterableValueType<std::deque<pb::fixed32_t>>>);
static_assert(
    std::is_same_v<pb::fixed32_t, IterableValueType<std::list<pb::fixed32_t>>>);
static_assert(
    std::is_same_v<pb::fixed32_t, IterableValueType<std::set<pb::fixed32_t>>>);
static_assert(std::is_same_v<pb::fixed32_t,
                             IterableValueType<std::vector<pb::fixed32_t>>>);

static_assert(
    std::is_same_v<SomeClass, IterableValueType<std::deque<SomeClass>>>);
static_assert(
    std::is_same_v<SomeClass, IterableValueType<std::list<SomeClass>>>);
static_assert(
    std::is_same_v<SomeClass, IterableValueType<std::set<SomeClass>>>);
static_assert(
    std::is_same_v<SomeClass, IterableValueType<std::vector<SomeClass>>>);

static_assert(
    std::is_same_v<std::string, IterableValueType<std::deque<std::string>>>);
static_assert(
    std::is_same_v<std::string, IterableValueType<std::list<std::string>>>);
static_assert(
    std::is_same_v<std::string, IterableValueType<std::set<std::string>>>);
static_assert(
    std::is_same_v<std::string, IterableValueType<std::vector<std::string>>>);

}  // namespace
