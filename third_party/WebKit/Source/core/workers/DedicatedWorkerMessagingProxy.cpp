// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/DedicatedWorkerMessagingProxy.h"

#include <memory>
#include "bindings/core/v8/V8CacheOptions.h"
#include "core/dom/Document.h"
#include "core/events/ErrorEvent.h"
#include "core/events/MessageEvent.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/inspector/MainThreadDebugger.h"
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
#include "public/platform/TaskType.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/service_manager/public/interfaces/interface_provider.mojom-blink.h"
#include "third_party/WebKit/public/platform/dedicated_worker_factory.mojom-blink.h"

namespace blink {
namespace {

service_manager::mojom::blink::InterfaceProviderPtrInfo
ConnectToWorkerInterfaceProvider(Document* document,
                                 scoped_refptr<SecurityOrigin> script_origin) {
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
  scoped_refptr<SerializedScriptValue> message;
  Vector<MessagePortChannel> channels;
  v8_inspector::V8StackTraceId stack_id;
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
    ReferrerPolicy referrer_policy,
    const v8_inspector::V8StackTraceId& stack_id) {
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

  auto global_scope_creation_params =
      std::make_unique<GlobalScopeCreationParams>(
          script_url, user_agent, source_code, nullptr, csp->Headers().get(),
          referrer_policy, starter_origin, ReleaseWorkerClients(),
          document->AddressSpace(),
          OriginTrialContext::GetTokens(document).get(),
          std::make_unique<WorkerSettings>(document->GetSettings()),
          kV8CacheOptionsDefault,
          ConnectToWorkerInterfaceProvider(document,
                                           SecurityOrigin::Create(script_url)));

  InitializeWorkerThread(std::move(global_scope_creation_params),
                         CreateBackingThreadStartupData(ToIsolate(document)),
                         script_url, stack_id);
}

void DedicatedWorkerMessagingProxy::PostMessageToWorkerGlobalScope(
    scoped_refptr<SerializedScriptValue> message,
    Vector<MessagePortChannel> channels,
    const v8_inspector::V8StackTraceId& stack_id) {
  DCHECK(IsParentContextThread());
  if (AskedToTerminate())
    return;

  if (GetWorkerThread()) {
    WTF::CrossThreadClosure task = CrossThreadBind(
        &DedicatedWorkerObjectProxy::ProcessMessageFromWorkerObject,
        CrossThreadUnretained(&WorkerObjectProxy()), std::move(message),
        WTF::Passed(std::move(channels)),
        CrossThreadUnretained(GetWorkerThread()), stack_id);
    GetWorkerThread()
        ->GetTaskRunner(TaskType::kPostedMessage)
        ->PostTask(BLINK_FROM_HERE, std::move(task));
  } else {
    queued_early_tasks_.push_back(
        QueuedTask{std::move(message), std::move(channels), stack_id});
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
        CrossThreadUnretained(GetWorkerThread()), queued_task.stack_id);
    GetWorkerThread()
        ->GetTaskRunner(TaskType::kPostedMessage)
        ->PostTask(BLINK_FROM_HERE, std::move(task));
  }
  queued_early_tasks_.clear();
}

bool DedicatedWorkerMessagingProxy::HasPendingActivity() const {
  DCHECK(IsParentContextThread());
  return !AskedToTerminate();
}

void DedicatedWorkerMessagingProxy::PostMessageToWorkerObject(
    scoped_refptr<SerializedScriptValue> message,
    Vector<MessagePortChannel> channels,
    const v8_inspector::V8StackTraceId& stack_id) {
  DCHECK(IsParentContextThread());
  if (!worker_object_ || AskedToTerminate())
    return;

  MessagePortArray* ports =
      MessagePort::EntanglePorts(*GetExecutionContext(), std::move(channels));
  MainThreadDebugger::Instance()->ExternalAsyncTaskStarted(stack_id);
  worker_object_->DispatchEvent(
      MessageEvent::Create(ports, std::move(message)));
  MainThreadDebugger::Instance()->ExternalAsyncTaskFinished(stack_id);
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
  GetWorkerThread()
      ->GetTaskRunner(TaskType::kDOMManipulation)
      ->PostTask(BLINK_FROM_HERE,
                 CrossThreadBind(
                     &DedicatedWorkerObjectProxy::ProcessUnhandledException,
                     CrossThreadUnretained(worker_object_proxy_.get()),
                     exception_id, CrossThreadUnretained(GetWorkerThread())));
}

void DedicatedWorkerMessagingProxy::Trace(blink::Visitor* visitor) {
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
