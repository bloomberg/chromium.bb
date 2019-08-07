// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/platform_paint_worklet_layer_painter.h"

#include <utility>

#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"
#include "third_party/blink/renderer/platform/graphics/paint_worklet_paint_dispatcher.h"

namespace blink {

PlatformPaintWorkletLayerPainter::PlatformPaintWorkletLayerPainter(
    scoped_refptr<PaintWorkletPaintDispatcher> dispatcher)
    : dispatcher_(dispatcher) {
  TRACE_EVENT0(
      TRACE_DISABLED_BY_DEFAULT("cc"),
      "PlatformPaintWorkletLayerPainter::PlatformPaintWorkletLayerPainter");
}

PlatformPaintWorkletLayerPainter::~PlatformPaintWorkletLayerPainter() {
  TRACE_EVENT0(
      TRACE_DISABLED_BY_DEFAULT("cc"),
      "PlatformPaintWorkletLayerPainter::~PlatformPaintWorkletLayerPainter");
}

sk_sp<PaintRecord> PlatformPaintWorkletLayerPainter::Paint(
    cc::PaintWorkletInput* input) {
  return dispatcher_->Paint(input);
}

}  // namespace blink
