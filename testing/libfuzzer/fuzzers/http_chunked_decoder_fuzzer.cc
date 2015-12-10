// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "net/http/http_chunked_decoder.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size) {
  const char* data_ptr = reinterpret_cast<const char*>(data);
  std::vector<char> buffer(data_ptr, data_ptr + size);
  net::HttpChunkedDecoder decoder;
  decoder.FilterBuf(buffer.data(), buffer.size());

  return 0;
}
