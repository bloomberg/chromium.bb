/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DragImage_h
#define DragImage_h

#include <memory>
#include "platform/geometry/FloatSize.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/GraphicsTypes.h"
#include "platform/graphics/ImageOrientation.h"
#include "platform/graphics/paint/DisplayItemClient.h"
#include "platform/graphics/paint/PaintImage.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Forward.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blink {

class FontDescription;
class Image;
class KURL;

class PLATFORM_EXPORT DragImage {
  USING_FAST_MALLOC(DragImage);
  WTF_MAKE_NONCOPYABLE(DragImage);

 public:
  static std::unique_ptr<DragImage> Create(
      Image*,
      RespectImageOrientationEnum = kDoNotRespectImageOrientation,
      float device_scale_factor = 1,
      InterpolationQuality = kInterpolationHigh,
      float opacity = 1,
      FloatSize image_scale = FloatSize(1, 1));

  static std::unique_ptr<DragImage> Create(const KURL&,
                                           const String& label,
                                           const FontDescription& system_font,
                                           float device_scale_factor);
  ~DragImage();

  static FloatSize ClampedImageScale(const IntSize&,
                                     const IntSize&,
                                     const IntSize& max_size);

  const SkBitmap& Bitmap() { return bitmap_; }
  float ResolutionScale() const { return resolution_scale_; }
  IntSize Size() const { return IntSize(bitmap_.width(), bitmap_.height()); }

  void Scale(float scale_x, float scale_y);

  static PaintImage ResizeAndOrientImage(
      const PaintImage&,
      ImageOrientation,
      FloatSize image_scale = FloatSize(1, 1),
      float opacity = 1.0,
      InterpolationQuality = kInterpolationNone);

 private:
  DragImage(const SkBitmap&, float resolution_scale, InterpolationQuality);

  SkBitmap bitmap_;
  float resolution_scale_;
  InterpolationQuality interpolation_quality_;
};

}  // namespace blink

#endif  // DragImage_h
