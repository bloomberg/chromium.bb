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
#include "core/workers/WorkletGlobalScopeProxy.h"
#include "core/workers/WorkletObjectProxy.h"

namespace blink {

class ConsoleMessage;
class LocalFrame;
class ScriptSourceCode;

class CORE_EXPORT MainThreadWorkletGlobalScope
    : public WorkletGlobalScope,
      public WorkletGlobalScopeProxy,
      public WorkletScriptLoader::Client,
      public ContextClient {
  USING_GARBAGE_COLLECTED_MIXIN(MainThreadWorkletGlobalScope);

 public:
  MainThreadWorkletGlobalScope(LocalFrame*,
                               const KURL&,
                               const String& user_agent,
                               PassRefPtr<SecurityOrigin>,
                               v8::Isolate*,
                               WorkletObjectProxy*);
  ~MainThreadWorkletGlobalScope() override;
  bool IsMainThreadWorkletGlobalScope() const final { return true; }

  // WorkerOrWorkletGlobalScope
  void CountFeature(UseCounter::Feature) final;
  void CountDeprecation(UseCounter::Feature) final;
  WorkerThread* GetThread() const final;

  // WorkletGlobalScopeProxy
  void FetchAndInvokeScript(int32_t request_id, const KURL& script_url) final;
  void EvaluateScript(const ScriptSourceCode&) final;
  void TerminateWorkletGlobalScope() final;

  // WorkletScriptLoader::Client
  void NotifyWorkletScriptLoadingFinished(WorkletScriptLoader*,
                                          const ScriptSourceCode&) final;

  // ExecutionContext
  void AddConsoleMessage(ConsoleMessage*) final;
  void ExceptionThrown(ErrorEvent*) final;

  DECLARE_VIRTUAL_TRACE();

 private:
  HeapHashSet<Member<WorkletScriptLoader>> loader_set_;

  Member<WorkletObjectProxy> object_proxy_;
};

DEFINE_TYPE_CASTS(MainThreadWorkletGlobalScope,
                  ExecutionContext,
                  context,
                  context->IsMainThreadWorkletGlobalScope(),
                  context.IsMainThreadWorkletGlobalScope());

}  // namespace blink

#endif  // MainThreadWorkletGlobalScope_h
