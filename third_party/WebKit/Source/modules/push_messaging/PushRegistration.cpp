// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/push_messaging/PushRegistration.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "modules/push_messaging/PushError.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"
#include "public/platform/Platform.h"
#include "public/platform/WebPushProvider.h"
#include "public/platform/WebPushRegistration.h"
#include "wtf/OwnPtr.h"

namespace blink {

PushRegistration* PushRegistration::take(ScriptPromiseResolver*, WebPushRegistration* pushRegistration, ServiceWorkerRegistration* serviceWorkerRegistration)
{
    OwnPtr<WebPushRegistration> registration = adoptPtr(pushRegistration);
    return new PushRegistration(registration->endpoint, registration->registrationId, serviceWorkerRegistration);
}

void PushRegistration::dispose(WebPushRegistration* pushRegistration)
{
    if (pushRegistration)
        delete pushRegistration;
}

PushRegistration::PushRegistration(const String& endpoint, const String& registrationId, ServiceWorkerRegistration* serviceWorkerRegistration)
    : m_endpoint(endpoint)
    , m_registrationId(registrationId)
    , m_serviceWorkerRegistration(serviceWorkerRegistration)
{
}

PushRegistration::~PushRegistration()
{
}

ScriptPromise PushRegistration::unregister(ScriptState* scriptState)
{
    RefPtrWillBeRawPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    WebPushProvider* webPushProvider = Platform::current()->pushProvider();
    ASSERT(webPushProvider);

    webPushProvider->unregister(m_serviceWorkerRegistration->webRegistration(), new CallbackPromiseAdapter<bool, PushError>(resolver));
    return promise;
}

void PushRegistration::trace(Visitor* visitor)
{
    visitor->trace(m_serviceWorkerRegistration);
}

} // namespace blink
