// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/ThreadedWorkletGlobalScope.h"

#include "base/memory/scoped_refptr.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/ConsoleMessageStorage.h"
#include "core/inspector/WorkerThreadDebugger.h"
#include "core/workers/GlobalScopeCreationParams.h"
#include "core/workers/WorkerReportingProxy.h"
#include "core/workers/WorkerThread.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/Assertions.h"
#include "public/platform/Platform.h"

namespace blink {

ThreadedWorkletGlobalScope::ThreadedWorkletGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    v8::Isolate* isolate,
    WorkerThread* thread)
    : WorkletGlobalScope(
          std::move(creation_params),
          isolate,
          thread->GetWorkerReportingProxy(),
          // Specify |kUnspecedLoading| because these task runners are used
          // during module loading and this usage is not explicitly spec'ed.
          thread->GetParentFrameTaskRunners()->Get(TaskType::kUnspecedLoading),
          thread->GetTaskRunner(TaskType::kUnspecedLoading)),
      thread_(thread) {}

ThreadedWorkletGlobalScope::~ThreadedWorkletGlobalScope() {
  DCHECK(!thread_);
}

void ThreadedWorkletGlobalScope::Dispose() {
  DCHECK(IsContextThread());
  WorkletGlobalScope::Dispose();
  thread_ = nullptr;
}

bool ThreadedWorkletGlobalScope::IsContextThread() const {
  return GetThread()->IsCurrentThread();
}

void ThreadedWorkletGlobalScope::AddConsoleMessage(
    ConsoleMessage* console_message) {
  DCHECK(IsContextThread());
  GetThread()->GetWorkerReportingProxy().ReportConsoleMessage(
      console_message->Source(), console_message->Level(),
      console_message->Message(), console_message->Location());
  GetThread()->GetConsoleMessageStorage()->AddConsoleMessage(this,
                                                             console_message);
}

void ThreadedWorkletGlobalScope::ExceptionThrown(ErrorEvent* error_event) {
  DCHECK(IsContextThread());
  if (WorkerThreadDebugger* debugger =
          WorkerThreadDebugger::From(GetThread()->GetIsolate()))
    debugger->ExceptionThrown(thread_, error_event);
}

}  // namespace blink
