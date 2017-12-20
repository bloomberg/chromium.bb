// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebNotificationManager_h
#define WebNotificationManager_h

#include "public/platform/WebCallbacks.h"
#include "public/platform/WebString.h"
#include "public/platform/WebVector.h"
#include "public/platform/modules/notifications/WebNotificationData.h"
#include "public/platform/modules/notifications/WebNotificationResources.h"
#include <memory>
#include <stdint.h>

namespace blink {

class WebNotificationDelegate;
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

  // Shows a page notification on the user's system. These notifications will
  // have their events delivered to the delegate specified in this call.
  virtual void Show(const WebSecurityOrigin&,
                    const WebNotificationData&,
                    std::unique_ptr<WebNotificationResources>,
                    WebNotificationDelegate*) = 0;

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

  // Closes a notification previously shown, and removes it if being shown.
  virtual void Close(WebNotificationDelegate*) = 0;

  // Closes a persistent notification identified by its notification Id.
  virtual void ClosePersistent(const WebSecurityOrigin&,
                               const WebString& tag,
                               const WebString& notification_id) = 0;

  // Indicates that the delegate object is being destroyed, and must no longer
  // be used by the embedder to dispatch events.
  virtual void NotifyDelegateDestroyed(WebNotificationDelegate*) = 0;
};

}  // namespace blink

#endif  // WebNotificationManager_h
