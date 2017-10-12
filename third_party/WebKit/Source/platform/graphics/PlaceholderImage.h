// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PlaceholderImage_h
#define PlaceholderImage_h

#include "platform/SharedBuffer.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/Image.h"
#include "platform/graphics/ImageOrientation.h"
#include "platform/wtf/RefPtr.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blink {

class FloatPoint;
class FloatRect;
class FloatSize;
class GraphicsContext;
class ImageObserver;

// A generated placeholder image that shows a translucent gray rectangle.
class PLATFORM_EXPORT PlaceholderImage final : public Image {
 public:
  static RefPtr<PlaceholderImage> Create(ImageObserver* observer,
                                         const IntSize& size) {
    return WTF::AdoptRef(new PlaceholderImage(observer, size));
  }

  ~PlaceholderImage() override;

  IntSize Size() const override { return size_; }

  void Draw(PaintCanvas*,
            const PaintFlags&,
            const FloatRect& dest_rect,
            const FloatRect& src_rect,
            RespectImageOrientationEnum,
            ImageClampingMode,
            ImageDecodingMode) override;

  void DestroyDecodedData() override;

  PaintImage PaintImageForCurrentFrame() override;

  bool IsPlaceholderImage() const override { return true; }

 private:
  PlaceholderImage(ImageObserver*, const IntSize&);

  bool CurrentFrameHasSingleSecurityOrigin() const override { return true; }

  bool CurrentFrameKnownToBeOpaque(
      MetadataMode = kUseCurrentMetadata) override {
    // Placeholder images are translucent.
    return false;
  }

  void DrawPattern(GraphicsContext&,
                   const FloatRect& src_rect,
                   const FloatSize& scale,
                   const FloatPoint& phase,
                   SkBlendMode,
                   const FloatRect& dest_rect,
                   const FloatSize& repeat_spacing) override;

  const IntSize size_;

  // Lazily initialized.
  sk_sp<PaintRecord> paint_record_for_current_frame_;
  PaintImage::ContentId paint_record_content_id_;
};

}  // namespace blink

#endif
