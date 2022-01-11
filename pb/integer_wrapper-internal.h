// Copyright 2022 Yuri Wiitala. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

namespace pb::internal {

// A thin type-wrapper around native integer types to indicate that the value
// should be serialized/parsed using an alternate encoding scheme.
template <typename Integer, bool is_fixed>
class IntegerWrapper {
 public:
  // Implicit construction from the native integer type.
  constexpr IntegerWrapper() : value_(0) {}
  constexpr IntegerWrapper(Integer x) : value_(x) {}

  // Implicit conversions to the native integer type. In the non-const case, a
  // reference to the value is returned so that it can be directly mutated or
  // accessed/mutated using all the normal integer operators (such as a += 42).
  constexpr operator const Integer&() const { return value_; }
  constexpr operator Integer&() { return value_; }

  // Explicit access to the value.
  constexpr Integer value() const { return value_; }

 private:
  Integer value_;  // The decoded value.
};

}  // namespace pb::internal
