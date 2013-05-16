// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/message_center/cocoa/popup_collection.h"

#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#import "ui/base/test/ui_cocoa_test_helper.h"
#import "ui/message_center/cocoa/notification_controller.h"
#import "ui/message_center/cocoa/popup_controller.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_constants.h"
#include "ui/message_center/notification.h"

@implementation MCNotificationController (TestingInterface)
- (NSImageView*)iconView {
  return icon_.get();
}
@end

class PopupCollectionTest : public ui::CocoaTest {
 public:
  PopupCollectionTest() {
    message_center::MessageCenter::Initialize();
    center_ = message_center::MessageCenter::Get();
    collection_.reset(
        [[MCPopupCollection alloc] initWithMessageCenter:center_]);
  }

  virtual void TearDown() OVERRIDE {
    collection_.reset();  // Close all popups.
    ui::CocoaTest::TearDown();
  }

  virtual ~PopupCollectionTest() {
    message_center::MessageCenter::Shutdown();
  }

  void AddThreeNotifications() {
    center_->AddNotification(message_center::NOTIFICATION_TYPE_SIMPLE,
                             "1",
                             ASCIIToUTF16("One"),
                             ASCIIToUTF16("This is the first notification to"
                                          " be displayed"),
                             string16(),
                             std::string(),
                             NULL);
    center_->AddNotification(message_center::NOTIFICATION_TYPE_SIMPLE,
                             "2",
                             ASCIIToUTF16("Two"),
                             ASCIIToUTF16("This is the second notification."),
                             string16(),
                             std::string(),
                             NULL);
    center_->AddNotification(message_center::NOTIFICATION_TYPE_SIMPLE,
                             "3",
                             ASCIIToUTF16("Three"),
                             ASCIIToUTF16("This is the third notification "
                                          "that has a much longer body "
                                          "than the other notifications. It "
                                          "may not fit on the screen if we "
                                          "set the screen size too small."),
                             string16(),
                             std::string(),
                             NULL);
  }

  bool CheckSpacingBetween(MCPopupController* upper, MCPopupController* lower) {
    CGFloat minY = NSMinY([[upper window] frame]);
    CGFloat maxY = NSMaxY([[lower window] frame]);
    CGFloat delta = minY - maxY;
    EXPECT_EQ(message_center::kMarginBetweenItems, delta);
    return delta == message_center::kMarginBetweenItems;
  }

  message_center::MessageCenter* center_;
  scoped_nsobject<MCPopupCollection> collection_;
};

TEST_F(PopupCollectionTest, AddThreeCloseOne) {
  EXPECT_EQ(0u, [[collection_ popups] count]);
  AddThreeNotifications();
  EXPECT_EQ(3u, [[collection_ popups] count]);

  center_->RemoveNotification("2", true);
  EXPECT_EQ(2u, [[collection_ popups] count]);
}

TEST_F(PopupCollectionTest, AttemptFourOneOffscreen) {
  [collection_ setScreenFrame:NSMakeRect(0, 0, 800, 300)];

  EXPECT_EQ(0u, [[collection_ popups] count]);
  AddThreeNotifications();
  EXPECT_EQ(2u, [[collection_ popups] count]);  // "3" does not fit on screen.

  center_->AddNotification(message_center::NOTIFICATION_TYPE_SIMPLE,
                           "4",
                           ASCIIToUTF16("Four"),
                           ASCIIToUTF16("This is the fourth notification."),
                           string16(),
                           std::string(),
                           NULL);

  // Remove "1" and "4" should fit on screen.
  center_->RemoveNotification("1", true);
  ASSERT_EQ(2u, [[collection_ popups] count]);

  EXPECT_EQ("2", [[[collection_ popups] objectAtIndex:0] notificationID]);
  EXPECT_EQ("4", [[[collection_ popups] objectAtIndex:1] notificationID]);

  // Remove "2" and "3" should fit on screen.
  center_->RemoveNotification("2", true);
  ASSERT_EQ(2u, [[collection_ popups] count]);

  EXPECT_EQ("4", [[[collection_ popups] objectAtIndex:0] notificationID]);
  EXPECT_EQ("3", [[[collection_ popups] objectAtIndex:1] notificationID]);
}

TEST_F(PopupCollectionTest, LayoutSpacing) {
  const CGFloat kScreenSize = 500;
  [collection_ setScreenFrame:NSMakeRect(0, 0, kScreenSize, kScreenSize)];

  AddThreeNotifications();
  NSArray* popups = [collection_ popups];

  EXPECT_EQ(message_center::kMarginBetweenItems,
            kScreenSize - NSMaxY([[[popups objectAtIndex:0] window] frame]));

  EXPECT_TRUE(CheckSpacingBetween([popups objectAtIndex:0],
                                  [popups objectAtIndex:1]));
  EXPECT_TRUE(CheckSpacingBetween([popups objectAtIndex:1],
                                  [popups objectAtIndex:2]));

  // Set priority so that kMaxVisiblePopupNotifications does not hide it.
  scoped_ptr<base::DictionaryValue> optional(new base::DictionaryValue);
  optional->SetInteger(message_center::kPriorityKey,
                       message_center::HIGH_PRIORITY);
  center_->AddNotification(message_center::NOTIFICATION_TYPE_SIMPLE,
                           "4",
                           ASCIIToUTF16("Four"),
                           ASCIIToUTF16("This is the fourth notification."),
                           string16(),
                           std::string(),
                           optional.get());
  EXPECT_TRUE(CheckSpacingBetween([popups objectAtIndex:2],
                                  [popups objectAtIndex:3]));

  // Remove "2".
  center_->RemoveNotification("2", true);
  EXPECT_TRUE(CheckSpacingBetween([popups objectAtIndex:0],
                                  [popups objectAtIndex:1]));
  EXPECT_TRUE(CheckSpacingBetween([popups objectAtIndex:1],
                                  [popups objectAtIndex:2]));

  // Remove "1".
  center_->RemoveNotification("2", true);
  EXPECT_EQ(message_center::kMarginBetweenItems,
            kScreenSize - NSMaxY([[[popups objectAtIndex:0] window] frame]));
  EXPECT_TRUE(CheckSpacingBetween([popups objectAtIndex:0],
                                  [popups objectAtIndex:1]));
}

TEST_F(PopupCollectionTest, TinyScreen) {
  [collection_ setScreenFrame:NSMakeRect(0, 0, 800, 100)];

  EXPECT_EQ(0u, [[collection_ popups] count]);
  center_->AddNotification(message_center::NOTIFICATION_TYPE_SIMPLE,
                           "1",
                           ASCIIToUTF16("One"),
                           ASCIIToUTF16("This is the first notification to"
                                        " be displayed"),
                           string16(),
                           std::string(),
                           NULL);
  EXPECT_EQ(1u, [[collection_ popups] count]);

  // Now give the notification a longer message so that it no longer fits.
  center_->UpdateNotification("1",
                              "1",
                              ASCIIToUTF16("One"),
                              ASCIIToUTF16("This is now a very very very very "
                                           "very very very very very very very "
                                           "very very very very very very very "
                                           "very very very very very very very "
                                           "very very very very very very very "
                                           "very very very very very very very "
                                           "very very very very very very very "
                                           "long notification."),
                              NULL);
  EXPECT_EQ(0u, [[collection_ popups] count]);
}

TEST_F(PopupCollectionTest, UpdateIconAndBody) {
  AddThreeNotifications();
  NSArray* popups = [collection_ popups];

  EXPECT_EQ(3u, [popups count]);

  // Update "2" icon.
  MCNotificationController* controller =
      [[popups objectAtIndex:1] notificationController];
  EXPECT_FALSE([[controller iconView] image]);
  center_->SetNotificationIcon("2",
      gfx::Image([[NSImage imageNamed:NSImageNameUser] retain]));
  EXPECT_TRUE([[controller iconView] image]);

  EXPECT_EQ(3u, [popups count]);
  EXPECT_TRUE(CheckSpacingBetween([popups objectAtIndex:0],
                                  [popups objectAtIndex:1]));
  EXPECT_TRUE(CheckSpacingBetween([popups objectAtIndex:1],
                                  [popups objectAtIndex:2]));

  // Replace "1".
  controller = [[popups objectAtIndex:0] notificationController];
  NSRect old_frame = [[controller view] frame];
  center_->AddNotification(message_center::NOTIFICATION_TYPE_SIMPLE,
                           "1",
                           ASCIIToUTF16("One is going to get a much longer "
                                        "title than it previously had."),
                           ASCIIToUTF16("This is the first notification to "
                                        "be displayed, but it will also be "
                                        "updated to have a significantly "
                                        "longer body"),
                           string16(),
                           std::string(),
                           NULL);
  EXPECT_GT(NSHeight([[controller view] frame]), NSHeight(old_frame));

  // Test updated spacing.
  EXPECT_EQ(3u, [popups count]);
  EXPECT_TRUE(CheckSpacingBetween([popups objectAtIndex:0],
                                  [popups objectAtIndex:1]));
  EXPECT_TRUE(CheckSpacingBetween([popups objectAtIndex:1],
                                  [popups objectAtIndex:2]));
  EXPECT_EQ("1", [[popups objectAtIndex:0] notificationID]);
  EXPECT_EQ("2", [[popups objectAtIndex:1] notificationID]);
  EXPECT_EQ("3", [[popups objectAtIndex:2] notificationID]);
}
