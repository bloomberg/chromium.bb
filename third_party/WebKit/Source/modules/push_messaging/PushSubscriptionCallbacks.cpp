// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/push_messaging/PushSubscriptionCallbacks.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "modules/push_messaging/PushError.h"
#include "modules/push_messaging/PushSubscription.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"
#include "public/platform/modules/push_messaging/WebPushSubscription.h"

namespace blink {

PushSubscriptionCallbacks::PushSubscriptionCallbacks(ScriptPromiseResolver* resolver, ServiceWorkerRegistration* serviceWorkerRegistration)
    : m_resolver(resolver)
    , m_serviceWorkerRegistration(serviceWorkerRegistration)
{
    ASSERT(m_resolver);
    ASSERT(m_serviceWorkerRegistration);
}

PushSubscriptionCallbacks::~PushSubscriptionCallbacks()
{
}

void PushSubscriptionCallbacks::onSuccess(WebPassOwnPtr<WebPushSubscription> webPushSubscription)
{
    if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped())
        return;

    m_resolver->resolve(PushSubscription::take(m_resolver.get(), webPushSubscription.release(), m_serviceWorkerRegistration));
}

void PushSubscriptionCallbacks::onError(const WebPushError& error)
{
    if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped())
        return;
    m_resolver->reject(PushError::take(m_resolver.get(), error));
}

} // namespace blink
