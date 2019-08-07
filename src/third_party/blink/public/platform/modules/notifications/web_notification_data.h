// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_NOTIFICATIONS_WEB_NOTIFICATION_DATA_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_NOTIFICATIONS_WEB_NOTIFICATION_DATA_H_

#include "base/optional.h"
#include "base/time/time.h"
#include "third_party/blink/public/mojom/notifications/notification.mojom-shared.h"
#include "third_party/blink/public/platform/modules/notifications/web_notification_action.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_vector.h"

namespace blink {

// Structure representing the data associated with a Web Notification.
// Currently we're using the corresponding mojom struct
// blink::mojom::blink::NotificationData everywhere inside Blink,
// WebNotificationData is only used to carry notification data across the
// boundary between Content layer and Blink (the only two functions:
// WebServiceWorkerContextProxy::DispatchNotification{Click, Close}Event),
// ultimately WebNotificationData will also be replaced by the mojom one there
// via Onion Soup effort.
struct WebNotificationData {
  WebString title;
  mojom::NotificationDirection direction =
      mojom::NotificationDirection::LEFT_TO_RIGHT;
  WebString lang;
  WebString body;
  WebString tag;
  WebURL image;
  WebURL icon;
  WebURL badge;
  WebVector<int> vibrate;
  double timestamp = 0;
  bool renotify = false;
  bool silent = false;
  bool require_interaction = false;
  WebVector<char> data;
  WebVector<WebNotificationAction> actions;
  base::Optional<base::Time> show_trigger_timestamp;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_NOTIFICATIONS_WEB_NOTIFICATION_DATA_H_
