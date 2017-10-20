// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/TaskRunnerHelper.h"

#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalFrame.h"
#include "core/workers/MainThreadWorkletGlobalScope.h"
#include "core/workers/WorkerOrWorkletGlobalScope.h"
#include "core/workers/WorkerThread.h"
#include "platform/WebFrameScheduler.h"
#include "platform/WebTaskRunner.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"

namespace blink {

RefPtr<WebTaskRunner> TaskRunnerHelper::Get(TaskType type, LocalFrame* frame) {
  return frame->FrameScheduler()->GetTaskRunner(type);
}

RefPtr<WebTaskRunner> TaskRunnerHelper::Get(TaskType type, Document* document) {
  DCHECK(document);
  if (document->ContextDocument() && document->ContextDocument()->GetFrame())
    return Get(type, document->ContextDocument()->GetFrame());
  // In most cases, ContextDocument() will get us to a relevant Frame. In some
  // cases, though, there isn't a good candidate (most commonly when either the
  // passed-in document or ContextDocument() used to be attached to a Frame but
  // has since been detached).
  return Platform::Current()->CurrentThread()->GetWebTaskRunner();
}

RefPtr<WebTaskRunner> TaskRunnerHelper::Get(
    TaskType type,
    ExecutionContext* execution_context) {
  DCHECK(execution_context);
  if (execution_context->IsDocument())
    return Get(type, ToDocument(execution_context));
  if (execution_context->IsWorkerOrWorkletGlobalScope())
    return Get(type, ToWorkerOrWorkletGlobalScope(execution_context));
  // This should only happen for a NullExecutionContext in a unit test.
  return Platform::Current()->CurrentThread()->GetWebTaskRunner();
}

RefPtr<WebTaskRunner> TaskRunnerHelper::Get(TaskType type,
                                            ScriptState* script_state) {
  return Get(type, ExecutionContext::From(script_state));
}

RefPtr<WebTaskRunner> TaskRunnerHelper::Get(
    TaskType type,
    WorkerOrWorkletGlobalScope* global_scope) {
  DCHECK(global_scope);
  DCHECK(global_scope->IsContextThread());
  if (global_scope->IsMainThreadWorkletGlobalScope()) {
    // MainThreadWorkletGlobalScope lives on the main thread and its GetThread()
    // doesn't return a valid worker thread. Instead, retrieve a task runner
    // from the frame.
    return Get(type, ToMainThreadWorkletGlobalScope(global_scope)->GetFrame());
  }
  return Get(type, global_scope->GetThread());
}

RefPtr<WebTaskRunner> TaskRunnerHelper::Get(TaskType type,
                                            WorkerThread* worker_thread) {
  return worker_thread->GetGlobalScopeScheduler()->GetTaskRunner(type);
}

}  // namespace blink
