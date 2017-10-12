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
  static RefPtr<PaintGeneratedImage> Create(sk_sp<PaintRecord> record,
                                            const IntSize& size) {
    return WTF::AdoptRef(new PaintGeneratedImage(std::move(record), size));
  }
  ~PaintGeneratedImage() override {}

 protected:
  void Draw(PaintCanvas*,
            const PaintFlags&,
            const FloatRect&,
            const FloatRect&,
            RespectImageOrientationEnum,
            ImageClampingMode,
            ImageDecodingMode) override;
  void DrawTile(GraphicsContext&, const FloatRect&) final;

  PaintGeneratedImage(sk_sp<PaintRecord> record, const IntSize& size)
      : GeneratedImage(size), record_(std::move(record)) {}

  sk_sp<PaintRecord> record_;
};

}  // namespace blink

#endif  // PaintGeneratedImage_h
