// Copyright 2022 Yuri Wiitala. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pb/codec/parse.h"

#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "gtest/gtest.h"
#include "pb/codec/limits.h"
#include "pb/field_list.h"
#include "pb/integer_wrapper.h"

namespace pb::codec {
namespace {

template <typename Value>
void TestParse(int line_num,
               const Value& expected,
               std::string_view wire_bytes) {
  SCOPED_TRACE(::testing::Message() << "see line " << line_num);
  Value result{};
  auto* const buffer = reinterpret_cast<const uint8_t*>(wire_bytes.data());
  auto* after_it = ParseValue(buffer, buffer + wire_bytes.size(), 0, result);
  EXPECT_EQ(after_it, buffer + wire_bytes.size());
  if (expected != result) {
    ADD_FAILURE();
  }
}

template <typename Value>
void TestThatParseFails(int line_num, std::string_view wire_bytes) {
  SCOPED_TRACE(::testing::Message() << "see line " << line_num);
  Value result{};
  auto* const buffer = reinterpret_cast<const uint8_t*>(wire_bytes.data());
  auto* after_it = ParseValue(buffer, buffer + wire_bytes.size(), 0, result);
  EXPECT_EQ(nullptr, after_it);
}

template <typename Integral>
void RunNativeIntegerTest(const char* type_name) {
  SCOPED_TRACE(::testing::Message() << "for " << type_name);

  TestParse(__LINE__, Integral{0}, std::string_view("\x00", 1));
  // Technically, this is a valid varint encoding, even if it is inefficient.
  TestParse(__LINE__, Integral{0}, std::string_view("\x80\x80\x00", 3));

  // Positive values.
  TestParse(__LINE__, Integral{1}, std::string_view("\x01", 1));
  TestParse(__LINE__, Integral{2}, std::string_view("\x02", 1));
  TestParse(__LINE__, Integral{3}, std::string_view("\x03", 1));
  TestParse(__LINE__, Integral{std::numeric_limits<int8_t>::max()},
            std::string_view("\x7f", 1));
  TestParse(__LINE__, Integral{1} << 7, std::string_view("\x80\x01", 2));
  TestParse(__LINE__, (Integral{1} << 14) - 1, std::string_view("\xff\x7f", 2));
  TestParse(__LINE__, Integral{1} << 14, std::string_view("\x80\x80\x01", 3));
  TestParse(__LINE__, Integral{std::numeric_limits<int16_t>::max()},
            std::string_view("\xff\xff\x01", 3));
  TestParse(__LINE__, (Integral{1} << 21) - 1,
            std::string_view("\xff\xff\x7f", 3));
  TestParse(__LINE__, Integral{1} << 21,
            std::string_view("\x80\x80\x80\x01", 4));
  TestParse(__LINE__, (Integral{1} << 28) - 1,
            std::string_view("\xff\xff\xff\x7f", 4));
  TestParse(__LINE__, Integral{1} << 28,
            std::string_view("\x80\x80\x80\x80\x01", 5));
  TestParse(__LINE__, Integral{std::numeric_limits<int32_t>::max()},
            std::string_view("\xff\xff\xff\xff\x07", 5));
  if constexpr (sizeof(Integral) >= 8) {
    TestParse(__LINE__, (Integral{1} << 35) - 1,
              std::string_view("\xff\xff\xff\xff\x7f", 5));
    TestParse(__LINE__, Integral{1} << 35,
              std::string_view("\x80\x80\x80\x80\x80\x01", 6));
    TestParse(__LINE__, (Integral{1} << 42) - 1,
              std::string_view("\xff\xff\xff\xff\xff\x7f", 6));
    TestParse(__LINE__, Integral{1} << 42,
              std::string_view("\x80\x80\x80\x80\x80\x80\x01", 7));
    TestParse(__LINE__, Integral{std::numeric_limits<int64_t>::max()},
              std::string_view("\xff\xff\xff\xff\xff\xff\xff\xff\x7f", 9));
    if constexpr (std::is_unsigned_v<Integral>) {
      TestParse(
          __LINE__, Integral{std::numeric_limits<uint64_t>::max()},
          std::string_view("\xff\xff\xff\xff\xff\xff\xff\xff\xff\x01", 10));
    }
  }

  // Negative values.
  if constexpr (std::is_signed_v<Integral>) {
    TestParse(__LINE__, Integral{-1},
              std::string_view("\xff\xff\xff\xff\xff\xff\xff\xff\xff\x01", 10));
    if constexpr (sizeof(Integral) == 4) {
      TestParse(
          __LINE__, std::numeric_limits<Integral>::min(),
          std::string_view("\x80\x80\x80\x80\xf8\xff\xff\xff\xff\x01", 10));
    } else if constexpr (sizeof(Integral) == 8) {
      TestParse(
          __LINE__, std::numeric_limits<Integral>::min(),
          std::string_view("\x80\x80\x80\x80\x80\x80\x80\x80\x80\x01", 10));
    }
  }

  // Garbage: Zero bytes should not parse.
  TestThatParseFails<Integral>(__LINE__, std::string_view("", 0));

  // Garbage: Varints longer than the maximum of 64 bits (should truncate).
  TestParse(
      __LINE__, Integral{0},
      std::string_view("\x80\x80\x80\x80\x80\x80\x80\x80\x80\xf0\x7f", 11));
  TestParse(
      __LINE__, Integral{10},
      std::string_view(
          "\x8a\x80\x80\x80\x80\x80\x80\x80\x80\xf0\xff\xff\xff\x7f", 14));

  // Garbage: Varints that never ended (i.e., the end of the buffer was reached
  // prematurely).
  TestThatParseFails<Integral>(__LINE__, std::string_view("\x80", 1));
  TestThatParseFails<Integral>(__LINE__, std::string_view("\x80\x80", 2));
  TestThatParseFails<Integral>(__LINE__,
                               std::string_view("\x80\x80\x80\x80", 4));
  TestThatParseFails<Integral>(
      __LINE__,
      std::string_view("\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80", 10));
}

TEST(ParseTest, NativeIntegers) {
  RunNativeIntegerTest<int32_t>("int32_t");
  RunNativeIntegerTest<int64_t>("int64_t");
  RunNativeIntegerTest<uint32_t>("uint32_t");
  RunNativeIntegerTest<uint64_t>("uint64_t");
}

TEST(ParseTest, TruncatedIntegers) {
  TestParse(__LINE__, int32_t{-1}, std::string_view("\xff\xff\xff\xff\x7f", 5));
  TestParse(__LINE__, int32_t{0},
            std::string_view("\x80\x80\x80\x80\x80\x01", 6));
  TestParse(__LINE__, int32_t{-1},
            std::string_view("\xff\xff\xff\xff\xff\xff\xff\xff\x7f", 9));
  TestParse(__LINE__, int32_t{-1},
            std::string_view("\xff\xff\xff\xff\xff\xff\xff\xff\xff\x01", 10));

  TestParse(__LINE__, std::numeric_limits<uint32_t>::max(),
            std::string_view("\xff\xff\xff\xff\x7f", 5));
  TestParse(__LINE__, uint32_t{0},
            std::string_view("\x80\x80\x80\x80\x80\x01", 6));
  TestParse(__LINE__, std::numeric_limits<uint32_t>::max(),
            std::string_view("\xff\xff\xff\xff\xff\xff\xff\xff\x7f", 9));
  TestParse(__LINE__, std::numeric_limits<uint32_t>::max(),
            std::string_view("\xff\xff\xff\xff\xff\xff\xff\xff\xff\x01", 10));
}

TEST(ParseTest, Bools) {
  TestParse(__LINE__, false, std::string_view("\x00", 1));
  TestParse(__LINE__, true, std::string_view("\x01", 1));

  TestThatParseFails<bool>(__LINE__, std::string_view("", 0));
}

TEST(ParseTest, IntegerValuesParsedAsBools) {
  TestParse(__LINE__, true, std::string_view("\x80\x01", 2));
  TestParse(__LINE__, true, std::string_view("\x8a\x01", 2));
  TestParse(__LINE__, true, std::string_view("\x80\x80\x80\x80\x01", 5));
  TestParse(__LINE__, true,
            std::string_view("\x80\x80\x80\x80\x80\x80\x80\x80\x80\x01", 10));
  TestParse(__LINE__, true,
            std::string_view("\xff\xff\xff\xff\xff\xff\xff\xff\xff\x01", 10));

  // Technically, this is a valid varint encoding, even if it is inefficient.
  TestParse(__LINE__, false,
            std::string_view("\x80\x80\x80\x80\x80\x80\x80\x00", 8));
}

TEST(ParseTest, Enums) {
  enum Foo : int32_t {
    kOne = 1,
    kTwo = 2,
    kSome = 127,
    kLots = 128,
    kDoNotDoThisWithProtobufs = -1,  // Because <0 requires 10 wire bytes.
  };

  TestParse(__LINE__, kOne, std::string_view("\x01", 1));
  TestParse(__LINE__, kTwo, std::string_view("\x02", 1));
  TestParse(__LINE__, kSome, std::string_view("\x7F", 1));
  TestParse(__LINE__, kLots, std::string_view("\x80\x01", 2));
  TestParse(__LINE__, kDoNotDoThisWithProtobufs,
            std::string_view("\xff\xff\xff\xff\xff\xff\xff\xff\xff\x01", 10));

  TestThatParseFails<Foo>(__LINE__, std::string_view("", 0));
}

TEST(ParseTest, EnumClasses) {
  enum class Bar : uint8_t {
    kA = 0x01,
    kB = 0x02,
    kC = 0x87,
    kD = 0xE0,
    kE = 0xFF,
  };

  TestParse(__LINE__, Bar::kA, std::string_view("\x01", 1));
  TestParse(__LINE__, Bar::kB, std::string_view("\x02", 1));
  TestParse(__LINE__, Bar::kC, std::string_view("\x87\x01", 2));
  TestParse(__LINE__, Bar::kD, std::string_view("\xE0\x01", 2));
  TestParse(__LINE__, Bar::kE, std::string_view("\xFF\x01", 2));

  TestThatParseFails<Bar>(__LINE__, std::string_view("", 0));
}

TEST(ParseTest, ZigZagEncodedInts) {
  TestParse(__LINE__, pb::sint32_t{0}, std::string_view("\x00", 1));
  TestParse(__LINE__, pb::sint32_t{-1}, std::string_view("\x01", 1));
  TestParse(__LINE__, pb::sint32_t{1}, std::string_view("\x02", 1));
  TestParse(__LINE__, pb::sint32_t{std::numeric_limits<int32_t>::max()},
            std::string_view("\xfe\xff\xff\xff\x0f", 5));
  TestParse(__LINE__, pb::sint32_t{std::numeric_limits<int32_t>::min()},
            std::string_view("\xff\xff\xff\xff\x0f", 5));

  TestParse(__LINE__, pb::sint64_t{0}, std::string_view("\x00", 1));
  TestParse(__LINE__, pb::sint64_t{-1}, std::string_view("\x01", 1));
  TestParse(__LINE__, pb::sint64_t{1}, std::string_view("\x02", 1));
  TestParse(__LINE__, pb::sint64_t{std::numeric_limits<int64_t>::max()},
            std::string_view("\xfe\xff\xff\xff\xff\xff\xff\xff\xff\x01", 10));
  TestParse(__LINE__, pb::sint64_t{std::numeric_limits<int64_t>::min()},
            std::string_view("\xff\xff\xff\xff\xff\xff\xff\xff\xff\x01", 10));

  TestThatParseFails<pb::sint32_t>(__LINE__, std::string_view("", 0));
  TestThatParseFails<pb::sint64_t>(__LINE__, std::string_view("", 0));
}

TEST(ParseTest, NativeFloats) {
  TestParse(__LINE__, 0.0f, std::string_view("\x00\x00\x00\x00", 4));
  TestParse(__LINE__, 1.0f, std::string_view("\x00\x00\x80\x3f", 4));
  TestParse(__LINE__, -1.0f, std::string_view("\x00\x00\x80\xbf", 4));
  TestParse(__LINE__, 42.0f, std::string_view("\x00\x00\x28\x42", 4));
  TestParse(__LINE__, std::numeric_limits<float>::infinity(),
            std::string_view("\x00\x00\x80\x7f", 4));
  TestParse(__LINE__, -std::numeric_limits<float>::infinity(),
            std::string_view("\x00\x00\x80\xff", 4));

  // Fewer than than 4 bytes should fail the parse.
  for (std::size_t i = 0; i < 4; ++i) {
    TestThatParseFails<float>(
        __LINE__, std::string_view(&("\x00\x00\x28\x42"[4 - i]), i));
  }
}

TEST(ParseTest, NativeDoubles) {
  TestParse(__LINE__, 0.0,
            std::string_view("\x00\x00\x00\x00\x00\x00\x00\x00", 8));
  TestParse(__LINE__, 1.0,
            std::string_view("\x00\x00\x00\x00\x00\x00\xf0\x3f", 8));
  TestParse(__LINE__, -1.0,
            std::string_view("\x00\x00\x00\x00\x00\x00\xf0\xbf", 8));
  TestParse(__LINE__, 42.0,
            std::string_view("\x00\x00\x00\x00\x00\x00\x45\x40", 8));
  TestParse(__LINE__, std::numeric_limits<double>::infinity(),
            std::string_view("\x00\x00\x00\x00\x00\x00\xf0\x7f", 8));
  TestParse(__LINE__, -std::numeric_limits<double>::infinity(),
            std::string_view("\x00\x00\x00\x00\x00\x00\xf0\xff", 8));

  // Fewer than 8 bytes should fail the parse.
  for (std::size_t i = 0; i < 8; ++i) {
    TestThatParseFails<double>(
        __LINE__,
        std::string_view(&("\x00\x00\x00\x00\x00\x00\x45\x40"[8 - i]), i));
  }
}

TEST(ParseTest, FixedSizeIntegers) {
  TestParse(__LINE__, pb::fixed32_t{0},
            std::string_view("\x00\x00\x00\x00", 4));
  TestParse(__LINE__, pb::fixed32_t{1},
            std::string_view("\x01\x00\x00\x00", 4));
  TestParse(__LINE__, pb::fixed32_t{1337},
            std::string_view("\x39\x05\x00\x00", 4));
  TestParse(__LINE__, pb::fixed32_t{std::numeric_limits<uint32_t>::max()},
            std::string_view("\xff\xff\xff\xff", 4));

  TestParse(__LINE__, pb::sfixed32_t{0},
            std::string_view("\x00\x00\x00\x00", 4));
  TestParse(__LINE__, pb::sfixed32_t{1},
            std::string_view("\x01\x00\x00\x00", 4));
  TestParse(__LINE__, pb::sfixed32_t{1337},
            std::string_view("\x39\x05\x00\x00", 4));
  TestParse(__LINE__, pb::sfixed32_t{-1},
            std::string_view("\xff\xff\xff\xff", 4));
  TestParse(__LINE__, pb::sfixed32_t{std::numeric_limits<int32_t>::min()},
            std::string_view("\x00\x00\x00\x80", 4));
  TestParse(__LINE__, pb::sfixed32_t{std::numeric_limits<int32_t>::max()},
            std::string_view("\xff\xff\xff\x7f", 4));

  TestParse(__LINE__, pb::fixed64_t{0},
            std::string_view("\x00\x00\x00\x00\x00\x00\x00\x00", 8));
  TestParse(__LINE__, pb::fixed64_t{1},
            std::string_view("\x01\x00\x00\x00\x00\x00\x00\x00", 8));
  TestParse(__LINE__, pb::fixed64_t{1337},
            std::string_view("\x39\x05\x00\x00\x00\x00\x00\x00", 8));
  TestParse(__LINE__, pb::fixed64_t{std::numeric_limits<uint64_t>::max()},
            std::string_view("\xff\xff\xff\xff\xff\xff\xff\xff", 8));

  TestParse(__LINE__, pb::sfixed64_t{0},
            std::string_view("\x00\x00\x00\x00\x00\x00\x00\x00", 8));
  TestParse(__LINE__, pb::sfixed64_t{1},
            std::string_view("\x01\x00\x00\x00\x00\x00\x00\x00", 8));
  TestParse(__LINE__, pb::sfixed64_t{1337},
            std::string_view("\x39\x05\x00\x00\x00\x00\x00\x00", 8));
  TestParse(__LINE__, pb::sfixed64_t{-1},
            std::string_view("\xff\xff\xff\xff\xff\xff\xff\xff", 8));
  TestParse(__LINE__, pb::sfixed64_t{std::numeric_limits<int64_t>::min()},
            std::string_view("\x00\x00\x00\x00\x00\x00\x00\x80", 8));
  TestParse(__LINE__, pb::sfixed64_t{std::numeric_limits<int64_t>::max()},
            std::string_view("\xff\xff\xff\xff\xff\xff\xff\x7f", 8));

  // An insufficient number of bytes should fail the parse.
  for (std::size_t i = 0; i < 4; ++i) {
    TestThatParseFails<pb::fixed32_t>(
        __LINE__, std::string_view(&("\x39\x05\x00\x00"[4 - i]), i));
    TestThatParseFails<pb::sfixed32_t>(
        __LINE__, std::string_view(&("\x39\x05\x00\x00"[4 - i]), i));
  }
  for (std::size_t i = 0; i < 8; ++i) {
    TestThatParseFails<pb::fixed64_t>(
        __LINE__,
        std::string_view(&("\x39\x05\x00\x00\x00\x00\x00\x00"[8 - i]), i));
    TestThatParseFails<pb::sfixed64_t>(
        __LINE__,
        std::string_view(&("\x39\x05\x00\x00\x00\x00\x00\x00"[8 - i]), i));
  }
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

  TestParse(__LINE__, empty, std::string_view("\x00", 1));
  TestParse(__LINE__, one,
            std::string_view("\x01"
                             "1",
                             1 + 1));
  TestParse(__LINE__, two,
            std::string_view("\x02"
                             "12",
                             1 + 2));
  TestParse(__LINE__, one_hundred_twenty_seven,
            std::string_view("\x7f"
                             "--------------------------------"
                             "--------------------------------"
                             "--------------------------------"
                             "-------------------------------",
                             1 + 127));
  TestParse(__LINE__, one_hundred_twenty_eight,
            std::string_view("\x80\x01"
                             "********************************"
                             "********************************"
                             "********************************"
                             "********************************",
                             2 + 128));

  TestThatParseFails<String>(__LINE__, std::string_view("", 0));

  // Garbage: Unexpected end-of-buffer reached.
  TestThatParseFails<String>(__LINE__, std::string_view("\x01", 1));
  TestThatParseFails<String>(__LINE__, std::string_view("\x02a", 2));
  TestThatParseFails<String>(
      __LINE__, std::string_view("\x1fI wish I was transmitted fully.", 16));
  TestThatParseFails<String>(
      __LINE__, std::string_view("\x1fI wish I was transmitted fully.", 31));

  // Garbage: String larger than maximum allowed serialized size.
  //
  // NOTE: The size of this string_view is waaay out-of-bounds. With ASAN
  // enabled, this will test that the parser does not even attempt to read the
  // bytes after the length varint.
  TestThatParseFails<String>(
      __LINE__, std::string_view("\x80\x80\x80\x20I am a huge string...",
                                 4 + kMaxSerializedSize));
  TestThatParseFails<String>(
      __LINE__, std::string_view("\xff\xff\xff\x1fI am a huge string...",
                                 4 + (kMaxSerializedSize - 1)));
  TestThatParseFails<String>(
      __LINE__, std::string_view("\xfe\xff\xff\x1fI am a huge string...",
                                 4 + (kMaxSerializedSize - 2)));
  TestThatParseFails<String>(
      __LINE__, std::string_view("\xfd\xff\xff\x1fI am a huge string...",
                                 4 + (kMaxSerializedSize - 3)));
}

TEST(ParseTest, Strings) {
  RunStringsTest<std::string>("std::string");
  RunStringsTest<std::string_view>("std::string_view");
}

TEST(ParseTest, Messages) {
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

  // clang-format off
  constexpr std::string_view wire_bytes(
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
      93);
  // clang-format on

  Message message{};
  auto* const buffer = reinterpret_cast<const uint8_t*>(wire_bytes.data());
  auto* const after_it =
      ParseValue(buffer, buffer + wire_bytes.size(), 0, message);
  EXPECT_EQ(after_it, buffer + wire_bytes.size());

  EXPECT_EQ(9871236u, message.an_uint64);
  EXPECT_EQ(789365, message.an_int32);
  EXPECT_EQ(99, message.a_sint32);
  EXPECT_EQ(true, message.a_bool);
  EXPECT_EQ(Message::kA, message.an_enum);
  EXPECT_EQ(2.718, message.a_double);
  EXPECT_EQ(3.14f, message.a_float);
  EXPECT_EQ(-123, message.a_sfixed64);
  EXPECT_EQ(456u, message.a_fixed32);
  EXPECT_EQ("yarn", message.a_string);
  EXPECT_EQ("sunsets", message.a_string_view);
  EXPECT_EQ("kittens", message.a_nested_message.a_string);
  EXPECT_EQ(0, message.a_nested_message.an_int);
  EXPECT_TRUE(message.a_nested_message_ptr);
  ASSERT_TRUE(message.optional_float.has_value());
  EXPECT_EQ(1.0e6f, message.optional_float.value());
  ASSERT_TRUE(message.optional_nested_message.has_value());
  EXPECT_TRUE(message.optional_nested_message.value().a_string.empty());
  EXPECT_EQ(0, message.optional_nested_message.value().an_int);

  TestThatParseFails<Message>(__LINE__, std::string_view("", 0));
}

// Test the parsing behaviors around the various required/optional/repeated
// rules (using empty messages, for test simplicity).
TEST(ParseTest, MessageFieldRules) {
  struct EmptyMessage {
    using ProtobufFields = FieldList<>;
  };
  EmptyMessage empty_message{};
  auto* buffer = reinterpret_cast<const uint8_t*>("\x00");
  auto* after_it = ParseValue(buffer, buffer + 1, 0, empty_message);
  EXPECT_EQ(buffer + 1, after_it);

  struct OuterMessage1 {
    EmptyMessage empty;
    using ProtobufFields = FieldList<Field<&OuterMessage1::empty, 1>>;
  };
  OuterMessage1 outer_message_1{};
  buffer = reinterpret_cast<const uint8_t*>("\x02\x0a\x00");
  after_it = ParseValue(buffer, buffer + 3, 0, outer_message_1);
  EXPECT_EQ(buffer + 3, after_it);

  struct OuterMessage2 {
    std::optional<EmptyMessage> empty;
    using ProtobufFields = FieldList<Field<&OuterMessage2::empty, 1>>;
  };
  OuterMessage2 outer_message_2a{};
  buffer = reinterpret_cast<const uint8_t*>("\x00");
  after_it = ParseValue(buffer, buffer + 1, 0, outer_message_2a);
  EXPECT_EQ(buffer + 1, after_it);
  EXPECT_FALSE(outer_message_2a.empty.has_value());
  OuterMessage2 outer_message_2b{};
  buffer = reinterpret_cast<const uint8_t*>("\x02\x0a\x00");
  after_it = ParseValue(buffer, buffer + 3, 0, outer_message_2b);
  EXPECT_EQ(buffer + 3, after_it);
  EXPECT_TRUE(outer_message_2b.empty.has_value());

  struct OuterMessage3 {
    std::unique_ptr<EmptyMessage> empty;
    using ProtobufFields = FieldList<Field<&OuterMessage3::empty, 1>>;
  };
  OuterMessage3 outer_message_3a{};
  buffer = reinterpret_cast<const uint8_t*>("\x00");
  after_it = ParseValue(buffer, buffer + 1, 0, outer_message_3a);
  EXPECT_EQ(buffer + 1, after_it);
  EXPECT_FALSE(!!outer_message_3a.empty);
  OuterMessage3 outer_message_3b{};
  buffer = reinterpret_cast<const uint8_t*>("\x02\x0a\x00");
  after_it = ParseValue(buffer, buffer + 3, 0, outer_message_3b);
  EXPECT_EQ(buffer + 3, after_it);
  EXPECT_TRUE(!!outer_message_3b.empty);

  struct OuterMessage4 {
    std::vector<EmptyMessage> empties;
    using ProtobufFields = FieldList<Field<&OuterMessage4::empties, 1>>;
  };
  OuterMessage4 outer_message_4a{};
  buffer = reinterpret_cast<const uint8_t*>("\x00");
  after_it = ParseValue(buffer, buffer + 1, 0, outer_message_4a);
  EXPECT_EQ(buffer + 1, after_it);
  EXPECT_TRUE(outer_message_4a.empties.empty());
  OuterMessage4 outer_message_4b{};
  buffer = reinterpret_cast<const uint8_t*>("\x06\x0a\x00\x0a\x00\x0a\x00");
  after_it = ParseValue(buffer, buffer + 7, 0, outer_message_4b);
  EXPECT_EQ(buffer + 7, after_it);
  EXPECT_EQ(3u, outer_message_4b.empties.size());
}

TEST(ParseTest, PackedRepeatedFields) {
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

  // clang-format off
  constexpr std::string_view wire_bytes(
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
      90);
  // clang-format on

  Message message{};
  auto* const buffer = reinterpret_cast<const uint8_t*>(wire_bytes.data());
  auto* after_it = ParseValue(buffer, buffer + wire_bytes.size(), 0, message);
  EXPECT_EQ(after_it, buffer + wire_bytes.size());

  EXPECT_EQ((std::vector<int>{1, 2, 3}), message.repeated_ints);
  EXPECT_EQ((std::vector<bool>{true, false, true, false, false, true, true}),
            message.repeated_bools);
  EXPECT_EQ((std::vector<double>{3.14, 2.71828, -256.0, 999.95}),
            message.repeated_doubles);
  EXPECT_EQ((std::vector<float>{3.14f, 2.71828f, -256.0f, 999.95f}),
            message.repeated_floats);
  EXPECT_EQ((std::vector<int>{}), message.empty_ints);
  EXPECT_EQ((std::vector<pb::sint32_t>{0, -1, 1}), message.repeated_sints);
  EXPECT_EQ((std::vector<pb::fixed32_t>{13, 42, 1, 0}),
            message.repeated_fixeds);

  // Parsing again should result in "merge" behavior where elements are appended
  // to the end of the existing containers.
  after_it = ParseValue(buffer, buffer + wire_bytes.size(), 0, message);
  EXPECT_EQ(after_it, buffer + wire_bytes.size());

  EXPECT_EQ((std::vector<int>{1, 2, 3, 1, 2, 3}), message.repeated_ints);
  EXPECT_EQ((std::vector<bool>{true, false, true, false, false, true, true,
                               true, false, true, false, false, true, true}),
            message.repeated_bools);
  EXPECT_EQ((std::vector<double>{3.14, 2.71828, -256.0, 999.95, 3.14, 2.71828,
                                 -256.0, 999.95}),
            message.repeated_doubles);
  EXPECT_EQ((std::vector<float>{3.14f, 2.71828f, -256.0f, 999.95f, 3.14f,
                                2.71828f, -256.0f, 999.95f}),
            message.repeated_floats);
  EXPECT_EQ((std::vector<int>{}), message.empty_ints);
  EXPECT_EQ((std::vector<pb::sint32_t>{0, -1, 1, 0, -1, 1}),
            message.repeated_sints);
  EXPECT_EQ((std::vector<pb::fixed32_t>{13, 42, 1, 0, 13, 42, 1, 0}),
            message.repeated_fixeds);
}

TEST(ParseTest, UnpackedRepeatedFields) {
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

  // clang-format off
  constexpr std::string_view wire_bytes(
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
      36);
  // clang-format on

  Message message{};
  auto* const buffer = reinterpret_cast<const uint8_t*>(wire_bytes.data());
  auto* after_it = ParseValue(buffer, buffer + wire_bytes.size(), 0, message);
  EXPECT_EQ(after_it, buffer + wire_bytes.size());

  EXPECT_EQ((std::vector<std::string>{"a", "b", "c", "d", "e"}),
            message.repeated_strings);
  ASSERT_EQ(4u, message.repeated_things.size());
  EXPECT_EQ(1, message.repeated_things[0].an_int);
  EXPECT_EQ(2, message.repeated_things[1].an_int);
  EXPECT_EQ(3, message.repeated_things[2].an_int);
  EXPECT_EQ(4, message.repeated_things[3].an_int);
  ASSERT_EQ(1u, message.repeated_thing_ptrs.size());
  ASSERT_TRUE(!!message.repeated_thing_ptrs[0]);
  EXPECT_EQ(0, message.repeated_thing_ptrs[0]->an_int);

  // Parsing again should result in "merge" behavior where elements are appended
  // to the end of the existing containers.
  after_it = ParseValue(buffer, buffer + wire_bytes.size(), 0, message);
  EXPECT_EQ(after_it, buffer + wire_bytes.size());

  EXPECT_EQ((std::vector<std::string>{"a", "b", "c", "d", "e", "a", "b", "c",
                                      "d", "e"}),
            message.repeated_strings);
  ASSERT_EQ(8u, message.repeated_things.size());
  EXPECT_EQ(1, message.repeated_things[0].an_int);
  EXPECT_EQ(2, message.repeated_things[1].an_int);
  EXPECT_EQ(3, message.repeated_things[2].an_int);
  EXPECT_EQ(4, message.repeated_things[3].an_int);
  EXPECT_EQ(1, message.repeated_things[4].an_int);
  EXPECT_EQ(2, message.repeated_things[5].an_int);
  EXPECT_EQ(3, message.repeated_things[6].an_int);
  EXPECT_EQ(4, message.repeated_things[7].an_int);
  ASSERT_EQ(2u, message.repeated_thing_ptrs.size());
  ASSERT_TRUE(!!message.repeated_thing_ptrs[0]);
  EXPECT_EQ(0, message.repeated_thing_ptrs[0]->an_int);
  ASSERT_TRUE(!!message.repeated_thing_ptrs[1]);
  EXPECT_EQ(0, message.repeated_thing_ptrs[1]->an_int);
}

TEST(ParseTest, Maps) {
  struct Thing {
    int an_int = 0;

    using ProtobufFields = FieldList<Field<&Thing::an_int, 1>>;
  };

  struct Message {
    std::map<int, int> int_to_int_map;
    std::map<std::string, int> string_to_int_map;
    std::map<int, std::string> int_to_string_map;
    std::map<int, Thing> int_to_thing_map;
    std::map<int, std::unique_ptr<Thing>> int_to_thing_ptr_map;

    using ProtobufFields = FieldList<Field<&Message::int_to_int_map, 1>,
                                     Field<&Message::string_to_int_map, 2>,
                                     Field<&Message::int_to_string_map, 3>,
                                     Field<&Message::int_to_thing_map, 4>,
                                     Field<&Message::int_to_thing_ptr_map, 5>>;
  };

  // clang-format off
  constexpr std::string_view wire_bytes(
      // "Total size of message fields is 124 bytes".
      "\x7c"
      // int_to_int_map: { 1→2, 2→3, 3→1 }
      "\x0a" "\x04" "\x08\x01" "\x10\x02"
      "\x0a" "\x04" "\x08\x02" "\x10\x03"
      "\x0a" "\x04" "\x08\x03" "\x10\x01"
      // string_to_int_map: { "one"→2, "two"→3, "three"→1 }
      "\x12" "\x07" "\x0a\x03" "one" "\x10\x02"
      "\x12" "\x09" "\x0a\x05" "three" "\x10\x01"  // Lexicographical key order!
      "\x12" "\x07" "\x0a\x03" "two" "\x10\x03"
      // int_to_string_map: { 1→"two", 2→"three", 3→"one" }
      "\x1a" "\x07" "\x08\x01" "\x12\x03" "two"
      "\x1a" "\x09" "\x08\x02" "\x12\x05" "three"
      "\x1a" "\x07" "\x08\x03" "\x12\x03" "one"
      // int_to_thing_map: { 1→Thing{2}, 2→Thing{3}, 3→Thing{1} }
      "\x22" "\x06" "\x08\x01" "\x12\x02\x08\x02"
      "\x22" "\x06" "\x08\x02" "\x12\x02\x08\x03"
      "\x22" "\x06" "\x08\x03" "\x12\x02\x08\x01"
      // int_to_thing_ptr_map: { 1→Thing{2}, 2→Thing{3}, 3→Thing{1} }
      "\x2a" "\x06" "\x08\x01" "\x12\x02\x08\x02"
      "\x2a" "\x06" "\x08\x02" "\x12\x02\x08\x03"
      "\x2a" "\x06" "\x08\x03" "\x12\x02\x08\x01",
      125);
  // clang-format on

  Message message{};
  auto* const buffer = reinterpret_cast<const uint8_t*>(wire_bytes.data());
  auto* const after_it =
      ParseValue(buffer, buffer + wire_bytes.size(), 0, message);
  EXPECT_EQ(after_it, buffer + wire_bytes.size());

  EXPECT_EQ(3u, message.int_to_int_map.size());
  EXPECT_EQ(2, message.int_to_int_map[1]);
  EXPECT_EQ(3, message.int_to_int_map[2]);
  EXPECT_EQ(1, message.int_to_int_map[3]);

  EXPECT_EQ(3u, message.string_to_int_map.size());
  EXPECT_EQ(2, message.string_to_int_map["one"]);
  EXPECT_EQ(3, message.string_to_int_map["two"]);
  EXPECT_EQ(1, message.string_to_int_map["three"]);

  EXPECT_EQ(3u, message.int_to_string_map.size());
  EXPECT_EQ("two", message.int_to_string_map[1]);
  EXPECT_EQ("three", message.int_to_string_map[2]);
  EXPECT_EQ("one", message.int_to_string_map[3]);

  EXPECT_EQ(3u, message.int_to_thing_map.size());
  EXPECT_EQ(2, message.int_to_thing_map[1].an_int);
  EXPECT_EQ(3, message.int_to_thing_map[2].an_int);
  EXPECT_EQ(1, message.int_to_thing_map[3].an_int);

  EXPECT_EQ(3u, message.int_to_thing_ptr_map.size());
  ASSERT_TRUE(message.int_to_thing_ptr_map[1]);
  EXPECT_EQ(2, message.int_to_thing_ptr_map[1]->an_int);
  ASSERT_TRUE(message.int_to_thing_ptr_map[2]);
  EXPECT_EQ(3, message.int_to_thing_ptr_map[2]->an_int);
  ASSERT_TRUE(message.int_to_thing_ptr_map[3]);
  EXPECT_EQ(1, message.int_to_thing_ptr_map[3]->an_int);
}

TEST(ParseTest, MessagesHavingBigFieldNumbers) {
  struct Message {
    int alice;
    int bob;
    int charlie;

    using ProtobufFields = FieldList<Field<&Message::alice, 1>,
                                     Field<&Message::bob, 2048>,
                                     // 536870911 is 2^29 - 1, the max allowed.
                                     Field<&Message::charlie, 536870911>>;
  };

  // clang-format off
  constexpr std::string_view wire_bytes(
      // "Total size of message fields is 12 bytes".
      "\x0c"
      // "Field 1, varint" "1"
      "\x08" "\x01"
      // "Field 2048, varint" "2"
      "\x80\x80\x01" "\x02"
      // "Field 536870911, varint" "3"
      "\xf8\xff\xff\xff\x0f" "\x03",
      13);
  // clang-format on

  Message message{};
  auto* const buffer = reinterpret_cast<const uint8_t*>(wire_bytes.data());
  auto* const after_it =
      ParseValue(buffer, buffer + wire_bytes.size(), 0, message);
  EXPECT_EQ(after_it, buffer + wire_bytes.size());

  EXPECT_EQ(1, message.alice);
  EXPECT_EQ(2, message.bob);
  EXPECT_EQ(3, message.charlie);
}

TEST(ParseTest, TooDeeplyNestedMessages) {
  struct Message {
    std::unique_ptr<Message> next;

    using ProtobufFields = FieldList<Field<&Message::next, 1>>;
  };

  const auto prepend_wire_bytes_to_nest_one_deeper =
      [](std::vector<uint8_t>& wire_bytes) {
        static const uint8_t kZeros[3] = {0, 0, 0};

        // Make room at the front of the vector.
        const auto nested_size = wire_bytes.size();
        const auto num_bytes_to_add = nested_size < 128 ? 2 : 3;
        wire_bytes.insert(wire_bytes.begin(), &kZeros[0],
                          &kZeros[num_bytes_to_add]);

        // "Field 0, length-delimited."
        wire_bytes[0] = '\x0a';
        // Length varint.
        if (nested_size < 128) {
          wire_bytes[1] = static_cast<uint8_t>(nested_size);
        } else {
          ASSERT_LT(nested_size, 128u * 128u);
          wire_bytes[1] = static_cast<uint8_t>(0x80 | (nested_size & 0x7f));
          wire_bytes[2] = static_cast<uint8_t>(nested_size >> 7);
        }
      };

  const auto compute_parsed_nesting_depth = [](const Message& outermost) {
    int count = 0;
    for (auto* m = &outermost; m->next; m = m->next.get()) {
      ++count;
    }
    return count;
  };

  // The parse should succeed up to the maximum message depth limit.
  std::vector<uint8_t> wire_bytes;
  for (int i = 1; i <= kMaxMessageNestingDepth; ++i) {
    prepend_wire_bytes_to_nest_one_deeper(wire_bytes);
    Message message{};
    EXPECT_TRUE(codec::ParseFields(
        wire_bytes.data(), wire_bytes.data() + wire_bytes.size(), 0, message));

    // Confirm the parse built-up a chain of exactly |i| nested messages.
    EXPECT_EQ(i, compute_parsed_nesting_depth(message));
  }

  // If the wire bytes encode a chain that exceeds the maximum message depth
  // limit, the parse should fail.
  for (int i = 0; i < 3; ++i) {
    prepend_wire_bytes_to_nest_one_deeper(wire_bytes);
    Message message{};
    EXPECT_FALSE(codec::ParseFields(
        wire_bytes.data(), wire_bytes.data() + wire_bytes.size(), 0, message));
  }
}

TEST(ParseTest, BackwardsCompatibility_FieldAdded) {
  struct Message_V1 {
    int an_int;

    using ProtobufFields = FieldList<Field<&Message_V1::an_int, 1>>;
  };

  struct Message_V2 : public Message_V1 {
    std::optional<std::string> a_string;

    using ProtobufFields = FieldList<Field<&Message_V1::an_int, 1>,
                                     Field<&Message_V2::a_string, 2>>;
  };

  // clang-format off
  constexpr std::string_view v1_wire_bytes(
      // "Total size of message fields is 2 bytes".
      "\x02"
      // "Field 1, varint" "16"
      "\x08" "\x10",
      3);

  constexpr std::string_view v2_wire_bytes(
      // "Total size of message fields is 7 bytes".
      "\x07"
      // "Field 1, varint" "1"
      "\x08" "\x01"
      // "Field 2, length-delimited" "abc"
      "\x12" "\x03" "abc",
      8);
  // clang-format on

  // Parse version 1 wire bytes into version 1 message structure.
  Message_V1 message_v1_1{};
  auto* buffer = reinterpret_cast<const uint8_t*>(v1_wire_bytes.data());
  auto* after_it =
      ParseValue(buffer, buffer + v1_wire_bytes.size(), 0, message_v1_1);
  EXPECT_EQ(after_it, buffer + v1_wire_bytes.size());
  EXPECT_EQ(16, message_v1_1.an_int);

  // Parse version 1 wire bytes into version 2 message structure.
  Message_V2 message_v2_1{};
  buffer = reinterpret_cast<const uint8_t*>(v1_wire_bytes.data());
  after_it = ParseValue(buffer, buffer + v1_wire_bytes.size(), 0, message_v2_1);
  EXPECT_EQ(after_it, buffer + v1_wire_bytes.size());
  EXPECT_EQ(16, message_v2_1.an_int);
  EXPECT_FALSE(message_v2_1.a_string.has_value());

  // Parse version 2 wire bytes into version 1 message structure.
  Message_V1 message_v1_2{};
  buffer = reinterpret_cast<const uint8_t*>(v2_wire_bytes.data());
  after_it = ParseValue(buffer, buffer + v2_wire_bytes.size(), 0, message_v1_2);
  EXPECT_EQ(after_it, buffer + v2_wire_bytes.size());
  EXPECT_EQ(1, message_v1_2.an_int);

  // Parse version 2 wire bytes into version 2 message structure.
  Message_V2 message_v2_2{};
  buffer = reinterpret_cast<const uint8_t*>(v2_wire_bytes.data());
  after_it = ParseValue(buffer, buffer + v2_wire_bytes.size(), 0, message_v2_2);
  EXPECT_EQ(after_it, buffer + v2_wire_bytes.size());
  EXPECT_EQ(1, message_v2_2.an_int);
  EXPECT_TRUE(message_v2_2.a_string.has_value());
  EXPECT_EQ("abc", message_v2_2.a_string.value_or(""));
}

TEST(ParseTest, BackwardsCompatibility_FieldDeleted) {
  // Technically, this is already being tested in
  // ParseTest.BackwardsCompatibility_FieldAdded, where the V2 wire bytes are
  // being parsed into a Message_V1 structure. However, code is documentation,
  // and so I make it easy on the reader's brain to show it explicitly:

  struct Message_V1 {
    std::optional<int> an_int;
    std::string a_string;

    using ProtobufFields = FieldList<Field<&Message_V1::an_int, 1>,
                                     Field<&Message_V1::a_string, 2>>;
  };

  struct Message_V2 {
    std::string a_string;

    using ProtobufFields = FieldList<Field<&Message_V2::a_string, 2>>;
  };

  // clang-format off
  constexpr std::string_view v1_wire_bytes(
      // "Total size of message fields is 7 bytes".
      "\x07"
      // "Field 1, varint" "1"
      "\x08" "\x01"
      // "Field 2, length-delimited" "abc"
      "\x12" "\x03" "abc",
      8);

  constexpr std::string_view v2_wire_bytes(
      // "Total size of message fields is 2 bytes".
      "\x05"
      // "Field 2, length-delimited" "def"
      "\x12" "\x03" "def",
      6);
  // clang-format on

  // Parse version 1 wire bytes into version 1 message structure.
  Message_V1 message_v1_1{};
  auto* buffer = reinterpret_cast<const uint8_t*>(v1_wire_bytes.data());
  auto* after_it =
      ParseValue(buffer, buffer + v1_wire_bytes.size(), 0, message_v1_1);
  EXPECT_EQ(after_it, buffer + v1_wire_bytes.size());
  EXPECT_TRUE(message_v1_1.an_int.has_value());
  EXPECT_EQ(1, message_v1_1.an_int.value_or(-999));
  EXPECT_EQ("abc", message_v1_1.a_string);

  // Parse version 1 wire bytes into version 2 message structure.
  Message_V2 message_v2_1{};
  buffer = reinterpret_cast<const uint8_t*>(v1_wire_bytes.data());
  after_it = ParseValue(buffer, buffer + v1_wire_bytes.size(), 0, message_v2_1);
  EXPECT_EQ(after_it, buffer + v1_wire_bytes.size());
  EXPECT_EQ("abc", message_v2_1.a_string);

  // Parse version 2 wire bytes into version 1 message structure.
  Message_V1 message_v1_2{};
  buffer = reinterpret_cast<const uint8_t*>(v2_wire_bytes.data());
  after_it = ParseValue(buffer, buffer + v2_wire_bytes.size(), 0, message_v1_2);
  EXPECT_EQ(after_it, buffer + v2_wire_bytes.size());
  EXPECT_FALSE(message_v1_2.an_int.has_value());
  EXPECT_EQ("def", message_v1_2.a_string);

  // Parse version 2 wire bytes into version 2 message structure.
  Message_V2 message_v2_2{};
  buffer = reinterpret_cast<const uint8_t*>(v2_wire_bytes.data());
  after_it = ParseValue(buffer, buffer + v2_wire_bytes.size(), 0, message_v2_2);
  EXPECT_EQ(after_it, buffer + v2_wire_bytes.size());
  EXPECT_EQ("def", message_v2_2.a_string);
}

TEST(ParseTest, BackwardsCompatibility_OptionalBecomesRepeatable) {
  // Note: Google's public documentation makes it clear that the "repeated
  // field" to "optional field" backwards-compatibility does NOT hold for scalar
  // numeric types. It only works for string/bytes, and messages.

  struct Message_V1 {
    std::optional<std::string> optional_string;

    using ProtobufFields = FieldList<Field<&Message_V1::optional_string, 1>>;
  };

  struct Message_V2 {
    std::vector<std::string> strings;

    using ProtobufFields = FieldList<Field<&Message_V2::strings, 1>>;
  };

  // clang-format off
  constexpr std::string_view v1_wire_bytes(
      // "Total size of message fields is 5 bytes".
      "\x05"
      // "Field 1, length-delimited" "abc"
      "\x0a" "\x03" "abc",
      6);

  constexpr std::string_view v2_wire_bytes(
      // "Total size of message fields is 15 bytes".
      "\x0f"
      // "Field 1, length-delimited" "abc"
      "\x0a" "\x03" "abc"
      // "Field 1, length-delimited" "def"
      "\x0a" "\x03" "def"
      // "Field 1, length-delimited" "ghi"
      "\x0a" "\x03" "ghi",
      16);
  // clang-format on

  // Parse version 1 wire bytes into version 1 message structure.
  Message_V1 message_v1_1{};
  auto* buffer = reinterpret_cast<const uint8_t*>(v1_wire_bytes.data());
  auto* after_it =
      ParseValue(buffer, buffer + v1_wire_bytes.size(), 0, message_v1_1);
  EXPECT_EQ(after_it, buffer + v1_wire_bytes.size());
  EXPECT_TRUE(message_v1_1.optional_string.has_value());
  EXPECT_EQ("abc", message_v1_1.optional_string.value_or(""));

  // Parse version 1 wire bytes into version 2 message structure.
  Message_V2 message_v2_1{};
  buffer = reinterpret_cast<const uint8_t*>(v1_wire_bytes.data());
  after_it = ParseValue(buffer, buffer + v1_wire_bytes.size(), 0, message_v2_1);
  EXPECT_EQ(after_it, buffer + v1_wire_bytes.size());
  ASSERT_EQ(1u, message_v2_1.strings.size());
  EXPECT_EQ("abc", message_v2_1.strings[0]);

  // Parse version 2 wire bytes into version 1 message structure.
  Message_V1 message_v1_2{};
  buffer = reinterpret_cast<const uint8_t*>(v2_wire_bytes.data());
  after_it = ParseValue(buffer, buffer + v2_wire_bytes.size(), 0, message_v1_2);
  EXPECT_EQ(after_it, buffer + v2_wire_bytes.size());
  EXPECT_TRUE(message_v1_2.optional_string.has_value());
  EXPECT_EQ("ghi", message_v1_2.optional_string.value_or(""));

  // Parse version 2 wire bytes into version 2 message structure.
  Message_V2 message_v2_2{};
  buffer = reinterpret_cast<const uint8_t*>(v2_wire_bytes.data());
  after_it = ParseValue(buffer, buffer + v2_wire_bytes.size(), 0, message_v2_2);
  EXPECT_EQ(after_it, buffer + v2_wire_bytes.size());
  ASSERT_EQ(3u, message_v2_2.strings.size());
  EXPECT_EQ("abc", message_v2_2.strings[0]);
  EXPECT_EQ("def", message_v2_2.strings[1]);
  EXPECT_EQ("ghi", message_v2_2.strings[2]);
}

}  // namespace
}  // namespace pb::codec
