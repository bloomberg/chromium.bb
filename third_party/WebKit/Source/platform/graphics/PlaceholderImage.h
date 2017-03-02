// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PlaceholderImage_h
#define PlaceholderImage_h

#include "platform/SharedBuffer.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/Image.h"
#include "platform/graphics/ImageOrientation.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "wtf/PassRefPtr.h"

namespace blink {

class FloatRect;
class ImageObserver;

// A generated placeholder image that shows a translucent gray rectangle.
class PLATFORM_EXPORT PlaceholderImage final : public Image {
 public:
  static PassRefPtr<PlaceholderImage> create(ImageObserver* observer,
                                             const IntSize& size) {
    return adoptRef(new PlaceholderImage(observer, size));
  }

  ~PlaceholderImage() override;

  IntSize size() const override { return m_size; }

  sk_sp<SkImage> imageForCurrentFrame(const ColorBehavior&) override;

  void draw(PaintCanvas*,
            const PaintFlags&,
            const FloatRect& destRect,
            const FloatRect& srcRect,
            RespectImageOrientationEnum,
            ImageClampingMode) override;

  void destroyDecodedData() override;

 private:
  PlaceholderImage(ImageObserver* observer, const IntSize& size)
      : Image(observer), m_size(size) {}

  bool currentFrameHasSingleSecurityOrigin() const override { return true; }

  bool currentFrameKnownToBeOpaque(MetadataMode = UseCurrentMetadata) override {
    // Placeholder images are translucent.
    return false;
  }

  IntSize m_size;
  // Lazily initialized.
  sk_sp<SkImage> m_imageForCurrentFrame;
};

}  // namespace blink

#endif
