// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/push_messaging/PushRegistrationCallbacks.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "modules/push_messaging/PushError.h"
#include "modules/push_messaging/PushRegistration.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"

namespace blink {

PushRegistrationCallbacks::PushRegistrationCallbacks(PassRefPtrWillBeRawPtr<ScriptPromiseResolver> resolver, ServiceWorkerRegistration* registration)
    : m_resolver(resolver)
    , m_serviceWorkerRegistration(registration)
{
    ASSERT(m_resolver);
    ASSERT(m_serviceWorkerRegistration);
}

PushRegistrationCallbacks::~PushRegistrationCallbacks()
{
}

void PushRegistrationCallbacks::onSuccess(WebPushRegistration* webPushRegistration)
{
    if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped()) {
        PushRegistration::dispose(webPushRegistration);
        return;
    }

    if (!webPushRegistration) {
        m_resolver->resolve(v8::Null(m_resolver->scriptState()->isolate()));
        return;
    }
    m_resolver->resolve(PushRegistration::take(m_resolver.get(), webPushRegistration, m_serviceWorkerRegistration));
}

void PushRegistrationCallbacks::onError(WebPushError* error)
{
    if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped()) {
        PushError::dispose(error);
        return;
    }
    m_resolver->reject(PushError::take(m_resolver.get(), error));
}

} // namespace blink
