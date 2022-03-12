// Copyright 2022 Yuri Wiitala. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <sstream>
#include <string>

#include "gtest/gtest.h"
#include "pb/field_list.h"
#include "pb/inspection.h"
#include "pb/serialize.h"

namespace pb {
namespace {

TEST(InspectionTest, InspectionRenderingContext_ComputesRowOffset) {
  uint8_t buffer[256]{};
  InspectionRenderingContext context(&buffer[0], 256, 16);
  for (int i = 0; i < 16; ++i) {
    EXPECT_EQ(0, context.ComputeRowIndexOffset(&buffer[i]));
  }
  for (int i = 16; i < 32; ++i) {
    EXPECT_EQ(16, context.ComputeRowIndexOffset(&buffer[i]));
  }
  for (int i = 32; i < 48; ++i) {
    EXPECT_EQ(32, context.ComputeRowIndexOffset(&buffer[i]));
  }
}

// Helper to test the private ByteSpan::ComputeUtf8CharacterCount() method.
std::ptrdiff_t ComputeUtf8CharacterCount(const char* begin, const char* end) {
  auto* const ubegin = reinterpret_cast<const uint8_t*>(begin);
  auto* const uend = reinterpret_cast<const uint8_t*>(end);
  return BytesSpan(ubegin, uend, 0, ubegin).utf8_char_count();
}

TEST(InspectionTest, BytesSpan_ComputesUtf8CharacterCount) {
  static const char kEmpty[] = "";
  EXPECT_EQ(0, ComputeUtf8CharacterCount(&kEmpty[0], &kEmpty[0]));

  // All 7-bit ASCII chars are valid.
  for (int i = 0; i < 0x7F; ++i) {
    const char str[] = {static_cast<char>(i)};
    EXPECT_EQ(1, ComputeUtf8CharacterCount(std::begin(str), std::end(str)));
  }

  // Reject any strings with characters having an invalid first byte.
  for (int i = 0x80; i <= 0xC1; ++i) {
    const char str1[] = {static_cast<char>(i)};
    EXPECT_GT(0, ComputeUtf8CharacterCount(std::begin(str1), std::end(str1)));
    const char str2[] = {'A', static_cast<char>(i)};
    EXPECT_GT(0, ComputeUtf8CharacterCount(std::begin(str2), std::end(str2)));
  }

  static const char kValidTwoByteChar[] = {'\xC2', '\x80'};
  EXPECT_EQ(1, ComputeUtf8CharacterCount(std::begin(kValidTwoByteChar),
                                         std::end(kValidTwoByteChar)));

  static const char kInvalidFourByteChar[] = {'\xF5', '\x80', '\x80', '\x8f'};
  EXPECT_GT(0, ComputeUtf8CharacterCount(std::begin(kInvalidFourByteChar),
                                         std::end(kInvalidFourByteChar)));

  // TODO: This test is not comprehensive of all edge cases. Add them.
}

// Helper to call PrintInspection() on the given |spans| and return the result
// as a std::string.
std::string MakePrintedInspection(
    const std::vector<std::unique_ptr<WireSpan>>& spans,
    std::ptrdiff_t max_bytes = 2048) {
  std::ostringstream oss;
  auto* const offset_zero =
      (spans.empty() || !spans[0]) ? nullptr : spans[0]->begin();
  InspectionRenderingContext(offset_zero, max_bytes, 16).Print(spans, oss);
  return std::move(oss).str();
}

// Helper to call MessageSpan::MakeInspection() on the given |message| and
// return the result as a std::string.
std::string MakePrintedInspection(const MessageSpan& message) {
  InspectionRenderingContext context(message.begin());
  std::string result;
  for (auto& line : message.MakeInspection(context)) {
    result += std::move(line);
    result.push_back('\n');
  }
  return result;
}

TEST(InspectionTest, EmptyString) {
  static const uint8_t kEmptyString[] = "";

  EXPECT_TRUE(
      ScanForMessageFields(kEmptyString, kEmptyString + 0, true).empty());
  EXPECT_TRUE(
      ScanForMessageFields(kEmptyString, kEmptyString + 0, false).empty());

  auto message = ParseProbableMessage(kEmptyString, kEmptyString + 0);
  ASSERT_TRUE(message);
  EXPECT_TRUE(message->fields().empty());

  EXPECT_TRUE(MakePrintedInspection({}).empty());
}

TEST(InspectionTest, GarbageString) {
  static const uint8_t kGarbage[] = "garbage";

  EXPECT_TRUE(ScanForMessageFields(kGarbage, kGarbage + sizeof(kGarbage), false)
                  .empty());
  EXPECT_FALSE(ParseProbableMessage(kGarbage, kGarbage + sizeof(kGarbage)));

  const auto spans =
      ScanForMessageFields(kGarbage, kGarbage + sizeof(kGarbage), true);
  ASSERT_EQ(1u, spans.size());
  EXPECT_EQ(kGarbage, spans[0]->begin());
  EXPECT_EQ(kGarbage + sizeof(kGarbage), spans[0]->end());
  EXPECT_EQ(
      "00000000  67 61 72 62 61 67 65 00                          garbage␀\n",
      MakePrintedInspection(spans));
}

struct Message {
  std::string a;
  double b;
  sint32_t c;

  using ProtobufFields = FieldList<Field<&Message::a, 1>,
                                   Field<&Message::b, 2>,
                                   Field<&Message::c, 3>>;
};

const Message kTestMessage1 = {
    .a = "The ratio of the circumference of a circle to its diameter is "
         "approximately:",
    .b = 3.141593,
    .c = -1,
};

const Message kTestMessage2 = {
    .a = "However, my favorite irrational number is approximately:",
    .b = 2.71828,
    .c = 42,
};

TEST(InspectionTest, OneMessage) {
  auto wire_size = ComputeSerializedSize(kTestMessage1);
  ASSERT_GT(wire_size, 0);
  std::vector<uint8_t> buffer(wire_size);
  Serialize(kTestMessage1, buffer.data());

  const auto parsed =
      ParseProbableMessage(buffer.data(), buffer.data() + buffer.size());
  ASSERT_TRUE(parsed);
  ASSERT_EQ(3u, parsed->fields().size());
  EXPECT_EQ(
      // clang-format off
      "00000000                                                   [0] = 89-byte message {\n"
      "00000000  0a 4c 54 68 65 20 72 61 74 69 6f 20 6f 66 20 74    ⦙ [1] = 76-char UTF-8: The ratio of t\n"
      "00000010  68 65 20 63 69 72 63 75 6d 66 65 72 65 6e 63 65    ⦙     he circumference\n"
      "00000020  20 6f 66 20 61 20 63 69 72 63 6c 65 20 74 6f 20    ⦙      of a circle to \n"
      "00000030  69 74 73 20 64 69 61 6d 65 74 65 72 20 69 73 20    ⦙     its diameter is \n"
      "00000040  61 70 70 72 6f 78 69 6d 61 74 65 6c 79 3a          ⦙     approximately:\n"
      "00000040                                            11 7f    ⦙ [2] = double{3.14159} | (s)fixed64{4614256657332092287}\n"
      "00000050  bd c2 82 fb 21 09 40                               ⦙ \n"
      "00000050                       18 01                         ⦙ [3] = (u)intXX{1} | sintXX{-1} | bool{true}\n"
      "00000050                                                   }\n",
      // clang-format on
      MakePrintedInspection(*parsed));
}

TEST(InspectionTest, OneMessageSurroundedByGarbage) {
  const auto message_wire_size = ComputeSerializedSize(kTestMessage1);
  ASSERT_GT(message_wire_size, 0);
  std::vector<uint8_t> buffer(message_wire_size + 33 + 50);
  std::fill(buffer.begin(), buffer.begin() + 33, '\x37');
  Serialize(kTestMessage1, buffer.data() + 33);
  std::fill(buffer.begin() + 33 + message_wire_size,
            buffer.begin() + 33 + message_wire_size + 50, '\xf7');

  const auto spans =
      ScanForMessageFields(buffer.data(), buffer.data() + buffer.size(), true);
  ASSERT_EQ(5u, spans.size());
  EXPECT_EQ(
      // clang-format off
      "00000000  37 37 37 37 37 37 37 37 37 37 37 37 37 37 37 37  7777777777777777\n"
      "00000010  37 37 37 37 37 37 37 37 37 37 37 37 37 37 37 37  7777777777777777\n"
      "00000020  37                                               7\n"
      "00000020     0a 4c 54 68 65 20 72 61 74 69 6f 20 6f 66 20  [1] = 76-char UTF-8: The ratio of \n"
      "00000030  74 68 65 20 63 69 72 63 75 6d 66 65 72 65 6e 63      the circumferenc\n"
      "00000040  65 20 6f 66 20 61 20 63 69 72 63 6c 65 20 74 6f      e of a circle to\n"
      "00000050  20 69 74 73 20 64 69 61 6d 65 74 65 72 20 69 73       its diameter is\n"
      "00000060  20 61 70 70 72 6f 78 69 6d 61 74 65 6c 79 3a          approximately:\n"
      "00000060                                               11  [2] = double{3.14159} | (s)fixed64{4614256657332092287}\n"
      "00000070  7f bd c2 82 fb 21 09 40                          \n"
      "00000070                          18 01                    [3] = (u)intXX{1} | sintXX{-1} | bool{true}\n"
      "00000070                                f7 f7 f7 f7 f7 f7  ≈≈≈≈≈≈\n"
      "00000080  f7 f7 f7 f7 f7 f7 f7 f7 f7 f7 f7 f7 f7 f7 f7 f7  ≈≈≈≈≈≈≈≈≈≈≈≈≈≈≈≈\n"
      "00000090  f7 f7 f7 f7 f7 f7 f7 f7 f7 f7 f7 f7 f7 f7 f7 f7  ≈≈≈≈≈≈≈≈≈≈≈≈≈≈≈≈\n"
      "000000a0  f7 f7 f7 f7 f7 f7 f7 f7 f7 f7 f7 f7              ≈≈≈≈≈≈≈≈≈≈≈≈\n",
      // clang-format on
      MakePrintedInspection(spans));
}

TEST(InspectionTest, TwoMessages) {
  auto message1_wire_size = ComputeSerializedSize(kTestMessage1);
  ASSERT_GT(message1_wire_size, 0);
  auto message2_wire_size = ComputeSerializedSize(kTestMessage2);
  ASSERT_GT(message2_wire_size, 0);
  std::vector<uint8_t> buffer(message1_wire_size + message2_wire_size);
  Serialize(kTestMessage1, buffer.data());
  Serialize(kTestMessage2, buffer.data() + message1_wire_size);

  const auto spans =
      ScanForMessageFields(buffer.data(), buffer.data() + buffer.size(), true);
  ASSERT_EQ(6u, spans.size());
  EXPECT_EQ(
      // clang-format off
      "00000000  0a 4c 54 68 65 20 72 61 74 69 6f 20 6f 66 20 74  [1] = 76-char UTF-8: The ratio of t\n"
      "00000010  68 65 20 63 69 72 63 75 6d 66 65 72 65 6e 63 65      he circumference\n"
      "00000020  20 6f 66 20 61 20 63 69 72 63 6c 65 20 74 6f 20       of a circle to \n"
      "00000030  69 74 73 20 64 69 61 6d 65 74 65 72 20 69 73 20      its diameter is \n"
      "00000040  61 70 70 72 6f 78 69 6d 61 74 65 6c 79 3a            approximately:\n"
      "00000040                                            11 7f  [2] = double{3.14159} | (s)fixed64{4614256657332092287}\n"
      "00000050  bd c2 82 fb 21 09 40                             \n"
      "00000050                       18 01                       [3] = (u)intXX{1} | sintXX{-1} | bool{true}\n"
      "00000050                             0a 38 48 6f 77 65 76  [1] = 56-char UTF-8: Howev\n"
      "00000060  65 72 2c 20 6d 79 20 66 61 76 6f 72 69 74 65 20      er, my favorite \n"
      "00000070  69 72 72 61 74 69 6f 6e 61 6c 20 6e 75 6d 62 65      irrational numbe\n"
      "00000080  72 20 69 73 20 61 70 70 72 6f 78 69 6d 61 74 65      r is approximate\n"
      "00000090  6c 79 3a                                             ly:\n"
      "00000090           11 90 f7 aa 95 09 bf 05 40              [2] = double{2.71828} | (s)fixed64{4613303441197561744}\n"
      "00000090                                      18 54        [3] = (u)intXX{84} | sintXX{42}\n",
      // clang-format on
      MakePrintedInspection(spans));
}

TEST(InspectionTest, OneMessageWithInspectionLimit) {
  auto wire_size = ComputeSerializedSize(kTestMessage1);
  ASSERT_GT(wire_size, 0);
  std::vector<uint8_t> buffer(wire_size);
  Serialize(kTestMessage1, buffer.data());

  const auto spans =
      ScanForMessageFields(buffer.data(), buffer.data() + buffer.size(), true);
  ASSERT_EQ(3u, spans.size());
  EXPECT_EQ(
      // clang-format off
      "00000000  0a 4c 54 68 65 20 72 61 74 69 6f 20 6f 66 20 74  [1] = 76-char UTF-8: The ratio of t\n"
      "00000010  68 65 20 63 69 72 63 75 6d 66 65 72 65 6e 63 65      he circumference…\n",
      // clang-format on
      MakePrintedInspection(spans, 30));
}

TEST(InspectionTest, NestedMessages) {
  struct OuterMessage {
    std::string a;
    Message b;
    int c;

    using ProtobufFields = FieldList<Field<&OuterMessage::a, 1>,
                                     Field<&OuterMessage::b, 2>,
                                     Field<&OuterMessage::c, 3>>;
  };

  const OuterMessage outer_message{
      .a = "a string",
      .b =
          Message{
              .a = "a string in the nested message",
              .b = -1.0,
              .c = 16,
          },
      .c = -999999,
  };

  auto wire_size = ComputeSerializedSize(outer_message);
  ASSERT_GT(wire_size, 0);
  std::vector<uint8_t> buffer(wire_size);
  Serialize(outer_message, buffer.data());

  const auto spans =
      ScanForMessageFields(buffer.data(), buffer.data() + buffer.size(), true);
  ASSERT_EQ(3u, spans.size());
  EXPECT_EQ(
      // clang-format off
      "00000000  0a 08 61 20 73 74 72 69 6e 67                    [1] = 8-char UTF-8: a string\n"
      "00000000                                12 2b              [2] = 43-byte message {\n"
      "00000000                                      0a 1e 61 20    ⦙ [1] = 30-char UTF-8: a \n"
      "00000010  73 74 72 69 6e 67 20 69 6e 20 74 68 65 20 6e 65    ⦙     string in the ne\n"
      "00000020  73 74 65 64 20 6d 65 73 73 61 67 65                ⦙     sted message\n"
      "00000020                                      11 00 00 00    ⦙ [2] = double{-1} | fixed64{13830554455654793216} | sfixed64{-4616189618054758400}\n"
      "00000030  00 00 00 f0 bf                                     ⦙ \n"
      "00000030                 18 20                               ⦙ [3] = (u)intXX{32} | sintXX{16}\n"
      "00000030                                                   }\n"
      "00000030                       18 c1 fb c2 ff ff ff ff ff  [3] = uintXX{18446744073708551617} | intXX{-999999} | sintXX{-9223372036854275809}\n"
      "00000040  ff 01                                            \n",
      // clang-format on
      MakePrintedInspection(spans));
}

}  // namespace
}  // namespace pb
