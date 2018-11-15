// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "third_party/snappy/src/snappy.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const char* uncompressed = reinterpret_cast<const char*>(data);
  std::string compressed;
  snappy::Compress(uncompressed, size, &compressed);
  CHECK(snappy::IsValidCompressedBuffer(compressed.data(), compressed.size()));

  std::string uncompressed_after_compress;
  CHECK(snappy::Uncompress(compressed.data(), compressed.size(),
                           &uncompressed_after_compress));
  CHECK_EQ(uncompressed_after_compress, std::string(uncompressed, size));

  return 0;
}
