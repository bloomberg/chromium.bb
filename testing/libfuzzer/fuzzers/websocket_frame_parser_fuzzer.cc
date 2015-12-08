// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "net/websockets/websocket_frame_parser.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size) {
  net::WebSocketFrameParser parser;
  std::vector<scoped_ptr<net::WebSocketFrameChunk>> frame_chunks;
  parser.Decode(reinterpret_cast<const char*>(data), size, &frame_chunks);

  return 0;
}
