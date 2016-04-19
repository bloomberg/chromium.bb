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
#include "modules/notifications/NotificationResourcesLoader.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"
#include "platform/Histogram.h"
#include "platform/heap/Handle.h"
#include "public/platform/Platform.h"
#include "public/platform/WebSecurityOrigin.h"
#include "public/platform/modules/notifications/WebNotificationData.h"
#include "wtf/RefPtr.h"

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
            notifications.append(Notification::create(resolver->getExecutionContext(), notificationInfo.persistentId, notificationInfo.data, true /* showing */));

        return notifications;
    }

private:
    NotificationArray() = delete;
};

} // namespace

ServiceWorkerRegistrationNotifications::ServiceWorkerRegistrationNotifications(ExecutionContext* executionContext, ServiceWorkerRegistration* registration)
    : ContextLifecycleObserver(executionContext), m_registration(registration)
{
}

ServiceWorkerRegistrationNotifications::~ServiceWorkerRegistrationNotifications()
{
}

ScriptPromise ServiceWorkerRegistrationNotifications::showNotification(ScriptState* scriptState, ServiceWorkerRegistration& registration, const String& title, const NotificationOptions& options, ExceptionState& exceptionState)
{
    ExecutionContext* executionContext = scriptState->getExecutionContext();

    // If context object's active worker is null, reject promise with a TypeError exception.
    if (!registration.active())
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

    OwnPtr<WebNotificationShowCallbacks> callbacks = adoptPtr(new CallbackPromiseAdapter<void, void>(resolver));
    ServiceWorkerRegistrationNotifications::from(executionContext, registration).prepareShow(data, callbacks.release());

    return promise;
}

ScriptPromise ServiceWorkerRegistrationNotifications::getNotifications(ScriptState* scriptState, ServiceWorkerRegistration& registration, const GetNotificationOptions& options)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    WebNotificationGetCallbacks* callbacks = new CallbackPromiseAdapter<NotificationArray, void>(resolver);

    WebNotificationManager* notificationManager = Platform::current()->notificationManager();
    ASSERT(notificationManager);

    notificationManager->getNotifications(options.tag(), registration.webRegistration(), callbacks);
    return promise;
}

void ServiceWorkerRegistrationNotifications::contextDestroyed()
{
    for (auto loader : m_loaders)
        loader->stop();
}

DEFINE_TRACE(ServiceWorkerRegistrationNotifications)
{
    visitor->trace(m_registration);
    visitor->trace(m_loaders);
    Supplement<ServiceWorkerRegistration>::trace(visitor);
    ContextLifecycleObserver::trace(visitor);
}

const char* ServiceWorkerRegistrationNotifications::supplementName()
{
    return "ServiceWorkerRegistrationNotifications";
}

ServiceWorkerRegistrationNotifications& ServiceWorkerRegistrationNotifications::from(ExecutionContext* executionContext, ServiceWorkerRegistration& registration)
{
    ServiceWorkerRegistrationNotifications* supplement = static_cast<ServiceWorkerRegistrationNotifications*>(Supplement<ServiceWorkerRegistration>::from(registration, supplementName()));
    if (!supplement) {
        supplement = new ServiceWorkerRegistrationNotifications(executionContext, &registration);
        provideTo(registration, supplementName(), supplement);
    }
    return *supplement;
}

void ServiceWorkerRegistrationNotifications::prepareShow(const WebNotificationData& data, PassOwnPtr<WebNotificationShowCallbacks> callbacks)
{
    RefPtr<SecurityOrigin> origin = getExecutionContext()->getSecurityOrigin();
    NotificationResourcesLoader* loader = new NotificationResourcesLoader(bind<NotificationResourcesLoader*>(&ServiceWorkerRegistrationNotifications::didLoadResources, WeakPersistentThisPointer<ServiceWorkerRegistrationNotifications>(this), origin.release(), data, callbacks));
    m_loaders.add(loader);
    loader->start(getExecutionContext(), data);
}

void ServiceWorkerRegistrationNotifications::didLoadResources(PassRefPtr<SecurityOrigin> origin, const WebNotificationData& data, PassOwnPtr<WebNotificationShowCallbacks> callbacks, NotificationResourcesLoader* loader)
{
    DCHECK(m_loaders.contains(loader));

    WebNotificationManager* notificationManager = Platform::current()->notificationManager();
    DCHECK(notificationManager);

    notificationManager->showPersistent(WebSecurityOrigin(origin.get()), data, loader->getResources(), m_registration->webRegistration(), callbacks.leakPtr());
    m_loaders.remove(loader);
}

} // namespace blink
