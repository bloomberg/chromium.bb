// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <string.h>

#include "third_party/zlib/zlib.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size) {
  const int NUM_ITEMS = 1024 * 1024;
  const int BUF_SIZE  = NUM_ITEMS * sizeof(uint8_t);
  uint8_t *buffer = new uint8_t[NUM_ITEMS];
  uLongf buffer_length = (uLongf)BUF_SIZE;
  memset(buffer, 0, BUF_SIZE);
  if (Z_OK != uncompress(buffer, &buffer_length, data,
                         static_cast<uLong>(size))) {
    delete[] buffer;
    return 0;
  }
  delete[] buffer;
  return 0;
}
