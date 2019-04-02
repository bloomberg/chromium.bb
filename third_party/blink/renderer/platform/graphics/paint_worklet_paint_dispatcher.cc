// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint_worklet_paint_dispatcher.h"

#include <utility>

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

// static
std::unique_ptr<PlatformPaintWorkletLayerPainter>
PaintWorkletPaintDispatcher::CreateCompositorThreadPainter(
    scoped_refptr<PaintWorkletPaintDispatcher>& paint_dispatcher) {
  DCHECK(IsMainThread());
  scoped_refptr<PaintWorkletPaintDispatcher> dispatcher =
      base::MakeRefCounted<PaintWorkletPaintDispatcher>();
  paint_dispatcher = dispatcher;

  return std::make_unique<PlatformPaintWorkletLayerPainter>(dispatcher);
}

void PaintWorkletPaintDispatcher::RegisterPaintWorkletPainter(
    PaintWorkletPainter* painter,
    scoped_refptr<base::SingleThreadTaskRunner> painter_runner) {
  TRACE_EVENT0("cc",
               "PaintWorkletPaintDispatcher::RegisterPaintWorkletPainter");

  DCHECK(painter);
  DCHECK(painter_map_.find(painter) == painter_map_.end());
  painter_map_.insert(painter, painter_runner);
}

void PaintWorkletPaintDispatcher::UnregisterPaintWorkletPainter(
    PaintWorkletPainter* painter) {
  TRACE_EVENT0("cc",
               "PaintWorkletPaintDispatcher::"
               "UnregisterPaintWorkletPainter");
  DCHECK(painter);
  DCHECK(painter_map_.find(painter) != painter_map_.end());
  painter_map_.erase(painter);
}

// TODO(xidachen): we should bundle all PaintWorkletInputs and send them to the
// |worklet_queue| once, instead of sending one PaintWorkletInput at a time.
// This avoids thread hop and boost performance.
sk_sp<cc::PaintRecord> PaintWorkletPaintDispatcher::Paint(
    cc::PaintWorkletInput* input) {
  TRACE_EVENT0("cc", "PaintWorkletPaintDispatcher::Paint");
  sk_sp<cc::PaintRecord> output = sk_make_sp<cc::PaintOpBuffer>();
  // TODO(xidachen): call the actual paint function in PaintWorkletProxyClient.
  return output;
}

}  // namespace blink
