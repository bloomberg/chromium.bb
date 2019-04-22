// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_PAUSABLE_TASK_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_PAUSABLE_TASK_H_

#include <memory>

#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/heap/self_keep_alive.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"

namespace blink {

// A class that monitors the lifetime and suspension state of a context, and
// runs a task either when the context is unsuspended or when the context is
// invalidated.
class CORE_EXPORT PausableTask final
    : public GarbageCollectedFinalized<PausableTask>,
      public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(PausableTask);

 public:
  // Note: This asserts that the context is currently suspended.
  PausableTask(ExecutionContext*, WebLocalFrame::PausableTaskCallback);
  virtual ~PausableTask();

  // Checks if the context is paused, and, if not, executes the callback
  // immediately. Otherwise constructs a PausableTask that will run the
  // callback when execution is unpaused.
  static void Post(ExecutionContext*, WebLocalFrame::PausableTaskCallback);

  // ContextLifecycleObserver:
  void ContextDestroyed(ExecutionContext*) override;

 private:
  void Run();
  void Dispose();

  WebLocalFrame::PausableTaskCallback callback_;
  TaskHandle task_handle_;

  SelfKeepAlive<PausableTask> keep_alive_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_PAUSABLE_TASK_H_
