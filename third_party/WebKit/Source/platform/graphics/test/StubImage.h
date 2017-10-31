// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StubImage_h
#define StubImage_h

#include "platform/graphics/Image.h"

namespace blink {

class StubImage : public Image {
 public:
  StubImage() {}

  bool CurrentFrameKnownToBeOpaque(MetadataMode) override { return false; }
  IntSize Size() const override { return IntSize(10, 10); }
  void DestroyDecodedData() override {}
  PaintImage PaintImageForCurrentFrame() override { return PaintImage(); }
  void Draw(PaintCanvas*,
            const PaintFlags&,
            const FloatRect& dst_rect,
            const FloatRect& src_rect,
            RespectImageOrientationEnum,
            ImageClampingMode,
            ImageDecodingMode) override {}
};

}  // namespace blink

#endif  // StubImage_h
