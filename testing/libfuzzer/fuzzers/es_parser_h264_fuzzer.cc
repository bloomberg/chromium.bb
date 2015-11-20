// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "media/formats/mp2t/es_parser_h264.h"

static void NewVideoConfig(const media::VideoDecoderConfig& config) {}
static void EmitBuffer(scoped_refptr<media::StreamParserBuffer> buffer) {}

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const unsigned char* data,
                                      unsigned long size) {
  media::mp2t::EsParserH264 es_parser(base::Bind(&NewVideoConfig),
                                      base::Bind(&EmitBuffer));
  if (!es_parser.Parse(data, size, media::kNoTimestamp(),
                       media::kNoDecodeTimestamp())) {
    return 0;
  }
  return 0;
}
