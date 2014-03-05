/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(LEGACY_NOTIFICATIONS)

#include "modules/notifications/NotificationCenter.h"

#include "core/dom/Document.h"
#include "core/workers/WorkerGlobalScope.h"
#include "modules/notifications/NotificationClient.h"
#include "platform/weborigin/SecurityOrigin.h"

namespace WebCore {

PassRefPtrWillBeRawPtr<NotificationCenter> NotificationCenter::create(ExecutionContext* context, NotificationClient* client)
{
    RefPtrWillBeRawPtr<NotificationCenter> notificationCenter = adoptRefWillBeNoop(new NotificationCenter(context, client));
    notificationCenter->suspendIfNeeded();
    return notificationCenter.release();
}

NotificationCenter::NotificationCenter(ExecutionContext* context, NotificationClient* client)
    : ActiveDOMObject(context)
    , m_client(client)
{
    ScriptWrappable::init(this);
}

int NotificationCenter::checkPermission()
{
    if (!client() || !executionContext())
        return NotificationClient::PermissionDenied;

    switch (executionContext()->securityOrigin()->canShowNotifications()) {
    case SecurityOrigin::AlwaysAllow:
        return NotificationClient::PermissionAllowed;
    case SecurityOrigin::AlwaysDeny:
        return NotificationClient::PermissionDenied;
    case SecurityOrigin::Ask:
        return m_client->checkPermission(executionContext());
    }

    ASSERT_NOT_REACHED();
    return m_client->checkPermission(executionContext());
}

void NotificationCenter::requestPermission(PassOwnPtr<VoidCallback> callback)
{
    if (!client() || !executionContext())
        return;

    switch (executionContext()->securityOrigin()->canShowNotifications()) {
    case SecurityOrigin::AlwaysAllow:
    case SecurityOrigin::AlwaysDeny: {
        m_callbacks.add(NotificationRequestCallback::createAndStartTimer(this, callback));
        return;
    }
    case SecurityOrigin::Ask:
        return m_client->requestPermission(executionContext(), callback);
    }

    ASSERT_NOT_REACHED();
    m_client->requestPermission(executionContext(), callback);
}

void NotificationCenter::stop()
{
    m_client = 0;
}

void NotificationCenter::requestTimedOut(NotificationCenter::NotificationRequestCallback* request)
{
    m_callbacks.remove(request);
}

void NotificationCenter::trace(Visitor* visitor)
{
    visitor->trace(m_callbacks);
}

PassRefPtrWillBeRawPtr<NotificationCenter::NotificationRequestCallback> NotificationCenter::NotificationRequestCallback::createAndStartTimer(NotificationCenter* center, PassOwnPtr<VoidCallback> callback)
{
    RefPtrWillBeRawPtr<NotificationCenter::NotificationRequestCallback> requestCallback = adoptRefWillBeNoop(new NotificationCenter::NotificationRequestCallback(center, callback));
    requestCallback->startTimer();
    return requestCallback.release();
}

NotificationCenter::NotificationRequestCallback::NotificationRequestCallback(NotificationCenter* center, PassOwnPtr<VoidCallback> callback)
    : m_notificationCenter(center)
    , m_timer(this, &NotificationCenter::NotificationRequestCallback::timerFired)
    , m_callback(callback)
{
}

void NotificationCenter::NotificationRequestCallback::startTimer()
{
    m_timer.startOneShot(0);
}

void NotificationCenter::NotificationRequestCallback::timerFired(Timer<NotificationCenter::NotificationRequestCallback>*)
{
    if (m_callback)
        m_callback->handleEvent();
    m_notificationCenter->requestTimedOut(this);
}

void NotificationCenter::NotificationRequestCallback::trace(Visitor* visitor)
{
    visitor->trace(m_notificationCenter);
}

} // namespace WebCore

#endif // ENABLE(LEGACY_NOTIFICATIONS)
