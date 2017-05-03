// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/background_sync/SyncManager.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"
#include "platform/bindings/ScriptState.h"
#include "platform/heap/Persistent.h"
#include "platform/wtf/Functional.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"

namespace blink {

SyncManager::SyncManager(ServiceWorkerRegistration* registration)
    : registration_(registration) {
  DCHECK(registration);
}

ScriptPromise SyncManager::registerFunction(ScriptState* script_state,
                                            const String& tag) {
  // TODO(jkarlin): Wait for the registration to become active instead of
  // rejecting. See crbug.com/542437.
  if (!registration_->active())
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(kAbortError,
                             "Registration failed - no active Service Worker"));

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  mojom::blink::SyncRegistrationPtr sync_registration =
      mojom::blink::SyncRegistration::New();
  sync_registration->id = SyncManager::kUnregisteredSyncID;
  sync_registration->tag = tag;
  sync_registration->network_state =
      blink::mojom::BackgroundSyncNetworkState::ONLINE;

  GetBackgroundSyncServicePtr()->Register(
      std::move(sync_registration),
      registration_->WebRegistration()->RegistrationId(),
      ConvertToBaseCallback(
          WTF::Bind(SyncManager::RegisterCallback, WrapPersistent(resolver))));

  return promise;
}

ScriptPromise SyncManager::getTags(ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  GetBackgroundSyncServicePtr()->GetRegistrations(
      registration_->WebRegistration()->RegistrationId(),
      ConvertToBaseCallback(WTF::Bind(&SyncManager::GetRegistrationsCallback,
                                      WrapPersistent(resolver))));

  return promise;
}

const mojom::blink::BackgroundSyncServicePtr&
SyncManager::GetBackgroundSyncServicePtr() {
  if (!background_sync_service_.get()) {
    Platform::Current()->GetInterfaceProvider()->GetInterface(
        mojo::MakeRequest(&background_sync_service_));
  }
  return background_sync_service_;
}

// static
void SyncManager::RegisterCallback(ScriptPromiseResolver* resolver,
                                   mojom::blink::BackgroundSyncError error,
                                   mojom::blink::SyncRegistrationPtr options) {
  // TODO(iclelland): Determine the correct error message to return in each case
  switch (error) {
    case mojom::blink::BackgroundSyncError::NONE:
      if (!options) {
        resolver->Resolve(v8::Null(resolver->GetScriptState()->GetIsolate()));
        return;
      }
      resolver->Resolve();
      break;
    case mojom::blink::BackgroundSyncError::NOT_FOUND:
      NOTREACHED();
      break;
    case mojom::blink::BackgroundSyncError::STORAGE:
      resolver->Reject(
          DOMException::Create(kUnknownError, "Background Sync is disabled."));
      break;
    case mojom::blink::BackgroundSyncError::NOT_ALLOWED:
      resolver->Reject(
          DOMException::Create(kInvalidAccessError,
                               "Attempted to register a sync event without a "
                               "window or registration tag too long."));
      break;
    case mojom::blink::BackgroundSyncError::PERMISSION_DENIED:
      resolver->Reject(
          DOMException::Create(kPermissionDeniedError, "Permission denied."));
      break;
    case mojom::blink::BackgroundSyncError::NO_SERVICE_WORKER:
      resolver->Reject(
          DOMException::Create(kUnknownError, "No service worker is active."));
      break;
  }
}

// static
void SyncManager::GetRegistrationsCallback(
    ScriptPromiseResolver* resolver,
    mojom::blink::BackgroundSyncError error,
    WTF::Vector<mojom::blink::SyncRegistrationPtr> registrations) {
  // TODO(iclelland): Determine the correct error message to return in each case
  switch (error) {
    case mojom::blink::BackgroundSyncError::NONE: {
      Vector<String> tags;
      for (const auto& r : registrations) {
        tags.push_back(r->tag);
      }
      resolver->Resolve(tags);
      break;
    }
    case mojom::blink::BackgroundSyncError::NOT_FOUND:
    case mojom::blink::BackgroundSyncError::NOT_ALLOWED:
    case mojom::blink::BackgroundSyncError::PERMISSION_DENIED:
      // These errors should never be returned from
      // BackgroundSyncManager::GetRegistrations
      NOTREACHED();
      break;
    case mojom::blink::BackgroundSyncError::STORAGE:
      resolver->Reject(
          DOMException::Create(kUnknownError, "Background Sync is disabled."));
      break;
    case mojom::blink::BackgroundSyncError::NO_SERVICE_WORKER:
      resolver->Reject(
          DOMException::Create(kUnknownError, "No service worker is active."));
      break;
  }
}

DEFINE_TRACE(SyncManager) {
  visitor->Trace(registration_);
}

}  // namespace blink
