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
  WorkerOrWorkletGlobalScope();
  virtual ~WorkerOrWorkletGlobalScope();

  // ExecutionContext
  bool isWorkerOrWorkletGlobalScope() const final { return true; }
  void postTask(TaskType,
                const WebTraceLocation&,
                std::unique_ptr<ExecutionContextTask>,
                const String& taskNameForInstrumentation = emptyString) final;

  virtual ScriptWrappable* getScriptWrappable() const = 0;
  virtual WorkerOrWorkletScriptController* scriptController() = 0;

  // Returns true when the WorkerOrWorkletGlobalScope is closing (e.g. via
  // WorkerGlobalScope#close() method). If this returns true, the worker is
  // going to be shutdown after the current task execution. Globals that
  // don't support close operation should always return false.
  virtual bool isClosing() const = 0;

  // Should be called before destroying the global scope object. Allows
  // sub-classes to perform any cleanup needed.
  virtual void dispose() = 0;

  // Called from UseCounter to record API use in this execution context.
  virtual void countFeature(UseCounter::Feature) = 0;

  // Called from UseCounter to record deprecated API use in this execution
  // context. Sub-classes should call addDeprecationMessage() in this function.
  virtual void countDeprecation(UseCounter::Feature) = 0;

  // May return nullptr if this global scope is not threaded (i.e.,
  // MainThreadWorkletGlobalScope) or after dispose() is called.
  virtual WorkerThread* thread() const = 0;

 protected:
  // Adds a deprecation message to the console.
  void addDeprecationMessage(UseCounter::Feature);

 private:
  void runTask(std::unique_ptr<ExecutionContextTask>, bool isInstrumented);

  BitVector m_deprecationWarningBits;
};

DEFINE_TYPE_CASTS(
    WorkerOrWorkletGlobalScope,
    ExecutionContext,
    context,
    (context->isWorkerGlobalScope() || context->isWorkletGlobalScope()),
    (context.isWorkerGlobalScope() || context.isWorkletGlobalScope()));

}  // namespace blink

#endif  // WorkerOrWorkletGlobalScope_h
