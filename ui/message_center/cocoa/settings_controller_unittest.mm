// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/message_center/cocoa/settings_controller.h"

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#import "ui/base/test/ui_cocoa_test_helper.h"

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

class FakeNotifierSettingsProvider : public NotifierSettingsProvider {
 public:
  FakeNotifierSettingsProvider(
      const std::vector<Notifier*>& notifiers)
      : notifiers_(notifiers) {}

  virtual void GetNotifierList(
      std::vector<Notifier*>* notifiers) OVERRIDE {
    notifiers->clear();
    for (Notifier* notifier : notifiers_)
      notifiers->push_back(notifier);
  }

  virtual void SetNotifierEnabled(const Notifier& notifier,
                                  bool enabled) OVERRIDE {
    enabled_[&notifier] = enabled;
  }
  virtual void OnNotifierSettingsClosing() OVERRIDE {}

  bool WasEnabled(const Notifier& notifier) {
    return enabled_[&notifier];
  }

 private:
  std::vector<Notifier*> notifiers_;
  std::map<const Notifier*, bool> enabled_;
};

Notifier* NewNotifier(const std::string& id,
                      const std::string& title,
                      bool enabled) {
  return new Notifier(id, base::UTF8ToUTF16(title), enabled);
}

}  // namespace

TEST_F(CocoaTest, Basic) {
  std::vector<Notifier*> notifiers;
  notifiers.push_back(NewNotifier("id", "title", /*enabled=*/true));
  notifiers.push_back(NewNotifier("id2", "other title", /*enabled=*/false));

  FakeNotifierSettingsProvider provider(notifiers);

  scoped_nsobject<MCSettingsController> controller(
      [[MCSettingsController alloc] initWithProvider:&provider]);
  [controller view];

  EXPECT_EQ(notifiers.size(), [controller scrollViewItemCount]);

  STLDeleteElements(&notifiers);
}

TEST_F(CocoaTest, Toggle) {
  std::vector<Notifier*> notifiers;
  notifiers.push_back(NewNotifier("id", "title", /*enabled=*/true));
  notifiers.push_back(NewNotifier("id2", "other title", /*enabled=*/false));

  FakeNotifierSettingsProvider provider(notifiers);

  scoped_nsobject<MCSettingsController> controller(
      [[MCSettingsController alloc] initWithProvider:&provider]);
  [controller view];

  NSButton* toggleSecond = [controller bottomMostButton];

  [toggleSecond performClick:nil];
  EXPECT_TRUE(provider.WasEnabled(*notifiers.back()));

  [toggleSecond performClick:nil];
  EXPECT_FALSE(provider.WasEnabled(*notifiers.back()));

  STLDeleteElements(&notifiers);
}

}  // namespace message_center
