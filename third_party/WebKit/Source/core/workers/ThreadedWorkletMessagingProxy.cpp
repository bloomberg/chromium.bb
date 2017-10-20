// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/ThreadedWorkletMessagingProxy.h"

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
#include "core/workers/WorkletPendingTasks.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WebTaskRunner.h"

namespace blink {

ThreadedWorkletMessagingProxy::ThreadedWorkletMessagingProxy(
    ExecutionContext* execution_context,
    WorkerClients* worker_clients)
    : ThreadedMessagingProxyBase(execution_context, worker_clients) {}

void ThreadedWorkletMessagingProxy::Initialize() {
  DCHECK(IsMainThread());
  if (AskedToTerminate())
    return;

  worklet_object_proxy_ = CreateObjectProxy(this, GetParentFrameTaskRunners());

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

void ThreadedWorkletMessagingProxy::Trace(blink::Visitor* visitor) {
  ThreadedMessagingProxyBase::Trace(visitor);
}

void ThreadedWorkletMessagingProxy::FetchAndInvokeScript(
    const KURL& module_url_record,
    WorkletModuleResponsesMap* module_responses_map,
    WebURLRequest::FetchCredentialsMode credentials_mode,
    scoped_refptr<WebTaskRunner> outside_settings_task_runner,
    WorkletPendingTasks* pending_tasks) {
  DCHECK(IsMainThread());
  TaskRunnerHelper::Get(TaskType::kUnspecedLoading, GetWorkerThread())
      ->PostTask(BLINK_FROM_HERE,
                 CrossThreadBind(
                     &ThreadedWorkletObjectProxy::FetchAndInvokeScript,
                     CrossThreadUnretained(worklet_object_proxy_.get()),
                     module_url_record,
                     WrapCrossThreadPersistent(module_responses_map),
                     credentials_mode, std::move(outside_settings_task_runner),
                     WrapCrossThreadPersistent(pending_tasks),
                     CrossThreadUnretained(GetWorkerThread())));
}

void ThreadedWorkletMessagingProxy::WorkletObjectDestroyed() {
  DCHECK(IsMainThread());
  ParentObjectDestroyed();
}

void ThreadedWorkletMessagingProxy::TerminateWorkletGlobalScope() {
  DCHECK(IsMainThread());
  TerminateGlobalScope();
}

std::unique_ptr<ThreadedWorkletObjectProxy>
    ThreadedWorkletMessagingProxy::CreateObjectProxy(
        ThreadedWorkletMessagingProxy* messaging_proxy,
        ParentFrameTaskRunners* parent_frame_task_runners) {
  return ThreadedWorkletObjectProxy::Create(messaging_proxy,
                                            parent_frame_task_runners);
}

ThreadedWorkletObjectProxy&
    ThreadedWorkletMessagingProxy::WorkletObjectProxy() {
  DCHECK(worklet_object_proxy_);
  return *worklet_object_proxy_;
}

}  // namespace blink
