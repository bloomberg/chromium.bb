// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/ThreadedWorkletMessagingProxy.h"

#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/V8CacheOptions.h"
#include "core/dom/Document.h"
#include "core/dom/SecurityContext.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/origin_trials/OriginTrialContext.h"
#include "core/workers/GlobalScopeCreationParams.h"
#include "core/workers/ThreadedWorkletObjectProxy.h"
#include "core/workers/WorkerClients.h"
#include "core/workers/WorkerInspectorProxy.h"
#include "core/workers/WorkletGlobalScope.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WebTaskRunner.h"

namespace blink {

// TODO(nhiroki): This should be in sync with WorkletModuleTreeClient as much as
// possible because this will be replaced with that when module loading is ready
// for threaded worklets (https://crbug.com/727194).
class ThreadedWorkletMessagingProxy::LoaderClient final
    : public GarbageCollectedFinalized<
          ThreadedWorkletMessagingProxy::LoaderClient>,
      public WorkletScriptLoader::Client {
  USING_GARBAGE_COLLECTED_MIXIN(LoaderClient);

 public:
  LoaderClient(RefPtr<WebTaskRunner> outside_settings_task_runner,
               WorkletPendingTasks* pending_tasks,
               ThreadedWorkletMessagingProxy* proxy)
      : outside_settings_task_runner_(std::move(outside_settings_task_runner)),
        pending_tasks_(pending_tasks),
        proxy_(proxy) {}

  // Implementation of the second half of the "fetch and invoke a worklet
  // script" algorithm:
  // https://drafts.css-houdini.org/worklets/#fetch-and-invoke-a-worklet-script
  void NotifyWorkletScriptLoadingFinished(
      WorkletScriptLoader* loader,
      const ScriptSourceCode& source_code) final {
    DCHECK(IsMainThread());
    proxy_->NotifyLoadingFinished(loader);

    if (!loader->WasScriptLoadSuccessful()) {
      // Step 3: "If script is null, then queue a task on outsideSettings's
      // responsible event loop to run these steps:"
      // The steps are implemented in WorkletPendingTasks::Abort().
      outside_settings_task_runner_->PostTask(
          BLINK_FROM_HERE, WTF::Bind(&WorkletPendingTasks::Abort,
                                     WrapPersistent(pending_tasks_.Get())));
      return;
    }

    // Step 4: "Run a module script given script."
    proxy_->EvaluateScript(source_code);

    // Step 5: "Queue a task on outsideSettings's responsible event loop to run
    // these steps:"
    // The steps are implemented in WorkletPendingTasks::DecrementCounter().
    outside_settings_task_runner_->PostTask(
        BLINK_FROM_HERE, WTF::Bind(&WorkletPendingTasks::DecrementCounter,
                                   WrapPersistent(pending_tasks_.Get())));
  }

  DEFINE_INLINE_TRACE() {
    visitor->Trace(pending_tasks_);
    visitor->Trace(proxy_);
  }

 private:
  RefPtr<WebTaskRunner> outside_settings_task_runner_;
  Member<WorkletPendingTasks> pending_tasks_;
  Member<ThreadedWorkletMessagingProxy> proxy_;
};

ThreadedWorkletMessagingProxy::ThreadedWorkletMessagingProxy(
    ExecutionContext* execution_context,
    WorkerClients* worker_clients)
    : ThreadedMessagingProxyBase(execution_context, worker_clients) {
  worklet_object_proxy_ =
      ThreadedWorkletObjectProxy::Create(this, GetParentFrameTaskRunners());
}

void ThreadedWorkletMessagingProxy::Initialize() {
  DCHECK(IsMainThread());
  if (AskedToTerminate())
    return;

  Document* document = ToDocument(GetExecutionContext());
  SecurityOrigin* starter_origin = document->GetSecurityOrigin();
  KURL script_url = document->Url();

  ContentSecurityPolicy* csp = document->GetContentSecurityPolicy();
  DCHECK(csp);

  WorkerThreadStartMode start_mode =
      GetWorkerInspectorProxy()->WorkerStartMode(document);
  std::unique_ptr<WorkerSettings> worker_settings =
      WTF::WrapUnique(new WorkerSettings(document->GetSettings()));

  // TODO(ikilpatrick): Decide on sensible a value for referrerPolicy.
  auto global_scope_creation_params =
      WTF::MakeUnique<GlobalScopeCreationParams>(
          script_url, document->UserAgent(), String(), nullptr, start_mode,
          csp->Headers().get(), /* referrerPolicy */ String(), starter_origin,
          ReleaseWorkerClients(), document->AddressSpace(),
          OriginTrialContext::GetTokens(document).get(),
          std::move(worker_settings), kV8CacheOptionsDefault);

  // Worklets share the pre-initialized backing thread so that we don't have to
  // specify the backing thread startup data.
  InitializeWorkerThread(std::move(global_scope_creation_params), WTF::nullopt,
                         script_url);
}

DEFINE_TRACE(ThreadedWorkletMessagingProxy) {
  visitor->Trace(loaders_);
  ThreadedMessagingProxyBase::Trace(visitor);
}

void ThreadedWorkletMessagingProxy::FetchAndInvokeScript(
    const KURL& module_url_record,
    WebURLRequest::FetchCredentialsMode credentials_mode,
    RefPtr<WebTaskRunner> outside_settings_task_runner,
    WorkletPendingTasks* pending_tasks) {
  DCHECK(IsMainThread());
  LoaderClient* client = new LoaderClient(
      std::move(outside_settings_task_runner), pending_tasks, this);
  WorkletScriptLoader* loader = WorkletScriptLoader::Create(
      ToDocument(GetExecutionContext())->Fetcher(), client);
  loaders_.insert(loader);
  loader->FetchScript(module_url_record);
}

void ThreadedWorkletMessagingProxy::WorkletObjectDestroyed() {
  DCHECK(IsMainThread());
  ParentObjectDestroyed();
}

void ThreadedWorkletMessagingProxy::TerminateWorkletGlobalScope() {
  DCHECK(IsMainThread());
  for (const auto& loader : loaders_)
    loader->Cancel();
  loaders_.clear();
  TerminateGlobalScope();
}

void ThreadedWorkletMessagingProxy::NotifyLoadingFinished(
    WorkletScriptLoader* loader) {
  loaders_.erase(loader);
}

void ThreadedWorkletMessagingProxy::EvaluateScript(
    const ScriptSourceCode& script_source_code) {
  DCHECK(IsMainThread());
  TaskRunnerHelper::Get(TaskType::kMiscPlatformAPI, GetWorkerThread())
      ->PostTask(
          BLINK_FROM_HERE,
          CrossThreadBind(&ThreadedWorkletObjectProxy::EvaluateScript,
                          CrossThreadUnretained(worklet_object_proxy_.get()),
                          script_source_code.Source(), script_source_code.Url(),
                          CrossThreadUnretained(GetWorkerThread())));
}

}  // namespace blink
