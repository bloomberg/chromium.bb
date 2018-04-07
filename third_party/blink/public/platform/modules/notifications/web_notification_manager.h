// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_NOTIFICATIONS_WEB_NOTIFICATION_MANAGER_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_NOTIFICATIONS_WEB_NOTIFICATION_MANAGER_H_

#include <stdint.h>
#include <memory>
#include "third_party/blink/public/platform/modules/notifications/web_notification_data.h"
#include "third_party/blink/public/platform/modules/notifications/web_notification_resources.h"
#include "third_party/blink/public/platform/web_callbacks.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"

namespace blink {

class WebSecurityOrigin;
class WebServiceWorkerRegistration;

// Structure representing the info associated with a persistent notification.
struct WebPersistentNotificationInfo {
  WebString notification_id;
  WebNotificationData data;
};

using WebNotificationGetCallbacks =
    WebCallbacks<const WebVector<WebPersistentNotificationInfo>&, void>;
using WebNotificationShowCallbacks = WebCallbacks<void, void>;

// Provides the services to show platform notifications to the user.
class WebNotificationManager {
 public:
  virtual ~WebNotificationManager() = default;

  // Shows a persistent notification on the user's system. These notifications
  // will have their events delivered to a Service Worker rather than the
  // object's delegate. Will take ownership of the WebNotificationShowCallbacks
  // object.
  virtual void ShowPersistent(
      const WebSecurityOrigin&,
      const WebNotificationData&,
      std::unique_ptr<WebNotificationResources>,
      WebServiceWorkerRegistration*,
      std::unique_ptr<WebNotificationShowCallbacks>) = 0;

  // Asynchronously gets the persistent notifications belonging to the Service
  // Worker Registration.  If |filterTag| is not an empty string, only the
  // notification with the given tag will be considered. Will take ownership of
  // the WebNotificationGetCallbacks object.
  virtual void GetNotifications(
      const WebString& filter_tag,
      WebServiceWorkerRegistration*,
      std::unique_ptr<WebNotificationGetCallbacks>) = 0;

  // Closes a persistent notification identified by its notification Id.
  virtual void ClosePersistent(const WebSecurityOrigin&,
                               const WebString& tag,
                               const WebString& notification_id) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_NOTIFICATIONS_WEB_NOTIFICATION_MANAGER_H_
