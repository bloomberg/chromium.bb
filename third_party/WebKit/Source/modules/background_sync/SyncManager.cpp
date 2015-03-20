// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/background_sync/SyncManager.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "modules/background_sync/SyncCallbacks.h"
#include "modules/background_sync/SyncRegistrationOptions.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"
#include "public/platform/Platform.h"
#include "public/platform/modules/background_sync/WebSyncProvider.h"
#include "public/platform/modules/background_sync/WebSyncRegistration.h"
#include "wtf/RefPtr.h"


namespace blink {
namespace {

/* This is the minimum period which will be allowed by the Background
 * Sync manager process. It is recorded here in order to be able to
 * respond to syncManager.minAllowablePeriod.
 * The time is expressed in milliseconds,
 */
unsigned long kMinAllowablePeriod = 12 * 60 * 60 * 1000;

WebSyncProvider* backgroundSyncProvider()
{
    WebSyncProvider* webSyncProvider = Platform::current()->backgroundSyncProvider();
    ASSERT(webSyncProvider);
    return webSyncProvider;
}

}

SyncManager::SyncManager(ServiceWorkerRegistration* registration)
    : m_registration(registration)
{
    ASSERT(registration);
}

unsigned long SyncManager::minAllowablePeriod()
{
    return kMinAllowablePeriod;
}

ScriptPromise SyncManager::registerFunction(blink::ScriptState* scriptState, const SyncRegistrationOptions& options)
{
    if (!m_registration->active())
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(AbortError, "Registration failed - no active Service Worker"));

    RefPtrWillBeRawPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    WebSyncRegistration::NetworkType networkType;
    String minRequiredNetwork = options.minRequiredNetwork();
    if (minRequiredNetwork == "network-any") {
        networkType = WebSyncRegistration::NetworkType::NetworkTypeAny;
    } else if (minRequiredNetwork == "network-non-mobile") {
        networkType = WebSyncRegistration::NetworkType::NetworkTypeNonMobile;
    } else if (minRequiredNetwork ==  "network-offline") {
        networkType = WebSyncRegistration::NetworkType::NetworkTypeOffline;
    } else {
        networkType = WebSyncRegistration::NetworkType::NetworkTypeOnline;
    }
    WebSyncRegistration webSyncRegistration = WebSyncRegistration(options.id(), options.minDelay(), options.maxDelay(), options.minPeriod(), networkType, options.allowOnBattery(), options.idleRequired());
    backgroundSyncProvider()->registerBackgroundSync(&webSyncRegistration, m_registration->webRegistration(), new SyncRegistrationCallbacks(resolver, m_registration));

    return promise;
}

ScriptPromise SyncManager::getRegistration(blink::ScriptState* scriptState, const String& syncRegistrationId)
{
    if (!m_registration->active())
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(AbortError, "Operation failed - no active Service Worker"));

    RefPtrWillBeRawPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    backgroundSyncProvider()->getRegistration(syncRegistrationId, m_registration->webRegistration(), new SyncRegistrationCallbacks(resolver, m_registration));

    return promise;
}

ScriptPromise SyncManager::getRegistrations(blink::ScriptState* scriptState)
{
    if (!m_registration->active())
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(AbortError, "Operation failed - no active Service Worker"));

    RefPtrWillBeRawPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    backgroundSyncProvider()->getRegistrations(m_registration->webRegistration(), new SyncGetRegistrationsCallbacks(resolver, m_registration));

    return promise;
}

DEFINE_TRACE(SyncManager)
{
    visitor->trace(m_registration);
}

} // namespace blink
