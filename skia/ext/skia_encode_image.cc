// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/skia_encode_image.h"

#include "third_party/skia/include/core/SkImageEncoder.h"

static skia::ImageEncoderFunction g_image_encoder = nullptr;

void skia::SetImageEncoder(skia::ImageEncoderFunction image_encoder) {
  g_image_encoder = image_encoder;
}

bool SkEncodeImage(SkWStream* stream,
                   const SkPixmap& pixmap,
                   SkEncodedImageFormat format,
                   int quality) {
  skia::ImageEncoderFunction encoder = g_image_encoder;
  return encoder && encoder(stream, pixmap, format, quality);
}
