// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/MainThreadWorkletGlobalScope.h"

#include "core/dom/Document.h"
#include "core/frame/Deprecation.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/MainThreadDebugger.h"
#include "core/probe/CoreProbes.h"

namespace blink {

MainThreadWorkletGlobalScope::MainThreadWorkletGlobalScope(
    LocalFrame* frame,
    const KURL& url,
    const String& user_agent,
    v8::Isolate* isolate,
    WorkerReportingProxy& reporting_proxy)
    : WorkletGlobalScope(url,
                         user_agent,
                         frame->GetDocument()->GetSecurityOrigin(),
                         isolate,
                         nullptr /* worker_clients */,
                         reporting_proxy),
      ContextClient(frame) {}

MainThreadWorkletGlobalScope::~MainThreadWorkletGlobalScope() {}

WorkerThread* MainThreadWorkletGlobalScope::GetThread() const {
  NOTREACHED();
  return nullptr;
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
