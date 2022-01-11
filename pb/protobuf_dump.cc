// Copyright 2022 Yuri Wiitala. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <vector>

#include "pb/codec/limits.h"
#include "pb/inspection.h"

int main(int argc, char* argv[]) {
  // If a path was provided on the command line, open a file. Else, read from
  // stdin.
  std::istream* in;
  std::fstream fs;
  if (argc > 1) {
    fs.open(argv[1], std::fstream::in);
    in = &fs;
  } else {
    in = &std::cin;
  }

  if (!*in) {
    std::cerr << "Failed to open file.\n";
    return 1;
  }

  // Read the whole file into memory.
  static constexpr int32_t kChunkSize = 4096;
  std::vector<uint8_t> buffer;
  while (static_cast<int64_t>(buffer.size()) < pb::codec::kMaxSerializedSize) {
    buffer.resize(buffer.size() + kChunkSize);
    in->read(
        reinterpret_cast<char*>(buffer.data() + buffer.size() - kChunkSize),
        kChunkSize);
    if (!*in) {
      const auto num_read = static_cast<std::size_t>(in->gcount());
      buffer.resize(buffer.size() - kChunkSize + num_read);
      break;
    }
  }

  // Interpret and print the results to stdout.
  pb::InspectionRenderingContext(buffer.data(),
                                 static_cast<std::ptrdiff_t>(buffer.size()))
      .Print(pb::ScanForMessageFields(buffer.data(),
                                      buffer.data() + buffer.size(), true),
             std::cout);

  if (argc > 1) {
    fs.close();
  }
  return 0;
}
