// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint_worklet_paint_dispatcher.h"

#include <utility>

#include "base/synchronization/waitable_event.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"

namespace blink {

namespace {
class AutoSignal {
 public:
  explicit AutoSignal(base::WaitableEvent* event) : event_(event) {
    DCHECK(event);
  }
  ~AutoSignal() { event_->Signal(); }

 private:
  base::WaitableEvent* event_;

  DISALLOW_COPY_AND_ASSIGN(AutoSignal);
};
}  // namespace

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
  MutexLocker lock(painter_map_mutex_);
  DCHECK(painter_map_.find(painter) == painter_map_.end());
  painter_map_.insert(painter, painter_runner);
}

void PaintWorkletPaintDispatcher::UnregisterPaintWorkletPainter(
    PaintWorkletPainter* painter) {
  TRACE_EVENT0("cc",
               "PaintWorkletPaintDispatcher::"
               "UnregisterPaintWorkletPainter");
  DCHECK(painter);
  MutexLocker lock(painter_map_mutex_);
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

  PaintWorkletPainterToTaskRunnerMap copied_painter_map;
  {
    MutexLocker lock(painter_map_mutex_);
    if (painter_map_.IsEmpty())
      return output;
    // TODO(xidachen): copying is a temporary solution to prevent deadlock. It
    // should be automatically solved with CC work.
    copied_painter_map = painter_map_;
  }

  base::WaitableEvent done_event;

  for (auto& pair : copied_painter_map) {
    if (pair.key->GetWorkletId() != input->WorkletId())
      continue;
    PaintWorkletPainter* painter = pair.key;
    scoped_refptr<base::SingleThreadTaskRunner> task_runner = pair.value;

    DCHECK(!task_runner->BelongsToCurrentThread());
    std::unique_ptr<AutoSignal> done =
        std::make_unique<AutoSignal>(&done_event);

    PostCrossThreadTask(
        *task_runner, FROM_HERE,
        CrossThreadBind(
            [](PaintWorkletPainter* painter, cc::PaintWorkletInput* input,
               std::unique_ptr<AutoSignal> completion,
               sk_sp<cc::PaintRecord>* output) {
              *output = painter->Paint(input);
            },
            WrapCrossThreadPersistent(painter), CrossThreadUnretained(input),
            WTF::Passed(std::move(done)), CrossThreadUnretained(&output)));
  }

  done_event.Wait();

  return output;
}

}  // namespace blink
