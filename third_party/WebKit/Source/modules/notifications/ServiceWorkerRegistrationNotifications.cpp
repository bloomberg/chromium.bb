// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/notifications/ServiceWorkerRegistrationNotifications.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/V8ThrowException.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "modules/notifications/Notification.h"
#include "modules/notifications/NotificationOptions.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/Platform.h"
#include "public/platform/WebSerializedOrigin.h"
#include "public/platform/modules/notifications/WebNotificationData.h"
#include "public/platform/modules/notifications/WebNotificationManager.h"

namespace blink {

ScriptPromise ServiceWorkerRegistrationNotifications::showNotification(ScriptState* scriptState, ServiceWorkerRegistration& serviceWorkerRegistration, const String& title, const NotificationOptions& options)
{
    ExecutionContext* executionContext = scriptState->executionContext();

    // If context object's active worker is null, reject promise with a TypeError exception.
    if (!serviceWorkerRegistration.active())
        return ScriptPromise::reject(scriptState, V8ThrowException::createTypeError(scriptState->isolate(), "No active registration available on the ServiceWorkerRegistration."));

    // If permission for notification's origin is not "granted", reject promise with a TypeError exception, and terminate these substeps.
    if (Notification::checkPermission(executionContext) != WebNotificationPermissionAllowed)
        return ScriptPromise::reject(scriptState, V8ThrowException::createTypeError(scriptState->isolate(), "No notification permission has been granted for this origin."));

    RefPtrWillBeRawPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    // FIXME: Do the appropriate CORS checks on the icon URL.

    KURL iconUrl;
    if (options.hasIcon() && !options.icon().isEmpty()) {
        iconUrl = executionContext->completeURL(options.icon());
        if (!iconUrl.isValid())
            iconUrl = KURL();
    }

    WebNotificationData::Direction dir = options.dir() == "rtl" ? WebNotificationData::DirectionRightToLeft : WebNotificationData::DirectionLeftToRight;
    WebNotificationData notification(title, dir, options.lang(), options.body(), options.tag(), iconUrl, options.silent());
    WebNotificationShowCallbacks* callbacks = new CallbackPromiseAdapter<void, void>(resolver);

    SecurityOrigin* origin = executionContext->securityOrigin();
    ASSERT(origin);

    WebNotificationManager* notificationManager = Platform::current()->notificationManager();
    ASSERT(notificationManager);

    notificationManager->showPersistent(WebSerializedOrigin(*origin), notification, serviceWorkerRegistration.webRegistration(), callbacks);
    return promise;
}

} // namespace blink
