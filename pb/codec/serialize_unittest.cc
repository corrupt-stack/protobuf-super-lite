// Copyright 2022 Yuri Wiitala. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pb/codec/serialize.h"

#include <cstdint>
#include <cstring>
#include <iomanip>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "gtest/gtest.h"
#include "pb/codec/limits.h"
#include "pb/integer_wrapper.h"

namespace pb::codec {
namespace {

template <typename Byte>
std::string HexDump(const std::basic_string_view<Byte>& bytes) {
  std::ostringstream oss;
  oss << std::hex << std::setfill('0');
  for (const auto b : bytes) {
    oss << std::setw(2) << static_cast<uint32_t>(static_cast<uint8_t>(b));
  }
  return oss.str();
}

template <typename Value>
void TestSerialization(int line_num,
                       const Value& value,
                       std::string_view expected) {
  SCOPED_TRACE(::testing::Message() << "see line " << line_num);
  EXPECT_EQ(static_cast<int32_t>(expected.size()),
            ComputeSerializedValueSize(value));
  uint8_t buffer[256]{};
  ASSERT_LE(expected.size(), sizeof(buffer));
  auto* end = SerializeValue(value, buffer);
  ASSERT_EQ(buffer + expected.size(), end);
  const auto compare_result =
      std::memcmp(expected.data(), buffer, expected.size());
  if (compare_result != 0) {
    ADD_FAILURE() << "Expected: " << HexDump(expected) << "\n  Actual: "
                  << HexDump(std::basic_string_view<uint8_t>(buffer,
                                                             expected.size()));
  }
}

template <typename Integral>
void RunNativeIntegerTest(const char* type_name) {
  SCOPED_TRACE(::testing::Message() << "for " << type_name);

  // Positive values.
  TestSerialization(__LINE__, Integral{1}, std::string_view("\x01", 1));
  TestSerialization(__LINE__, Integral{2}, std::string_view("\x02", 1));
  TestSerialization(__LINE__, Integral{3}, std::string_view("\x03", 1));
  TestSerialization(__LINE__, Integral{std::numeric_limits<int8_t>::max()},
                    std::string_view("\x7f", 1));
  TestSerialization(__LINE__, Integral{1} << 7,
                    std::string_view("\x80\x01", 2));
  TestSerialization(__LINE__, (Integral{1} << 14) - 1,
                    std::string_view("\xff\x7f", 2));
  TestSerialization(__LINE__, Integral{1} << 14,
                    std::string_view("\x80\x80\x01", 3));
  TestSerialization(__LINE__, Integral{std::numeric_limits<int16_t>::max()},
                    std::string_view("\xff\xff\x01", 3));
  TestSerialization(__LINE__, (Integral{1} << 21) - 1,
                    std::string_view("\xff\xff\x7f", 3));
  TestSerialization(__LINE__, Integral{1} << 21,
                    std::string_view("\x80\x80\x80\x01", 4));
  TestSerialization(__LINE__, (Integral{1} << 28) - 1,
                    std::string_view("\xff\xff\xff\x7f", 4));
  TestSerialization(__LINE__, Integral{1} << 28,
                    std::string_view("\x80\x80\x80\x80\x01", 5));
  TestSerialization(__LINE__, Integral{std::numeric_limits<int32_t>::max()},
                    std::string_view("\xff\xff\xff\xff\x07", 5));
  if constexpr (sizeof(Integral) >= 8) {
    TestSerialization(__LINE__, (Integral{1} << 35) - 1,
                      std::string_view("\xff\xff\xff\xff\x7f", 5));
    TestSerialization(__LINE__, Integral{1} << 35,
                      std::string_view("\x80\x80\x80\x80\x80\x01", 6));
    TestSerialization(__LINE__, (Integral{1} << 42) - 1,
                      std::string_view("\xff\xff\xff\xff\xff\x7f", 6));
    TestSerialization(__LINE__, Integral{1} << 42,
                      std::string_view("\x80\x80\x80\x80\x80\x80\x01", 7));
    TestSerialization(
        __LINE__, Integral{std::numeric_limits<int64_t>::max()},
        std::string_view("\xff\xff\xff\xff\xff\xff\xff\xff\x7f", 9));
    if constexpr (std::is_unsigned_v<Integral>) {
      TestSerialization(
          __LINE__, Integral{std::numeric_limits<uint64_t>::max()},
          std::string_view("\xff\xff\xff\xff\xff\xff\xff\xff\xff\x01", 10));
    }
  }

  // Negative values.
  if constexpr (std::is_signed_v<Integral>) {
    TestSerialization(
        __LINE__, Integral{-1},
        std::string_view("\xff\xff\xff\xff\xff\xff\xff\xff\xff\x01", 10));
    if constexpr (sizeof(Integral) == 4) {
      TestSerialization(
          __LINE__, std::numeric_limits<Integral>::min(),
          std::string_view("\x80\x80\x80\x80\xf8\xff\xff\xff\xff\x01", 10));
    } else if constexpr (sizeof(Integral) == 8) {
      TestSerialization(
          __LINE__, std::numeric_limits<Integral>::min(),
          std::string_view("\x80\x80\x80\x80\x80\x80\x80\x80\x80\x01", 10));
    }
  }
}

TEST(SerializeTest, NativeIntegers) {
  RunNativeIntegerTest<int32_t>("int32_t");
  RunNativeIntegerTest<int64_t>("int64_t");
  RunNativeIntegerTest<uint32_t>("uint32_t");
  RunNativeIntegerTest<uint64_t>("uint64_t");
}

TEST(SerializeTest, Bools) {
  TestSerialization(__LINE__, false, std::string_view("\x00", 1));
  TestSerialization(__LINE__, true, std::string_view("\x01", 1));
}

TEST(SerializeTest, Enums) {
  enum Foo : int32_t {
    kOne = 1,
    kTwo = 2,
    kSome = 127,
    kLots = 128,
    kDoNotDoThisWithProtobufs = -1,  // Because <0 requires 10 wire bytes.
  };

  TestSerialization(__LINE__, kOne, std::string_view("\x01", 1));
  TestSerialization(__LINE__, kTwo, std::string_view("\x02", 1));
  TestSerialization(__LINE__, kSome, std::string_view("\x7F", 1));
  TestSerialization(__LINE__, kLots, std::string_view("\x80\x01", 2));
  TestSerialization(
      __LINE__, kDoNotDoThisWithProtobufs,
      std::string_view("\xff\xff\xff\xff\xff\xff\xff\xff\xff\x01", 10));
}

TEST(SerializeTest, EnumClasses) {
  enum class Bar : uint8_t {
    kA = 0x01,
    kB = 0x02,
    kC = 0x87,
    kD = 0xE0,
    kE = 0xFF,
  };
  TestSerialization(__LINE__, Bar::kA, std::string_view("\x01", 1));
  TestSerialization(__LINE__, Bar::kB, std::string_view("\x02", 1));
  TestSerialization(__LINE__, Bar::kC, std::string_view("\x87\x01", 2));
  TestSerialization(__LINE__, Bar::kD, std::string_view("\xE0\x01", 2));
  TestSerialization(__LINE__, Bar::kE, std::string_view("\xFF\x01", 2));
}

TEST(SerializeTest, ZigZagEncodedInts) {
  TestSerialization(__LINE__, pb::sint32_t{0}, std::string_view("\x00", 1));
  TestSerialization(__LINE__, pb::sint32_t{-1}, std::string_view("\x01", 1));
  TestSerialization(__LINE__, pb::sint32_t{1}, std::string_view("\x02", 1));
  TestSerialization(__LINE__, pb::sint32_t{std::numeric_limits<int32_t>::max()},
                    std::string_view("\xfe\xff\xff\xff\x0f", 5));
  TestSerialization(__LINE__, pb::sint32_t{std::numeric_limits<int32_t>::min()},
                    std::string_view("\xff\xff\xff\xff\x0f", 5));

  TestSerialization(__LINE__, pb::sint64_t{0}, std::string_view("\x00", 1));
  TestSerialization(__LINE__, pb::sint64_t{-1}, std::string_view("\x01", 1));
  TestSerialization(__LINE__, pb::sint64_t{1}, std::string_view("\x02", 1));
  TestSerialization(
      __LINE__, pb::sint64_t{std::numeric_limits<int64_t>::max()},
      std::string_view("\xfe\xff\xff\xff\xff\xff\xff\xff\xff\x01", 10));
  TestSerialization(
      __LINE__, pb::sint64_t{std::numeric_limits<int64_t>::min()},
      std::string_view("\xff\xff\xff\xff\xff\xff\xff\xff\xff\x01", 10));
}

TEST(SerializeTest, NativeFloats) {
  TestSerialization(__LINE__, 0.0f, std::string_view("\x00\x00\x00\x00", 4));
  TestSerialization(__LINE__, 1.0f, std::string_view("\x00\x00\x80\x3f", 4));
  TestSerialization(__LINE__, -1.0f, std::string_view("\x00\x00\x80\xbf", 4));
  TestSerialization(__LINE__, 42.0f, std::string_view("\x00\x00\x28\x42", 4));
  TestSerialization(__LINE__, std::numeric_limits<float>::infinity(),
                    std::string_view("\x00\x00\x80\x7f", 4));
  TestSerialization(__LINE__, -std::numeric_limits<float>::infinity(),
                    std::string_view("\x00\x00\x80\xff", 4));
}

TEST(SerializeTest, NativeDoubles) {
  TestSerialization(__LINE__, 0.0,
                    std::string_view("\x00\x00\x00\x00\x00\x00\x00\x00", 8));
  TestSerialization(__LINE__, 1.0,
                    std::string_view("\x00\x00\x00\x00\x00\x00\xf0\x3f", 8));
  TestSerialization(__LINE__, -1.0,
                    std::string_view("\x00\x00\x00\x00\x00\x00\xf0\xbf", 8));
  TestSerialization(__LINE__, 42.0,
                    std::string_view("\x00\x00\x00\x00\x00\x00\x45\x40", 8));
  TestSerialization(__LINE__, std::numeric_limits<double>::infinity(),
                    std::string_view("\x00\x00\x00\x00\x00\x00\xf0\x7f", 8));
  TestSerialization(__LINE__, -std::numeric_limits<double>::infinity(),
                    std::string_view("\x00\x00\x00\x00\x00\x00\xf0\xff", 8));
}

TEST(SerializeTest, FixedSizeIntegers) {
  TestSerialization(__LINE__, pb::fixed32_t{0},
                    std::string_view("\x00\x00\x00\x00", 4));
  TestSerialization(__LINE__, pb::fixed32_t{1},
                    std::string_view("\x01\x00\x00\x00", 4));
  TestSerialization(__LINE__, pb::fixed32_t{1337},
                    std::string_view("\x39\x05\x00\x00", 4));
  TestSerialization(__LINE__,
                    pb::fixed32_t{std::numeric_limits<uint32_t>::max()},
                    std::string_view("\xff\xff\xff\xff", 4));

  TestSerialization(__LINE__, pb::sfixed32_t{0},
                    std::string_view("\x00\x00\x00\x00", 4));
  TestSerialization(__LINE__, pb::sfixed32_t{1},
                    std::string_view("\x01\x00\x00\x00", 4));
  TestSerialization(__LINE__, pb::sfixed32_t{1337},
                    std::string_view("\x39\x05\x00\x00", 4));
  TestSerialization(__LINE__, pb::sfixed32_t{-1},
                    std::string_view("\xff\xff\xff\xff", 4));
  TestSerialization(__LINE__,
                    pb::sfixed32_t{std::numeric_limits<int32_t>::min()},
                    std::string_view("\x00\x00\x00\x80", 4));
  TestSerialization(__LINE__,
                    pb::sfixed32_t{std::numeric_limits<int32_t>::max()},
                    std::string_view("\xff\xff\xff\x7f", 4));

  TestSerialization(__LINE__, pb::fixed64_t{0},
                    std::string_view("\x00\x00\x00\x00\x00\x00\x00\x00", 8));
  TestSerialization(__LINE__, pb::fixed64_t{1},
                    std::string_view("\x01\x00\x00\x00\x00\x00\x00\x00", 8));
  TestSerialization(__LINE__, pb::fixed64_t{1337},
                    std::string_view("\x39\x05\x00\x00\x00\x00\x00\x00", 8));
  TestSerialization(__LINE__,
                    pb::fixed64_t{std::numeric_limits<uint64_t>::max()},
                    std::string_view("\xff\xff\xff\xff\xff\xff\xff\xff", 8));

  TestSerialization(__LINE__, pb::sfixed64_t{0},
                    std::string_view("\x00\x00\x00\x00\x00\x00\x00\x00", 8));
  TestSerialization(__LINE__, pb::sfixed64_t{1},
                    std::string_view("\x01\x00\x00\x00\x00\x00\x00\x00", 8));
  TestSerialization(__LINE__, pb::sfixed64_t{1337},
                    std::string_view("\x39\x05\x00\x00\x00\x00\x00\x00", 8));
  TestSerialization(__LINE__, pb::sfixed64_t{-1},
                    std::string_view("\xff\xff\xff\xff\xff\xff\xff\xff", 8));
  TestSerialization(__LINE__,
                    pb::sfixed64_t{std::numeric_limits<int64_t>::min()},
                    std::string_view("\x00\x00\x00\x00\x00\x00\x00\x80", 8));
  TestSerialization(__LINE__,
                    pb::sfixed64_t{std::numeric_limits<int64_t>::max()},
                    std::string_view("\xff\xff\xff\xff\xff\xff\xff\x7f", 8));
}

template <typename String>
void RunStringsTest(const char* type_name) {
  SCOPED_TRACE(::testing::Message() << "for " << type_name);

  const String empty = "";
  const String one = "1";
  const String two = "12";
  const String one_hundred_twenty_seven =
      "----------------------------------------------------------------"
      "---------------------------------------------------------------";
  const String one_hundred_twenty_eight =
      "****************************************************************"
      "****************************************************************";

  TestSerialization(__LINE__, empty, std::string_view("\x00", 1));
  TestSerialization(__LINE__, one,
                    std::string_view("\x01"
                                     "1",
                                     1 + 1));
  TestSerialization(__LINE__, two,
                    std::string_view("\x02"
                                     "12",
                                     1 + 2));
  TestSerialization(__LINE__, one_hundred_twenty_seven,
                    std::string_view("\x7f"
                                     "--------------------------------"
                                     "--------------------------------"
                                     "--------------------------------"
                                     "-------------------------------",
                                     1 + 127));
  TestSerialization(__LINE__, one_hundred_twenty_eight,
                    std::string_view("\x80\x01"
                                     "********************************"
                                     "********************************"
                                     "********************************"
                                     "********************************",
                                     2 + 128));
}

TEST(SerializeTest, Strings) {
  RunStringsTest<std::string>("std::string");
  RunStringsTest<std::string_view>("std::string_view");
}

TEST(SerializeTest, Messages) {
  struct NestedMessage {
    std::string a_string;
    int an_int = 0;

    using ProtobufFields = FieldList<Field<&NestedMessage::a_string, 1>,
                                     Field<&NestedMessage::an_int, 2>>;
  };

  struct Message {
    uint64_t an_uint64;
    int32_t an_int32;
    pb::sint32_t a_sint32;
    bool a_bool;
    enum { kA = 128, kB = 20 } an_enum;
    double a_double;
    float a_float;
    pb::sfixed64_t a_sfixed64;
    pb::fixed32_t a_fixed32;
    std::string a_string;
    std::string_view a_string_view;
    NestedMessage a_nested_message;
    std::unique_ptr<NestedMessage> a_nested_message_ptr;
    std::optional<float> optional_float;
    std::optional<NestedMessage> optional_nested_message;

    using ProtobufFields =
        FieldList<Field<&Message::an_uint64, 1>,
                  Field<&Message::an_int32, 2>,
                  Field<&Message::a_sint32, 3>,
                  Field<&Message::a_bool, 4>,
                  Field<&Message::an_enum, 5>,
                  Field<&Message::a_double, 6>,
                  Field<&Message::a_float, 7>,
                  Field<&Message::a_sfixed64, 8>,
                  Field<&Message::a_fixed32, 9>,
                  Field<&Message::a_string, 10>,
                  Field<&Message::a_string_view, 11>,
                  Field<&Message::a_nested_message, 12>,
                  Field<&Message::a_nested_message_ptr, 13>,
                  Field<&Message::optional_float, 16>,
                  Field<&Message::optional_nested_message, 19>>;
  };

  const Message message{
      .an_uint64 = 9871236,
      .an_int32 = 789365,
      .a_sint32 = 99,
      .a_bool = true,
      .an_enum = Message::kA,
      .a_double = 2.718,
      .a_float = 3.14f,
      .a_sfixed64 = -123,
      .a_fixed32 = 456,
      .a_string = "yarn",
      .a_string_view = "sunsets",
      .a_nested_message =
          {
              .a_string = "kittens",
              .an_int = 0,
          },
      .a_nested_message_ptr = std::make_unique<NestedMessage>(),
      .optional_float = 1.0e6f,
      .optional_nested_message = NestedMessage{},
  };

  // clang-format off
  TestSerialization(
      __LINE__, message,
      std::string_view(
          // "Total size of message fields is 92 bytes".
          "\x5c"
          // "Field 1, varint" "9871236".
          "\x08" "\x84\xbf\xda\x04"
          // "Field 2, varint" "789365".
          "\x10" "\xf5\x96\x30"
          // "Field 3, varint" "zigzag{99}".
          "\x18" "\xc6\x01"
          // "Field 4, varint" "bool{true}".
          "\x20" "\x01"
          // "Field 5, varint" "enum 128 value".
          "\x28" "\x80\x01"
          // "Field 6, fixed64" "double{2.718}".
          "\x31" "\x58\x39\xb4\xc8\x76\xbe\x05\x40"
          // "Field 7, fixed32" "float{3.14}".
          "\x3d" "\xc3\xf5\x48\x40"
          // "Field 8, fixed64" "-123".
          "\x41" "\x85\xff\xff\xff\xff\xff\xff\xff"
          // "Field 9, fixed32" "456".
          "\x4d" "\xc8\x01\x00\x00"
          // "Field 10, length-delimited" "contains 4 bytes" "yarn".
          "\x52" "\x04" "yarn"
          // "Field 11, length-delimited" "contains 7 bytes" "sunsets".
          "\x5a" "\x07" "sunsets"
          // "Field 12, length-delimited" "contains 11 bytes" "{"kittens", 0}".
          "\x62" "\x0b" "\x0a\x07kittens\x10\x00"
          // "Field 13, length-delimited" "contains 4 bytes" "EmptyMessage{}".
          "\x6a" "\x04" "\x0a\x00\x10\x00"
          // "Field 16, fixed32" "float{1.0e6}".
          "\x85\x01" "\x00\x24\x74\x49"
          // "Field 19, length-delimited" "contains 4 bytes" "EmptyMessage{}".
          "\x9a\x01" "\x04" "\x0a\x00\x10\x00",
          93));
  // clang-format on
}

// Test the behaviors around the various required/optional/repeated rules using
// empty messages.
TEST(SerializeTest, MessageFieldRules) {
  struct EmptyMessage {
    using ProtobufFields = FieldList<>;
  };
  const EmptyMessage empty_message{};
  TestSerialization(__LINE__, empty_message, std::string_view("\x00", 1));

  struct OuterMessage1 {
    EmptyMessage empty;
    using ProtobufFields = FieldList<Field<&OuterMessage1::empty, 1>>;
  };
  const OuterMessage1 outer_message_1{};
  TestSerialization(__LINE__, outer_message_1,
                    std::string_view("\x02\x0a\x00", 3));

  struct OuterMessage2 {
    std::optional<EmptyMessage> empty;
    using ProtobufFields = FieldList<Field<&OuterMessage2::empty, 1>>;
  };
  OuterMessage2 outer_message_2{};
  TestSerialization(__LINE__, outer_message_2, std::string_view("\x00", 1));
  outer_message_2.empty.emplace();
  TestSerialization(__LINE__, outer_message_2,
                    std::string_view("\x02\x0a\x00", 3));

  struct OuterMessage3 {
    std::unique_ptr<EmptyMessage> empty;
    using ProtobufFields = FieldList<Field<&OuterMessage3::empty, 1>>;
  };
  OuterMessage3 outer_message_3{};
  TestSerialization(__LINE__, outer_message_3, std::string_view("\x00", 1));
  outer_message_3.empty = std::make_unique<EmptyMessage>();
  TestSerialization(__LINE__, outer_message_3,
                    std::string_view("\x02\x0a\x00", 3));

  struct OuterMessage4 {
    std::vector<EmptyMessage> empties;
    using ProtobufFields = FieldList<Field<&OuterMessage4::empties, 1>>;
  };
  OuterMessage4 outer_message_4{};
  TestSerialization(__LINE__, outer_message_4, std::string_view("\x00", 1));
  outer_message_4.empties.push_back({});
  outer_message_4.empties.push_back({});
  outer_message_4.empties.push_back({});
  TestSerialization(__LINE__, outer_message_4,
                    std::string_view("\x06\x0a\x00\x0a\x00\x0a\x00", 7));
}

TEST(SerializeTest, NullStringViewInMessageIsSkipped) {
  struct Message {
    int an_int;
    std::string_view hello;
    std::string_view null;
    std::string_view world;

    using ProtobufFields = FieldList<Field<&Message::an_int, 1>,
                                     Field<&Message::hello, 2>,
                                     Field<&Message::null, 3>,
                                     Field<&Message::world, 4>>;
  };

  const Message message{
      .an_int = 792310,
      .hello = "hello",
      .null = {},
      .world = "world",
  };

  // clang-format off
  TestSerialization(
      __LINE__, message,
      std::string_view(
          // "Total size of message fields is 18 bytes".
          "\x12"
          // "Field 1, varint" "792310".
          "\x08" "\xf6\xad\x30"
          // "Field 2, length-delimited" "contains 5 bytes" "hello".
          "\x12" "\x05" "hello"
          // "Field 4, length-delimited" "contains 5 bytes" "world".
          "\x22" "\x05" "world",
          19));
  // clang-format on
}

TEST(SerializeTest, PackedRepeatedFields) {
  struct Message {
    std::vector<int> repeated_ints;
    std::vector<bool> repeated_bools;
    std::vector<double> repeated_doubles;
    std::vector<float> repeated_floats;
    std::vector<int> empty_ints;
    std::vector<pb::sint32_t> repeated_sints;
    std::vector<pb::fixed32_t> repeated_fixeds;

    using ProtobufFields = FieldList<Field<&Message::repeated_ints, 1>,
                                     Field<&Message::repeated_bools, 2>,
                                     Field<&Message::repeated_doubles, 3>,
                                     Field<&Message::repeated_floats, 4>,
                                     Field<&Message::empty_ints, 5>,
                                     Field<&Message::repeated_sints, 6>,
                                     Field<&Message::repeated_fixeds, 7>>;
  };

  static const Message message{
      .repeated_ints = {1, 2, 3},
      .repeated_bools = {true, false, true, false, false, true, true},
      .repeated_doubles = {3.14, 2.71828, -256.0, 999.95},
      .repeated_floats = {3.14f, 2.71828f, -256.0f, 999.95f},
      .empty_ints = {},
      .repeated_sints = {0, -1, 1},
      .repeated_fixeds = {13, 42, 1, 0},
  };

  // clang-format off
  TestSerialization(
      __LINE__, message,
      std::string_view(
          // "Total size of message fields is 89 bytes".
          "\x59"
          // "Field 1, length-delimited" "contains 3 bytes" "1" "2" "3".
          "\x0a" "\x03" "\x01" "\x02" "\x03"
          // "Field 2, length-delimited" "contains 7 bytes" "true" "false"
          // "true" "false" "false" "true" "true".
          "\x12" "\x07" "\x01" "\x00" "\x01" "\x00" "\x00" "\x01" "\x01"
          // "Field 3, length-delimited" "contains 32 bytes" "3.14" "2.71828"
          // "-256.0" "999.95".
          "\x1a" "\x20"
          "\x1f\x85\xeb\x51\xb8\x1e\x09\x40"
          "\x90\xf7\xaa\x95\x09\xbf\x05\x40"
          "\x00\x00\x00\x00\x00\x00\x70\xc0"
          "\x9a\x99\x99\x99\x99\x3f\x8f\x40"
          // "Field 4, length-delimited" "contains 16 bytes" "3.14f" "2.71828f"
          // "-256.0f" "999.95f".
          "\x22" "\x10"
          "\xc3\xf5\x48\x40"
          "\x4d\xf8\x2d\x40"
          "\x00\x00\x80\xc3"
          "\xcd\xfc\x79\x44"
          // NOTE: Field 5 has zero elements, and should NOT appear on-the-wire.
          // "Field 6, length-delimited" "contains 3 bytes" "0" "-1" "1"
          "\x32" "\x03" "\x00" "\x01" "\x02"
          // "Field 7, length-delimited" "contains 16 bytes" "13" "42" "1" "0"
          "\x3a" "\x10"
          "\x0d\x00\x00\x00"
          "\x2a\x00\x00\x00"
          "\x01\x00\x00\x00"
          "\x00\x00\x00\x00",
          90));
  // clang-format on
}

TEST(SerializeTest, UnpackedRepeatedFields) {
  struct Thing {
    int an_int = 0;

    using ProtobufFields = FieldList<Field<&Thing::an_int, 1>>;
  };

  struct Message {
    std::vector<std::string> repeated_strings;
    std::vector<Thing> repeated_things;
    std::vector<std::unique_ptr<Thing>> repeated_thing_ptrs;

    using ProtobufFields = FieldList<Field<&Message::repeated_strings, 1>,
                                     Field<&Message::repeated_things, 2>,
                                     Field<&Message::repeated_thing_ptrs, 3>>;
  };

  Message message{
      .repeated_strings = {"a", "b", "c", "d", "e"},
      .repeated_things = {Thing{1}, Thing{2}, Thing{3}, Thing{4}},
      .repeated_thing_ptrs = {},
  };
  message.repeated_thing_ptrs.push_back(std::make_unique<Thing>());

  // clang-format off
  TestSerialization(
      __LINE__, message,
      std::string_view(
          // "Total size of message fields is 35 bytes".
          "\x23"
          // 5X: "Field 1, length-delimited" "one char (varies)".
          "\x0a" "\x01" "a"
          "\x0a" "\x01" "b"
          "\x0a" "\x01" "c"
          "\x0a" "\x01" "d"
          "\x0a" "\x01" "e"
          // 4X: "Field 2, length-delimited" "Thing{number}".
          "\x12" "\x02" "\x08\x01"
          "\x12" "\x02" "\x08\x02"
          "\x12" "\x02" "\x08\x03"
          "\x12" "\x02" "\x08\x04"
          // 1X: "Field 3, length-delimited" "Thing{0}".
          "\x1a" "\x02" "\x08\x00",
          36));
  // clang-format on
}

TEST(SerializeTest, MessagesTooBigInEncodedSize) {
  struct Message {
    std::string a_string;

    using ProtobufFields = FieldList<Field<&Message::a_string, 1>>;
  };

  constexpr static int kTagSize = 1;
  constexpr static int kLengthVarintSize = 4;

  Message message{};

  message.a_string.assign(pb::codec::kMaxSerializedSize, '!');
  EXPECT_EQ(pb::codec::kWouldSerializeTooManyBytes,
            ComputeSerializedValueSize(message.a_string));

  message.a_string.resize(pb::codec::kMaxSerializedSize - kLengthVarintSize);
  EXPECT_EQ(pb::codec::kMaxSerializedSize,
            ComputeSerializedValueSize(message.a_string));
  EXPECT_EQ(pb::codec::kWouldSerializeTooManyBytes,
            ComputeSerializedSizeOfFields(message,
                                          typename Message::ProtobufFields{}));

  message.a_string.resize(pb::codec::kMaxSerializedSize - kTagSize -
                          kLengthVarintSize);
  EXPECT_GT(pb::codec::kMaxSerializedSize,
            ComputeSerializedValueSize(message.a_string));
  EXPECT_EQ(pb::codec::kMaxSerializedSize,
            ComputeSerializedSizeOfFields(message,
                                          typename Message::ProtobufFields{}));
}

TEST(SerializeTest, MessagesHavingBigFieldNumbers) {
  struct Message {
    int alice;
    int bob;
    int charlie;

    using ProtobufFields = FieldList<Field<&Message::alice, 1>,
                                     Field<&Message::bob, 2048>,
                                     // 536870911 is 2^29 - 1, the max allowed.
                                     Field<&Message::charlie, 536870911>>;
  };

  const Message message{1, 2, 3};

  // clang-format off
  TestSerialization(__LINE__, message,
                    std::string_view(
                        // "Total size of message fields is 12 bytes".
                        "\x0c"
                        // "Field 1, varint" "1"
                        "\x08" "\x01"
                        // "Field 2048, varint" "2"
                        "\x80\x80\x01" "\x02"
                        // "Field 536870911, varint" "3"
                        "\xf8\xff\xff\xff\x0f" "\x03",
                        13));
  // clang-format on
}

TEST(SerializeTest, AnyIterableForRepeatedFields_AccessByNativeReference) {
  // Most common among STL-like containers: When the dereference operator is
  // called on an iterator, it returns an lvalue reference.
  struct IterableContainer {
    const int* begin() const { return &elements_[0]; }
    const int* end() const { return &elements_[3]; }

   private:
    int elements_[3] = {1, 2, 3};
  };

  struct Message {
    IterableContainer container;

    using ProtobufFields = FieldList<Field<&Message::container, 1>>;
  };

  Message message{};

  // clang-format off
  TestSerialization(
      __LINE__, message,
      std::string_view(
          // "Total size of message fields is 5 bytes".
          "\x05"
          // "Field 1, length-delimited" "3 bytes follow" "1" "2" "3".
          "\x0a" "\x03" "\x01" "\x02" "\x03",
          6));
  // clang-format on
}

TEST(SerializeTest, AnyIterableForRepeatedFields_AccessByProxyReference) {
  // Some STL-like containers (e.g., std::vector<bool>): When the dereference
  // operator is called on an iterator, it returns a proxy reference, not an
  // lvalue reference.
  struct IterableContainer {
    using value_type = bool;

    auto begin() const { return elements_.cbegin(); }
    auto end() const { return elements_.cend(); }

   private:
    std::vector<bool> elements_ = {true, false, false};
  };

  struct Message {
    IterableContainer container;

    using ProtobufFields = FieldList<Field<&Message::container, 1>>;
  };

  Message message{};

  // clang-format off
  TestSerialization(
      __LINE__, message,
      std::string_view(
          // "Total size of message fields is 6 bytes".
          "\x05"
          // "Field 1, length-delimited" "3 bytes follow" "true" "false"
          // "false".
          "\x0a" "\x03" "\x01" "\x00" "\x00",
          6));
  // clang-format on
}

TEST(SerializeTest, Maps) {
  struct Message {
    std::map<std::string, int> a_map;

    using ProtobufFields = FieldList<Field<&Message::a_map, 1>>;
  };

  Message message{};
  message.a_map["alice"] = 28;
  message.a_map["bob"] = 27;
  message.a_map["charlie"] = 211;

  // clang-format off
  TestSerialization(
      __LINE__, message,
      std::string_view(
          // "Total size of message fields is 34 bytes".
          "\x22"
          // "Field 1, length-delimited" "9 bytes" "alice --> 28"
          "\x0a" "\x09" "\x0a\x05" "alice" "\x10" "\x1c"
          // "Field 1, length-delimited" "7 bytes" "bob --> 27"
          "\x0a" "\x07" "\x0a\x03" "bob" "\x10" "\x1b"
          // "Field 1, length-delimited" "12 bytes" "charlie --> 211"
          "\x0a" "\x0c" "\x0a\x07" "charlie" "\x10" "\xd3\x01",
          35));
  // clang-format on
}

}  // namespace
}  // namespace pb::codec
