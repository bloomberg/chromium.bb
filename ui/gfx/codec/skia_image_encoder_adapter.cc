// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/codec/skia_image_encoder_adapter.h"

#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkStream.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"

bool gfx::EncodeSkiaImage(SkWStream* dst,
                          const SkPixmap& pixmap,
                          SkEncodedImageFormat format,
                          int quality) {
  if (kN32_SkColorType != pixmap.colorType() ||
      (kPremul_SkAlphaType != pixmap.alphaType() &&
       kOpaque_SkAlphaType != pixmap.alphaType())) {
    return false;
  }
  std::vector<unsigned char> buffer;
  switch (format) {
    case SkEncodedImageFormat::kPNG:
      return gfx::PNGCodec::Encode(
                 reinterpret_cast<const unsigned char*>(pixmap.addr()),
                 gfx::PNGCodec::FORMAT_SkBitmap,
                 gfx::Size(pixmap.width(), pixmap.height()),
                 static_cast<int>(pixmap.rowBytes()), false,
                 std::vector<gfx::PNGCodec::Comment>(), &buffer) &&
             dst->write(buffer.data(), buffer.size());
    case SkEncodedImageFormat::kJPEG:
      return gfx::JPEGCodec::Encode(
                 reinterpret_cast<const unsigned char*>(pixmap.addr()),
                 gfx::JPEGCodec::FORMAT_SkBitmap, pixmap.width(),
                 pixmap.height(), static_cast<int>(pixmap.rowBytes()), quality,
                 &buffer) &&
             dst->write(buffer.data(), buffer.size());
    default:
      return false;
  }
}
