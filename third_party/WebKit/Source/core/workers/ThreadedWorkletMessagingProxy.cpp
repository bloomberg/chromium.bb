// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/ThreadedWorkletMessagingProxy.h"

#include "base/single_thread_task_runner.h"
#include "bindings/core/v8/V8CacheOptions.h"
#include "core/dom/Document.h"
#include "core/execution_context/SecurityContext.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/inspector/ThreadDebugger.h"
#include "core/origin_trials/OriginTrialContext.h"
#include "core/workers/GlobalScopeCreationParams.h"
#include "core/workers/ThreadedWorkletObjectProxy.h"
#include "core/workers/WorkerClients.h"
#include "core/workers/WorkerInspectorProxy.h"
#include "core/workers/WorkletGlobalScope.h"
#include "core/workers/WorkletModuleResponsesMap.h"
#include "core/workers/WorkletPendingTasks.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WebTaskRunner.h"
#include "public/platform/TaskType.h"

namespace blink {

ThreadedWorkletMessagingProxy::ThreadedWorkletMessagingProxy(
    ExecutionContext* execution_context)
    : ThreadedMessagingProxyBase(execution_context) {}

void ThreadedWorkletMessagingProxy::Initialize(
    WorkerClients* worker_clients,
    WorkletModuleResponsesMap* module_responses_map) {
  DCHECK(IsMainThread());
  if (AskedToTerminate())
    return;

  worklet_object_proxy_ = CreateObjectProxy(this, GetParentFrameTaskRunners());

  Document* document = ToDocument(GetExecutionContext());
  ContentSecurityPolicy* csp = document->GetContentSecurityPolicy();
  DCHECK(csp);

  auto global_scope_creation_params =
      std::make_unique<GlobalScopeCreationParams>(
          document->Url(), document->UserAgent(), csp->Headers().get(),
          document->GetReferrerPolicy(), document->GetSecurityOrigin(),
          document->IsSecureContext(), worker_clients, document->AddressSpace(),
          OriginTrialContext::GetTokens(document).get(),
          base::UnguessableToken::Create(),
          std::make_unique<WorkerSettings>(document->GetSettings()),
          kV8CacheOptionsDefault, module_responses_map);

  // Worklets share the pre-initialized backing thread so that we don't have to
  // specify the backing thread startup data.
  InitializeWorkerThread(std::move(global_scope_creation_params), WTF::nullopt);
}

void ThreadedWorkletMessagingProxy::Trace(blink::Visitor* visitor) {
  ThreadedMessagingProxyBase::Trace(visitor);
}

void ThreadedWorkletMessagingProxy::FetchAndInvokeScript(
    const KURL& module_url_record,
    network::mojom::FetchCredentialsMode credentials_mode,
    scoped_refptr<base::SingleThreadTaskRunner> outside_settings_task_runner,
    WorkletPendingTasks* pending_tasks) {
  DCHECK(IsMainThread());
  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kUnspecedLoading), FROM_HERE,
      CrossThreadBind(&ThreadedWorkletObjectProxy::FetchAndInvokeScript,
                      CrossThreadUnretained(worklet_object_proxy_.get()),
                      module_url_record,
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
