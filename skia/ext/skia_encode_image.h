// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_SKIA_ENCODE_IMAGE_H
#define SKIA_EXT_SKIA_ENCODE_IMAGE_H

#include "third_party/skia/include/core/SkEncodedImageFormat.h"
#include "third_party/skia/include/core/SkTypes.h"

class SkPixmap;
class SkWStream;

namespace skia {

using ImageEncoderFunction = bool (*)(SkWStream*,
                                      const SkPixmap&,
                                      SkEncodedImageFormat,
                                      int quality);

SK_API void SetImageEncoder(ImageEncoderFunction);

}  // namespace skia

#endif  // SKIA_EXT_SKIA_ENCODE_IMAGE_H
