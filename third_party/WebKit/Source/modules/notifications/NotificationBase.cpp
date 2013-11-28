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
#include "modules/notifications/NotificationBase.h"

#include "core/dom/Document.h"
#include "core/page/WindowFocusAllowedIndicator.h"
#include "modules/notifications/NotificationController.h"

namespace WebCore {

NotificationBase::NotificationBase(const String& title, ExecutionContext* context, NotificationClient* client)
    : ActiveDOMObject(context)
    , m_title(title)
    , m_dir("auto")
    , m_state(Idle)
    , m_client(client)
{
    ASSERT(m_client);
}

NotificationBase::~NotificationBase()
{
}

void NotificationBase::show()
{
    // prevent double-showing
    if (m_state == Idle) {
        if (!toDocument(executionContext())->page())
            return;
        if (NotificationController::from(toDocument(executionContext())->page())->client()->checkPermission(executionContext()) != NotificationClient::PermissionAllowed) {
            dispatchErrorEvent();
            return;
        }
        if (m_client->show(this)) {
            m_state = Showing;
        }
    }
}

void NotificationBase::close()
{
    switch (m_state) {
    case Idle:
        break;
    case Showing:
        m_client->cancel(this);
        break;
    case Closed:
        break;
    }
}

void NotificationBase::dispatchShowEvent()
{
    dispatchEvent(Event::create(EventTypeNames::show));
}

void NotificationBase::dispatchClickEvent()
{
    UserGestureIndicator gestureIndicator(DefinitelyProcessingNewUserGesture);
    WindowFocusAllowedIndicator windowFocusAllowed;
    dispatchEvent(Event::create(EventTypeNames::click));
}

void NotificationBase::dispatchErrorEvent()
{
    dispatchEvent(Event::create(EventTypeNames::error));
}

void NotificationBase::dispatchCloseEvent()
{
    dispatchEvent(Event::create(EventTypeNames::close));
    m_state = Closed;
}

TextDirection NotificationBase::direction() const
{
    // FIXME: Resolve dir()=="auto" against the document.
    return dir() == "rtl" ? RTL : LTR;
}

const String& NotificationBase::permissionString(NotificationClient::Permission permission)
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

bool NotificationBase::dispatchEvent(PassRefPtr<Event> event)
{
    // Do not dispatch if the context is gone.
    if (!executionContext())
        return false;

    return EventTarget::dispatchEvent(event);
}

void NotificationBase::stop()
{
    if (m_client)
        m_client->notificationObjectDestroyed(this);

    m_client = 0;
    m_state = Closed;
}

bool NotificationBase::hasPendingActivity() const
{
    return m_state == Showing;
}

} // namespace WebCore
