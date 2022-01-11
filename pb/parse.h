// Copyright 2022 Yuri Wiitala. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <cassert>
#include <cstdint>
#include <memory>
#include <type_traits>

#include "pb/codec/parse.h"

namespace pb {

// Parses the buffer given by the range |begin| to |end|, merging field data
// into the given |message|. Returns false if the parse failed.
template <class Message,
          std::enable_if_t<std::is_class_v<typename Message::ProtobufFields>,
                           int> = 0>
[[nodiscard]] bool MergeFromBuffer(const uint8_t* begin,
                                   const uint8_t* end,
                                   Message& message) {
  assert(begin && (begin <= end));
  return !!codec::ParseFields(begin, end, 0, message);
}

// Parses the buffer given by the range |begin| to |end| into a heap-allocated
// Message. Returs "null" if the parse failed.
//
// This is generally used for larger messages and/or frequently-moved messages.
// If a smaller and/or never-moved message is to be parsed, it is usually faster
// to call MergeFromBuffer() on a local variable (i.e., stack-allocated) Message
// instead.
template <class Message,
          std::enable_if_t<std::is_class_v<typename Message::ProtobufFields>,
                           int> = 0>
[[nodiscard]] std::unique_ptr<Message> Parse(const uint8_t* begin,
                                             const uint8_t* end) {
  assert(begin && (begin <= end));
  auto message = std::make_unique<Message>();
  if (!codec::ParseFields(begin, end, 0, *message)) {
    message.reset();
  }
  return message;
}

}  // namespace pb
