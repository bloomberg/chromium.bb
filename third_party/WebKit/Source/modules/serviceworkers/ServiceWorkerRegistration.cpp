// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/ServiceWorkerRegistration.h"

#include <memory>
#include <utility>
#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/events/Event.h"
#include "modules/EventTargetModules.h"
#include "modules/serviceworkers/ServiceWorkerContainerClient.h"
#include "modules/serviceworkers/ServiceWorkerError.h"
#include "platform/bindings/ScriptState.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerProvider.h"

namespace blink {

ServiceWorkerRegistration* ServiceWorkerRegistration::Take(
    ScriptPromiseResolver* resolver,
    std::unique_ptr<WebServiceWorkerRegistration::Handle> handle) {
  return GetOrCreate(resolver->GetExecutionContext(), std::move(handle));
}

bool ServiceWorkerRegistration::HasPendingActivity() const {
  return !stopped_;
}

const AtomicString& ServiceWorkerRegistration::InterfaceName() const {
  return EventTargetNames::ServiceWorkerRegistration;
}

void ServiceWorkerRegistration::DispatchUpdateFoundEvent() {
  DispatchEvent(Event::Create(EventTypeNames::updatefound));
}

void ServiceWorkerRegistration::SetInstalling(
    std::unique_ptr<WebServiceWorker::Handle> handle) {
  if (!GetExecutionContext())
    return;
  installing_ = ServiceWorker::From(GetExecutionContext(),
                                    WTF::WrapUnique(handle.release()));
}

void ServiceWorkerRegistration::SetWaiting(
    std::unique_ptr<WebServiceWorker::Handle> handle) {
  if (!GetExecutionContext())
    return;
  waiting_ = ServiceWorker::From(GetExecutionContext(),
                                 WTF::WrapUnique(handle.release()));
}

void ServiceWorkerRegistration::SetActive(
    std::unique_ptr<WebServiceWorker::Handle> handle) {
  if (!GetExecutionContext())
    return;
  active_ = ServiceWorker::From(GetExecutionContext(),
                                WTF::WrapUnique(handle.release()));
}

ServiceWorkerRegistration* ServiceWorkerRegistration::GetOrCreate(
    ExecutionContext* execution_context,
    std::unique_ptr<WebServiceWorkerRegistration::Handle> handle) {
  DCHECK(handle);

  ServiceWorkerRegistration* existing_registration =
      static_cast<ServiceWorkerRegistration*>(handle->Registration()->Proxy());
  if (existing_registration) {
    DCHECK_EQ(existing_registration->GetExecutionContext(), execution_context);
    return existing_registration;
  }

  return new ServiceWorkerRegistration(execution_context, std::move(handle));
}

NavigationPreloadManager* ServiceWorkerRegistration::navigationPreload() {
  if (!navigation_preload_)
    navigation_preload_ = NavigationPreloadManager::Create(this);
  return navigation_preload_;
}

String ServiceWorkerRegistration::scope() const {
  return handle_->Registration()->Scope().GetString();
}

ScriptPromise ServiceWorkerRegistration::update(ScriptState* script_state) {
  ServiceWorkerContainerClient* client =
      ServiceWorkerContainerClient::From(GetExecutionContext());
  if (!client || !client->Provider())
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(kInvalidStateError,
                             "Failed to update a ServiceWorkerRegistration: No "
                             "associated provider is available."));

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  handle_->Registration()->Update(
      std::make_unique<
          CallbackPromiseAdapter<void, ServiceWorkerErrorForUpdate>>(resolver));
  return promise;
}

ScriptPromise ServiceWorkerRegistration::unregister(ScriptState* script_state) {
  ServiceWorkerContainerClient* client =
      ServiceWorkerContainerClient::From(GetExecutionContext());
  if (!client || !client->Provider())
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(kInvalidStateError,
                             "Failed to unregister a "
                             "ServiceWorkerRegistration: No "
                             "associated provider is available."));

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  handle_->Registration()->Unregister(
      std::make_unique<CallbackPromiseAdapter<bool, ServiceWorkerError>>(
          resolver));
  return promise;
}

ServiceWorkerRegistration::ServiceWorkerRegistration(
    ExecutionContext* execution_context,
    std::unique_ptr<WebServiceWorkerRegistration::Handle> handle)
    : ContextLifecycleObserver(execution_context),
      handle_(std::move(handle)),
      stopped_(false) {
  DCHECK(handle_);
  DCHECK(!handle_->Registration()->Proxy());

  if (!execution_context)
    return;
  handle_->Registration()->SetProxy(this);
}

ServiceWorkerRegistration::~ServiceWorkerRegistration() {}

void ServiceWorkerRegistration::Dispose() {
  // Promptly clears a raw reference from content/ to an on-heap object
  // so that content/ doesn't access it in a lazy sweeping phase.
  handle_.reset();
}

void ServiceWorkerRegistration::Trace(blink::Visitor* visitor) {
  visitor->Trace(installing_);
  visitor->Trace(waiting_);
  visitor->Trace(active_);
  visitor->Trace(navigation_preload_);
  EventTargetWithInlineData::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
  Supplementable<ServiceWorkerRegistration>::Trace(visitor);
}

void ServiceWorkerRegistration::ContextDestroyed(ExecutionContext*) {
  if (stopped_)
    return;
  stopped_ = true;
  handle_->Registration()->ProxyStopped();
}

}  // namespace blink
