// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintGeneratedImage_h
#define PaintGeneratedImage_h

#include "platform/geometry/IntSize.h"
#include "platform/graphics/GeneratedImage.h"
#include "platform/graphics/paint/PaintRecord.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blink {

class PLATFORM_EXPORT PaintGeneratedImage : public GeneratedImage {
 public:
  static PassRefPtr<PaintGeneratedImage> create(sk_sp<PaintRecord> record,
                                                const IntSize& size) {
    return adoptRef(new PaintGeneratedImage(std::move(record), size));
  }
  ~PaintGeneratedImage() override {}

 protected:
  void draw(PaintCanvas*,
            const PaintFlags&,
            const FloatRect&,
            const FloatRect&,
            RespectImageOrientationEnum,
            ImageClampingMode) override;
  void drawTile(GraphicsContext&, const FloatRect&) final;

  PaintGeneratedImage(sk_sp<PaintRecord> record, const IntSize& size)
      : GeneratedImage(size), m_record(std::move(record)) {}

  sk_sp<PaintRecord> m_record;
};

}  // namespace blink

#endif  // PaintGeneratedImage_h
