// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/background_sync/SyncCallbacks.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "modules/background_sync/SyncError.h"
#include "modules/background_sync/SyncRegistration.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"

namespace blink {

SyncRegistrationCallbacks::SyncRegistrationCallbacks(PassRefPtrWillBeRawPtr<ScriptPromiseResolver> resolver, ServiceWorkerRegistration* serviceWorkerRegistration)
    : m_resolver(resolver)
    , m_serviceWorkerRegistration(serviceWorkerRegistration)
{
    ASSERT(m_resolver);
    ASSERT(m_serviceWorkerRegistration);
}

SyncRegistrationCallbacks::~SyncRegistrationCallbacks()
{
}

void SyncRegistrationCallbacks::onSuccess(WebSyncRegistration* webSyncRegistration)
{
    if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped()) {
        SyncRegistration::dispose(webSyncRegistration);
        return;
    }

    if (!webSyncRegistration) {
        m_resolver->resolve(v8::Null(m_resolver->scriptState()->isolate()));
        return;
    }
    m_resolver->resolve(SyncRegistration::take(m_resolver.get(), webSyncRegistration, m_serviceWorkerRegistration));
}

void SyncRegistrationCallbacks::onError(WebSyncError* error)
{
    if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped()) {
        SyncError::dispose(error);
        return;
    }
    m_resolver->reject(SyncError::take(m_resolver.get(), error));
}

SyncUnregistrationCallbacks::SyncUnregistrationCallbacks(PassRefPtrWillBeRawPtr<ScriptPromiseResolver> resolver, ServiceWorkerRegistration* serviceWorkerRegistration)
    : m_resolver(resolver)
    , m_serviceWorkerRegistration(serviceWorkerRegistration)
{
    ASSERT(m_resolver);
    ASSERT(m_serviceWorkerRegistration);
}

SyncUnregistrationCallbacks::~SyncUnregistrationCallbacks()
{
}

void SyncUnregistrationCallbacks::onSuccess(bool* status)
{
    if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped()) {
        return;
    }

    m_resolver->resolve(*status);
}

void SyncUnregistrationCallbacks::onError(WebSyncError* error)
{
    if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped()) {
        SyncError::dispose(error);
        return;
    }
    m_resolver->reject(SyncError::take(m_resolver.get(), error));
}

SyncGetRegistrationsCallbacks::SyncGetRegistrationsCallbacks(PassRefPtrWillBeRawPtr<ScriptPromiseResolver> resolver, ServiceWorkerRegistration* serviceWorkerRegistration)
    : m_resolver(resolver)
    , m_serviceWorkerRegistration(serviceWorkerRegistration)
{
    ASSERT(m_resolver);
    ASSERT(m_serviceWorkerRegistration);
}

SyncGetRegistrationsCallbacks::~SyncGetRegistrationsCallbacks()
{
}

void SyncGetRegistrationsCallbacks::onSuccess(WebVector<WebSyncRegistration>* webSyncRegistrations)
{
    if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped()) {
        for (size_t i = 0; i < webSyncRegistrations->size(); ++i) {
            WebSyncRegistration webSyncRegistration = (*webSyncRegistrations)[i];
            SyncRegistration::dispose(&webSyncRegistration);
        }
        return;
    }

    if (!webSyncRegistrations) {
        m_resolver->resolve(v8::Null(m_resolver->scriptState()->isolate()));
        return;
    }

    Vector<SyncRegistration*> syncRegistrations;
    for (size_t i = 0; i < webSyncRegistrations->size(); ++i) {
        WebSyncRegistration webSyncRegistration = (*webSyncRegistrations)[i];
        SyncRegistration* reg = SyncRegistration::take(m_resolver.get(), &webSyncRegistration, m_serviceWorkerRegistration);
        syncRegistrations.append(reg);
    }
    m_resolver->resolve(syncRegistrations);
}

void SyncGetRegistrationsCallbacks::onError(WebSyncError* error)
{
    if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped()) {
        SyncError::dispose(error);
        return;
    }
    m_resolver->reject(SyncError::take(m_resolver.get(), error));
}

} // namespace blink
