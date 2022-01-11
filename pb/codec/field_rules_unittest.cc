// Copyright 2022 Yuri Wiitala. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pb/codec/field_rules.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include "gtest/gtest.h"
#include "pb/field_list.h"
#include "pb/integer_wrapper.h"

namespace {

template <typename T>
struct Message {
  T member;
};

template <typename T>
using Field = pb::Field<&Message<T>::member, 1>;

using pb::codec::CanEncodeAsAPackedRepeatedField;
using pb::codec::IsRepeatedField;

static_assert(!IsRepeatedField<Field<int>>());
static_assert(!IsRepeatedField<Field<bool>>());
static_assert(!IsRepeatedField<Field<pb::sint32_t>>());
static_assert(!IsRepeatedField<Field<pb::fixed32_t>>());

enum Foo {
  A = 1,
  B = 2,
};
static_assert(!IsRepeatedField<Field<Foo>>());

static_assert(!IsRepeatedField<Field<std::string>>());

class SomeMessage {
 public:
  int an_int = 42;
  std::optional<std::string> optional_string;
  std::unique_ptr<std::string> pointer_to_big_thing;
  using ProtobufFields =
      pb::FieldList<pb::Field<&SomeMessage::an_int, 1>,
                    pb::Field<&SomeMessage::optional_string, 2>,
                    pb::Field<&SomeMessage::pointer_to_big_thing, 3>>;
};
static_assert(!IsRepeatedField<Field<SomeMessage>>());

static_assert(!IsRepeatedField<Field<std::optional<int>>>());
static_assert(!IsRepeatedField<Field<std::unique_ptr<int>>>());

static_assert(IsRepeatedField<Field<std::vector<int>>>());
static_assert(IsRepeatedField<Field<std::vector<bool>>>());
static_assert(IsRepeatedField<Field<std::vector<pb::sint32_t>>>());
static_assert(IsRepeatedField<Field<std::vector<pb::fixed32_t>>>());
static_assert(IsRepeatedField<Field<std::vector<Foo>>>());
static_assert(IsRepeatedField<Field<std::vector<std::string>>>());
static_assert(IsRepeatedField<Field<std::vector<std::string_view>>>());
static_assert(IsRepeatedField<Field<std::vector<SomeMessage>>>());
static_assert(
    IsRepeatedField<Field<std::vector<std::unique_ptr<SomeMessage>>>>());

static_assert(CanEncodeAsAPackedRepeatedField<Field<std::vector<int>>>());
static_assert(CanEncodeAsAPackedRepeatedField<Field<std::vector<bool>>>());
static_assert(CanEncodeAsAPackedRepeatedField<Field<std::vector<double>>>());
static_assert(CanEncodeAsAPackedRepeatedField<Field<std::vector<float>>>());
static_assert(
    CanEncodeAsAPackedRepeatedField<Field<std::vector<pb::sint32_t>>>());
static_assert(
    CanEncodeAsAPackedRepeatedField<Field<std::vector<pb::sint64_t>>>());
static_assert(
    CanEncodeAsAPackedRepeatedField<Field<std::vector<pb::fixed64_t>>>());
static_assert(
    CanEncodeAsAPackedRepeatedField<Field<std::vector<pb::sfixed64_t>>>());
static_assert(
    CanEncodeAsAPackedRepeatedField<Field<std::vector<pb::fixed32_t>>>());
static_assert(
    CanEncodeAsAPackedRepeatedField<Field<std::vector<pb::sfixed32_t>>>());
static_assert(CanEncodeAsAPackedRepeatedField<Field<std::vector<Foo>>>());

static_assert(
    !CanEncodeAsAPackedRepeatedField<Field<std::vector<std::string>>>());
static_assert(
    !CanEncodeAsAPackedRepeatedField<Field<std::vector<std::string_view>>>());
static_assert(
    !CanEncodeAsAPackedRepeatedField<Field<std::vector<SomeMessage>>>());
static_assert(!CanEncodeAsAPackedRepeatedField<
              Field<std::vector<std::unique_ptr<SomeMessage>>>>());

using pb::codec::GetTheOneValue;
using pb::codec::IsStoringOneValue;

TEST(FieldRules, GetZeroOrOneValue_std_optional) {
  SomeMessage message;
  ASSERT_FALSE(IsStoringOneValue(message.optional_string));

  const auto kHelloString = std::string("Hello!");
  message.optional_string = kHelloString;
  ASSERT_TRUE(IsStoringOneValue(message.optional_string));
  EXPECT_EQ(kHelloString, GetTheOneValue(message.optional_string));
  EXPECT_NE(&kHelloString, &GetTheOneValue(message.optional_string));
}

TEST(FieldRules, GetZeroOrOneValue_std_unique_ptr) {
  SomeMessage message;
  ASSERT_FALSE(IsStoringOneValue(message.pointer_to_big_thing));

  const auto kBigThing = std::string("I am a big thing.");
  message.pointer_to_big_thing = std::make_unique<std::string>(kBigThing);
  ASSERT_TRUE(IsStoringOneValue(message.pointer_to_big_thing));
  EXPECT_EQ(kBigThing, GetTheOneValue(message.pointer_to_big_thing));
  EXPECT_NE(&kBigThing, &GetTheOneValue(message.pointer_to_big_thing));
}

TEST(FieldRules, GetZeroOrOneValue_RequiredField) {
  SomeMessage message;
  ASSERT_TRUE(IsStoringOneValue(message.an_int));
  EXPECT_EQ(42, GetTheOneValue(message.an_int));
}

}  // namespace
