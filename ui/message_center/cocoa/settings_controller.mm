// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/message_center/cocoa/settings_controller.h"

#import "base/memory/scoped_nsobject.h"
#include "ui/message_center/message_center_style.h"


namespace message_center {

NotifierSettingsDelegateMac::~NotifierSettingsDelegateMac() {}

void NotifierSettingsDelegateMac::UpdateIconImage(const std::string& id,
                                                  const gfx::Image& icon) {
  // TODO(thakis): Implement.
}

void NotifierSettingsDelegateMac::UpdateFavicon(const GURL& url,
                                                const gfx::Image& icon) {
  // TODO(thakis): Implement.
}

NotifierSettingsDelegate* ShowSettings(NotifierSettingsProvider* provider,
                                       gfx::NativeView context) {
  // The caller of this function (the tray) retains |controller| while it's
  // visible.
  MCSettingsController* controller =
      [[[MCSettingsController alloc] initWithProvider:provider] autorelease];
  return [controller delegate];
}

}  // namespace message_center

@implementation MCSettingsController

- (id)initWithProvider:(message_center::NotifierSettingsProvider*)provider {
  if ((self = [super initWithNibName:nil bundle:nil])) {
    delegate_.reset(new message_center::NotifierSettingsDelegateMac(self));
    provider_ = provider;
  }
  return self;
}

- (void)loadView {
  // TODO(thakis): Add real contents.
  scoped_nsobject<NSBox> view([[NSBox alloc] initWithFrame:
      NSMakeRect(0, 0, message_center::kNotificationWidth, 100)]);
  [view setBorderType:NSNoBorder];
  [view setBoxType:NSBoxCustom];
  [view setContentViewMargins:NSZeroSize];
  [view setFillColor:[NSColor redColor]];
  [view setTitlePosition:NSNoTitle];
  [self setView:view];
}

- (message_center::NotifierSettingsDelegateMac*)delegate {
  return delegate_.get();
}

@end
