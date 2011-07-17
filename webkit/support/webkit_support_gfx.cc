// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/support/webkit_support_gfx.h"

#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/size.h"

namespace webkit_support {

// Decode a PNG into an RGBA pixel array.
bool DecodePNG(const unsigned char* input, size_t input_size,
               std::vector<unsigned char>* output,
               int* width, int* height) {
  return gfx::PNGCodec::Decode(input, input_size, gfx::PNGCodec::FORMAT_RGBA,
                               output, width, height);
}

// Encode an RGBA pixel array into a PNG.
bool EncodeRGBAPNG(const unsigned char* input,
                   int width,
                   int height,
                   int row_byte_width,
                   std::vector<unsigned char>* output) {
  return gfx::PNGCodec::Encode(input, gfx::PNGCodec::FORMAT_RGBA,
      gfx::Size(width, height), row_byte_width, false,
      std::vector<gfx::PNGCodec::Comment>(), output);
}

// Encode an BGRA pixel array into a PNG.
bool EncodeBGRAPNG(const unsigned char* input,
                   int width,
                   int height,
                   int row_byte_width,
                   bool discard_transparency,
                   std::vector<unsigned char>* output) {
  return gfx::PNGCodec::Encode(input, gfx::PNGCodec::FORMAT_BGRA,
      gfx::Size(width, height), row_byte_width, discard_transparency,
      std::vector<gfx::PNGCodec::Comment>(), output);
}

bool EncodeBGRAPNGWithChecksum(const unsigned char* input,
                               int width,
                               int height,
                               int row_byte_width,
                               bool discard_transparency,
                               const std::string& checksum,
                               std::vector<unsigned char>* output) {
  std::vector<gfx::PNGCodec::Comment> comments;
  comments.push_back(gfx::PNGCodec::Comment("checksum", checksum));
  return gfx::PNGCodec::Encode(input, gfx::PNGCodec::FORMAT_BGRA,
      gfx::Size(width, height), row_byte_width, discard_transparency,
      comments, output);
}

}  // namespace webkit_support
