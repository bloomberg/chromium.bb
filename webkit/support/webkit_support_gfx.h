// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_SUPPORT_WEBKIT_SUPPORT_GFX_H_
#define WEBKIT_SUPPORT_WEBKIT_SUPPORT_GFX_H_

#include <string>
#include <vector>

namespace webkit_support {

// Decode a PNG into an RGBA pixel array.
bool DecodePNG(const unsigned char* input, size_t input_size,
               std::vector<unsigned char>* output,
               int* width, int* height);

// Encode an RGBA pixel array into a PNG.
bool EncodeRGBAPNG(const unsigned char* input,
                   int width,
                   int height,
                   int row_byte_width,
                   std::vector<unsigned char>* output);

// Encode an BGRA pixel array into a PNG.
bool EncodeBGRAPNG(const unsigned char* input,
                   int width,
                   int height,
                   int row_byte_width,
                   bool discard_transparency,
                   std::vector<unsigned char>* output);

bool EncodeBGRAPNGWithChecksum(const unsigned char* input,
                               int width,
                               int height,
                               int row_byte_width,
                               bool discard_transparency,
                               const std::string& checksum,
                               std::vector<unsigned char>* output);

bool EncodeRGBAPNGWithChecksum(const unsigned char* input,
                               int width,
                               int height,
                               int row_byte_width,
                               bool discard_transparency,
                               const std::string& checksum,
                               std::vector<unsigned char>* output);

}  // namespace webkit_support

#endif  // WEBKIT_SUPPORT_WEBKIT_SUPPORT_GFX_H_
