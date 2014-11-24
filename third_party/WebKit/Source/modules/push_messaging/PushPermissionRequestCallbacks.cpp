// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/push_messaging/PushPermissionRequestCallbacks.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "modules/push_messaging/PushManager.h"

namespace blink {

PushPermissionRequestCallbacks::PushPermissionRequestCallbacks(PushManager* manager, WebPushClient* client, PassRefPtr<ScriptPromiseResolver> resolver, WebServiceWorkerProvider* serviceWorkerProvider)
    : m_manager(manager)
    , m_client(client)
    , m_resolver(resolver)
    , m_serviceWorkerProvider(serviceWorkerProvider)
{
}

PushPermissionRequestCallbacks::~PushPermissionRequestCallbacks()
{
}

void PushPermissionRequestCallbacks::onSuccess()
{
    m_manager->doRegister(m_client, m_resolver, m_serviceWorkerProvider);
}

void PushPermissionRequestCallbacks::onError()
{
    // FIXME: This should use a PermissionDeniedError.
    m_resolver->reject(DOMException::create(AbortError, "Registration failed - permission denied"));
}

} // namespace blink
