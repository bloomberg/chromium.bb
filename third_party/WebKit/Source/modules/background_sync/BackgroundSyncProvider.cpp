// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/background_sync/BackgroundSyncProvider.h"

#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"
#include "public/platform/modules/background_sync/WebSyncError.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerRegistration.h"
#include "wtf/Functional.h"

namespace blink {

void BackgroundSyncProvider::registerBackgroundSync(
    mojom::blink::SyncRegistrationPtr options,
    WebServiceWorkerRegistration* serviceWorkerRegistration,
    std::unique_ptr<SyncRegistrationCallbacks> callbacks) {
  DCHECK(options);
  DCHECK(serviceWorkerRegistration);
  DCHECK(callbacks);
  int64_t serviceWorkerRegistrationId =
      serviceWorkerRegistration->registrationId();

  GetBackgroundSyncServicePtr()->Register(
      std::move(options), serviceWorkerRegistrationId,
      convertToBaseCallback(WTF::bind(&BackgroundSyncProvider::registerCallback,
                                      WTF::passed(std::move(callbacks)))));
}

void BackgroundSyncProvider::getRegistrations(
    WebServiceWorkerRegistration* serviceWorkerRegistration,
    std::unique_ptr<SyncGetRegistrationsCallbacks> callbacks) {
  DCHECK(serviceWorkerRegistration);
  DCHECK(callbacks);
  int64_t serviceWorkerRegistrationId =
      serviceWorkerRegistration->registrationId();

  GetBackgroundSyncServicePtr()->GetRegistrations(
      serviceWorkerRegistrationId,
      convertToBaseCallback(
          WTF::bind(&BackgroundSyncProvider::getRegistrationsCallback,
                    WTF::passed(std::move(callbacks)))));
}

// static
void BackgroundSyncProvider::registerCallback(
    std::unique_ptr<SyncRegistrationCallbacks> callbacks,
    mojom::blink::BackgroundSyncError error,
    mojom::blink::SyncRegistrationPtr options) {
  // TODO(iclelland): Determine the correct error message to return in each case
  switch (error) {
    case mojom::blink::BackgroundSyncError::NONE:
      if (!options.is_null())
        callbacks->onSuccess(std::move(options));
      break;
    case mojom::blink::BackgroundSyncError::NOT_FOUND:
      NOTREACHED();
      break;
    case mojom::blink::BackgroundSyncError::STORAGE:
      callbacks->onError(blink::WebSyncError(WebSyncError::ErrorTypeUnknown,
                                             "Background Sync is disabled."));
      break;
    case mojom::blink::BackgroundSyncError::NOT_ALLOWED:
      callbacks->onError(
          blink::WebSyncError(WebSyncError::ErrorTypeNoPermission,
                              "Attempted to register a sync event without a "
                              "window or registration tag too long."));
      break;
    case mojom::blink::BackgroundSyncError::PERMISSION_DENIED:
      callbacks->onError(blink::WebSyncError(
          WebSyncError::ErrorTypePermissionDenied, "Permission denied."));
      break;
    case mojom::blink::BackgroundSyncError::NO_SERVICE_WORKER:
      callbacks->onError(blink::WebSyncError(WebSyncError::ErrorTypeUnknown,
                                             "No service worker is active."));
      break;
  }
}

// static
void BackgroundSyncProvider::getRegistrationsCallback(
    std::unique_ptr<SyncGetRegistrationsCallbacks> syncGetRegistrationCallbacks,
    mojom::blink::BackgroundSyncError error,
    mojo::WTFArray<mojom::blink::SyncRegistrationPtr> registrations) {
  // TODO(iclelland): Determine the correct error message to return in each case
  switch (error) {
    case mojom::blink::BackgroundSyncError::NONE: {
      WebVector<mojom::blink::SyncRegistration*> results(registrations.size());
      for (size_t i = 0; i < registrations.size(); ++i) {
        results[i] = registrations[i].get();
      }
      syncGetRegistrationCallbacks->onSuccess(results);
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
      syncGetRegistrationCallbacks->onError(blink::WebSyncError(
          WebSyncError::ErrorTypeUnknown, "Background Sync is disabled."));
      break;
    case mojom::blink::BackgroundSyncError::NO_SERVICE_WORKER:
      syncGetRegistrationCallbacks->onError(blink::WebSyncError(
          WebSyncError::ErrorTypeUnknown, "No service worker is active."));
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
