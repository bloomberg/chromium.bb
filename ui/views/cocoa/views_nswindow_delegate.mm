// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views/cocoa/views_nswindow_delegate.h"

#include "base/logging.h"
#import "ui/views/cocoa/bridged_native_widget.h"

@implementation ViewsNSWindowDelegate

- (id)initWithBridgedNativeWidget:(views::BridgedNativeWidget*)parent {
  DCHECK(parent);
  if ((self = [super init])) {
    parent_ = parent;
  }
  return self;
}

- (views::NativeWidgetMac*)nativeWidgetMac {
  return parent_->native_widget_mac();
}

// NSWindowDelegate implementation.

- (void)windowWillClose:(NSNotification*)notification {
  DCHECK([parent_->ns_window() isEqual:[notification object]]);
  parent_->OnWindowWillClose();
}

@end
