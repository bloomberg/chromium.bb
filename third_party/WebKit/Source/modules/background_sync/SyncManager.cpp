// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/background_sync/SyncManager.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "modules/background_sync/BackgroundSyncProvider.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"
#include "public/platform/Platform.h"
#include "wtf/PtrUtil.h"
#include "wtf/ThreadSpecific.h"

namespace blink {

// static
BackgroundSyncProvider* SyncManager::backgroundSyncProvider() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<BackgroundSyncProvider>,
                                  syncProvider,
                                  new ThreadSpecific<BackgroundSyncProvider>);
  return syncProvider;
}

SyncManager::SyncManager(ServiceWorkerRegistration* registration)
    : m_registration(registration) {
  DCHECK(registration);
}

ScriptPromise SyncManager::registerFunction(ScriptState* scriptState,
                                            const String& tag) {
  // TODO(jkarlin): Wait for the registration to become active instead of
  // rejecting. See crbug.com/542437.
  if (!m_registration->active())
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        DOMException::create(AbortError,
                             "Registration failed - no active Service Worker"));

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  mojom::blink::SyncRegistrationPtr syncRegistration =
      mojom::blink::SyncRegistration::New();
  syncRegistration->id = SyncManager::kUnregisteredSyncID;
  syncRegistration->tag = tag;
  syncRegistration->network_state =
      blink::mojom::BackgroundSyncNetworkState::ONLINE;

  backgroundSyncProvider()->registerBackgroundSync(
      std::move(syncRegistration), m_registration->webRegistration(), resolver);

  return promise;
}

ScriptPromise SyncManager::getTags(ScriptState* scriptState) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  backgroundSyncProvider()->getRegistrations(m_registration->webRegistration(),
                                             resolver);

  return promise;
}

DEFINE_TRACE(SyncManager) {
  visitor->trace(m_registration);
}

}  // namespace blink
