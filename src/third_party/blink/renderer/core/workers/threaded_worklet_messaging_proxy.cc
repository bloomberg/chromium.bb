// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/threaded_worklet_messaging_proxy.h"

#include "base/single_thread_task_runner.h"
#include "third_party/blink/public/mojom/script/script_type.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_cache_options.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/inspector/thread_debugger.h"
#include "third_party/blink/renderer/core/loader/worker_fetch_context.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/threaded_worklet_object_proxy.h"
#include "third_party/blink/renderer/core/workers/worker_clients.h"
#include "third_party/blink/renderer/core/workers/worker_content_settings_client.h"
#include "third_party/blink/renderer/core/workers/worklet_global_scope.h"
#include "third_party/blink/renderer/core/workers/worklet_module_responses_map.h"
#include "third_party/blink/renderer/core/workers/worklet_pending_tasks.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/loader/fetch/worker_resource_timing_notifier.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"

namespace blink {

ThreadedWorkletMessagingProxy::ThreadedWorkletMessagingProxy(
    ExecutionContext* execution_context)
    : ThreadedMessagingProxyBase(execution_context) {}

void ThreadedWorkletMessagingProxy::Initialize(
    WorkerClients* worker_clients,
    WorkletModuleResponsesMap* module_responses_map,
    const base::Optional<WorkerBackingThreadStartupData>& thread_startup_data) {
  DCHECK(IsMainThread());
  if (AskedToTerminate())
    return;

  worklet_object_proxy_ =
      CreateObjectProxy(this, GetParentExecutionContextTaskRunners());

  // For now we don't use global scope name for threaded worklets.
  // TODO(nhiroki): Threaded worklets may want to have the global scope name to
  // distinguish multiple worklets created from the same script URL like
  // LayoutWorklet and PaintWorklet.
  const String global_scope_name = g_empty_string;

  Document* document = To<Document>(GetExecutionContext());
  ContentSecurityPolicy* csp = document->GetContentSecurityPolicy();
  DCHECK(csp);

  ProvideContentSettingsClientToWorker(
      worker_clients,
      document->GetFrame()->Client()->CreateWorkerContentSettingsClient());

  auto global_scope_creation_params =
      std::make_unique<GlobalScopeCreationParams>(
          document->Url(), mojom::ScriptType::kModule,
          OffMainThreadWorkerScriptFetchOption::kEnabled, global_scope_name,
          document->UserAgent(),
          document->GetFrame()->Client()->CreateWorkerFetchContext(),
          csp->Headers(), document->GetReferrerPolicy(),
          document->GetSecurityOrigin(), document->IsSecureContext(),
          document->GetHttpsState(), worker_clients, document->AddressSpace(),
          OriginTrialContext::GetTokens(document).get(),
          base::UnguessableToken::Create(),
          std::make_unique<WorkerSettings>(document->GetSettings()),
          kV8CacheOptionsDefault, module_responses_map,
          service_manager::mojom::blink::InterfaceProviderPtrInfo(),
          BeginFrameProviderParams(), nullptr /* parent_feature_policy */,
          document->GetAgentClusterID());

  // Worklets share the pre-initialized backing thread so that we don't have to
  // specify the backing thread startup data.
  InitializeWorkerThread(std::move(global_scope_creation_params),
                         thread_startup_data);
}

void ThreadedWorkletMessagingProxy::Trace(blink::Visitor* visitor) {
  ThreadedMessagingProxyBase::Trace(visitor);
}

void ThreadedWorkletMessagingProxy::FetchAndInvokeScript(
    const KURL& module_url_record,
    network::mojom::FetchCredentialsMode credentials_mode,
    const FetchClientSettingsObjectSnapshot& outside_settings_object,
    WorkerResourceTimingNotifier& outside_resource_timing_notifier,
    scoped_refptr<base::SingleThreadTaskRunner> outside_settings_task_runner,
    WorkletPendingTasks* pending_tasks) {
  DCHECK(IsMainThread());
  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kInternalLoading), FROM_HERE,
      CrossThreadBindOnce(
          &ThreadedWorkletObjectProxy::FetchAndInvokeScript,
          CrossThreadUnretained(worklet_object_proxy_.get()), module_url_record,
          credentials_mode, WTF::Passed(outside_settings_object.CopyData()),
          WrapCrossThreadPersistent(&outside_resource_timing_notifier),
          std::move(outside_settings_task_runner),
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
    ParentExecutionContextTaskRunners* parent_execution_context_task_runners) {
  return ThreadedWorkletObjectProxy::Create(
      messaging_proxy, parent_execution_context_task_runners);
}

ThreadedWorkletObjectProxy&
    ThreadedWorkletMessagingProxy::WorkletObjectProxy() {
  DCHECK(worklet_object_proxy_);
  return *worklet_object_proxy_;
}

}  // namespace blink
