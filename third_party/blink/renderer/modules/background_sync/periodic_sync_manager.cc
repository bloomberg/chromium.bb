// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/background_sync/periodic_sync_manager.h"

#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/background_sync/background_sync_options.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_registration.h"

namespace blink {

PeriodicSyncManager::PeriodicSyncManager(
    ServiceWorkerRegistration* registration,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : registration_(registration), task_runner_(std::move(task_runner)) {
  DCHECK(registration_);
}

ScriptPromise PeriodicSyncManager::registerPeriodicSync(
    ScriptState* script_state,
    const String& tag,
    const BackgroundSyncOptions* options) {
  if (!registration_->active()) {
    return ScriptPromise::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kInvalidStateError,
                          "Registration failed - no active Service Worker"));
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  mojom::blink::SyncRegistrationOptionsPtr sync_registration =
      mojom::blink::SyncRegistrationOptions::New(tag, options->minInterval());

  GetBackgroundSyncServicePtr()->Register(
      std::move(sync_registration), registration_->RegistrationId(),
      WTF::Bind(&PeriodicSyncManager::RegisterCallback, WrapPersistent(this),
                WrapPersistent(resolver)));

  return promise;
}

ScriptPromise PeriodicSyncManager::getTags(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  // Creating a Periodic Background Sync registration requires an activated
  // service worker, so if |registration_| has not been activated yet, we can
  // skip the Mojo roundtrip.
  if (!registration_->active()) {
    return ScriptPromise::Cast(script_state,
                               v8::Array::New(script_state->GetIsolate()));
  }

  // TODO(crbug.com/932591): Optimize this to only get the tags from the browser
  // process instead of the registrations themselves.
  GetBackgroundSyncServicePtr()->GetRegistrations(
      registration_->RegistrationId(),
      WTF::Bind(&PeriodicSyncManager::GetRegistrationsCallback,
                WrapPersistent(this), WrapPersistent(resolver)));

  return promise;
}

ScriptPromise PeriodicSyncManager::unregister(ScriptState* script_state,
                                              const String& tag) {
  if (!registration_->active()) {
    return ScriptPromise::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kInvalidStateError,
                          "Unregister failed - no active Service Worker"));
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  GetBackgroundSyncServicePtr()->Unregister(
      registration_->RegistrationId(), tag,
      WTF::Bind(&PeriodicSyncManager::UnregisterCallback, WrapPersistent(this),
                WrapPersistent(resolver)));
  return promise;
}

const mojom::blink::PeriodicBackgroundSyncServicePtr&
PeriodicSyncManager::GetBackgroundSyncServicePtr() {
  if (!background_sync_service_.get()) {
    Platform::Current()->GetInterfaceProvider()->GetInterface(
        mojo::MakeRequest(&background_sync_service_));
  }
  return background_sync_service_;
}

void PeriodicSyncManager::RegisterCallback(
    ScriptPromiseResolver* resolver,
    mojom::blink::BackgroundSyncError error,
    mojom::blink::SyncRegistrationOptionsPtr options) {
  switch (error) {
    case mojom::blink::BackgroundSyncError::NONE:
      resolver->Resolve();
      break;
    case mojom::blink::BackgroundSyncError::NOT_FOUND:
      NOTREACHED();
      break;
    case mojom::blink::BackgroundSyncError::STORAGE:
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kUnknownError, "Unknown error."));
      break;
    case mojom::blink::BackgroundSyncError::NOT_ALLOWED:
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kInvalidAccessError,
          "Attempted to register a sync event without a "
          "window or registration tag too long."));
      break;
    case mojom::blink::BackgroundSyncError::PERMISSION_DENIED:
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError, "Permission denied."));
      break;
    case mojom::blink::BackgroundSyncError::NO_SERVICE_WORKER:
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kUnknownError, "No service worker is active."));
      break;
  }
}

void PeriodicSyncManager::GetRegistrationsCallback(
    ScriptPromiseResolver* resolver,
    mojom::blink::BackgroundSyncError error,
    WTF::Vector<mojom::blink::SyncRegistrationOptionsPtr> registrations) {
  switch (error) {
    case mojom::blink::BackgroundSyncError::NONE: {
      Vector<String> tags;
      for (const auto& registration : registrations)
        tags.push_back(registration->tag);
      resolver->Resolve(tags);
      break;
    }
    case mojom::blink::BackgroundSyncError::NOT_FOUND:
    case mojom::blink::BackgroundSyncError::NOT_ALLOWED:
    case mojom::blink::BackgroundSyncError::PERMISSION_DENIED:
      // These errors should never be returned from
      // BackgroundSyncManager::GetPeriodicSyncRegistrations
      NOTREACHED();
      break;
    case mojom::blink::BackgroundSyncError::STORAGE:
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kUnknownError, "Unknown error."));
      break;
    case mojom::blink::BackgroundSyncError::NO_SERVICE_WORKER:
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kUnknownError, "No service worker is active."));
      break;
  }
}

void PeriodicSyncManager::UnregisterCallback(
    ScriptPromiseResolver* resolver,
    mojom::blink::BackgroundSyncError error) {
  switch (error) {
    case mojom::blink::BackgroundSyncError::NONE:
      resolver->Resolve();
      break;
    case mojom::blink::BackgroundSyncError::NO_SERVICE_WORKER:
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kUnknownError, "No service worker is active."));
      break;
    case mojom::blink::BackgroundSyncError::STORAGE:
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kUnknownError, "Unknown error."));
      break;
    case mojom::blink::BackgroundSyncError::NOT_FOUND:
    case mojom::blink::BackgroundSyncError::NOT_ALLOWED:
    case mojom::BackgroundSyncError::PERMISSION_DENIED:
      NOTREACHED();
      break;
  }
}

void PeriodicSyncManager::Trace(blink::Visitor* visitor) {
  visitor->Trace(registration_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
