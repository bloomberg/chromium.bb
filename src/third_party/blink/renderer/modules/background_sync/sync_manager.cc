// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/background_sync/sync_manager.h"

#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/callback_promise_adapter.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_registration.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

SyncManager::SyncManager(ServiceWorkerRegistration* registration,
                         scoped_refptr<base::SequencedTaskRunner> task_runner)
    : registration_(registration),
      background_sync_service_(registration->GetExecutionContext()) {
  DCHECK(registration);
  Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
      background_sync_service_.BindNewPipeAndPassReceiver(task_runner));
}

ScriptPromise SyncManager::registerFunction(ScriptState* script_state,
                                            const String& tag,
                                            ExceptionState& exception_state) {
  if (!registration_->active()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Registration failed - no active Service Worker");
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  mojom::blink::SyncRegistrationOptionsPtr sync_registration =
      mojom::blink::SyncRegistrationOptions::New();
  sync_registration->tag = tag;

  background_sync_service_->Register(
      std::move(sync_registration), registration_->RegistrationId(),
      WTF::Bind(&SyncManager::RegisterCallback, WrapPersistent(this),
                WrapPersistent(resolver)));

  return promise;
}

ScriptPromise SyncManager::getTags(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  background_sync_service_->GetRegistrations(
      registration_->RegistrationId(),
      WTF::Bind(&SyncManager::GetRegistrationsCallback,
                WrapPersistent(resolver)));

  return promise;
}

void SyncManager::RegisterCallback(
    ScriptPromiseResolver* resolver,
    mojom::blink::BackgroundSyncError error,
    mojom::blink::SyncRegistrationOptionsPtr options) {
  // TODO(iclelland): Determine the correct error message to return in each case
  switch (error) {
    case mojom::blink::BackgroundSyncError::NONE:
      if (!options) {
        resolver->Resolve(v8::Null(resolver->GetScriptState()->GetIsolate()));
        return;
      }
      resolver->Resolve();
      // Let the service know that the registration promise is resolved so that
      // it can fire the event.

      background_sync_service_->DidResolveRegistration(
          mojom::blink::BackgroundSyncRegistrationInfo::New(
              registration_->RegistrationId(), options->tag,
              mojom::blink::BackgroundSyncType::ONE_SHOT));
      break;
    case mojom::blink::BackgroundSyncError::NOT_FOUND:
      NOTREACHED();
      break;
    case mojom::blink::BackgroundSyncError::STORAGE:
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kUnknownError, "Background Sync is disabled."));
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
          DOMExceptionCode::kInvalidStateError,
          "Registration failed - no active Service Worker"));
      break;
  }
}

// static
void SyncManager::GetRegistrationsCallback(
    ScriptPromiseResolver* resolver,
    mojom::blink::BackgroundSyncError error,
    WTF::Vector<mojom::blink::SyncRegistrationOptionsPtr> registrations) {
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
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kUnknownError, "Background Sync is disabled."));
      break;
    case mojom::blink::BackgroundSyncError::NO_SERVICE_WORKER:
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kUnknownError, "No service worker is active."));
      break;
  }
}

void SyncManager::Trace(Visitor* visitor) {
  visitor->Trace(registration_);
  visitor->Trace(background_sync_service_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
