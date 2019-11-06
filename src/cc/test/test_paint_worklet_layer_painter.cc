// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/test_paint_worklet_layer_painter.h"

#include "cc/test/skia_common.h"
#include "cc/test/test_paint_worklet_input.h"

namespace cc {

TestPaintWorkletLayerPainter::TestPaintWorkletLayerPainter() = default;

TestPaintWorkletLayerPainter::~TestPaintWorkletLayerPainter() = default;

sk_sp<PaintRecord> TestPaintWorkletLayerPainter::Paint(PaintWorkletInput*) {
  auto manual_record = sk_make_sp<PaintOpBuffer>();
  scoped_refptr<TestPaintWorkletInput> input =
      base::MakeRefCounted<TestPaintWorkletInput>(gfx::SizeF(100, 100));
  PaintImage image = CreatePaintWorkletPaintImage(input);
  manual_record->push<DrawImageOp>(image, 0.f, 0.f, nullptr);
  return manual_record;
}

}  // namespace cc
