// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/notifications/ServiceWorkerRegistrationNotifications.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/ExecutionContext.h"
#include "modules/notifications/GetNotificationOptions.h"
#include "modules/notifications/Notification.h"
#include "modules/notifications/NotificationData.h"
#include "modules/notifications/NotificationOptions.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"
#include "platform/Histogram.h"
#include "public/platform/Platform.h"
#include "public/platform/WebSecurityOrigin.h"
#include "public/platform/modules/notifications/WebNotificationData.h"
#include "public/platform/modules/notifications/WebNotificationManager.h"

namespace blink {
namespace {

// Allows using a CallbackPromiseAdapter with a WebVector to resolve the
// getNotifications() promise with a HeapVector owning Notifications.
class NotificationArray {
public:
    using WebType = const WebVector<WebPersistentNotificationInfo>&;

    static HeapVector<Member<Notification>> take(ScriptPromiseResolver* resolver, const WebVector<WebPersistentNotificationInfo>& notificationInfos)
    {
        HeapVector<Member<Notification>> notifications;
        for (const WebPersistentNotificationInfo& notificationInfo : notificationInfos)
            notifications.append(Notification::create(resolver->executionContext(), notificationInfo.persistentId, notificationInfo.data, true /* showing */));

        return notifications;
    }

private:
    NotificationArray() = delete;
};

} // namespace

ScriptPromise ServiceWorkerRegistrationNotifications::showNotification(ScriptState* scriptState, ServiceWorkerRegistration& serviceWorkerRegistration, const String& title, const NotificationOptions& options, ExceptionState& exceptionState)
{
    ExecutionContext* executionContext = scriptState->executionContext();

    // If context object's active worker is null, reject promise with a TypeError exception.
    if (!serviceWorkerRegistration.active())
        return ScriptPromise::reject(scriptState, V8ThrowException::createTypeError(scriptState->isolate(), "No active registration available on the ServiceWorkerRegistration."));

    // If permission for notification's origin is not "granted", reject promise with a TypeError exception, and terminate these substeps.
    if (Notification::checkPermission(executionContext) != WebNotificationPermissionAllowed)
        return ScriptPromise::reject(scriptState, V8ThrowException::createTypeError(scriptState->isolate(), "No notification permission has been granted for this origin."));

    // Validate the developer-provided values to get a WebNotificationData object.
    WebNotificationData data = createWebNotificationData(executionContext, title, options, exceptionState);
    if (exceptionState.hadException())
        return exceptionState.reject(scriptState);

    // Log number of actions developer provided in linear histogram: 0 -> underflow bucket, 1-16 -> distinct buckets, 17+ -> overflow bucket.
    DEFINE_THREAD_SAFE_STATIC_LOCAL(EnumerationHistogram, notificationCountHistogram, new EnumerationHistogram("Notifications.PersistentNotificationActionCount", 17));
    notificationCountHistogram.count(options.actions().size());

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    WebNotificationShowCallbacks* callbacks = new CallbackPromiseAdapter<void, void>(resolver);

    SecurityOrigin* origin = executionContext->securityOrigin();
    WebNotificationManager* notificationManager = Platform::current()->notificationManager();
    ASSERT(notificationManager);

    notificationManager->showPersistent(WebSecurityOrigin(origin), data, serviceWorkerRegistration.webRegistration(), callbacks);
    return promise;
}

ScriptPromise ServiceWorkerRegistrationNotifications::getNotifications(ScriptState* scriptState, ServiceWorkerRegistration& serviceWorkerRegistration, const GetNotificationOptions& options)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    WebNotificationGetCallbacks* callbacks = new CallbackPromiseAdapter<NotificationArray, void>(resolver);

    WebNotificationManager* notificationManager = Platform::current()->notificationManager();
    ASSERT(notificationManager);

    notificationManager->getNotifications(options.tag(), serviceWorkerRegistration.webRegistration(), callbacks);
    return promise;
}

} // namespace blink
