// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/notification_data_conversions.h"

#include "third_party/blink/public/mojom/notifications/notification.mojom-shared.h"
#include "third_party/blink/public/platform/modules/notifications/web_notification_action.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

WebNotificationData ToWebNotificationData(
    const PlatformNotificationData& platform_data) {
  WebNotificationData web_data;
  web_data.title = WebString::FromUTF16(platform_data.title);
  switch (platform_data.direction) {
    case PlatformNotificationData::DIRECTION_LEFT_TO_RIGHT:
      web_data.direction = mojom::NotificationDirection::LEFT_TO_RIGHT;
      break;
    case PlatformNotificationData::DIRECTION_RIGHT_TO_LEFT:
      web_data.direction = mojom::NotificationDirection::RIGHT_TO_LEFT;
      break;
    case PlatformNotificationData::DIRECTION_AUTO:
      web_data.direction = mojom::NotificationDirection::AUTO;
      break;
  }

  web_data.lang = WebString::FromUTF8(platform_data.lang);
  web_data.body = WebString::FromUTF16(platform_data.body);
  web_data.tag = WebString::FromUTF8(platform_data.tag);
  web_data.image = WebURL(KURL(platform_data.image));
  web_data.icon = WebURL(KURL(platform_data.icon));
  web_data.badge = WebURL(KURL(platform_data.badge));
  web_data.vibrate = platform_data.vibration_pattern;
  web_data.timestamp = platform_data.timestamp.ToJsTime();
  web_data.renotify = platform_data.renotify;
  web_data.silent = platform_data.silent;
  web_data.require_interaction = platform_data.require_interaction;
  web_data.data = platform_data.data;
  WebVector<WebNotificationAction> resized(platform_data.actions.size());
  web_data.actions.Swap(resized);
  for (size_t i = 0; i < platform_data.actions.size(); ++i) {
    switch (platform_data.actions[i].type) {
      case PLATFORM_NOTIFICATION_ACTION_TYPE_BUTTON:
        web_data.actions[i].type = WebNotificationAction::kButton;
        break;
      case PLATFORM_NOTIFICATION_ACTION_TYPE_TEXT:
        web_data.actions[i].type = WebNotificationAction::kText;
        break;
      default:
        NOTREACHED() << "Unknown platform data type: "
                     << platform_data.actions[i].type;
    }
    web_data.actions[i].action =
        WebString::FromUTF8(platform_data.actions[i].action);
    web_data.actions[i].title =
        WebString::FromUTF16(platform_data.actions[i].title);
    web_data.actions[i].icon = WebURL(KURL(platform_data.actions[i].icon));
    web_data.actions[i].placeholder =
        WebString::FromUTF16(platform_data.actions[i].placeholder);
  }

  return web_data;
}

}  // namespace blink
