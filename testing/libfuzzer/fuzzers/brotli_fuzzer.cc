// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "third_party/brotli/dec/decode.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const unsigned char* data, size_t size) {
  int kBufferSize = 1024;
  uint8_t* buffer = new uint8_t[kBufferSize];
  BrotliState* state = new BrotliState();
  BrotliStateInit(state);

  size_t avail_in = size;
  const uint8_t* next_in = data;
  BrotliResult result = BROTLI_RESULT_NEEDS_MORE_OUTPUT;
  while (result == BROTLI_RESULT_NEEDS_MORE_OUTPUT) {
    size_t avail_out = kBufferSize;
    uint8_t* next_out = buffer;
    size_t total_out;
    result = BrotliDecompressStream(
        &avail_in, &next_in, &avail_out, &next_out, &total_out, state);
  }

  BrotliStateCleanup(state);
  delete state;
  delete[] buffer;
  return 0;
}
