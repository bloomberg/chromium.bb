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
#include "modules/notifications/WebKitNotification.h"

#if ENABLE(LEGACY_NOTIFICATIONS)

#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ScriptWrappable.h"
#include "core/dom/ExecutionContext.h"
#include "modules/notifications/NotificationCenter.h"
#include "modules/notifications/NotificationClient.h"

namespace WebCore {

PassRefPtrWillBeRawPtr<WebKitNotification> WebKitNotification::create(const String& title, const String& body, const String& iconUrl, ExecutionContext* context, ExceptionState& es, PassRefPtrWillBeRawPtr<NotificationCenter> provider)
{
    RefPtrWillBeRawPtr<WebKitNotification> notification = adoptRefWillBeRefCountedGarbageCollected(new WebKitNotification(title, body, iconUrl, context, es, provider));
    notification->suspendIfNeeded();

    return notification.release();
}

WebKitNotification::WebKitNotification(const String& title, const String& body, const String& iconUrl, ExecutionContext* context, ExceptionState& es, PassRefPtrWillBeRawPtr<NotificationCenter> provider)
    : NotificationBase(title, context, provider->client())
{
    ScriptWrappable::init(this);

    if (provider->checkPermission() != NotificationClient::PermissionAllowed) {
        es.throwSecurityError("Notification permission has not been granted.");
        return;
    }

    KURL icon = iconUrl.isEmpty() ? KURL() : executionContext()->completeURL(iconUrl);
    if (!icon.isEmpty() && !icon.isValid()) {
        es.throwDOMException(SyntaxError, "'" + iconUrl + "' is not a valid icon URL.");
        return;
    }

    setBody(body);
    setIconUrl(icon);
}

WebKitNotification::~WebKitNotification()
{
}

const AtomicString& WebKitNotification::interfaceName() const
{
    return EventTargetNames::WebKitNotification;
}

} // namespace WebCore

#endif // ENABLE(LEGACY_NOTIFICATIONS)
