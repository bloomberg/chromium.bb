// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <vector>

#include "third_party/brotli/dec/decode.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const unsigned char* data, size_t size) {
  std::vector<uint8_t> uncompressed_buf(128 << 10);
  size_t uncompressed_size = uncompressed_buf.size();
  BrotliDecompressBuffer(size, data, &uncompressed_size, &uncompressed_buf[0]);
  return 0;
}
