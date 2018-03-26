// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/MainThreadWorkletGlobalScope.h"

#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/dom/Document.h"
#include "core/frame/Deprecation.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/MainThreadDebugger.h"
#include "core/probe/CoreProbes.h"
#include "core/workers/GlobalScopeCreationParams.h"

namespace blink {

MainThreadWorkletGlobalScope::MainThreadWorkletGlobalScope(
    LocalFrame* frame,
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    WorkerReportingProxy& reporting_proxy)
    : WorkletGlobalScope(
          std::move(creation_params),
          ToIsolate(frame),
          reporting_proxy,
          // Specify |kUnspecedLoading| because these task runners are used
          // during module loading and this usage is not explicitly spec'ed.
          frame->GetFrameScheduler()->GetTaskRunner(TaskType::kUnspecedLoading),
          frame->GetFrameScheduler()->GetTaskRunner(
              TaskType::kUnspecedLoading)),
      ContextClient(frame) {}

MainThreadWorkletGlobalScope::~MainThreadWorkletGlobalScope() = default;

WorkerThread* MainThreadWorkletGlobalScope::GetThread() const {
  NOTREACHED();
  return nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
MainThreadWorkletGlobalScope::GetTaskRunner(TaskType type) {
  DCHECK(IsContextThread());
  // MainThreadWorkletGlobalScope lives on the main thread and its GetThread()
  // doesn't return a valid worker thread. Instead, retrieve a task runner
  // from the frame.
  return GetFrame()->GetFrameScheduler()->GetTaskRunner(type);
}

// TODO(nhiroki): Add tests for termination.
void MainThreadWorkletGlobalScope::Terminate() {
  Dispose();
}

void MainThreadWorkletGlobalScope::AddConsoleMessage(
    ConsoleMessage* console_message) {
  GetFrame()->Console().AddMessage(console_message);
}

void MainThreadWorkletGlobalScope::ExceptionThrown(ErrorEvent* event) {
  MainThreadDebugger::Instance()->ExceptionThrown(this, event);
}

CoreProbeSink* MainThreadWorkletGlobalScope::GetProbeSink() {
  return probe::ToCoreProbeSink(GetFrame());
}

void MainThreadWorkletGlobalScope::Trace(blink::Visitor* visitor) {
  WorkletGlobalScope::Trace(visitor);
  ContextClient::Trace(visitor);
}

}  // namespace blink
