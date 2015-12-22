// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/push_messaging/PushSubscription.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/V8ObjectBuilder.h"
#include "modules/push_messaging/PushError.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "public/platform/Platform.h"
#include "public/platform/modules/push_messaging/WebPushProvider.h"
#include "public/platform/modules/push_messaging/WebPushSubscription.h"
#include "wtf/OwnPtr.h"
#include "wtf/text/Base64.h"

namespace blink {

PushSubscription* PushSubscription::take(ScriptPromiseResolver*, PassOwnPtr<WebPushSubscription> pushSubscription, ServiceWorkerRegistration* serviceWorkerRegistration)
{
    if (!pushSubscription)
        return nullptr;
    return new PushSubscription(*pushSubscription, serviceWorkerRegistration);
}

void PushSubscription::dispose(WebPushSubscription* pushSubscription)
{
    if (pushSubscription)
        delete pushSubscription;
}

PushSubscription::PushSubscription(const WebPushSubscription& subscription, ServiceWorkerRegistration* serviceWorkerRegistration)
    : m_endpoint(subscription.endpoint)
    , m_p256dh(DOMArrayBuffer::create(subscription.p256dh.data(), subscription.p256dh.size()))
    , m_auth(DOMArrayBuffer::create(subscription.auth.data(), subscription.auth.size()))
    , m_serviceWorkerRegistration(serviceWorkerRegistration)
{
}

PushSubscription::~PushSubscription()
{
}

KURL PushSubscription::endpoint() const
{
    return m_endpoint;
}

PassRefPtr<DOMArrayBuffer> PushSubscription::getKey(const AtomicString& name) const
{
    if (name == "p256dh")
        return m_p256dh;
    if (name == "auth")
        return m_auth;

    return nullptr;
}

ScriptPromise PushSubscription::unsubscribe(ScriptState* scriptState)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    WebPushProvider* webPushProvider = Platform::current()->pushProvider();
    ASSERT(webPushProvider);

    webPushProvider->unsubscribe(m_serviceWorkerRegistration->webRegistration(), new CallbackPromiseAdapter<bool, PushError>(resolver));
    return promise;
}

ScriptValue PushSubscription::toJSONForBinding(ScriptState* scriptState)
{
    V8ObjectBuilder result(scriptState);
    result.addString("endpoint", endpoint());

    if (RuntimeEnabledFeatures::pushMessagingDataEnabled()) {
        ASSERT(m_p256dh);

        V8ObjectBuilder keys(scriptState);
        keys.add("p256dh", WTF::base64URLEncode(static_cast<const char*>(m_p256dh->data()), m_p256dh->byteLength()));
        keys.add("auth", WTF::base64URLEncode(static_cast<const char*>(m_auth->data()), m_auth->byteLength()));

        result.add("keys", keys);
    }

    return result.scriptValue();
}

DEFINE_TRACE(PushSubscription)
{
    visitor->trace(m_serviceWorkerRegistration);
}

} // namespace blink
