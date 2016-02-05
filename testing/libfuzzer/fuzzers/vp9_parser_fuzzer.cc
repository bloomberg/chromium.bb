// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "media/filters/vp9_parser.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  media::Vp9Parser parser;
  parser.SetStream(data, static_cast<off_t>(size));
  while (true) {
    media::Vp9FrameHeader fhdr;
    if (media::Vp9Parser::kOk != parser.ParseNextFrame(&fhdr)) {
      break;
    }
  }
  return 0;
}
