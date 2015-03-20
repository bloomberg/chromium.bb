// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/background_sync/SyncRegistration.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "modules/background_sync/SyncCallbacks.h"
#include "modules/background_sync/SyncError.h"
#include "modules/background_sync/SyncRegistrationOptions.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"
#include "public/platform/Platform.h"
#include "public/platform/modules/background_sync/WebSyncProvider.h"
#include "public/platform/modules/background_sync/WebSyncRegistration.h"
#include "wtf/OwnPtr.h"

namespace blink {

SyncRegistration* SyncRegistration::take(ScriptPromiseResolver*, WebSyncRegistration* syncRegistration, ServiceWorkerRegistration* serviceWorkerRegistration)
{
    OwnPtr<WebSyncRegistration> registration = adoptPtr(syncRegistration);
    SyncRegistrationOptions options = SyncRegistrationOptions();
    options.setAllowOnBattery(syncRegistration->allowOnBattery);
    options.setId(syncRegistration->id);
    options.setIdleRequired(syncRegistration->idleRequired);
    options.setMaxDelay(syncRegistration->maxDelayMs);
    options.setMinDelay(syncRegistration->minDelayMs);
    options.setMinPeriod(syncRegistration->minPeriodMs);
    switch (syncRegistration->minRequiredNetwork) {
    case WebSyncRegistration::NetworkType::NetworkTypeAny:
        options.setMinRequiredNetwork("network-any");
        break;
    case WebSyncRegistration::NetworkType::NetworkTypeOffline:
        options.setMinRequiredNetwork("network-offline");
        break;
    case WebSyncRegistration::NetworkType::NetworkTypeOnline:
        options.setMinRequiredNetwork("network-online");
        break;
    case WebSyncRegistration::NetworkType::NetworkTypeNonMobile:
        options.setMinRequiredNetwork("network-non-mobile");
        break;
    }
    return new SyncRegistration(options, serviceWorkerRegistration);
}

void SyncRegistration::dispose(WebSyncRegistration* syncRegistration)
{
    if (syncRegistration)
        delete syncRegistration;
}

SyncRegistration::SyncRegistration(const SyncRegistrationOptions& options, ServiceWorkerRegistration* serviceWorkerRegistration)
    : m_allowOnBattery(options.allowOnBattery())
    , m_id(options.id())
    , m_idleRequired(options.idleRequired())
    , m_maxDelay(options.maxDelay())
    , m_minDelay(options.minDelay())
    , m_minPeriod(options.minPeriod())
    , m_minRequiredNetwork(options.minRequiredNetwork())
    , m_serviceWorkerRegistration(serviceWorkerRegistration)
{
}

SyncRegistration::~SyncRegistration()
{
}

ScriptPromise SyncRegistration::unregister(ScriptState* scriptState)
{
    RefPtrWillBeRawPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    WebSyncProvider* webSyncProvider = Platform::current()->backgroundSyncProvider();
    ASSERT(webSyncProvider);

    webSyncProvider->unregisterBackgroundSync(m_id, m_serviceWorkerRegistration->webRegistration(), new SyncUnregistrationCallbacks(resolver, m_serviceWorkerRegistration));

    return promise;
}

DEFINE_TRACE(SyncRegistration)
{
    visitor->trace(m_allowOnBattery);
    visitor->trace(m_idleRequired);
    visitor->trace(m_serviceWorkerRegistration);
}

} // namespace blink
