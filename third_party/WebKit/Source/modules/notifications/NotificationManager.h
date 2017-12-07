// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NotificationManager_h
#define NotificationManager_h

#include "core/dom/ExecutionContext.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/modules/notifications/notification_service.mojom-blink.h"
#include "public/platform/modules/permissions/permission.mojom-blink.h"

namespace blink {

class NotificationPermissionCallback;
class ScriptPromise;
class ScriptPromiseResolver;
class ScriptState;

// The notification manager, unique to the execution context, is responsible for
// connecting and communicating with the Mojo notification service.
//
// TODO(peter): Make the NotificationManager responsible for resource loading.
class NotificationManager final
    : public GarbageCollectedFinalized<NotificationManager>,
      public Supplement<ExecutionContext> {
  USING_GARBAGE_COLLECTED_MIXIN(NotificationManager);
  WTF_MAKE_NONCOPYABLE(NotificationManager);

 public:
  static NotificationManager* From(ExecutionContext*);
  static const char* SupplementName();

  ~NotificationManager();

  // Returns the notification permission status of the current origin. This
  // method is synchronous to support the Notification.permission getter.
  mojom::blink::PermissionStatus GetPermissionStatus();

  ScriptPromise RequestPermission(
      ScriptState*,
      NotificationPermissionCallback* deprecated_callback);

  // Shows a notification that is not tied to any service worker.
  void DisplayNonPersistentNotification(const String& title);

  virtual void Trace(blink::Visitor*);

 private:
  explicit NotificationManager(ExecutionContext&);

  // Returns an initialized NotificationServicePtr. A connection will be
  // established the first time this method is called.
  const mojom::blink::NotificationServicePtr& GetNotificationService();

  void OnPermissionRequestComplete(ScriptPromiseResolver*,
                                   NotificationPermissionCallback*,
                                   mojom::blink::PermissionStatus);

  void OnNotificationServiceConnectionError();
  void OnPermissionServiceConnectionError();

  mojom::blink::NotificationServicePtr notification_service_;
  mojom::blink::PermissionServicePtr permission_service_;
};

}  // namespace blink

#endif  // NotificationManager_h
