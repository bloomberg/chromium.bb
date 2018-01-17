// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/mojo/NotificationStructTraits.h"

#include <algorithm>

#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebVector.h"
#include "public/platform/modules/notifications/WebNotificationData.h"
#include "public/platform/modules/notifications/notification.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kNotificationBaseUrl[] = "https://example.com/directory/";
}

namespace blink {

TEST(NotificationStructTraitsTest, Roundtrip) {
  WebNotificationData notification_data;
  notification_data.title = "Notification Title";
  notification_data.direction = WebNotificationData::kDirectionRightToLeft;
  notification_data.lang = "foo-lang";
  notification_data.body = "Notification body...";
  notification_data.tag = "notificationTag";

  const KURL kBaseUrl(kNotificationBaseUrl);
  notification_data.image = WebURL(KURL(kBaseUrl, "noti_img.png"));
  notification_data.icon = WebURL(KURL(kBaseUrl, "noti_icon.png"));
  notification_data.badge = WebURL(KURL(kBaseUrl, "noti_badge.png"));

  const int vibrate[] = {200, 100, 300};
  WebVector<char> vibrate_vector(vibrate, arraysize(vibrate));
  notification_data.vibrate = vibrate_vector;

  notification_data.timestamp = 1513963983000.;
  notification_data.renotify = true;
  notification_data.silent = true;
  notification_data.require_interaction = true;

  const char data[] = "some binary data";
  WebVector<char> data_vector(data, arraysize(data));
  notification_data.data = data_vector;

  WebVector<blink::WebNotificationAction> actions(static_cast<size_t>(2));

  actions[0].type = blink::WebNotificationAction::kButton;
  actions[0].action = "my_button_click";
  actions[0].title = "Button Title";
  actions[0].icon = blink::WebURL(KURL(kBaseUrl, "button.png"));

  actions[1].type = blink::WebNotificationAction::kText;
  actions[1].action = "on_reply";
  actions[1].title = "Reply";
  actions[1].icon = blink::WebURL(KURL(kBaseUrl, "replyButton.png"));
  actions[1].placeholder = "placeholder...";

  notification_data.actions = actions;

  WebNotificationData roundtrip_notification_data;

  ASSERT_TRUE(mojom::blink::NotificationData::Deserialize(
      mojom::blink::NotificationData::Serialize(&notification_data),
      &roundtrip_notification_data));

  EXPECT_EQ(notification_data.title, roundtrip_notification_data.title);
  EXPECT_EQ(notification_data.direction, roundtrip_notification_data.direction);
  EXPECT_EQ(notification_data.lang, roundtrip_notification_data.lang);
  EXPECT_EQ(notification_data.tag, roundtrip_notification_data.tag);
  EXPECT_EQ(notification_data.image, roundtrip_notification_data.image);
  EXPECT_EQ(notification_data.icon, roundtrip_notification_data.icon);
  EXPECT_EQ(notification_data.badge, roundtrip_notification_data.badge);

  ASSERT_EQ(notification_data.vibrate.size(),
            roundtrip_notification_data.vibrate.size());
  EXPECT_TRUE(std::equal(notification_data.vibrate.begin(),
                         notification_data.vibrate.end(),
                         roundtrip_notification_data.vibrate.begin()));

  EXPECT_EQ(notification_data.timestamp, roundtrip_notification_data.timestamp);
  EXPECT_EQ(notification_data.renotify, roundtrip_notification_data.renotify);
  EXPECT_EQ(notification_data.silent, roundtrip_notification_data.silent);
  EXPECT_EQ(notification_data.require_interaction,
            roundtrip_notification_data.require_interaction);

  ASSERT_EQ(notification_data.data.size(),
            roundtrip_notification_data.data.size());
  EXPECT_TRUE(std::equal(notification_data.data.begin(),
                         notification_data.data.end(),
                         roundtrip_notification_data.data.begin()));

  ASSERT_EQ(notification_data.actions.size(),
            roundtrip_notification_data.actions.size());
  for (size_t i = 0; i < notification_data.actions.size(); ++i) {
    SCOPED_TRACE(base::StringPrintf("Action index: %zd", i));
    EXPECT_EQ(notification_data.actions[i].type,
              roundtrip_notification_data.actions[i].type);
    EXPECT_EQ(notification_data.actions[i].action,
              roundtrip_notification_data.actions[i].action);
    EXPECT_EQ(notification_data.actions[i].title,
              roundtrip_notification_data.actions[i].title);
    EXPECT_EQ(notification_data.actions[i].icon,
              roundtrip_notification_data.actions[i].icon);
    EXPECT_EQ(notification_data.actions[i].placeholder,
              roundtrip_notification_data.actions[i].placeholder);
  }
}

}  // namespace blink
