/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/Document.h"
#include "core/events/Event.h"
#include "core/page/WindowFocusAllowedIndicator.h"
#include "modules/notifications/NotificationClient.h"
#include "modules/notifications/NotificationController.h"
#include "modules/notifications/NotificationOptions.h"
#include "modules/notifications/NotificationPermissionClient.h"

namespace blink {

Notification* Notification::create(ExecutionContext* context, const String& title, const NotificationOptions& options)
{
    NotificationClient& client = NotificationController::clientFrom(context);
    Notification* notification = adoptRefCountedGarbageCollectedWillBeNoop(new Notification(title, context, &client));

    notification->setBody(options.body());
    notification->setTag(options.tag());
    notification->setLang(options.lang());
    notification->setDir(options.dir());
    if (options.hasIcon()) {
        KURL iconUrl = options.icon().isEmpty() ? KURL() : context->completeURL(options.icon());
        if (!iconUrl.isEmpty() && iconUrl.isValid())
            notification->setIconUrl(iconUrl);
    }

    notification->suspendIfNeeded();
    return notification;
}

Notification::Notification(const String& title, ExecutionContext* context, NotificationClient* client)
    : ActiveDOMObject(context)
    , m_title(title)
    , m_dir("auto")
    , m_state(NotificationStateIdle)
    , m_client(client)
    , m_asyncRunner(this, &Notification::show)
{
    ASSERT(m_client);

    m_asyncRunner.runAsync();
}

Notification::~Notification()
{
}

void Notification::show()
{
    ASSERT(m_state == NotificationStateIdle);
    if (!toDocument(executionContext())->page())
        return;

    if (m_client->checkPermission(executionContext()) != NotificationClient::PermissionAllowed) {
        dispatchErrorEvent();
        return;
    }

    if (m_client->show(this))
        m_state = NotificationStateShowing;
}

void Notification::close()
{
    switch (m_state) {
    case NotificationStateIdle:
        break;
    case NotificationStateShowing:
        m_client->close(this);
        break;
    case NotificationStateClosed:
        break;
    }
}

void Notification::dispatchShowEvent()
{
    dispatchEvent(Event::create(EventTypeNames::show));
}

void Notification::dispatchClickEvent()
{
    UserGestureIndicator gestureIndicator(DefinitelyProcessingNewUserGesture);
    WindowFocusAllowedIndicator windowFocusAllowed;
    dispatchEvent(Event::create(EventTypeNames::click));
}

void Notification::dispatchErrorEvent()
{
    dispatchEvent(Event::create(EventTypeNames::error));
}

void Notification::dispatchCloseEvent()
{
    dispatchEvent(Event::create(EventTypeNames::close));
    m_state = NotificationStateClosed;
}

TextDirection Notification::direction() const
{
    // FIXME: Resolve dir()=="auto" against the document.
    return dir() == "rtl" ? RTL : LTR;
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

const String& Notification::permission(ExecutionContext* context)
{
    return permissionString(NotificationController::clientFrom(context).checkPermission(context));
}

void Notification::requestPermission(ExecutionContext* context, PassOwnPtrWillBeRawPtr<NotificationPermissionCallback> callback)
{
    // FIXME: Assert that this code-path will only be reached for Document environments
    // when Blink supports [Exposed] annotations on class members in IDL definitions.
    if (NotificationPermissionClient* permissionClient = NotificationPermissionClient::from(context)) {
        permissionClient->requestPermission(context, callback);
        return;
    }
}

bool Notification::dispatchEvent(PassRefPtrWillBeRawPtr<Event> event)
{
    ASSERT(m_state != NotificationStateClosed);

    return EventTarget::dispatchEvent(event);
}

const AtomicString& Notification::interfaceName() const
{
    return EventTargetNames::Notification;
}

void Notification::stop()
{
    m_state = NotificationStateClosed;

    if (m_client) {
        m_client->notificationObjectDestroyed(this);
        m_client = 0;
    }

    m_asyncRunner.stop();
}

bool Notification::hasPendingActivity() const
{
    return m_state == NotificationStateShowing || m_asyncRunner.isActive();
}

} // namespace blink
