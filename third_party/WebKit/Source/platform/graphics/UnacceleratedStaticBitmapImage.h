// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UnacceleratedStaticBitmapImage_h
#define UnacceleratedStaticBitmapImage_h

#include "platform/graphics/StaticBitmapImage.h"

namespace blink {

class PLATFORM_EXPORT UnacceleratedStaticBitmapImage final
    : public StaticBitmapImage {
 public:
  ~UnacceleratedStaticBitmapImage() override;
  static PassRefPtr<UnacceleratedStaticBitmapImage> Create(sk_sp<SkImage>);
  static PassRefPtr<UnacceleratedStaticBitmapImage> Create(PaintImage);

  bool CurrentFrameKnownToBeOpaque(MetadataMode = kUseCurrentMetadata) override;
  IntSize Size() const override;
  bool IsPremultiplied() const override;

  void Draw(PaintCanvas*,
            const PaintFlags&,
            const FloatRect& dst_rect,
            const FloatRect& src_rect,
            RespectImageOrientationEnum,
            ImageClampingMode) override;

  PaintImage PaintImageForCurrentFrame() override;

 private:
  UnacceleratedStaticBitmapImage(sk_sp<SkImage>);
  UnacceleratedStaticBitmapImage(PaintImage);

  PaintImage paint_image_;
};

}  // namespace blink

#endif
