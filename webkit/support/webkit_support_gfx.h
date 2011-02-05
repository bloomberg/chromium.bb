// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_SUPPORT_WEBKIT_SUPPORT_GFX_H_
#define WEBKIT_SUPPORT_WEBKIT_SUPPORT_GFX_H_

#include <vector>

#include "ui/gfx/codec/png_codec.h"

namespace webkit_support {

// Decode a PNG into an RGBA pixel array.
inline bool DecodePNG(const unsigned char* input, size_t input_size,
                      std::vector<unsigned char>* output,
                      int* width, int* height) {
  return gfx::PNGCodec::Decode(input, input_size, gfx::PNGCodec::FORMAT_RGBA,
                               output, width, height);
}

// Encode an RGBA pixel array into a PNG.
inline bool EncodeRGBAPNG(const unsigned char* input, int width, int height,
                          int row_byte_width,
                          std::vector<unsigned char>* output) {
  return gfx::PNGCodec::Encode(input, gfx::PNGCodec::FORMAT_RGBA, width,
                               height, row_byte_width, false, output);
}

// Encode an BGRA pixel array into a PNG.
inline bool EncodeBGRAPNG(const unsigned char* input, int width, int height,
                          int row_byte_width, bool discard_transparency,
                          std::vector<unsigned char>* output) {
  return gfx::PNGCodec::Encode(input, gfx::PNGCodec::FORMAT_BGRA, width,
                               height, row_byte_width, discard_transparency,
                               output);
}

}  // namespace webkit_support

#endif  // WEBKIT_SUPPORT_WEBKIT_SUPPORT_GFX_H_
