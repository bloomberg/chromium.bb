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

// PaintWorkletPaintDispatcher is responsible for mediating between the raster
// threads and the PaintWorklet thread(s). It receives requests from raster
// threads to paint a paint class instance represented by a PaintWorkletInput,
// dispatches the input to the appropriate PaintWorklet, synchronously receives
// the result, and passes it back to the raster thread.
//
// Each PaintWorklet (there is one per frame, either same-origin or
// same-process-cross-origin) has a backing thread, which may be shared between
// worklets, and a scheduler, which is not shared. All PaintWorklets for a
// single renderer process share one PaintWorkletPaintDispatcher on the
// compositor side.
class PLATFORM_EXPORT PaintWorkletPaintDispatcher
    : public ThreadSafeRefCounted<PaintWorkletPaintDispatcher> {
 public:
  static std::unique_ptr<PlatformPaintWorkletLayerPainter>
  CreateCompositorThreadPainter(
      scoped_refptr<PaintWorkletPaintDispatcher>& paintee);

  PaintWorkletPaintDispatcher() = default;

  // Dispatches a single paint class instance - represented by a
  // PaintWorkletInput - to the appropriate PaintWorklet thread, and blocks
  // until it receives the result.
  sk_sp<cc::PaintRecord> Paint(cc::PaintWorkletInput*);

  // Register and unregister a PaintWorklet (represented in this context by a
  // PaintWorkletPainter). A given PaintWorklet is registered once all its
  // global scopes have been created, and is usually only unregistered when the
  // associated PaintWorklet thread is being torn down.
  //
  // The passed in PaintWorkletPainter* should only be used on the given
  // base::SingleThreadTaskRunner.
  using PaintWorkletId = int;
  void RegisterPaintWorkletPainter(PaintWorkletPainter*,
                                   scoped_refptr<base::SingleThreadTaskRunner>);
  void UnregisterPaintWorkletPainter(PaintWorkletId);

  using PaintWorkletPainterToTaskRunnerMap =
      HashMap<PaintWorkletId,
              std::pair<CrossThreadPersistent<PaintWorkletPainter>,
                        scoped_refptr<base::SingleThreadTaskRunner>>>;
  const PaintWorkletPainterToTaskRunnerMap& PainterMapForTesting() const {
    return painter_map_;
  }

 private:
  // This class handles paint class instances for multiple PaintWorklets. These
  // are disambiguated via the PaintWorklets unique id; this map exists to do
  // that disambiguation.
  PaintWorkletPainterToTaskRunnerMap painter_map_;

  // The (Un)registerPaintWorkletPainter comes from the worklet thread, and the
  // Paint call is initiated from the raster threads - this mutex ensures that
  // accessing / updating the |painter_map_| is thread safe.
  Mutex painter_map_mutex_;

  DISALLOW_COPY_AND_ASSIGN(PaintWorkletPaintDispatcher);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_WORKLET_PAINT_DISPATCHER_H_
