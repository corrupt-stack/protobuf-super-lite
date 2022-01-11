// Copyright 2022 Yuri Wiitala. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pb/codec/tag.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "pb/codec/wire_type.h"
#include "pb/field_list.h"

namespace {

struct Message {
  int varint;
  double fixed64;
  float fixed32;
  std::string string;

  struct NestedMessage {
    int hi;
    using ProtobufFields = pb::FieldList<pb::Field<&NestedMessage::hi, 1>>;
  };
  NestedMessage message;

  // Packed Repeated.
  std::vector<int> varints;

  // Unpacked Repeated.
  std::vector<NestedMessage> messages;
  std::vector<std::unique_ptr<NestedMessage>> message_ptrs;

  // Optional.
  std::optional<NestedMessage> optional_message;
};

using pb::codec::GetTagForSerialization;

static_assert(GetTagForSerialization<pb::Field<&Message::varint, 1>>() == 0x08);
static_assert(GetTagForSerialization<pb::Field<&Message::fixed64, 2>>() ==
              0x11);
static_assert(GetTagForSerialization<pb::Field<&Message::fixed32, 3>>() ==
              0x1D);
static_assert(GetTagForSerialization<pb::Field<&Message::string, 4>>() == 0x22);
static_assert(GetTagForSerialization<pb::Field<&Message::message, 5>>() ==
              0x2A);
static_assert(GetTagForSerialization<pb::Field<&Message::varints, 6>>() ==
              0x32);
static_assert(GetTagForSerialization<pb::Field<&Message::messages, 7>>() ==
              0x3A);
static_assert(
    GetTagForSerialization<pb::Field<&Message::message_ptrs, 1337>>() ==
    0x29CA);
static_assert(GetTagForSerialization<
                  pb::Field<&Message::optional_message, (1 << 29) - 1>>() ==
              0xFFFFFFFA);

using pb::codec::GetWireTypeFromTag;
using pb::codec::WireType;

static_assert(GetWireTypeFromTag(0x08) == WireType::kVarint);
static_assert(GetWireTypeFromTag(0x11) == WireType::kFixed64Bit);
static_assert(GetWireTypeFromTag(0x1D) == WireType::kFixed32Bit);
static_assert(GetWireTypeFromTag(0x22) == WireType::kLengthDelimited);
static_assert(GetWireTypeFromTag(0x2A) == WireType::kLengthDelimited);
static_assert(GetWireTypeFromTag(0x32) == WireType::kLengthDelimited);
static_assert(GetWireTypeFromTag(0x3A) == WireType::kLengthDelimited);
static_assert(GetWireTypeFromTag(0x29CA) == WireType::kLengthDelimited);
static_assert(GetWireTypeFromTag(0xFFFFFFFA) == WireType::kLengthDelimited);

using pb::codec::GetFieldNumberFromTag;

static_assert(GetFieldNumberFromTag(0x08) == 1);
static_assert(GetFieldNumberFromTag(0x11) == 2);
static_assert(GetFieldNumberFromTag(0x1D) == 3);
static_assert(GetFieldNumberFromTag(0x22) == 4);
static_assert(GetFieldNumberFromTag(0x2A) == 5);
static_assert(GetFieldNumberFromTag(0x32) == 6);
static_assert(GetFieldNumberFromTag(0x3A) == 7);
static_assert(GetFieldNumberFromTag(0x29CA) == 1337);
static_assert(GetFieldNumberFromTag(0xFFFFFFFA) == ((1 << 29) - 1));

}  // namespace
