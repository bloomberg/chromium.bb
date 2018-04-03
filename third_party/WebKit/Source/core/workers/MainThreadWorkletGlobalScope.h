// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MainThreadWorkletGlobalScope_h
#define MainThreadWorkletGlobalScope_h

#include "core/CoreExport.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/execution_context/ExecutionContext.h"
#include "core/workers/WorkletGlobalScope.h"

namespace blink {

class ConsoleMessage;
class LocalFrame;
class WorkerReportingProxy;

class CORE_EXPORT MainThreadWorkletGlobalScope
    : public WorkletGlobalScope,
      public ContextClient {
  USING_GARBAGE_COLLECTED_MIXIN(MainThreadWorkletGlobalScope);

 public:
  MainThreadWorkletGlobalScope(LocalFrame*,
                               std::unique_ptr<GlobalScopeCreationParams>,
                               WorkerReportingProxy&);
  ~MainThreadWorkletGlobalScope() override;

  bool IsMainThreadWorkletGlobalScope() const final { return true; }

  // WorkerOrWorkletGlobalScope
  WorkerThread* GetThread() const final;
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(TaskType) override;

  void Terminate();

  // ExecutionContext
  void AddConsoleMessage(ConsoleMessage*) final;
  void ExceptionThrown(ErrorEvent*) final;
  CoreProbeSink* GetProbeSink() final;

  void Trace(blink::Visitor*) override;
};

DEFINE_TYPE_CASTS(MainThreadWorkletGlobalScope,
                  ExecutionContext,
                  context,
                  context->IsMainThreadWorkletGlobalScope(),
                  context.IsMainThreadWorkletGlobalScope());

}  // namespace blink

#endif  // MainThreadWorkletGlobalScope_h
