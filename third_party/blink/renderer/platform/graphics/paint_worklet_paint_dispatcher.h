// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_WORKLET_PAINT_DISPATCHER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_WORKLET_PAINT_DISPATCHER_H_

#include <memory>

#include "base/macros.h"
#include "third_party/blink/renderer/platform/graphics/platform_paint_worklet_layer_painter.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"

namespace blink {

// The dispatcher receives JS paint callback from the compositor, and dispatch
// the callback to the painter (on the paint worklet thread) that is associated
// with the given paint image.
class PLATFORM_EXPORT PaintWorkletPaintDispatcher
    : public ThreadSafeRefCounted<PaintWorkletPaintDispatcher> {
 public:
  static std::unique_ptr<PlatformPaintWorkletLayerPainter>
  CreateCompositorThreadPainter(
      scoped_refptr<PaintWorkletPaintDispatcher>& paintee);

  PaintWorkletPaintDispatcher() = default;

  sk_sp<cc::PaintRecord> Paint(cc::PaintWorkletInput*);

 private:
  DISALLOW_COPY_AND_ASSIGN(PaintWorkletPaintDispatcher);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_WORKLET_PAINT_DISPATCHER_H_
