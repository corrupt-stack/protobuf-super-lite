// Copyright 2022 Yuri Wiitala. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>
#include <utility>

#include "pb/codec/map_field_entry.h"
#include "pb/codec/map_field_entry_facade.h"
#include "pb/integer_wrapper.h"

namespace pb::codec {
namespace {

static_assert(CouldBeAMapFieldEntry<std::pair<int, int>>());
static_assert(CouldBeAMapFieldEntry<std::pair<sint32_t, int>>());
static_assert(CouldBeAMapFieldEntry<std::pair<fixed32_t, int>>());
static_assert(CouldBeAMapFieldEntry<std::pair<std::string, int>>());

static_assert(CouldBeAMapFieldEntry<std::pair<int, std::string>>());
static_assert(CouldBeAMapFieldEntry<std::pair<sint32_t, std::string>>());
static_assert(CouldBeAMapFieldEntry<std::pair<fixed32_t, std::string>>());
static_assert(CouldBeAMapFieldEntry<std::pair<std::string, std::string>>());

static_assert(CouldBeAMapFieldEntry<std::pair<const int, int>>());
static_assert(CouldBeAMapFieldEntry<std::pair<const sint64_t, int>>());
static_assert(CouldBeAMapFieldEntry<std::pair<const fixed64_t, int>>());
static_assert(CouldBeAMapFieldEntry<std::pair<const std::string, int>>());

struct Message {
  int an_int;
  float a_float;

  using ProtobufFields =
      FieldList<Field<&Message::an_int, 1>, Field<&Message::a_float, 2>>;
};
static_assert(CouldBeAMapFieldEntry<std::pair<const int, Message>>());

static_assert(!CouldBeAMapFieldEntry<std::pair<float, int>>());
static_assert(!CouldBeAMapFieldEntry<std::pair<double, int>>());

enum Foo { A = 1, B = 2 };
static_assert(!CouldBeAMapFieldEntry<std::pair<Foo, int>>());

static_assert(!CouldBeAMapFieldEntry<int>());
static_assert(!CouldBeAMapFieldEntry<std::string>());
static_assert(!CouldBeAMapFieldEntry<Message>());
static_assert(!CouldBeAMapFieldEntry<std::map<int, int>>());

// The MapFieldEntryFacade is a subclass of a "pair-like thing," but it should
// be considered a "message MapFieldEntry" when the type system deduces which
// serialization/parsing template functions to use.
static_assert(
    !CouldBeAMapFieldEntry<MapFieldEntryFacade<std::pair<int, int>>>());

}  // namespace
}  // namespace pb::codec
