// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/push_messaging/PushPermissionRequestCallback.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "modules/push_messaging/PushManager.h"

namespace blink {

PushPermissionRequestCallback::PushPermissionRequestCallback(PushManager* manager, WebPushClient* client, PassRefPtr<ScriptPromiseResolver> resolver, WebServiceWorkerProvider* serviceWorkerProvider)
    : m_manager(manager)
    , m_client(client)
    , m_resolver(resolver)
    , m_serviceWorkerProvider(serviceWorkerProvider)
{
}

PushPermissionRequestCallback::~PushPermissionRequestCallback()
{
}

void PushPermissionRequestCallback::run()
{
    m_manager->doRegister(m_client, m_resolver, m_serviceWorkerProvider);
}

} // namespace blink
