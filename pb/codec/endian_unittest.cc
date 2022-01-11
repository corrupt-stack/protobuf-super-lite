// Copyright 2022 Yuri Wiitala. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pb/codec/endian.h"

#include <cstdint>

#include "gtest/gtest.h"

namespace pb::codec {
namespace {

TEST(EndianTest, ReverseBytes64) {
  EXPECT_EQ(uint64_t{0x0807060504030201},
            ReverseBytes64(uint64_t{0x0102030405060708}));
}

TEST(EndianTest, ReverseBytes32) {
  EXPECT_EQ(uint32_t{0x04030201}, ReverseBytes32(uint32_t{0x01020304}));
}

#if defined(__i386__) || defined(__x86_64__) || defined(__arm__) || \
    defined(__aarch64__)

TEST(EndianTest, IsLittleEndianArchitecture) {
  EXPECT_EQ(true, IsLittleEndianArchitecture());
}

#elif defined(__powerpc64__)

TEST(EndianTest, IsLittleEndianArchitecture) {
  EXPECT_EQ(false, IsLittleEndianArchitecture());
}

#else

// No EndianTest.IsLittleEndianArchitecture for platforms/compilers that don't
// predefine one of the above macros.

#endif

}  // namespace
}  // namespace pb::codec
