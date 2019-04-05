// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_WORKLET_PAINT_DISPATCHER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_WORKLET_PAINT_DISPATCHER_H_

#include <memory>

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "third_party/blink/renderer/platform/graphics/paint_worklet_painter.h"
#include "third_party/blink/renderer/platform/graphics/platform_paint_worklet_layer_painter.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"

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

  // Interface for use by the PaintWorklet thread(s) to request calls.
  // (To the given Painter on the given TaskRunner.)
  void RegisterPaintWorkletPainter(
      PaintWorkletPainter*,
      scoped_refptr<base::SingleThreadTaskRunner> mutator_runner);

  void UnregisterPaintWorkletPainter(PaintWorkletPainter*);

  sk_sp<cc::PaintRecord> Paint(cc::PaintWorkletInput*);

 private:
  friend class PaintWorkletProxyClientTest;
  // We can have more than one task-runner because using a worklet inside a
  // frame with a different origin causes a new global scope => new thread.
  using PaintWorkletPainterToTaskRunnerMap =
      HashMap<CrossThreadPersistent<PaintWorkletPainter>,
              scoped_refptr<base::SingleThreadTaskRunner>>;

  PaintWorkletPainterToTaskRunnerMap painter_map_;

  // The (Un)registerPaintWorkletPainter comes from the worklet thread, and the
  // Paint call is initiated from the raster threads, this mutex ensures that
  // accessing / updating the |painter_map_| is thread safe.
  Mutex painter_map_mutex_;

  DISALLOW_COPY_AND_ASSIGN(PaintWorkletPaintDispatcher);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_WORKLET_PAINT_DISPATCHER_H_
