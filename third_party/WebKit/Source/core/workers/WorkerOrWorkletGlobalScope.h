// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkerOrWorkletGlobalScope_h
#define WorkerOrWorkletGlobalScope_h

#include "core/dom/ExecutionContext.h"
#include "core/frame/UseCounter.h"

namespace blink {

class ScriptWrappable;
class WorkerOrWorkletScriptController;
class WorkerThread;

class CORE_EXPORT WorkerOrWorkletGlobalScope : public ExecutionContext {
 public:
  explicit WorkerOrWorkletGlobalScope(v8::Isolate*);
  virtual ~WorkerOrWorkletGlobalScope();

  // ExecutionContext
  bool IsWorkerOrWorkletGlobalScope() const final { return true; }
  bool IsJSExecutionForbidden() const final;
  void DisableEval(const String& error_message) final;
  void PostTask(
      TaskType,
      const WebTraceLocation&,
      std::unique_ptr<ExecutionContextTask>,
      const String& task_name_for_instrumentation = g_empty_string) final;

  virtual ScriptWrappable* GetScriptWrappable() const = 0;

  // Returns true when the WorkerOrWorkletGlobalScope is closing (e.g. via
  // WorkerGlobalScope#close() method). If this returns true, the worker is
  // going to be shutdown after the current task execution. Globals that
  // don't support close operation should always return false.
  virtual bool IsClosing() const = 0;

  // Should be called before destroying the global scope object. Allows
  // sub-classes to perform any cleanup needed.
  virtual void Dispose();

  // Called from UseCounter to record API use in this execution context.
  void CountFeature(UseCounter::Feature);

  // Called from UseCounter to record deprecated API use in this execution
  // context.
  void CountDeprecation(UseCounter::Feature);

  // May return nullptr if this global scope is not threaded (i.e.,
  // MainThreadWorkletGlobalScope) or after dispose() is called.
  virtual WorkerThread* GetThread() const = 0;

  WorkerOrWorkletScriptController* ScriptController() {
    return script_controller_.Get();
  }

  DECLARE_VIRTUAL_TRACE();

 protected:
  virtual void ReportFeature(UseCounter::Feature) = 0;
  virtual void ReportDeprecation(UseCounter::Feature) = 0;

 private:
  void RunTask(std::unique_ptr<ExecutionContextTask>, bool is_instrumented);

  Member<WorkerOrWorkletScriptController> script_controller_;

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
