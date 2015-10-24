// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/background_sync/SyncCallbacks.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "modules/background_sync/PeriodicSyncRegistration.h"
#include "modules/background_sync/SyncError.h"
#include "modules/background_sync/SyncRegistration.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

SyncRegistrationCallbacks::SyncRegistrationCallbacks(ScriptPromiseResolver* resolver, ServiceWorkerRegistration* serviceWorkerRegistration)
    : m_resolver(resolver)
    , m_serviceWorkerRegistration(serviceWorkerRegistration)
{
    ASSERT(m_resolver);
    ASSERT(m_serviceWorkerRegistration);
}

SyncRegistrationCallbacks::~SyncRegistrationCallbacks()
{
}

void SyncRegistrationCallbacks::onSuccess(WebPassOwnPtr<WebSyncRegistration> webSyncRegistration)
{
    if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped()) {
        return;
    }

    OwnPtr<WebSyncRegistration> registration = webSyncRegistration.release();
    if (!registration) {
        m_resolver->resolve(v8::Null(m_resolver->scriptState()->isolate()));
        return;
    }
    switch (registration->periodicity) {
    case WebSyncRegistration::PeriodicityPeriodic:
        m_resolver->resolve(PeriodicSyncRegistration::take(m_resolver.get(), registration.release(), m_serviceWorkerRegistration));
        break;
    case WebSyncRegistration::PeriodicityOneShot:
        m_resolver->resolve(SyncRegistration::take(m_resolver.get(), registration.release(), m_serviceWorkerRegistration));
        break;
    }
}

void SyncRegistrationCallbacks::onError(const WebSyncError& error)
{
    if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped()) {
        return;
    }
    m_resolver->reject(SyncError::take(m_resolver.get(), error));
}

SyncNotifyWhenFinishedCallbacks::SyncNotifyWhenFinishedCallbacks(ScriptPromiseResolver* resolver, ServiceWorkerRegistration* serviceWorkerRegistration)
    : m_resolver(resolver)
    , m_serviceWorkerRegistration(serviceWorkerRegistration)
{
    ASSERT(m_resolver);
    ASSERT(m_serviceWorkerRegistration);
}

SyncNotifyWhenFinishedCallbacks::~SyncNotifyWhenFinishedCallbacks()
{
}

void SyncNotifyWhenFinishedCallbacks::onSuccess()
{
    if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped()) {
        return;
    }

    m_resolver->resolve();
}

void SyncNotifyWhenFinishedCallbacks::onError(const WebSyncError& error)
{
    if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped()) {
        return;
    }
    m_resolver->reject(SyncError::take(m_resolver.get(), error));
}

SyncUnregistrationCallbacks::SyncUnregistrationCallbacks(ScriptPromiseResolver* resolver, ServiceWorkerRegistration* serviceWorkerRegistration)
    : m_resolver(resolver)
    , m_serviceWorkerRegistration(serviceWorkerRegistration)
{
    ASSERT(m_resolver);
    ASSERT(m_serviceWorkerRegistration);
}

SyncUnregistrationCallbacks::~SyncUnregistrationCallbacks()
{
}

void SyncUnregistrationCallbacks::onSuccess(bool status)
{
    if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped()) {
        return;
    }

    m_resolver->resolve(status);
}

void SyncUnregistrationCallbacks::onError(const WebSyncError& error)
{
    if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped()) {
        return;
    }
    m_resolver->reject(SyncError::take(m_resolver.get(), error));
}

SyncGetRegistrationsCallbacks::SyncGetRegistrationsCallbacks(ScriptPromiseResolver* resolver, ServiceWorkerRegistration* serviceWorkerRegistration)
    : m_resolver(resolver)
    , m_serviceWorkerRegistration(serviceWorkerRegistration)
{
    ASSERT(m_resolver);
    ASSERT(m_serviceWorkerRegistration);
}

SyncGetRegistrationsCallbacks::~SyncGetRegistrationsCallbacks()
{
}

void SyncGetRegistrationsCallbacks::onSuccess(const WebVector<WebSyncRegistration*>& webSyncRegistrations)
{
    Vector<OwnPtr<WebSyncRegistration>> registrations;
    for (WebSyncRegistration* r : webSyncRegistrations) {
        registrations.append(adoptPtr(r));
    }
    if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped()) {
        return;
    }

    if (webSyncRegistrations.size() && webSyncRegistrations[0]->periodicity == WebSyncRegistration::PeriodicityOneShot) {
        HeapVector<Member<SyncRegistration>> syncRegistrations;
        for (auto& r : registrations) {
            SyncRegistration* reg = SyncRegistration::take(m_resolver.get(), r.release(), m_serviceWorkerRegistration);
            syncRegistrations.append(reg);
        }
        m_resolver->resolve(syncRegistrations);
    } else {
        HeapVector<Member<PeriodicSyncRegistration>> syncRegistrations;
        for (auto& r : registrations) {
            PeriodicSyncRegistration* reg = PeriodicSyncRegistration::take(m_resolver.get(), r.release(), m_serviceWorkerRegistration);
            syncRegistrations.append(reg);
        }
        m_resolver->resolve(syncRegistrations);
    }
}

void SyncGetRegistrationsCallbacks::onError(const WebSyncError& error)
{
    if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped()) {
        return;
    }
    m_resolver->reject(SyncError::take(m_resolver.get(), error));
}

SyncGetPermissionStatusCallbacks::SyncGetPermissionStatusCallbacks(ScriptPromiseResolver* resolver, ServiceWorkerRegistration* serviceWorkerRegistration)
    : m_resolver(resolver)
    , m_serviceWorkerRegistration(serviceWorkerRegistration)
{
    ASSERT(m_resolver);
    ASSERT(m_serviceWorkerRegistration);
}

SyncGetPermissionStatusCallbacks::~SyncGetPermissionStatusCallbacks()
{
}

void SyncGetPermissionStatusCallbacks::onSuccess(WebSyncPermissionStatus status)
{
    if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped()) {
        return;
    }

    m_resolver->resolve(permissionString(status));
}

void SyncGetPermissionStatusCallbacks::onError(const WebSyncError& error)
{
    if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped()) {
        return;
    }
    m_resolver->reject(SyncError::take(m_resolver.get(), error));
}

// static
String SyncGetPermissionStatusCallbacks::permissionString(WebSyncPermissionStatus status)
{
    switch (status) {
    case WebSyncPermissionStatusGranted:
        return "granted";
    case WebSyncPermissionStatusDenied:
        return "denied";
    case WebSyncPermissionStatusPrompt:
        return "prompt";
    }

    ASSERT_NOT_REACHED();
    return "denied";
}

} // namespace blink
