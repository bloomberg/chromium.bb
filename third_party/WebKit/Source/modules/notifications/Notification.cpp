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
#include "core/frame/UseCounter.h"
#include "core/page/WindowFocusAllowedIndicator.h"
#include "modules/notifications/NotificationOptions.h"
#include "modules/notifications/NotificationPermissionClient.h"
#include "public/platform/Platform.h"
#include "public/platform/WebNotificationData.h"
#include "public/platform/WebNotificationManager.h"
#include "public/platform/WebSerializedOrigin.h"

namespace blink {
namespace {

WebNotificationManager* notificationManager()
{
    return Platform::current()->notificationManager();
}

} // namespace

Notification* Notification::create(ExecutionContext* context, const String& title, const NotificationOptions& options)
{
    Notification* notification = new Notification(title, context);

    notification->setBody(options.body());
    notification->setTag(options.tag());
    notification->setLang(options.lang());
    notification->setDir(options.dir());
    if (options.hasIcon()) {
        KURL iconUrl = options.icon().isEmpty() ? KURL() : context->completeURL(options.icon());
        if (!iconUrl.isEmpty() && iconUrl.isValid())
            notification->setIconUrl(iconUrl);
    }

    String insecureOriginMessage;
    UseCounter::Feature feature = context->securityOrigin()->canAccessFeatureRequiringSecureOrigin(insecureOriginMessage)
        ? UseCounter::NotificationSecureOrigin : UseCounter::NotificationInsecureOrigin;
    UseCounter::count(context, feature);

    notification->suspendIfNeeded();
    return notification;
}

Notification::Notification(const String& title, ExecutionContext* context)
    : ActiveDOMObject(context)
    , m_title(title)
    , m_dir("auto")
    , m_state(NotificationStateIdle)
    , m_asyncRunner(this, &Notification::show)
{
    ASSERT(notificationManager());

    m_asyncRunner.runAsync();
}

Notification::~Notification()
{
}

void Notification::show()
{
    ASSERT(m_state == NotificationStateIdle);
    if (Notification::checkPermission(executionContext()) != WebNotificationPermissionAllowed) {
        dispatchErrorEvent();
        return;
    }

    SecurityOrigin* origin = executionContext()->securityOrigin();
    ASSERT(origin);

    // FIXME: Associate the appropriate text direction with the notification.
    // FIXME: Do CSP checks on the associated notification icon.
    WebNotificationData notificationData(m_title, WebNotificationData::DirectionLeftToRight, m_lang, m_body, m_tag, m_iconUrl);
    notificationManager()->show(WebSerializedOrigin(*origin), notificationData, this);

    m_state = NotificationStateShowing;
}

void Notification::close()
{
    if (m_state != NotificationStateShowing)
        return;

    m_state = NotificationStateClosed;
    notificationManager()->close(this);
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

const String& Notification::permissionString(WebNotificationPermission permission)
{
    DEFINE_STATIC_LOCAL(const String, allowedPermission, ("granted"));
    DEFINE_STATIC_LOCAL(const String, deniedPermission, ("denied"));
    DEFINE_STATIC_LOCAL(const String, defaultPermission, ("default"));

    switch (permission) {
    case WebNotificationPermissionAllowed:
        return allowedPermission;
    case WebNotificationPermissionDenied:
        return deniedPermission;
    case WebNotificationPermissionDefault:
        return defaultPermission;
    }

    ASSERT_NOT_REACHED();
    return deniedPermission;
}

const String& Notification::permission(ExecutionContext* context)
{
    return permissionString(checkPermission(context));
}

WebNotificationPermission Notification::checkPermission(ExecutionContext* context)
{
    SecurityOrigin* origin = context->securityOrigin();
    ASSERT(origin);

    return notificationManager()->checkPermission(WebSerializedOrigin(*origin));
}

void Notification::requestPermission(ExecutionContext* context, NotificationPermissionCallback* callback)
{
    // FIXME: Assert that this code-path will only be reached for Document environments
    // when Blink supports [Exposed] annotations on class members in IDL definitions.
    if (NotificationPermissionClient* permissionClient = NotificationPermissionClient::from(context))
        permissionClient->requestPermission(context, callback);
}

bool Notification::dispatchEvent(PassRefPtrWillBeRawPtr<Event> event)
{
    ASSERT(executionContext()->isContextThread());
    return EventTarget::dispatchEvent(event);
}

const AtomicString& Notification::interfaceName() const
{
    return EventTargetNames::Notification;
}

void Notification::stop()
{
    notificationManager()->notifyDelegateDestroyed(this);

    m_state = NotificationStateClosed;

    m_asyncRunner.stop();
}

bool Notification::hasPendingActivity() const
{
    return m_state == NotificationStateShowing || m_asyncRunner.isActive();
}

} // namespace blink
