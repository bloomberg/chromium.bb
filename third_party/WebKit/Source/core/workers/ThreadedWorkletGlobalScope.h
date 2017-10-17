// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ThreadedWorkletGlobalScope_h
#define ThreadedWorkletGlobalScope_h

#include "core/CoreExport.h"
#include "core/workers/WorkletGlobalScope.h"

namespace blink {

class WorkerThread;

class CORE_EXPORT ThreadedWorkletGlobalScope : public WorkletGlobalScope {
 public:
  ~ThreadedWorkletGlobalScope() override;
  void Dispose() override;

  // ExecutionContext
  bool IsThreadedWorkletGlobalScope() const final { return true; }
  bool IsContextThread() const final;
  void AddConsoleMessage(ConsoleMessage*) final;
  void ExceptionThrown(ErrorEvent*) final;

  WorkerThread* GetThread() const { return thread_; }

 protected:
  ThreadedWorkletGlobalScope(const KURL&,
                             const String& user_agent,
                             RefPtr<SecurityOrigin> document_security_origin,
                             v8::Isolate*,
                             WorkerThread*,
                             WorkerClients*);

 private:
  friend class ThreadedWorkletThreadForTest;

  WorkerThread* thread_;
};

DEFINE_TYPE_CASTS(ThreadedWorkletGlobalScope,
                  ExecutionContext,
                  context,
                  context->IsThreadedWorkletGlobalScope(),
                  context.IsThreadedWorkletGlobalScope());

}  // namespace blink

#endif  // ThreadedWorkletGlobalScope_h
