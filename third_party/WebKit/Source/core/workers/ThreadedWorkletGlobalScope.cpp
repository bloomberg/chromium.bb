// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/ThreadedWorkletGlobalScope.h"

#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/ConsoleMessageStorage.h"
#include "core/inspector/WorkerThreadDebugger.h"
#include "core/workers/WorkerReportingProxy.h"
#include "core/workers/WorkerThread.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/RefPtr.h"
#include "public/platform/Platform.h"

namespace blink {

ThreadedWorkletGlobalScope::ThreadedWorkletGlobalScope(
    const KURL& url,
    const String& user_agent,
    RefPtr<SecurityOrigin> document_security_origin,
    v8::Isolate* isolate,
    WorkerThread* thread,
    WorkerClients* worker_clients)
    : WorkletGlobalScope(url,
                         user_agent,
                         std::move(document_security_origin),
                         isolate,
                         worker_clients,
                         thread->GetWorkerReportingProxy()),
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
