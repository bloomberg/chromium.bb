// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MainThreadWorkletGlobalScope_h
#define MainThreadWorkletGlobalScope_h

#include "core/CoreExport.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/ExecutionContext.h"
#include "core/workers/WorkletGlobalScope.h"
#include "core/workers/WorkletGlobalScopeProxy.h"

namespace blink {

class ConsoleMessage;
class LocalFrame;
class ScriptSourceCode;

class CORE_EXPORT MainThreadWorkletGlobalScope : public WorkletGlobalScope,
                                                 public WorkletGlobalScopeProxy,
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
  void CountFeature(UseCounter::Feature) final;
  void CountDeprecation(UseCounter::Feature) final;
  WorkerThread* GetThread() const final;

  // WorkletGlobalScopeProxy
  void EvaluateScript(const ScriptSourceCode&) final;
  void TerminateWorkletGlobalScope() final;

  void AddConsoleMessage(ConsoleMessage*) final;
  void ExceptionThrown(ErrorEvent*) final;

  DEFINE_INLINE_VIRTUAL_TRACE() {
    WorkletGlobalScope::Trace(visitor);
    ContextClient::Trace(visitor);
  }
};

DEFINE_TYPE_CASTS(MainThreadWorkletGlobalScope,
                  ExecutionContext,
                  context,
                  context->IsMainThreadWorkletGlobalScope(),
                  context.IsMainThreadWorkletGlobalScope());

}  // namespace blink

#endif  // MainThreadWorkletGlobalScope_h
