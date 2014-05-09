// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/notification_view.h"

#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/notification.h"

namespace message_center {

/* Test fixture ***************************************************************/

typedef testing::Test NotificationViewTest;

TEST_F(NotificationViewTest, TestLineLimits) {
  message_center::RichNotificationData data;
  std::string id("id");
  NotifierId notifier_id(NotifierId::APPLICATION, "notifier");
  scoped_ptr<Notification> notification(
      new Notification(NOTIFICATION_TYPE_BASE_FORMAT,
                       id,
                       base::UTF8ToUTF16("test title"),
                       base::UTF8ToUTF16("test message"),
                       gfx::Image(),
                       base::string16() /* display_source */,
                       notifier_id,
                       data,
                       NULL /* delegate */));
  scoped_ptr<NotificationView> view(new NotificationView(NULL, *notification));

  EXPECT_EQ(5, view->GetMessageLineLimit(0, 360));
  EXPECT_EQ(5, view->GetMessageLineLimit(1, 360));
  EXPECT_EQ(3, view->GetMessageLineLimit(2, 360));

  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, 2, 2);
  bitmap.allocPixels();
  bitmap.eraseColor(SK_ColorGREEN);
  data.image = gfx::Image::CreateFrom1xBitmap(bitmap);
  notification.reset(new Notification(NOTIFICATION_TYPE_BASE_FORMAT,
                                      id,
                                      base::UTF8ToUTF16("test title"),
                                      base::UTF8ToUTF16("test message"),
                                      gfx::Image(),
                                      base::string16() /* display_source */,
                                      notifier_id,
                                      data,
                                      NULL /* delegate */));
  view.reset(new NotificationView(NULL, *notification));

  EXPECT_EQ(2, view->GetMessageLineLimit(0, 360));
  EXPECT_EQ(2, view->GetMessageLineLimit(1, 360));
  EXPECT_EQ(1, view->GetMessageLineLimit(2, 360));

  data.context_message = base::UTF8ToUTF16("foo");
  notification.reset(new Notification(NOTIFICATION_TYPE_BASE_FORMAT,
                                      id,
                                      base::UTF8ToUTF16("test title"),
                                      base::UTF8ToUTF16("test message"),
                                      gfx::Image(),
                                      base::string16() /* display_source */,
                                      notifier_id,
                                      data,
                                      NULL /* delegate */));
  view.reset(new NotificationView(NULL, *notification));

  EXPECT_EQ(1, view->GetMessageLineLimit(0, 360));
  EXPECT_EQ(1, view->GetMessageLineLimit(1, 360));
  EXPECT_EQ(0, view->GetMessageLineLimit(2, 360));
}

}  // namespace message_center
