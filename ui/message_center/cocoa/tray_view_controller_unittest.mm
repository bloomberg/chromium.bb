// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/message_center/cocoa/tray_view_controller.h"

#include "base/memory/scoped_nsobject.h"
#include "base/utf_string_conversions.h"
#import "ui/base/test/ui_cocoa_test_helper.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_constants.h"
#include "ui/message_center/notification.h"

class TrayViewControllerTest : public ui::CocoaTest {
 public:
  virtual void SetUp() OVERRIDE {
    ui::CocoaTest::SetUp();
    message_center::MessageCenter::Initialize();
    center_ = message_center::MessageCenter::Get();
    tray_.reset([[MCTrayViewController alloc] initWithMessageCenter:center_]);
    [tray_ view];  // Create the view.
  }

  virtual void TearDown() OVERRIDE {
    tray_.reset();
    message_center::MessageCenter::Shutdown();
    ui::CocoaTest::TearDown();
  }

 protected:
  message_center::MessageCenter* center_;  // Weak, global.

  scoped_nsobject<MCTrayViewController> tray_;
};

TEST_F(TrayViewControllerTest, AddRemoveOne) {
  NSScrollView* view = [[tray_ scrollView] documentView];
  EXPECT_EQ(0u, [[view subviews] count]);
  center_->AddNotification(message_center::NOTIFICATION_TYPE_SIMPLE,
                           "1",
                           ASCIIToUTF16("First notification"),
                           ASCIIToUTF16("This is a simple test."),
                           string16(),
                           std::string(),
                           NULL);
  [tray_ onMessageCenterTrayChanged];
  ASSERT_EQ(1u, [[view subviews] count]);

  // The view should have padding around it.
  NSView* notification = [[view subviews] objectAtIndex:0];
  NSRect notification_frame = [notification frame];
  EXPECT_CGFLOAT_EQ(2 * message_center::kMarginBetweenItems,
                    NSHeight([view frame]) - NSHeight(notification_frame));
  EXPECT_CGFLOAT_EQ(2 * message_center::kMarginBetweenItems,
                    NSWidth([view frame]) - NSWidth(notification_frame));
  EXPECT_GT(NSHeight([[tray_ view] frame]),
            NSHeight([[tray_ scrollView] frame]));

  center_->RemoveNotification("1", true);
  [tray_ onMessageCenterTrayChanged];
  EXPECT_EQ(0u, [[view subviews] count]);
  EXPECT_CGFLOAT_EQ(0, NSHeight([view frame]));
}

TEST_F(TrayViewControllerTest, AddThreeClearAll) {
  NSScrollView* view = [[tray_ scrollView] documentView];
  EXPECT_EQ(0u, [[view subviews] count]);
  center_->AddNotification(message_center::NOTIFICATION_TYPE_SIMPLE,
                           "1",
                           ASCIIToUTF16("First notification"),
                           ASCIIToUTF16("This is a simple test."),
                           string16(),
                           std::string(),
                           NULL);
  center_->AddNotification(message_center::NOTIFICATION_TYPE_SIMPLE,
                           "2",
                           ASCIIToUTF16("Second notification"),
                           ASCIIToUTF16("This is a simple test."),
                           string16(),
                           std::string(),
                           NULL);
  center_->AddNotification(message_center::NOTIFICATION_TYPE_SIMPLE,
                           "3",
                           ASCIIToUTF16("Third notification"),
                           ASCIIToUTF16("This is a simple test."),
                           string16(),
                           std::string(),
                           NULL);
  [tray_ onMessageCenterTrayChanged];
  ASSERT_EQ(3u, [[view subviews] count]);

  [tray_ clearAllNotifications:nil];
  [tray_ onMessageCenterTrayChanged];

  EXPECT_EQ(0u, [[view subviews] count]);
  EXPECT_CGFLOAT_EQ(0, NSHeight([view frame]));
}
