// Copyright 2022 Yuri Wiitala. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pb/inspection.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string_view>
#include <tuple>
#include <utility>

#include "pb/codec/limits.h"
#include "pb/codec/parse.h"
#include "pb/codec/tag.h"
#include "pb/codec/wire_type.h"
#include "pb/integer_wrapper.h"

namespace pb {

InspectionRenderingContext::InspectionRenderingContext(
    const uint8_t* buffer_begin,
    std::ptrdiff_t max_bytes,
    std::ptrdiff_t bytes_per_line)
    : offset_zero(buffer_begin),
      limit(buffer_begin + max_bytes),
      bytes_per_line(bytes_per_line) {
  assert(offset_zero <= limit);
  assert(bytes_per_line > 0);
}

// static
InspectionRenderingContext InspectionRenderingContext::MakeDefaultFor(
    const std::vector<std::unique_ptr<WireSpan>>& spans) {
  if (spans.empty() || !spans[0]) {
    return InspectionRenderingContext(nullptr);
  }
  return InspectionRenderingContext(spans[0]->begin());
}

std::ptrdiff_t InspectionRenderingContext::ComputeRowIndexOffset(
    const uint8_t* some_byte_in_the_row) const {
  const std::ptrdiff_t byte_offset = some_byte_in_the_row - offset_zero;
  assert(byte_offset >= 0);
  return (byte_offset / bytes_per_line) * bytes_per_line;
}

std::string InspectionRenderingContext::MakeHexDumpRow(
    std::ptrdiff_t row_offset,
    const uint8_t* begin,
    const uint8_t* end) const {
  assert(row_offset >= 0);
  assert(begin <= end);

  std::ostringstream oss;

  // Address offset column.
  oss << std::hex << std::setfill('0') << std::setw(8) << row_offset << ' ';

  // Hex byte value columns.
  //
  // Just print the hex representation of the bytes that are in the range
  // |begin| to |end|, and pad with spaces for any other bytes.
  auto* const row_begin = offset_zero + row_offset;
  for (std::ptrdiff_t i = 0; i < bytes_per_line; ++i) {
    oss << ' ';
    if (&row_begin[i] >= begin && &row_begin[i] < end) {
      oss << std::setw(2) << static_cast<uint16_t>(row_begin[i]);
    } else {
      oss << "  ";
    }
  }

  return std::move(oss).str();
}

std::vector<std::string> InspectionRenderingContext::MakeHexDumpRows(
    const uint8_t* begin,
    const uint8_t* end) const {
  assert(begin <= end);

  std::vector<std::string> lines;
  if (begin < offset_zero || offset_zero > end || limit <= begin) {
    return lines;
  }

  std::ptrdiff_t row_offset = ComputeRowIndexOffset(begin);
  const std::ptrdiff_t end_row_offset =
      ComputeRowIndexOffset(std::min(end, limit) - 1) + bytes_per_line;
  assert(end_row_offset >= row_offset);
  assert(bytes_per_line > 0);
  lines.reserve(
      static_cast<std::size_t>((end_row_offset - row_offset) / bytes_per_line));
  for (; row_offset != end_row_offset; row_offset += bytes_per_line) {
    lines.push_back(MakeHexDumpRow(row_offset, begin, end));
  }

  return lines;
}

WireSpan::WireSpan(const uint8_t* begin, const uint8_t* end)
    : begin_(begin), end_(end) {
  assert(begin_ <= end_);
}

WireSpan::~WireSpan() = default;

FieldSpan* WireSpan::AsFieldSpan() {
  return nullptr;
}

std::vector<std::string> WireSpan::MakeInspection(
    InspectionRenderingContext& context) const {
  auto lines = context.MakeHexDumpRows(begin_, end_);

  std::ostringstream oss;
  auto* row_begin =
      context.offset_zero + context.ComputeRowIndexOffset(begin());
  for (std::size_t i = 0, line_count = lines.size(); i != line_count;
       ++i, row_begin += context.bytes_per_line) {
    oss << context.indentation;
    PrintBytesForInspection(std::max(begin_, row_begin),
                            std::min(end_, row_begin + context.bytes_per_line),
                            oss);
    lines[i] += oss.str();
    oss.str(std::string{});
  }

  return lines;
}

// static
void WireSpan::PrintCodePage437CharForInspection(uint8_t ch,
                                                 std::ostream& utf8_out) {
  if (ch < 32) {
    static constexpr const char* kCodePage437LikeControlChars[32] = {
        "␀", "☺", "☻", "♥", "♦", "♣", "♠", "•", "◘", "○", "◙",
        "♂", "♀", "♪", "♫", "☼", "►", "◄", "↕", "‼", "¶", "§",
        "▬", "↨", "↑", "↓", "→", "←", "∟", "↔", "▲", "▼",
    };
    utf8_out << kCodePage437LikeControlChars[ch];
  } else if (ch < 127) {
    utf8_out << static_cast<char>(ch);
  } else {
    static constexpr const char* kCodePage437UpperChars[129] = {
        "⌂", "Ç", "ü", "é", "â", "ä", "à", "å", "ç", "ê", "ë", "è", "ï",
        "î", "ì", "Ä", "Å", "É", "æ", "Æ", "ô", "ö", "ò", "û", "ù", "ÿ",
        "Ö", "Ü", "¢", "£", "¥", "₧", "ƒ", "á", "í", "ó", "ú", "ñ", "Ñ",
        "ª", "º", "¿", "⌐", "¬", "½", "¼", "¡", "«", "»", "░", "▒", "▓",
        "│", "┤", "╡", "╢", "╖", "╕", "╣", "║", "╗", "╝", "╜", "╛", "┐",
        "└", "┴", "┬", "├", "─", "┼", "╞", "╟", "╚", "╔", "╩", "╦", "╠",
        "═", "╬", "╧", "╨", "╤", "╥", "╙", "╘", "╒", "╓", "╫", "╪", "┘",
        "┌", "█", "▄", "▌", "▐", "▀", "α", "ß", "Γ", "π", "Σ", "σ", "µ",
        "τ", "Φ", "Θ", "Ω", "δ", "∞", "φ", "ε", "∩", "≡", "±", "≥", "≤",
        "⌠", "⌡", "÷", "≈", "°", "∙", "·", "√", "ⁿ", "²", "■", " ",
    };
    utf8_out << kCodePage437UpperChars[ch - 127];
  }
}

// static
void WireSpan::PrintBytesForInspection(const uint8_t* begin,
                                       const uint8_t* end,
                                       std::ostream& utf8_out) {
  assert(begin <= end);
  for (auto* p = begin; p < end; ++p) {
    PrintCodePage437CharForInspection(*p, utf8_out);
  }
}

FieldSpan::FieldSpan(const uint8_t* begin,
                     const uint8_t* end,
                     int32_t field_number)
    : WireSpan(begin, end), field_number_(field_number) {}

FieldSpan::~FieldSpan() = default;

FieldSpan* FieldSpan::AsFieldSpan() {
  return this;
}

BytesSpan* FieldSpan::AsBytesSpan() {
  return nullptr;
}

MessageSpan* FieldSpan::AsMessageSpan() {
  return nullptr;
}

void FieldSpan::PrintInterpretationLeftHandSide(std::ostream& utf8_out) const {
  utf8_out << '[' << std::dec << field_number_ << "] = ";
}

VarintSpan::VarintSpan(const uint8_t* begin,
                       const uint8_t* end,
                       int32_t field_number,
                       uint64_t value)
    : FieldSpan(begin, end, field_number), value_(value) {}

VarintSpan::~VarintSpan() = default;

std::vector<std::string> VarintSpan::MakeInspection(
    InspectionRenderingContext& context) const {
  auto lines = context.MakeHexDumpRows(begin(), end());
  if (lines.empty()) {
    return lines;
  }

  std::ostringstream oss;
  oss << context.indentation;
  PrintInterpretationLeftHandSide(oss);
  if (as_signed() >= 0) {
    oss << "(u)intXX{" << as_unsigned();
  } else {
    oss << "uintXX{" << as_unsigned() << "} | intXX{" << as_signed();
  }
  oss << "} | sintXX{" << as_zigzag().value() << '}';
  if (value_ == 0 || value_ == 1) {
    oss << " | bool{" << (!!value_) << '}';
  }
  lines[0] += std::move(oss).str();

  for (std::size_t i = 1, line_count = lines.size(); i != line_count; ++i) {
    lines[i] += context.indentation;
  }

  return lines;
}

BytesSpan::BytesSpan(const uint8_t* begin,
                     const uint8_t* end,
                     int32_t field_number,
                     const uint8_t* bytes_begin)
    : FieldSpan(begin, end, field_number),
      bytes_begin_(bytes_begin),
      utf8_char_count_(ComputeUtf8CharacterCount(bytes_begin_, end)) {
  assert(bytes_begin_ >= begin);
  assert(bytes_begin_ <= end);
}

BytesSpan::~BytesSpan() = default;

std::string BytesSpan::MakeString() const {
  return std::string(bytes_begin(), bytes_end());
}

std::vector<std::string> BytesSpan::MakeInspection(
    InspectionRenderingContext& context) const {
  auto lines = context.MakeHexDumpRows(begin(), end());
  if (lines.empty()) {
    return lines;
  }

  std::ostringstream oss;
  oss << context.indentation;
  PrintInterpretationLeftHandSide(oss);
  if (is_utf8_string()) {
    oss << utf8_char_count_ << "-char UTF-8: ";
  } else {
    const auto byte_count = end() - bytes_begin_;
    oss << byte_count << " byte(s): ";
  }
  lines[0] += oss.str();
  oss.str(std::string{});

  static constexpr std::string_view kContinuationIndentation = "    ";
  context.indentation += kContinuationIndentation;

  auto* row_begin =
      context.offset_zero + context.ComputeRowIndexOffset(begin());
  for (std::size_t i = 0, line_count = lines.size(); i != line_count;
       ++i, row_begin += context.bytes_per_line) {
    if (i != 0) {
      lines[i] += context.indentation;
    }

    auto* const row_end = row_begin + context.bytes_per_line;
    if (bytes_begin_ < row_end) {
      auto* const within_begin = std::max(bytes_begin_, row_begin);
      auto* const within_end = std::min(end(), row_end);
      if (is_utf8_string()) {
        PrintUtf8ForInspection_Unsafe(within_begin, within_end, oss);
      } else {
        PrintBytesForInspection(within_begin, within_end, oss);
      }
      lines[i] += oss.str();
      oss.str(std::string{});
    }
  }

  context.indentation.erase(context.indentation.size() -
                            kContinuationIndentation.size());

  if ((end() > context.limit) &&
      (context.ComputeRowIndexOffset(end()) !=
       context.ComputeRowIndexOffset(context.limit))) {
    lines.back() += "…";
  }

  return lines;
}

BytesSpan* BytesSpan::AsBytesSpan() {
  return this;
}

// static
std::ptrdiff_t BytesSpan::ComputeUtf8CharacterCount(const uint8_t* begin,
                                                    const uint8_t* end) {
  // Start with the assumption that the number of chars is the same as the
  // number of bytes, and subtract from this whenever multi-byte sequences are
  // encountered.
  std::ptrdiff_t char_count = end - begin;

  // Reference: https://en.wikipedia.org/wiki/UTF-8#Codepage_layout
  for (auto* p = begin; p < end; ++p) {
    if ((p[0] >> 7) == 0) {  // Single-byte char.
      continue;
    }

    // 80 through C1 and F5 through FF are illegal for the first byte in a
    // multi-byte sequence, for various different reasons.
    if ((*p >= 0x80 && *p <= 0xC1) || (*p >= 0xF5)) {
      return -char_count;
    }

    if ((*p >> 5) == 0b110) {  // U+0080 to U+07FF.
      // No need to decode and sanity-check the value for two-byte sequences
      // since:
      //
      //   1. The first byte of this sequence is at least C2 and C2 00 will
      //      decode to U+0080.
      //   2. The first byte of this sequence is no larger than DF and DF BF
      //      will decode to U+07FF.
      ++p;
      // The 2nd byte must start with the bits 10.
      if (p == end || (*p >> 6) != 0b10) {
        return -char_count;
      }
      --char_count;
    } else if ((*p >> 4) == 0b1110) {  // U+0800 to U+FFFF (mostly).
      uint16_t value = static_cast<uint16_t>(*p << 12);
      ++p;
      if (p == end || (*p >> 6) != 0b10) {
        return -char_count;
      }
      value |= static_cast<uint16_t>(*p & 0b00111111) << 6;
      ++p;
      if (p == end || (*p >> 6) != 0b10) {
        return -char_count;
      }
      value |= *p & 0b00111111;
      // 3-byte sequences are not allowed to encode any value that can be
      // encoded in fewer bytes. Also, U+D800 through U+DFFF are illegal code
      // points.
      if (value < 0x800 || (value >= 0xD800 && value <= 0xDFFF)) {
        return -char_count;
      }
      char_count -= 2;
    } else if ((*p >> 3) == 0b11110) {  // U+10000 to U+10FFFF.
      uint32_t value = static_cast<uint32_t>(*p) << 18;
      ++p;
      if (p == end || (*p >> 6) != 0b10) {
        return -char_count;
      }
      value |= static_cast<uint32_t>(*p & 0b00111111) << 12;
      ++p;
      if (p == end || (*p >> 6) != 0b10) {
        return -char_count;
      }
      value |= static_cast<uint32_t>(*p & 0b00111111) << 6;
      ++p;
      if (p == end || (*p >> 6) != 0b10) {
        return -char_count;
      }
      value |= *p & 0b00111111;
      // 4-byte sequences are not allowed to encode any value that can be
      // encoded in fewer bytes. Also, anything greater than U+10FFFF is an
      // illegal code point.
      if (value < 0x10000 || value > 0x10FFFF) {
        return -char_count;
      }
      char_count -= 3;
    }
  }

  return char_count;
}

// static
void BytesSpan::PrintUtf8ForInspection_Unsafe(const uint8_t* begin,
                                              const uint8_t* soft_end,
                                              std::ostream& utf8_out) {
  for (auto* p = begin; p < soft_end; ++p) {
    if ((p[0] >> 7) == 0) {  // Single-byte char.
      PrintCodePage437CharForInspection(p[0], utf8_out);
    } else if ((p[0] >> 6) == 0b11) {  // Multi-byte char.
      // Special case: U+0080 through U+009F are Unicode C1 control codes. Don't
      // output them directly!
      if ((p[0] == 0b11000010) && ((p[1] >> 5) == 0b100)) {
        PrintCodePage437CharForInspection(0x80 | (p[1] & 0x1F), utf8_out);
      } else {
        utf8_out << static_cast<char>(p[0]) << static_cast<char>(p[1]);
        if ((p[0] >> 5) == 0b111) {
          utf8_out << static_cast<char>(p[2]);
          if ((p[0] >> 4) == 0b1111) {
            utf8_out << static_cast<char>(p[3]);
            ++p;
          }
          ++p;
        }
      }
      ++p;
    } else {
      // Skip, if the loop started in the middle of a UTF-8 multi-byte sequence.
    }
  }
}

MessageSpan::MessageSpan(const uint8_t* begin,
                         const uint8_t* end,
                         int32_t field_number,
                         std::vector<std::unique_ptr<FieldSpan>> message_fields)
    : FieldSpan(begin, end, field_number), fields_(std::move(message_fields)) {}

MessageSpan::~MessageSpan() = default;

std::vector<std::string> MessageSpan::MakeInspection(
    InspectionRenderingContext& context) const {
  std::vector<std::string> lines;

  auto* const length_varint_end =
      fields_.empty() ? end() : fields_.front()->begin();
  lines.push_back(
      context.MakeHexDumpRow(context.ComputeRowIndexOffset(begin()), begin(),
                             std::min(length_varint_end, context.limit)));

  {
    std::ostringstream oss;
    oss << context.indentation;
    PrintInterpretationLeftHandSide(oss);
    std::ptrdiff_t message_size = 0;
    if (!fields_.empty()) {
      message_size = fields_.back()->end() - fields_.front()->begin();
    }
    oss << message_size << "-byte message {";
    lines[0] += std::move(oss).str();
  }

  static constexpr std::string_view kIndentationWithFence = "  ⦙ ";
  context.indentation += kIndentationWithFence;
  bool message_dump_is_incomplete = false;
  for (const auto& field : fields_) {
    if (field->begin() >= context.limit) {
      message_dump_is_incomplete = true;
      break;
    }
    for (auto& line : field->MakeInspection(context)) {
      lines.push_back(std::move(line));
    }
  }
  context.indentation.erase(context.indentation.size() -
                            kIndentationWithFence.size());

  lines.push_back(context.MakeHexDumpRow(context.ComputeRowIndexOffset(end()),
                                         end(), end()));
  {
    std::ostringstream oss;
    oss << context.indentation;
    if (message_dump_is_incomplete) {
      oss << "…";
    }
    oss << '}';
    lines.back() += std::move(oss).str();
  }

  return lines;
}

MessageSpan* MessageSpan::AsMessageSpan() {
  return this;
}

Fixed64::Fixed64(const uint8_t* begin,
                 const uint8_t* end,
                 int32_t field_number,
                 fixed64_t value)
    : FieldSpan(begin, end, field_number), value_(value) {}

Fixed64::~Fixed64() = default;

double Fixed64::AsDouble() const {
  static_assert(sizeof(double) == sizeof(uint64_t));
  static_assert(std::numeric_limits<double>::is_iec559);
  double as_double;
  std::memcpy(&as_double, &static_cast<const uint64_t&>(value_),
              sizeof(double));
  return as_double;
}

::pb::fixed64_t Fixed64::AsFixed64() const {
  return value_;
}

::pb::sfixed64_t Fixed64::AsSignedFixed64() const {
  const ::pb::sfixed64_t wrapped = static_cast<int64_t>(value_.value());
  return wrapped;
}

std::vector<std::string> Fixed64::MakeInspection(
    InspectionRenderingContext& context) const {
  auto lines = context.MakeHexDumpRows(begin(), end());
  if (lines.empty()) {
    return lines;
  }

  std::ostringstream oss;
  oss << context.indentation;
  PrintInterpretationLeftHandSide(oss);
  oss << "double{" << AsDouble();
  if (AsSignedFixed64() >= 0) {
    oss << "} | (s)fixed64{" << value_;
  } else {
    oss << "} | fixed64{" << value_ << "} | sfixed64{"
        << static_cast<int64_t>(value_.value());
  }
  oss << '}';
  lines[0] += std::move(oss).str();

  for (std::size_t i = 1, line_count = lines.size(); i != line_count; ++i) {
    lines[i] += context.indentation;
  }

  return lines;
}

Fixed32::Fixed32(const uint8_t* begin,
                 const uint8_t* end,
                 int32_t field_number,
                 fixed32_t value)
    : FieldSpan(begin, end, field_number), value_(value) {}

Fixed32::~Fixed32() = default;

float Fixed32::AsFloat() const {
  static_assert(sizeof(float) == sizeof(uint32_t));
  static_assert(std::numeric_limits<float>::is_iec559);
  float as_float;
  std::memcpy(&as_float, &static_cast<const uint32_t&>(value_), sizeof(float));
  return as_float;
}

::pb::fixed32_t Fixed32::AsFixed32() const {
  return value_;
}

::pb::sfixed32_t Fixed32::AsSignedFixed32() const {
  const ::pb::sfixed32_t wrapped = static_cast<int32_t>(value_.value());
  return wrapped;
}

std::vector<std::string> Fixed32::MakeInspection(
    InspectionRenderingContext& context) const {
  auto lines = context.MakeHexDumpRows(begin(), end());
  if (lines.empty()) {
    return lines;
  }

  std::ostringstream oss;
  oss << context.indentation;
  PrintInterpretationLeftHandSide(oss);
  oss << "float{" << AsFloat();
  if (AsSignedFixed32() >= 0) {
    oss << "} | (s)fixed32{" << value_;
  } else {
    oss << "} | fixed32{" << value_ << "} | sfixed32{"
        << static_cast<int32_t>(value_.value());
  }
  oss << '}';
  lines[0] += std::move(oss).str();

  for (std::size_t i = 1, line_count = lines.size(); i != line_count; ++i) {
    lines[i] += context.indentation;
  }

  return lines;
}

namespace {

// Parses a Tag that starts at |begin| and ends at or before |end|. If a
// valid-looking Tag can be parsed, a pointer to its end is returned along with
// the parsed Tag value. Otherwise, null is returned.
std::tuple<const uint8_t*, codec::Tag> MaybeParseTag(const uint8_t* begin,
                                                     const uint8_t* end) {
  assert(begin <= end);

  codec::Tag tag;
  auto* const tag_end = codec::ParseValue(begin, end, -1, tag);
  static constexpr std::ptrdiff_t kMaxPossibleTagSize =
      (std::numeric_limits<codec::Tag>::digits + 6) / 7;
  if (tag_end && (tag_end - begin) <= kMaxPossibleTagSize &&
      codec::IsValidFieldNumber(codec::GetFieldNumberFromTag(tag))) {
    const auto wire_type = codec::GetWireTypeFromTag(tag);
    if (wire_type == codec::WireType::kVarint ||
        wire_type == codec::WireType::kFixed64Bit ||
        wire_type == codec::WireType::kLengthDelimited ||
        wire_type == codec::WireType::kFixed32Bit) {
      return {tag_end, tag};
    }
  }
  return {nullptr, {}};
}

// Scans the buffer from |begin| to |end|, and returns the [begin, end, value]
// of the first valid-looking Tag that is encountered. If no valid-looking Tags
// are found, nulls are returned.
std::tuple<const uint8_t*, const uint8_t*, codec::Tag> FindNextValidTag(
    const uint8_t* begin,
    const uint8_t* end) {
  assert(begin <= end);

  for (auto* tag_begin = begin; tag_begin != end; ++tag_begin) {
    const auto& [tag_end, tag] = MaybeParseTag(tag_begin, end);
    if (tag_end) {
      return {tag_begin, tag_end, tag};
    }
  }
  return {nullptr, nullptr, {}};
}

// Downcasts each WireSpan in |spans| to a FieldSpan, and returns them in a
// vector.
std::vector<std::unique_ptr<FieldSpan>> AsVectorOfFieldSpans(
    std::vector<std::unique_ptr<WireSpan>> spans) {
  std::vector<std::unique_ptr<FieldSpan>> fields(spans.size());
  for (std::size_t i = 0, size = fields.size(); i != size; ++i) {
    WireSpan* const span = spans[i].release();
    assert(span);
    FieldSpan* const field = span->AsFieldSpan();
    assert(field);
    fields[i].reset(field);
  }
  return fields;
}

// If the last element of |spans| is a WireSpan (not a subclass) that overlaps
// with the range |begin| to |end|, it is extended to encompass that range.
// Otherwise, a new WireSpan is appended to the end of the vector.
void MergeOrAppendSpanAtEnd(const uint8_t* begin,
                            const uint8_t* end,
                            std::vector<std::unique_ptr<WireSpan>>& spans) {
  assert(begin <= end);

  if (!spans.empty()) {
    WireSpan& span = *spans.back();
    if (!span.AsFieldSpan() && begin <= span.end() && begin >= span.begin() &&
        end > span.end()) {
      begin = span.begin();
      spans.back() = std::make_unique<WireSpan>(begin, end);
      return;
    }
  }
  spans.push_back(std::make_unique<WireSpan>(begin, end));
}

std::vector<std::unique_ptr<WireSpan>> ScanForMessageFieldsRecursively(
    const uint8_t* begin,
    const uint8_t* end,
    int nesting_level,
    bool permissive) {
  // TODO: There's a lot of opportunity to improve the detection heuristics
  // here. For example, maybe the linear scan should be replaced with a dynamic
  // programming problem, where the simplest valid message structure is
  // produced. Also, do something about common text strings being interpreted as
  // message structure just because the bytes *could* be a valid Message.

  std::vector<std::unique_ptr<WireSpan>> spans;

  while (begin < end) {
    const uint8_t* tag_begin;
    const uint8_t* tag_end;
    codec::Tag tag;
    if (permissive) {
      std::tie(tag_begin, tag_end, tag) = FindNextValidTag(begin, end);
      if (!tag_end) {
        MergeOrAppendSpanAtEnd(begin, end, spans);
        break;
      }
      if (tag_begin > begin) {
        MergeOrAppendSpanAtEnd(begin, tag_begin, spans);
      }
    } else {
      std::tie(tag_end, tag) = MaybeParseTag(begin, end);
      if (!tag_end) {
        return {};
      }
      tag_begin = begin;
    }

    switch (codec::GetWireTypeFromTag(tag)) {
      case codec::WireType::kVarint: {
        uint64_t value;
        auto* const value_end =
            codec::ParseValue(tag_end, end, nesting_level, value);
        static constexpr std::ptrdiff_t kMaxPossibleVarintSize =
            (std::numeric_limits<uint64_t>::digits + 6) / 7;
        if (value_end && (value_end - tag_end) <= kMaxPossibleVarintSize) {
          spans.push_back(std::make_unique<VarintSpan>(
              tag_begin, value_end, codec::GetFieldNumberFromTag(tag), value));
          begin = value_end;
        } else if (!permissive) {
          return {};
        } else {
          MergeOrAppendSpanAtEnd(tag_begin, tag_begin + 1, spans);
          begin = tag_begin + 1;
        }
        break;
      }

      case codec::WireType::kLengthDelimited: {
        int64_t size;
        auto* const size_end =
            codec::ParseValue(tag_end, end, nesting_level, size);
        static constexpr std::ptrdiff_t kMaxPossibleLengthSize =
            (std::numeric_limits<uint32_t>::digits + 6) / 7;
        if (size_end && (size_end - tag_end) <= kMaxPossibleLengthSize &&
            size >= 0 && size <= (end - size_end)) {
          auto* const value_end = size_end + size;

          // Try to parse exactly as a Message. If that fails, assume this field
          // is a string. Unfortunately, there is no reliable way to detect
          // these bytes as a packed-repeated scalar field.
          std::vector<std::unique_ptr<WireSpan>> nested_spans;
          if (nesting_level < codec::kMaxMessageNestingDepth) {
            nested_spans = ScanForMessageFieldsRecursively(
                size_end, value_end, nesting_level + 1, false);
          }

          if (nested_spans.empty()) {
            spans.push_back(std::make_unique<BytesSpan>(
                tag_begin, value_end, codec::GetFieldNumberFromTag(tag),
                size_end));
          } else {
            spans.push_back(std::make_unique<MessageSpan>(
                tag_begin, value_end, codec::GetFieldNumberFromTag(tag),
                AsVectorOfFieldSpans(std::move(nested_spans))));
          }
          begin = value_end;
        } else if (!permissive) {
          return {};
        } else {
          MergeOrAppendSpanAtEnd(tag_begin, tag_begin + 1, spans);
          begin = tag_begin + 1;
        }
        break;
      }

      case codec::WireType::kFixed64Bit: {
        fixed64_t value;
        auto* const value_end =
            codec::ParseValue(tag_end, end, nesting_level, value);
        if (value_end) {
          spans.push_back(std::make_unique<Fixed64>(
              tag_begin, value_end, codec::GetFieldNumberFromTag(tag), value));
          begin = value_end;
        } else if (!permissive) {
          return {};
        } else {
          MergeOrAppendSpanAtEnd(tag_begin, tag_begin + 1, spans);
          begin = tag_begin + 1;
        }
        break;
      }

      case codec::WireType::kFixed32Bit: {
        fixed32_t value;
        auto* const value_end =
            codec::ParseValue(tag_end, end, nesting_level, value);
        if (value_end) {
          spans.push_back(std::make_unique<Fixed32>(
              tag_begin, value_end, codec::GetFieldNumberFromTag(tag), value));
          begin = value_end;
        } else if (!permissive) {
          return {};
        } else {
          MergeOrAppendSpanAtEnd(tag_begin, tag_begin + 1, spans);
          begin = tag_begin + 1;
        }
        break;
      }

      default:
        [[unreachable]] break;
    }
  }

  return spans;
}

}  // namespace

std::vector<std::unique_ptr<WireSpan>> ScanForMessageFields(
    const uint8_t* begin,
    const uint8_t* end,
    bool permissive) {
  assert(begin <= end);
  return ScanForMessageFieldsRecursively(begin, end, 0, permissive);
}

std::unique_ptr<MessageSpan> ParseProbableMessage(const uint8_t* begin,
                                                  const uint8_t* end) {
  assert(begin <= end);
  if (auto spans = ScanForMessageFields(begin, end, false);
      spans.empty() == ((end - begin) == 0)) {
    return std::make_unique<MessageSpan>(
        begin, end, 0, AsVectorOfFieldSpans(std::move(spans)));
  }
  return {};
}

void InspectionRenderingContext::Print(
    const std::vector<std::unique_ptr<WireSpan>>& spans,
    std::ostream& utf8_out) {
  for (const auto& span : spans) {
    if (!span) {
      continue;
    }
    for (const auto& line : span->MakeInspection(*this)) {
      utf8_out << line;
      utf8_out << '\n';
    }
  }
}

}  // namespace pb
