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
#include "ui/message_center/notification_list.h"
#include "ui/message_center/notification_types.h"
#include "ui/message_center/views/message_center_controller.h"

namespace message_center {

/* Test fixture ***************************************************************/

class NotificationViewTest : public testing::Test,
                             public MessageCenterController {
 public:
  NotificationViewTest();
  virtual ~NotificationViewTest();

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  NotificationView* notification_view() { return notification_view_.get(); }
  Notification* notification() { return notification_.get(); }
  RichNotificationData* data() { return data_.get(); }

  // Overridden from MessageCenterController:
  virtual void ClickOnNotification(const std::string& notification_id) OVERRIDE;
  virtual void RemoveNotification(const std::string& notification_id,
                                  bool by_user) OVERRIDE;
  virtual scoped_ptr<ui::MenuModel> CreateMenuModel(
      const NotifierId& notifier_id,
      const base::string16& display_source) OVERRIDE;
  virtual bool HasClickedListener(const std::string& notification_id) OVERRIDE;
  virtual void ClickOnNotificationButton(const std::string& notification_id,
                                         int button_index) OVERRIDE;

 protected:
  const gfx::Image CreateTestImage(int width, int height) {
    return gfx::Image::CreateFrom1xBitmap(CreateBitmap(width, height));
  }

  const SkBitmap CreateBitmap(int width, int height) {
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, width, height);
    bitmap.allocPixels();
    bitmap.eraseRGB(0, 255, 0);
    return bitmap;
  }

 private:
  scoped_ptr<RichNotificationData> data_;
  scoped_ptr<Notification> notification_;
  scoped_ptr<NotificationView> notification_view_;

  DISALLOW_COPY_AND_ASSIGN(NotificationViewTest);
};

NotificationViewTest::NotificationViewTest() {
}

NotificationViewTest::~NotificationViewTest() {
}

void NotificationViewTest::SetUp() {
  // Create a dummy notification.
  SkBitmap bitmap;
  data_.reset(new RichNotificationData());
  notification_.reset(
      new Notification(NOTIFICATION_TYPE_BASE_FORMAT,
                       std::string("notification id"),
                       base::UTF8ToUTF16("title"),
                       base::UTF8ToUTF16("message"),
                       CreateTestImage(80, 80),
                       base::UTF8ToUTF16("display source"),
                       NotifierId(NotifierId::APPLICATION, "extension_id"),
                       *data_,
                       NULL));
  notification_->set_small_image(CreateTestImage(16, 16));
  notification_->set_image(CreateTestImage(320, 240));

  // Then create a new NotificationView with that single notification.
  notification_view_.reset(
      NotificationView::Create(this, *notification_, true));
}

void NotificationViewTest::TearDown() {
  notification_view_.reset();
}

void NotificationViewTest::ClickOnNotification(
    const std::string& notification_id) {
  // For this test, this method should not be invoked.
  NOTREACHED();
}

void NotificationViewTest::RemoveNotification(
    const std::string& notification_id,
    bool by_user) {
  // For this test, this method should not be invoked.
  NOTREACHED();
}

scoped_ptr<ui::MenuModel> NotificationViewTest::CreateMenuModel(
    const NotifierId& notifier_id,
    const base::string16& display_source) {
  // For this test, this method should not be invoked.
  NOTREACHED();
  return scoped_ptr<ui::MenuModel>();
}

bool NotificationViewTest::HasClickedListener(
    const std::string& notification_id) {
  return true;
}

void NotificationViewTest::ClickOnNotificationButton(
    const std::string& notification_id,
    int button_index) {
  // For this test, this method should not be invoked.
  NOTREACHED();
}

/* Unit tests *****************************************************************/

TEST_F(NotificationViewTest, CreateOrUpdateTest) {
  EXPECT_TRUE(NULL != notification_view()->title_view_);
  EXPECT_TRUE(NULL != notification_view()->message_view_);
  EXPECT_TRUE(NULL != notification_view()->icon_view_);
  EXPECT_TRUE(NULL != notification_view()->image_view_);

  notification()->set_image(gfx::Image());
  notification()->set_title(base::ASCIIToUTF16(""));
  notification()->set_message(base::ASCIIToUTF16(""));
  notification()->set_icon(gfx::Image());

  notification_view()->CreateOrUpdateViews(*notification());
  EXPECT_TRUE(NULL == notification_view()->title_view_);
  EXPECT_TRUE(NULL == notification_view()->message_view_);
  EXPECT_TRUE(NULL == notification_view()->image_view_);
  // We still expect an icon view for all layouts.
  EXPECT_TRUE(NULL != notification_view()->icon_view_);
}

TEST_F(NotificationViewTest, TestLineLimits) {
  notification()->set_image(CreateTestImage(0, 0));
  notification()->set_context_message(base::ASCIIToUTF16(""));
  notification_view()->CreateOrUpdateViews(*notification());

  EXPECT_EQ(5, notification_view()->GetMessageLineLimit(0, 360));
  EXPECT_EQ(5, notification_view()->GetMessageLineLimit(1, 360));
  EXPECT_EQ(3, notification_view()->GetMessageLineLimit(2, 360));

  notification()->set_image(CreateTestImage(2, 2));
  notification_view()->CreateOrUpdateViews(*notification());

  EXPECT_EQ(2, notification_view()->GetMessageLineLimit(0, 360));
  EXPECT_EQ(2, notification_view()->GetMessageLineLimit(1, 360));
  EXPECT_EQ(1, notification_view()->GetMessageLineLimit(2, 360));

  notification()->set_context_message(base::UTF8ToUTF16("foo"));
  notification_view()->CreateOrUpdateViews(*notification());

  EXPECT_TRUE(notification_view()->context_message_view_ != NULL);

  EXPECT_EQ(1, notification_view()->GetMessageLineLimit(0, 360));
  EXPECT_EQ(1, notification_view()->GetMessageLineLimit(1, 360));
  EXPECT_EQ(0, notification_view()->GetMessageLineLimit(2, 360));
}

}  // namespace message_center
