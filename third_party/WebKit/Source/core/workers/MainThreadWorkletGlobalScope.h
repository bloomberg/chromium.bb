// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MainThreadWorkletGlobalScope_h
#define MainThreadWorkletGlobalScope_h

#include "core/CoreExport.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/ExecutionContext.h"
#include "core/loader/WorkletScriptLoader.h"
#include "core/workers/WorkletGlobalScope.h"
#include "core/workers/WorkletPendingTasks.h"

namespace blink {

class ConsoleMessage;
class LocalFrame;
class ScriptSourceCode;

class CORE_EXPORT MainThreadWorkletGlobalScope
    : public WorkletGlobalScope,
      public WorkletScriptLoader::Client,
      public ContextClient {
  USING_GARBAGE_COLLECTED_MIXIN(MainThreadWorkletGlobalScope);

 public:
  MainThreadWorkletGlobalScope(LocalFrame*,
                               const KURL&,
                               const String& user_agent,
                               PassRefPtr<SecurityOrigin>,
                               v8::Isolate*);
  ~MainThreadWorkletGlobalScope() override;
  bool IsMainThreadWorkletGlobalScope() const final { return true; }

  // WorkerOrWorkletGlobalScope
  void ReportFeature(UseCounter::Feature) override;
  void ReportDeprecation(UseCounter::Feature) override;
  WorkerThread* GetThread() const final;

  void FetchAndInvokeScript(const KURL& module_url_record,
                            WorkletPendingTasks*);
  void Terminate();

  // WorkletScriptLoader::Client
  void NotifyWorkletScriptLoadingFinished(WorkletScriptLoader*,
                                          const ScriptSourceCode&) final;

  // ExecutionContext
  void AddConsoleMessage(ConsoleMessage*) final;
  void ExceptionThrown(ErrorEvent*) final;
  CoreProbeSink* GetProbeSink() final;

  DECLARE_VIRTUAL_TRACE();

 private:
  HeapHashMap<Member<WorkletScriptLoader>, Member<WorkletPendingTasks>>
      loader_map_;
};

DEFINE_TYPE_CASTS(MainThreadWorkletGlobalScope,
                  ExecutionContext,
                  context,
                  context->IsMainThreadWorkletGlobalScope(),
                  context.IsMainThreadWorkletGlobalScope());

}  // namespace blink

#endif  // MainThreadWorkletGlobalScope_h
