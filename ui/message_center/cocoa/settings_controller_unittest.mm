// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/message_center/cocoa/settings_controller.h"

#include "base/strings/utf_string_conversions.h"
#import "ui/base/test/ui_cocoa_test_helper.h"
#include "ui/message_center/fake_notifier_settings_provider.h"

@implementation MCSettingsController (TestingInterface)
- (NSUInteger)scrollViewItemCount {
  return [[[[self scrollView] documentView] subviews] count];
}

- (NSButton*)bottomMostButton {
  // The checkboxes are created bottom-to-top, so the first object is the
  // bottom-most.
  return [[[[self scrollView] documentView] subviews] objectAtIndex:0];
}
@end

namespace message_center {

using ui::CocoaTest;

namespace {

Notifier* NewNotifier(const std::string& id,
                      const std::string& title,
                      bool enabled) {
  NotifierId notifier_id(NotifierId::APPLICATION, id);
  return new Notifier(notifier_id, base::UTF8ToUTF16(title), enabled);
}

}  // namespace

TEST_F(CocoaTest, Basic) {
  // Notifiers are owned by settings controller.
  std::vector<Notifier*> notifiers;
  notifiers.push_back(NewNotifier("id", "title", /*enabled=*/true));
  notifiers.push_back(NewNotifier("id2", "other title", /*enabled=*/false));

  FakeNotifierSettingsProvider provider(notifiers);

  base::scoped_nsobject<MCSettingsController> controller(
      [[MCSettingsController alloc] initWithProvider:&provider]);
  [controller view];

  EXPECT_EQ(notifiers.size(), [controller scrollViewItemCount]);
}

TEST_F(CocoaTest, Toggle) {
  // Notifiers are owned by settings controller.
  std::vector<Notifier*> notifiers;
  notifiers.push_back(NewNotifier("id", "title", /*enabled=*/true));
  notifiers.push_back(NewNotifier("id2", "other title", /*enabled=*/false));

  FakeNotifierSettingsProvider provider(notifiers);

  base::scoped_nsobject<MCSettingsController> controller(
      [[MCSettingsController alloc] initWithProvider:&provider]);
  [controller view];

  NSButton* toggleSecond = [controller bottomMostButton];

  [toggleSecond performClick:nil];
  EXPECT_TRUE(provider.WasEnabled(*notifiers.back()));

  [toggleSecond performClick:nil];
  EXPECT_FALSE(provider.WasEnabled(*notifiers.back()));

  EXPECT_EQ(0, provider.closed_called_count());
  controller.reset();
  EXPECT_EQ(1, provider.closed_called_count());
}

}  // namespace message_center
