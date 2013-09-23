/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
#include "WebNotification.h"

#include "WebTextDirection.h"
#include "core/events/Event.h"
#include "core/dom/UserGestureIndicator.h"
#include "core/page/WindowFocusAllowedIndicator.h"
#include "modules/notifications/Notification.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "wtf/PassRefPtr.h"

using namespace WebCore;

namespace WebKit {

class WebNotificationPrivate : public Notification {
};

void WebNotification::reset()
{
    assign(0);
}

void WebNotification::assign(const WebNotification& other)
{
    WebNotificationPrivate* p = const_cast<WebNotificationPrivate*>(other.m_private);
    if (p)
        p->ref(); // FIXME: We should use WebPrivatePtr.
    assign(p);
}

bool WebNotification::lessThan(const WebNotification& other) const
{
    return reinterpret_cast<uintptr_t>(m_private) < reinterpret_cast<uintptr_t>(other.m_private);
}

bool WebNotification::isHTML() const
{
    return false;
}

WebURL WebNotification::url() const
{
    return WebURL();
}

WebURL WebNotification::iconURL() const
{
    return m_private->iconURL();
}

WebString WebNotification::title() const
{
    return m_private->title();
}

WebString WebNotification::body() const
{
    return m_private->body();
}

WebTextDirection WebNotification::direction() const
{
    return (m_private->direction() == RTL) ?
        WebTextDirectionRightToLeft :
        WebTextDirectionLeftToRight;
}

WebString WebNotification::replaceId() const
{
    return m_private->tag();
}

void WebNotification::detachPresenter()
{
    m_private->detachPresenter();
}

void WebNotification::dispatchDisplayEvent()
{
    m_private->dispatchShowEvent();
}

void WebNotification::dispatchErrorEvent(const WebKit::WebString& /* errorMessage */)
{
    // FIXME: errorMessage not supported by WebCore yet
    m_private->dispatchErrorEvent();
}

void WebNotification::dispatchCloseEvent(bool /* byUser */)
{
    // FIXME: byUser flag not supported by WebCore yet
    m_private->dispatchCloseEvent();
}

void WebNotification::dispatchClickEvent()
{
    m_private->dispatchClickEvent();
}

WebNotification::WebNotification(const WTF::PassRefPtr<Notification>& notification)
    : m_private(static_cast<WebNotificationPrivate*>(notification.leakRef()))
{
}

WebNotification& WebNotification::operator=(const WTF::PassRefPtr<Notification>& notification)
{
    assign(static_cast<WebNotificationPrivate*>(notification.leakRef()));
    return *this;
}

WebNotification::operator WTF::PassRefPtr<Notification>() const
{
    return WTF::PassRefPtr<Notification>(const_cast<WebNotificationPrivate*>(m_private));
}

void WebNotification::assign(WebNotificationPrivate* p)
{
    // p is already ref'd for us by the caller
    if (m_private)
        m_private->deref();
    m_private = p;
}

} // namespace WebKit
