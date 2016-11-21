// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/background_sync/BackgroundSyncProvider.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "platform/heap/Persistent.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerRegistration.h"
#include "wtf/Functional.h"

namespace blink {

void BackgroundSyncProvider::registerBackgroundSync(
    mojom::blink::SyncRegistrationPtr options,
    WebServiceWorkerRegistration* serviceWorkerRegistration,
    ScriptPromiseResolver* resolver) {
  DCHECK(options);
  DCHECK(serviceWorkerRegistration);
  DCHECK(resolver);
  int64_t serviceWorkerRegistrationId =
      serviceWorkerRegistration->registrationId();

  GetBackgroundSyncServicePtr()->Register(
      std::move(options), serviceWorkerRegistrationId,
      convertToBaseCallback(WTF::bind(&BackgroundSyncProvider::registerCallback,
                                      wrapPersistent(resolver))));
}

void BackgroundSyncProvider::getRegistrations(
    WebServiceWorkerRegistration* serviceWorkerRegistration,
    ScriptPromiseResolver* resolver) {
  DCHECK(serviceWorkerRegistration);
  DCHECK(resolver);
  int64_t serviceWorkerRegistrationId =
      serviceWorkerRegistration->registrationId();

  GetBackgroundSyncServicePtr()->GetRegistrations(
      serviceWorkerRegistrationId,
      convertToBaseCallback(
          WTF::bind(&BackgroundSyncProvider::getRegistrationsCallback,
                    wrapPersistent(resolver))));
}

// static
void BackgroundSyncProvider::registerCallback(
    ScriptPromiseResolver* resolver,
    mojom::blink::BackgroundSyncError error,
    mojom::blink::SyncRegistrationPtr options) {
  // TODO(iclelland): Determine the correct error message to return in each case
  switch (error) {
    case mojom::blink::BackgroundSyncError::NONE:
      if (!options) {
        resolver->resolve(v8::Null(resolver->getScriptState()->isolate()));
        return;
      }
      resolver->resolve();
      break;
    case mojom::blink::BackgroundSyncError::NOT_FOUND:
      NOTREACHED();
      break;
    case mojom::blink::BackgroundSyncError::STORAGE:
      resolver->reject(
          DOMException::create(UnknownError, "Background Sync is disabled."));
      break;
    case mojom::blink::BackgroundSyncError::NOT_ALLOWED:
      resolver->reject(
          DOMException::create(InvalidAccessError,
                               "Attempted to register a sync event without a "
                               "window or registration tag too long."));
      break;
    case mojom::blink::BackgroundSyncError::PERMISSION_DENIED:
      resolver->reject(
          DOMException::create(PermissionDeniedError, "Permission denied."));
      break;
    case mojom::blink::BackgroundSyncError::NO_SERVICE_WORKER:
      resolver->reject(
          DOMException::create(UnknownError, "No service worker is active."));
      break;
  }
}

// static
void BackgroundSyncProvider::getRegistrationsCallback(
    ScriptPromiseResolver* resolver,
    mojom::blink::BackgroundSyncError error,
    mojo::WTFArray<mojom::blink::SyncRegistrationPtr> registrations) {
  // TODO(iclelland): Determine the correct error message to return in each case
  switch (error) {
    case mojom::blink::BackgroundSyncError::NONE: {
      Vector<String> tags;
      for (const auto& r : registrations.storage()) {
        tags.append(r->tag);
      }
      resolver->resolve(tags);
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
      resolver->reject(
          DOMException::create(UnknownError, "Background Sync is disabled."));
      break;
    case mojom::blink::BackgroundSyncError::NO_SERVICE_WORKER:
      resolver->reject(
          DOMException::create(UnknownError, "No service worker is active."));
      break;
  }
}

mojom::blink::BackgroundSyncServicePtr&
BackgroundSyncProvider::GetBackgroundSyncServicePtr() {
  if (!m_backgroundSyncService.get()) {
    mojo::InterfaceRequest<mojom::blink::BackgroundSyncService> request =
        mojo::GetProxy(&m_backgroundSyncService);
    Platform::current()->interfaceProvider()->getInterface(std::move(request));
  }
  return m_backgroundSyncService;
}

}  // namespace blink
