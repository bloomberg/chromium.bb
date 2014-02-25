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
#include "bindings/v8/ScriptWrappable.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "modules/notifications/NotificationController.h"

namespace WebCore {

PassRefPtrWillBeRawPtr<Notification> Notification::create(ExecutionContext* context, const String& title, const Dictionary& options)
{
    NotificationClient* client = NotificationController::clientFrom(toDocument(context)->page());
    RefPtrWillBeRawPtr<Notification> notification = adoptRefCountedWillBeRefCountedGarbageCollected(new Notification(context, title, client));

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
        KURL iconUrl = argument.isEmpty() ? KURL() : context->completeURL(argument);
        if (!iconUrl.isEmpty() && iconUrl.isValid())
            notification->setIconUrl(iconUrl);
    }

    notification->suspendIfNeeded();
    return notification.release();
}

Notification::Notification(ExecutionContext* context, const String& title, NotificationClient* client)
    : NotificationBase(title, context, client)
    , m_asyncRunner(adoptPtr(new AsyncMethodRunner<Notification>(this, &Notification::showSoon)))
{
    ScriptWrappable::init(this);

    m_asyncRunner->runAsync();
}

Notification::~Notification()
{
}

const String& Notification::permission(ExecutionContext* context)
{
    ASSERT(toDocument(context)->page());
    return permissionString(NotificationController::from(toDocument(context)->page())->client()->checkPermission(context));
}

void Notification::requestPermission(ExecutionContext* context, PassOwnPtr<NotificationPermissionCallback> callback)
{
    ASSERT(toDocument(context)->page());
    NotificationController::from(toDocument(context)->page())->client()->requestPermission(context, callback);
}

const AtomicString& Notification::interfaceName() const
{
    return EventTargetNames::Notification;
}

void Notification::stop()
{
    NotificationBase::stop();
    if (m_asyncRunner)
        m_asyncRunner->stop();
}

bool Notification::hasPendingActivity() const
{
    return NotificationBase::hasPendingActivity() || (m_asyncRunner && m_asyncRunner->isActive());
}

void Notification::showSoon()
{
    ASSERT(executionContext()->isDocument());
    show();
}

} // namespace WebCore
