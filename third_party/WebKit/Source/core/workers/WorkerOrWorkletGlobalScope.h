// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkerOrWorkletGlobalScope_h
#define WorkerOrWorkletGlobalScope_h

#include "bindings/core/v8/V8CacheOptions.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/WebFeatureForward.h"
#include "core/workers/WorkerClients.h"
#include "platform/wtf/BitVector.h"

namespace blink {

class ResourceFetcher;
class ScriptWrappable;
class WorkerOrWorkletScriptController;
class WorkerReportingProxy;
class WorkerThread;

class CORE_EXPORT WorkerOrWorkletGlobalScope : public ExecutionContext {
 public:
  WorkerOrWorkletGlobalScope(v8::Isolate*,
                             WorkerClients*,
                             WorkerReportingProxy&);
  virtual ~WorkerOrWorkletGlobalScope();

  // ExecutionContext
  bool IsWorkerOrWorkletGlobalScope() const final { return true; }
  bool IsJSExecutionForbidden() const final;
  void DisableEval(const String& error_message) final;
  bool CanExecuteScripts(ReasonForCallingCanExecuteScripts) final;

  virtual ScriptWrappable* GetScriptWrappable() const = 0;

  // Evaluates the given main script as a classic script (as opposed to a module
  // script).
  // https://html.spec.whatwg.org/multipage/webappapis.html#classic-script
  virtual void EvaluateClassicScript(
      const KURL& script_url,
      String source_code,
      std::unique_ptr<Vector<char>> cached_meta_data) = 0;

  // Returns true when the WorkerOrWorkletGlobalScope is closing (e.g. via
  // WorkerGlobalScope#close() method). If this returns true, the worker is
  // going to be shutdown after the current task execution. Globals that
  // don't support close operation should always return false.
  virtual bool IsClosing() const = 0;

  // Should be called before destroying the global scope object. Allows
  // sub-classes to perform any cleanup needed.
  virtual void Dispose();

  // Called from UseCounter to record API use in this execution context.
  void CountFeature(WebFeature);

  // Called from UseCounter to record deprecated API use in this execution
  // context.
  void CountDeprecation(WebFeature);

  // May return nullptr if this global scope is not threaded (i.e.,
  // MainThreadWorkletGlobalScope) or after dispose() is called.
  virtual WorkerThread* GetThread() const = 0;

  // Available only when off-main-thread-fetch is enabled.
  ResourceFetcher* GetResourceFetcher();

  WorkerClients* Clients() const { return worker_clients_.Get(); }

  WorkerOrWorkletScriptController* ScriptController() {
    return script_controller_.Get();
  }

  WorkerReportingProxy& ReportingProxy() { return reporting_proxy_; }

  DECLARE_VIRTUAL_TRACE();

 private:
  CrossThreadPersistent<WorkerClients> worker_clients_;
  Member<ResourceFetcher> resource_fetcher_;
  Member<WorkerOrWorkletScriptController> script_controller_;

  WorkerReportingProxy& reporting_proxy_;

  // This is the set of features that this worker has used.
  BitVector used_features_;
};

DEFINE_TYPE_CASTS(
    WorkerOrWorkletGlobalScope,
    ExecutionContext,
    context,
    (context->IsWorkerGlobalScope() || context->IsWorkletGlobalScope()),
    (context.IsWorkerGlobalScope() || context.IsWorkletGlobalScope()));

}  // namespace blink

#endif  // WorkerOrWorkletGlobalScope_h
