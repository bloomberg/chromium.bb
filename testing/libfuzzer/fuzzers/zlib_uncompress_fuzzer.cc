// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "third_party/zlib/zlib.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size) {
  uint8_t buffer[1024 * 1024] = { 0 };
  size_t buffer_length = sizeof(buffer);
  if (Z_OK != uncompress(buffer, &buffer_length, data, size)) {
    return 0;
  }

  return 0;
}
