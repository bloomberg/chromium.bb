// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views/cocoa/views_nswindow_delegate.h"

#include "base/logging.h"
#import "ui/views/cocoa/bridged_native_widget.h"
#include "ui/views/widget/native_widget_mac.h"

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

- (void)windowDidFailToEnterFullScreen:(NSWindow*)window {
  // TODO(tapted): Handle these failure notifications, and simulate in a test.
  NOTREACHED();
}

- (void)windowDidFailToExitFullScreen:(NSWindow*)window {
  NOTREACHED();
}

- (void)windowDidBecomeKey:(NSNotification*)notification {
  parent_->native_widget_mac()->GetWidget()->OnNativeWidgetActivationChanged(
      true);
}

- (void)windowDidResignKey:(NSNotification*)notification {
  parent_->native_widget_mac()->GetWidget()->OnNativeWidgetActivationChanged(
      false);
}

- (void)windowWillClose:(NSNotification*)notification {
  DCHECK([parent_->ns_window() isEqual:[notification object]]);
  parent_->OnWindowWillClose();
}

- (void)windowWillEnterFullScreen:(NSNotification*)notification {
  parent_->OnFullscreenTransitionStart(true);
}

- (void)windowDidEnterFullScreen:(NSNotification*)notification {
  parent_->OnFullscreenTransitionComplete(true);
}

- (void)windowWillExitFullScreen:(NSNotification*)notification {
  parent_->OnFullscreenTransitionStart(false);
}

- (void)windowDidExitFullScreen:(NSNotification*)notification {
  parent_->OnFullscreenTransitionComplete(false);
}

@end
