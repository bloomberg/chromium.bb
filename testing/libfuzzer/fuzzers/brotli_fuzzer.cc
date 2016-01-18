// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "third_party/brotli/dec/decode.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const unsigned char* data, size_t size) {
  size_t addend = 0;
  if (size > 0)
    addend = data[size - 1] & 7;
  const uint8_t* next_in = data;

  const int kBufferSize = 1024;
  uint8_t* buffer = new uint8_t[kBufferSize];
  BrotliState* state = new BrotliState();
  BrotliStateInit(state);

  if (addend == 0)
    addend = size;
  /* Test both fast (addend == size) and slow (addend <= 7) decoding paths. */
  for (size_t i = 0; i < size;) {
    size_t next_i = i + addend;
    if (next_i > size)
      next_i = size;
    size_t avail_in = next_i - i;
    i = next_i;
    BrotliResult result = BROTLI_RESULT_NEEDS_MORE_OUTPUT;
    while (result == BROTLI_RESULT_NEEDS_MORE_OUTPUT) {
      size_t avail_out = kBufferSize;
      uint8_t* next_out = buffer;
      size_t total_out;
      result = BrotliDecompressStream(
          &avail_in, &next_in, &avail_out, &next_out, &total_out, state);
    }
    if (result != BROTLI_RESULT_NEEDS_MORE_INPUT)
      break;
  }

  BrotliStateCleanup(state);
  delete state;
  delete[] buffer;
  return 0;
}
