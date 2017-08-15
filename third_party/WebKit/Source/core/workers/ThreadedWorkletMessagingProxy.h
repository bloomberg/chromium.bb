// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ThreadedWorkletMessagingProxy_h
#define ThreadedWorkletMessagingProxy_h

#include "core/CoreExport.h"
#include "core/loader/WorkletScriptLoader.h"
#include "core/workers/ThreadedMessagingProxyBase.h"
#include "core/workers/WorkletGlobalScopeProxy.h"
#include "core/workers/WorkletPendingTasks.h"

namespace blink {

class ScriptSourceCode;
class ThreadedWorkletObjectProxy;
class WorkerClients;

class CORE_EXPORT ThreadedWorkletMessagingProxy
    : public ThreadedMessagingProxyBase,
      public WorkletGlobalScopeProxy {
  USING_GARBAGE_COLLECTED_MIXIN(ThreadedWorkletMessagingProxy);

 public:
  // WorkletGlobalScopeProxy implementation.
  void FetchAndInvokeScript(const KURL& module_url_record,
                            WorkletModuleResponsesMap*,
                            WebURLRequest::FetchCredentialsMode,
                            RefPtr<WebTaskRunner> outside_settings_task_runner,
                            WorkletPendingTasks*) final;
  void WorkletObjectDestroyed() final;
  void TerminateWorkletGlobalScope() final;

  void Initialize();

  DECLARE_VIRTUAL_TRACE();

 protected:
  ThreadedWorkletMessagingProxy(ExecutionContext*, WorkerClients*);

  ThreadedWorkletObjectProxy& WorkletObjectProxy() {
    return *worklet_object_proxy_;
  }

 private:
  friend class ThreadedWorkletMessagingProxyForTest;
  class LoaderClient;

  virtual std::unique_ptr<ThreadedWorkletObjectProxy> CreateObjectProxy(
      ThreadedWorkletMessagingProxy*,
      ParentFrameTaskRunners*);

  void NotifyLoadingFinished(WorkletScriptLoader*);
  void EvaluateScript(const ScriptSourceCode&);

  std::unique_ptr<ThreadedWorkletObjectProxy> worklet_object_proxy_;

  HeapHashSet<Member<WorkletScriptLoader>> loaders_;
};

}  // namespace blink

#endif  // ThreadedWorkletMessagingProxy_h
