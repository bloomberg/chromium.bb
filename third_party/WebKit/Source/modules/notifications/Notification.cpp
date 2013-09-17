/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2009, 2011, 2012 Apple Inc. All rights reserved.
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

#include "modules/notifications/Notification.h"

#include "bindings/v8/Dictionary.h"
#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/dom/ErrorEvent.h"
#include "core/dom/EventNames.h"
#include "core/loader/ThreadableLoader.h"
#include "core/page/DOMWindow.h"
#include "core/page/WindowFocusAllowedIndicator.h"
#include "core/platform/network/ResourceRequest.h"
#include "core/platform/network/ResourceResponse.h"
#include "core/workers/WorkerGlobalScope.h"
#include "modules/notifications/DOMWindowNotifications.h"
#include "modules/notifications/NotificationCenter.h"
#include "modules/notifications/NotificationClient.h"
#include "modules/notifications/NotificationController.h"
#include "modules/notifications/NotificationPermissionCallback.h"

namespace WebCore {

Notification::Notification()
    : ActiveDOMObject(0)
{
    ScriptWrappable::init(this);
}

#if ENABLE(LEGACY_NOTIFICATIONS)
Notification::Notification(const String& title, const String& body, const String& iconURI, ScriptExecutionContext* context, ExceptionState& es, PassRefPtr<NotificationCenter> provider)
    : ActiveDOMObject(context)
    , m_title(title)
    , m_body(body)
    , m_state(Idle)
    , m_notificationClient(provider->client())
{
    ASSERT(m_notificationClient);

    ScriptWrappable::init(this);
    if (provider->checkPermission() != NotificationClient::PermissionAllowed) {
        es.throwSecurityError(ExceptionMessages::failedToExecute("createNotification", "NotificationCenter", "Notification permission has not been granted."));
        return;
    }

    m_icon = iconURI.isEmpty() ? KURL() : scriptExecutionContext()->completeURL(iconURI);
    if (!m_icon.isEmpty() && !m_icon.isValid()) {
        es.throwDOMException(SyntaxError, ExceptionMessages::failedToExecute("createNotification", "NotificationCenter", "'" + iconURI + "' is not a valid icon URL."));
        return;
    }
}
#endif

Notification::Notification(ScriptExecutionContext* context, const String& title)
    : ActiveDOMObject(context)
    , m_title(title)
    , m_state(Idle)
    , m_taskTimer(adoptPtr(new Timer<Notification>(this, &Notification::taskTimerFired)))
{
    ScriptWrappable::init(this);

    m_notificationClient = NotificationController::clientFrom(toDocument(context)->page());
    ASSERT(m_notificationClient);

    m_taskTimer->startOneShot(0);
}

Notification::~Notification()
{
}

#if ENABLE(LEGACY_NOTIFICATIONS)
PassRefPtr<Notification> Notification::create(const String& title, const String& body, const String& iconURI, ScriptExecutionContext* context, ExceptionState& es, PassRefPtr<NotificationCenter> provider)
{
    RefPtr<Notification> notification(adoptRef(new Notification(title, body, iconURI, context, es, provider)));
    notification->suspendIfNeeded();
    return notification.release();
}
#endif

PassRefPtr<Notification> Notification::create(ScriptExecutionContext* context, const String& title, const Dictionary& options)
{
    RefPtr<Notification> notification(adoptRef(new Notification(context, title)));
    String argument;
    if (options.get("body", argument))
        notification->setBody(argument);
    if (options.get("tag", argument))
        notification->setTag(argument);
    if (options.get("lang", argument))
        notification->setLang(argument);
    if (options.get("dir", argument))
        notification->setDir(argument);
    if (options.get("icon", argument)) {
        KURL iconURI = argument.isEmpty() ? KURL() : context->completeURL(argument);
        if (!iconURI.isEmpty() && iconURI.isValid())
            notification->setIconURL(iconURI);
    }

    notification->suspendIfNeeded();
    return notification.release();
}

const AtomicString& Notification::interfaceName() const
{
    return eventNames().interfaceForNotification;
}

void Notification::show()
{
    // prevent double-showing
    if (m_state == Idle) {
        if (!toDocument(scriptExecutionContext())->page())
            return;
        if (NotificationController::from(toDocument(scriptExecutionContext())->page())->client()->checkPermission(scriptExecutionContext()) != NotificationClient::PermissionAllowed) {
            dispatchErrorEvent();
            return;
        }
        if (m_notificationClient->show(this)) {
            m_state = Showing;
            setPendingActivity(this);
        }
    }
}

void Notification::close()
{
    switch (m_state) {
    case Idle:
        break;
    case Showing:
        m_notificationClient->cancel(this);
        break;
    case Closed:
        break;
    }
}

EventTargetData* Notification::eventTargetData()
{
    return &m_eventTargetData;
}

EventTargetData* Notification::ensureEventTargetData()
{
    return &m_eventTargetData;
}

void Notification::contextDestroyed()
{
    ActiveDOMObject::contextDestroyed();
    m_notificationClient->notificationObjectDestroyed(this);
}

void Notification::finalize()
{
    if (m_state == Closed)
        return;
    m_state = Closed;
    unsetPendingActivity(this);
}

void Notification::dispatchShowEvent()
{
#if ENABLE(LEGACY_NOTIFICATIONS)
    dispatchEvent(Event::create(eventNames().displayEvent));
#endif
    dispatchEvent(Event::create(eventNames().showEvent));
}

void Notification::dispatchClickEvent()
{
    UserGestureIndicator gestureIndicator(DefinitelyProcessingNewUserGesture);
    WindowFocusAllowedIndicator windowFocusAllowed;
    dispatchEvent(Event::create(eventNames().clickEvent));
}

void Notification::dispatchCloseEvent()
{
    dispatchEvent(Event::create(eventNames().closeEvent));
    finalize();
}

void Notification::dispatchErrorEvent()
{
    dispatchEvent(Event::create(eventNames().errorEvent));
}

void Notification::taskTimerFired(Timer<Notification>* timer)
{
    ASSERT(scriptExecutionContext()->isDocument());
    ASSERT_UNUSED(timer, timer == m_taskTimer.get());
    show();
}

bool Notification::dispatchEvent(PassRefPtr<Event> event)
{
    // Do not dispatch if the context is gone.
    if (!scriptExecutionContext())
        return false;

    return EventTarget::dispatchEvent(event);
}

const String& Notification::permission(ScriptExecutionContext* context)
{
    ASSERT(toDocument(context)->page());
    return permissionString(NotificationController::from(toDocument(context)->page())->client()->checkPermission(context));
}

const String& Notification::permissionString(NotificationClient::Permission permission)
{
    DEFINE_STATIC_LOCAL(const String, allowedPermission, ("granted"));
    DEFINE_STATIC_LOCAL(const String, deniedPermission, ("denied"));
    DEFINE_STATIC_LOCAL(const String, defaultPermission, ("default"));

    switch (permission) {
    case NotificationClient::PermissionAllowed:
        return allowedPermission;
    case NotificationClient::PermissionDenied:
        return deniedPermission;
    case NotificationClient::PermissionNotAllowed:
        return defaultPermission;
    }

    ASSERT_NOT_REACHED();
    return deniedPermission;
}

void Notification::requestPermission(ScriptExecutionContext* context, PassRefPtr<NotificationPermissionCallback> callback)
{
    ASSERT(toDocument(context)->page());
    NotificationController::from(toDocument(context)->page())->client()->requestPermission(context, callback);
}

} // namespace WebCore
