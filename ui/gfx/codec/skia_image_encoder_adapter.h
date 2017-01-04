// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_CODEC_SKIA_IMAGE_ENCODER_ADAPTER_H
#define UI_GFX_CODEC_SKIA_IMAGE_ENCODER_ADAPTER_H

#include "third_party/skia/include/core/SkEncodedImageFormat.h"
#include "ui/gfx/gfx_export.h"

class SkWStream;
class SkPixmap;

namespace gfx {

// Matches signature of Skia's SkEncodeImage, but makes use of Chromium's
// encoders.
GFX_EXPORT bool EncodeSkiaImage(SkWStream* dst,
                                const SkPixmap& pixmap,
                                SkEncodedImageFormat format,
                                int quality);

}  // namespace gfx

#endif  // UI_GFX_CODEC_SKIA_IMAGE_ENCODER_ADAPTER_H
