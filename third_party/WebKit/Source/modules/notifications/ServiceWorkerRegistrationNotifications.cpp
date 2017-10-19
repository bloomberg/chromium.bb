// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/notifications/ServiceWorkerRegistrationNotifications.h"

#include <memory>
#include <utility>
#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/ExecutionContext.h"
#include "modules/notifications/GetNotificationOptions.h"
#include "modules/notifications/Notification.h"
#include "modules/notifications/NotificationData.h"
#include "modules/notifications/NotificationManager.h"
#include "modules/notifications/NotificationOptions.h"
#include "modules/notifications/NotificationResourcesLoader.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"
#include "platform/Histogram.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/RefPtr.h"
#include "public/platform/Platform.h"
#include "public/platform/WebSecurityOrigin.h"
#include "public/platform/modules/notifications/WebNotificationData.h"

namespace blink {
namespace {

// Allows using a CallbackPromiseAdapter with a WebVector to resolve the
// getNotifications() promise with a HeapVector owning Notifications.
class NotificationArray {
 public:
  using WebType = const WebVector<WebPersistentNotificationInfo>&;

  static HeapVector<Member<Notification>> Take(
      ScriptPromiseResolver* resolver,
      const WebVector<WebPersistentNotificationInfo>& notification_infos) {
    HeapVector<Member<Notification>> notifications;
    for (const WebPersistentNotificationInfo& notification_info :
         notification_infos)
      notifications.push_back(Notification::Create(
          resolver->GetExecutionContext(), notification_info.notification_id,
          notification_info.data, true /* showing */));

    return notifications;
  }

 private:
  NotificationArray() = delete;
};

}  // namespace

ServiceWorkerRegistrationNotifications::ServiceWorkerRegistrationNotifications(
    ExecutionContext* execution_context,
    ServiceWorkerRegistration* registration)
    : ContextLifecycleObserver(execution_context),
      registration_(registration) {}

ScriptPromise ServiceWorkerRegistrationNotifications::showNotification(
    ScriptState* script_state,
    ServiceWorkerRegistration& registration,
    const String& title,
    const NotificationOptions& options,
    ExceptionState& exception_state) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);

  // If context object's active worker is null, reject the promise with a
  // TypeError exception.
  if (!registration.active())
    return ScriptPromise::Reject(
        script_state,
        V8ThrowException::CreateTypeError(script_state->GetIsolate(),
                                          "No active registration available on "
                                          "the ServiceWorkerRegistration."));

  // If permission for notification's origin is not "granted", reject the
  // promise with a TypeError exception, and terminate these substeps.
  if (NotificationManager::From(execution_context)
          ->GetPermissionStatus(execution_context) !=
      mojom::blink::PermissionStatus::GRANTED)
    return ScriptPromise::Reject(
        script_state,
        V8ThrowException::CreateTypeError(
            script_state->GetIsolate(),
            "No notification permission has been granted for this origin."));

  // Validate the developer-provided options to get the WebNotificationData.
  WebNotificationData data = CreateWebNotificationData(
      execution_context, title, options, exception_state);
  if (exception_state.HadException())
    return exception_state.Reject(script_state);

  // Log number of actions developer provided in linear histogram:
  //     0    -> underflow bucket,
  //     1-16 -> distinct buckets,
  //     17+  -> overflow bucket.
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, notification_count_histogram,
      ("Notifications.PersistentNotificationActionCount", 17));
  notification_count_histogram.Count(options.actions().size());

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  std::unique_ptr<WebNotificationShowCallbacks> callbacks =
      WTF::WrapUnique(new CallbackPromiseAdapter<void, void>(resolver));
  ServiceWorkerRegistrationNotifications::From(execution_context, registration)
      .PrepareShow(data, std::move(callbacks));

  return promise;
}

ScriptPromise ServiceWorkerRegistrationNotifications::getNotifications(
    ScriptState* script_state,
    ServiceWorkerRegistration& registration,
    const GetNotificationOptions& options) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  auto callbacks =
      std::make_unique<CallbackPromiseAdapter<NotificationArray, void>>(
          resolver);

  WebNotificationManager* notification_manager =
      Platform::Current()->GetNotificationManager();
  DCHECK(notification_manager);

  notification_manager->GetNotifications(
      options.tag(), registration.WebRegistration(), std::move(callbacks));
  return promise;
}

void ServiceWorkerRegistrationNotifications::ContextDestroyed(
    ExecutionContext*) {
  for (auto loader : loaders_)
    loader->Stop();
}

void ServiceWorkerRegistrationNotifications::Trace(blink::Visitor* visitor) {
  visitor->Trace(registration_);
  visitor->Trace(loaders_);
  Supplement<ServiceWorkerRegistration>::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

const char* ServiceWorkerRegistrationNotifications::SupplementName() {
  return "ServiceWorkerRegistrationNotifications";
}

ServiceWorkerRegistrationNotifications&
ServiceWorkerRegistrationNotifications::From(
    ExecutionContext* execution_context,
    ServiceWorkerRegistration& registration) {
  ServiceWorkerRegistrationNotifications* supplement =
      static_cast<ServiceWorkerRegistrationNotifications*>(
          Supplement<ServiceWorkerRegistration>::From(registration,
                                                      SupplementName()));
  if (!supplement) {
    supplement = new ServiceWorkerRegistrationNotifications(execution_context,
                                                            &registration);
    ProvideTo(registration, SupplementName(), supplement);
  }
  return *supplement;
}

void ServiceWorkerRegistrationNotifications::PrepareShow(
    const WebNotificationData& data,
    std::unique_ptr<WebNotificationShowCallbacks> callbacks) {
  RefPtr<SecurityOrigin> origin = GetExecutionContext()->GetSecurityOrigin();
  NotificationResourcesLoader* loader = new NotificationResourcesLoader(
      WTF::Bind(&ServiceWorkerRegistrationNotifications::DidLoadResources,
                WrapWeakPersistent(this), std::move(origin), data,
                WTF::Passed(std::move(callbacks))));
  loaders_.insert(loader);
  loader->Start(GetExecutionContext(), data);
}

void ServiceWorkerRegistrationNotifications::DidLoadResources(
    RefPtr<SecurityOrigin> origin,
    const WebNotificationData& data,
    std::unique_ptr<WebNotificationShowCallbacks> callbacks,
    NotificationResourcesLoader* loader) {
  DCHECK(loaders_.Contains(loader));

  WebNotificationManager* notification_manager =
      Platform::Current()->GetNotificationManager();
  DCHECK(notification_manager);

  notification_manager->ShowPersistent(
      WebSecurityOrigin(origin.get()), data, loader->GetResources(),
      registration_->WebRegistration(), std::move(callbacks));
  loaders_.erase(loader);
}

}  // namespace blink
