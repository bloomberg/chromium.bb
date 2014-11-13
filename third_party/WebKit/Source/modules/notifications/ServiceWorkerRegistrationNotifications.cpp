// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/notifications/ServiceWorkerRegistrationNotifications.h"

#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "modules/notifications/NotificationOptions.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/WebNotificationData.h"

namespace blink {

ScriptPromise ServiceWorkerRegistrationNotifications::showNotification(ScriptState* scriptState, ServiceWorkerRegistration& serviceWorkerRegistration, const String& title, const NotificationOptions& options)
{
    KURL iconUrl;
    if (options.hasIcon() && !options.icon().isEmpty()) {
        iconUrl = scriptState->executionContext()->completeURL(options.icon());
        if (!iconUrl.isValid())
            iconUrl = KURL();
    }

    WebNotificationData notification(title, WebNotificationData::DirectionLeftToRight, options.lang(), options.body(), options.tag(), iconUrl);

    // FIXME: Hook this up with the Blink API once it's been implemented.
    return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError, "showNotification is not implemented yet."));
}

} // namespace blink
