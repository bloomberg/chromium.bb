// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/TaskRunnerHelper.h"

#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalFrame.h"
#include "core/workers/WorkerOrWorkletGlobalScope.h"
#include "core/workers/WorkerThread.h"
#include "platform/WebFrameScheduler.h"
#include "platform/WebTaskRunner.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"

namespace blink {

scoped_refptr<WebTaskRunner> TaskRunnerHelper::Get(TaskType type,
                                                   LocalFrame* frame) {
  return frame->GetTaskRunner(type);
}

scoped_refptr<WebTaskRunner> TaskRunnerHelper::Get(TaskType type,
                                                   Document* document) {
  return document->GetTaskRunner(type);
}

scoped_refptr<WebTaskRunner> TaskRunnerHelper::Get(
    TaskType type,
    ExecutionContext* execution_context) {
  return execution_context->GetTaskRunner(type);
}

scoped_refptr<WebTaskRunner> TaskRunnerHelper::Get(
    TaskType type,
    WorkerOrWorkletGlobalScope* global_scope) {
  return global_scope->GetTaskRunner(type);
}

scoped_refptr<WebTaskRunner> TaskRunnerHelper::Get(
    TaskType type,
    WorkerThread* worker_thread) {
  return worker_thread->GetGlobalScopeScheduler()->GetTaskRunner(type);
}

}  // namespace blink
