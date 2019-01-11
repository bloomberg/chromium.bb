// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/pausable_task.h"

#include "base/location.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

PausableTask::~PausableTask() = default;

// static
void PausableTask::Post(ExecutionContext* context,
                        WebLocalFrame::PausableTaskCallback callback) {
  DCHECK(!context->IsContextDestroyed());
  if (!context->IsContextPaused()) {
    std::move(callback).Run(WebLocalFrame::PausableTaskResult::kReady);
  } else {
    // Manages its own lifetime and invokes the callback when script is
    // unpaused.
    MakeGarbageCollected<PausableTask>(context, std::move(callback));
  }
}

void PausableTask::ContextDestroyed(ExecutionContext* destroyed_context) {
  DCHECK(callback_);

  Dispose();

  std::move(callback_).Run(
      WebLocalFrame::PausableTaskResult::kContextInvalidOrDestroyed);
}

void PausableTask::Run() {
  CHECK(!GetExecutionContext()->IsContextDestroyed());
  DCHECK(!GetExecutionContext()->IsContextPaused());
  DCHECK(callback_);

  auto callback = std::move(callback_);

  // Call Dispose() now, since it's possible that the callback will destroy the
  // context.
  Dispose();

  std::move(callback).Run(WebLocalFrame::PausableTaskResult::kReady);
}

PausableTask::PausableTask(ExecutionContext* context,
                           WebLocalFrame::PausableTaskCallback callback)
    : ContextLifecycleObserver(context),
      callback_(std::move(callback)),
      keep_alive_(this) {
  DCHECK(callback_);
  DCHECK(context);
  DCHECK(!context->IsContextDestroyed());
  DCHECK(context->IsContextPaused());
  task_handle_ = PostCancellableTask(
      *context->GetTaskRunner(TaskType::kInternalDefault), FROM_HERE,
      WTF::Bind(&PausableTask::Run, WrapPersistent(this)));
}

void PausableTask::Dispose() {
  // Remove object as a ContextLifecycleObserver.
  ContextLifecycleObserver::ClearContext();
  keep_alive_.Clear();
  task_handle_.Cancel();
}

}  // namespace blink
