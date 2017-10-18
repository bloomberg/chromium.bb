// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/DedicatedWorkerMessagingProxy.h"

#include <memory>
#include "bindings/core/v8/V8CacheOptions.h"
#include "core/dom/Document.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/events/ErrorEvent.h"
#include "core/events/MessageEvent.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/origin_trials/OriginTrialContext.h"
#include "core/workers/DedicatedWorker.h"
#include "core/workers/DedicatedWorkerObjectProxy.h"
#include "core/workers/DedicatedWorkerThread.h"
#include "core/workers/GlobalScopeCreationParams.h"
#include "core/workers/WorkerClients.h"
#include "core/workers/WorkerInspectorProxy.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WebTaskRunner.h"
#include "platform/wtf/WTF.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/service_manager/public/interfaces/interface_provider.mojom-blink.h"
#include "third_party/WebKit/public/platform/dedicated_worker_factory.mojom-blink.h"

namespace blink {
namespace {

service_manager::mojom::blink::InterfaceProviderPtrInfo
ConnectToWorkerInterfaceProvider(Document* document,
                                 RefPtr<SecurityOrigin> script_origin) {
  mojom::blink::DedicatedWorkerFactoryPtr worker_factory;
  document->GetInterfaceProvider()->GetInterface(&worker_factory);
  service_manager::mojom::blink::InterfaceProviderPtrInfo
      interface_provider_ptr;
  worker_factory->CreateDedicatedWorker(
      script_origin, mojo::MakeRequest(&interface_provider_ptr));
  return interface_provider_ptr;
}

}  // namespace

struct DedicatedWorkerMessagingProxy::QueuedTask {
  RefPtr<SerializedScriptValue> message;
  Vector<MessagePortChannel> channels;
};

DedicatedWorkerMessagingProxy::DedicatedWorkerMessagingProxy(
    ExecutionContext* execution_context,
    DedicatedWorker* worker_object,
    WorkerClients* worker_clients)
    : ThreadedMessagingProxyBase(execution_context, worker_clients),
      worker_object_(worker_object) {
  worker_object_proxy_ =
      DedicatedWorkerObjectProxy::Create(this, GetParentFrameTaskRunners());
}

DedicatedWorkerMessagingProxy::~DedicatedWorkerMessagingProxy() = default;

void DedicatedWorkerMessagingProxy::StartWorkerGlobalScope(
    const KURL& script_url,
    const String& user_agent,
    const String& source_code,
    const String& referrer_policy) {
  DCHECK(IsParentContextThread());
  if (AskedToTerminate()) {
    // Worker.terminate() could be called from JS before the thread was
    // created.
    return;
  }

  Document* document = ToDocument(GetExecutionContext());
  SecurityOrigin* starter_origin = document->GetSecurityOrigin();

  ContentSecurityPolicy* csp = document->GetContentSecurityPolicy();
  DCHECK(csp);

  WorkerThreadStartMode start_mode =
      GetWorkerInspectorProxy()->WorkerStartMode(document);
  std::unique_ptr<WorkerSettings> worker_settings =
      WTF::WrapUnique(new WorkerSettings(document->GetSettings()));

  auto global_scope_creation_params =
      WTF::MakeUnique<GlobalScopeCreationParams>(
          script_url, user_agent, source_code, nullptr, start_mode,
          csp->Headers().get(), referrer_policy, starter_origin,
          ReleaseWorkerClients(), document->AddressSpace(),
          OriginTrialContext::GetTokens(document).get(),
          std::move(worker_settings), kV8CacheOptionsDefault,
          ConnectToWorkerInterfaceProvider(document,
                                           SecurityOrigin::Create(script_url)));

  InitializeWorkerThread(std::move(global_scope_creation_params),
                         CreateBackingThreadStartupData(ToIsolate(document)),
                         script_url);
}

void DedicatedWorkerMessagingProxy::PostMessageToWorkerGlobalScope(
    RefPtr<SerializedScriptValue> message,
    Vector<MessagePortChannel> channels) {
  DCHECK(IsParentContextThread());
  if (AskedToTerminate())
    return;

  if (GetWorkerThread()) {
    WTF::CrossThreadClosure task = CrossThreadBind(
        &DedicatedWorkerObjectProxy::ProcessMessageFromWorkerObject,
        CrossThreadUnretained(&WorkerObjectProxy()), std::move(message),
        WTF::Passed(std::move(channels)),
        CrossThreadUnretained(GetWorkerThread()));
    TaskRunnerHelper::Get(TaskType::kPostedMessage, GetWorkerThread())
        ->PostTask(BLINK_FROM_HERE, std::move(task));
  } else {
    queued_early_tasks_.push_back(
        QueuedTask{std::move(message), std::move(channels)});
  }
}

void DedicatedWorkerMessagingProxy::WorkerThreadCreated() {
  DCHECK(IsParentContextThread());
  ThreadedMessagingProxyBase::WorkerThreadCreated();

  for (auto& queued_task : queued_early_tasks_) {
    WTF::CrossThreadClosure task = CrossThreadBind(
        &DedicatedWorkerObjectProxy::ProcessMessageFromWorkerObject,
        CrossThreadUnretained(&WorkerObjectProxy()),
        std::move(queued_task.message),
        WTF::Passed(std::move(queued_task.channels)),
        CrossThreadUnretained(GetWorkerThread()));
    TaskRunnerHelper::Get(TaskType::kPostedMessage, GetWorkerThread())
        ->PostTask(BLINK_FROM_HERE, std::move(task));
  }
  queued_early_tasks_.clear();
}

bool DedicatedWorkerMessagingProxy::HasPendingActivity() const {
  DCHECK(IsParentContextThread());
  return !AskedToTerminate();
}

void DedicatedWorkerMessagingProxy::PostMessageToWorkerObject(
    RefPtr<SerializedScriptValue> message,
    Vector<MessagePortChannel> channels) {
  DCHECK(IsParentContextThread());
  if (!worker_object_ || AskedToTerminate())
    return;

  MessagePortArray* ports =
      MessagePort::EntanglePorts(*GetExecutionContext(), std::move(channels));
  worker_object_->DispatchEvent(
      MessageEvent::Create(ports, std::move(message)));
}

void DedicatedWorkerMessagingProxy::DispatchErrorEvent(
    const String& error_message,
    std::unique_ptr<SourceLocation> location,
    int exception_id) {
  DCHECK(IsParentContextThread());
  if (!worker_object_)
    return;

  // We don't bother checking the askedToTerminate() flag here, because
  // exceptions should *always* be reported even if the thread is terminated.
  // This is intentionally different than the behavior in MessageWorkerTask,
  // because terminated workers no longer deliver messages (section 4.6 of the
  // WebWorker spec), but they do report exceptions.

  ErrorEvent* event =
      ErrorEvent::Create(error_message, location->Clone(), nullptr);
  if (worker_object_->DispatchEvent(event) != DispatchEventResult::kNotCanceled)
    return;

  // The HTML spec requires to queue an error event using the DOM manipulation
  // task source.
  // https://html.spec.whatwg.org/multipage/workers.html#runtime-script-errors-2
  TaskRunnerHelper::Get(TaskType::kDOMManipulation, GetWorkerThread())
      ->PostTask(BLINK_FROM_HERE,
                 CrossThreadBind(
                     &DedicatedWorkerObjectProxy::ProcessUnhandledException,
                     CrossThreadUnretained(worker_object_proxy_.get()),
                     exception_id, CrossThreadUnretained(GetWorkerThread())));
}

DEFINE_TRACE(DedicatedWorkerMessagingProxy) {
  visitor->Trace(worker_object_);
  ThreadedMessagingProxyBase::Trace(visitor);
}

WTF::Optional<WorkerBackingThreadStartupData>
DedicatedWorkerMessagingProxy::CreateBackingThreadStartupData(
    v8::Isolate* isolate) {
  using HeapLimitMode = WorkerBackingThreadStartupData::HeapLimitMode;
  using AtomicsWaitMode = WorkerBackingThreadStartupData::AtomicsWaitMode;
  return WorkerBackingThreadStartupData(
      isolate->IsHeapLimitIncreasedForDebugging()
          ? HeapLimitMode::kIncreasedForDebugging
          : HeapLimitMode::kDefault,
      AtomicsWaitMode::kAllow);
}

std::unique_ptr<WorkerThread>
DedicatedWorkerMessagingProxy::CreateWorkerThread() {
  return DedicatedWorkerThread::Create(CreateThreadableLoadingContext(),
                                       WorkerObjectProxy());
}

}  // namespace blink
