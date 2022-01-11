// Copyright 2022 Yuri Wiitala. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pb/codec/wire_type.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "pb/field_list.h"
#include "pb/integer_wrapper.h"

namespace {

using pb::codec::GetWireType;
using pb::codec::WireType;

static_assert(GetWireType<unsigned>() == WireType::kVarint);
static_assert(GetWireType<uint8_t>() == WireType::kVarint);
static_assert(GetWireType<uint16_t>() == WireType::kVarint);
static_assert(GetWireType<uint32_t>() == WireType::kVarint);
static_assert(GetWireType<uint64_t>() == WireType::kVarint);

static_assert(GetWireType<int>() == WireType::kVarint);
static_assert(GetWireType<int8_t>() == WireType::kVarint);
static_assert(GetWireType<int16_t>() == WireType::kVarint);
static_assert(GetWireType<int32_t>() == WireType::kVarint);
static_assert(GetWireType<int64_t>() == WireType::kVarint);

static_assert(GetWireType<bool>() == WireType::kVarint);

enum Foo {
  A = 1,
  B = 2,
};
static_assert(GetWireType<Foo>() == WireType::kVarint);

enum Bar : uint32_t {
  C = 3,
  D = 4,
};
static_assert(GetWireType<Bar>() == WireType::kVarint);

enum class Baz : int16_t {
  ONE = 1,
  TWO = 2,
  SOME = 127,
  LOTS = 128,
  DO_NOT_DO_THIS_WITH_PROTOBUFS = -1,  // Because <0 requires 10 wire bytes.
};
static_assert(GetWireType<Baz>() == WireType::kVarint);

static_assert(GetWireType<pb::sint32_t>() == WireType::kVarint);
static_assert(GetWireType<pb::sint64_t>() == WireType::kVarint);

static_assert(GetWireType<double>() == WireType::kFixed64Bit);
static_assert(GetWireType<pb::fixed64_t>() == WireType::kFixed64Bit);
static_assert(GetWireType<pb::sfixed64_t>() == WireType::kFixed64Bit);

static_assert(GetWireType<float>() == WireType::kFixed32Bit);
static_assert(GetWireType<pb::fixed32_t>() == WireType::kFixed32Bit);
static_assert(GetWireType<pb::sfixed32_t>() == WireType::kFixed32Bit);

static_assert(GetWireType<std::string>() == WireType::kLengthDelimited);
static_assert(GetWireType<std::string_view>() == WireType::kLengthDelimited);

class SomeMessage {
 public:
  int a_field;
  using ProtobufFields = pb::FieldList<pb::Field<&SomeMessage::a_field, 1>>;
};
static_assert(GetWireType<SomeMessage>() == WireType::kLengthDelimited);

static_assert(GetWireType<std::optional<int>>() == WireType::kVarint);
static_assert(GetWireType<std::optional<double>>() == WireType::kFixed64Bit);
static_assert(GetWireType<std::optional<pb::fixed32_t>>() ==
              WireType::kFixed32Bit);
static_assert(GetWireType<std::optional<std::string>>() ==
              WireType::kLengthDelimited);
static_assert(GetWireType<std::optional<SomeMessage>>() ==
              WireType::kLengthDelimited);

static_assert(GetWireType<std::unique_ptr<int>>() == WireType::kVarint);
static_assert(GetWireType<std::unique_ptr<double>>() == WireType::kFixed64Bit);
static_assert(GetWireType<std::unique_ptr<pb::fixed32_t>>() ==
              WireType::kFixed32Bit);
static_assert(GetWireType<std::unique_ptr<std::string>>() ==
              WireType::kLengthDelimited);
static_assert(GetWireType<std::unique_ptr<SomeMessage>>() ==
              WireType::kLengthDelimited);

}  // namespace
