// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views/cocoa/views_nswindow_delegate.h"

#include "base/logging.h"
#import "ui/views/cocoa/bridged_content_view.h"
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

- (NSCursor*)cursor {
  return cursor_.get();
}

- (void)setCursor:(NSCursor*)newCursor {
  if (cursor_.get() == newCursor)
    return;

  cursor_.reset([newCursor retain]);
  [parent_->ns_window() resetCursorRects];
}

- (void)onWindowOrderWillChange:(NSWindowOrderingMode)orderingMode {
  parent_->OnVisibilityChangedTo(orderingMode != NSWindowOut);
}

- (void)onWindowOrderChanged:(NSNotification*)notification {
  parent_->OnVisibilityChanged();
}

- (void)onWindowWillDisplay {
  parent_->OnVisibilityChangedTo(true);
}

// NSWindowDelegate implementation.

- (void)windowDidFailToEnterFullScreen:(NSWindow*)window {
  // Cocoa should already have sent an (unexpected) windowDidExitFullScreen:
  // notification, and the attempt to get back into fullscreen should fail.
  // Nothing to do except verify |parent_| is no longer trying to fullscreen.
  DCHECK(!parent_->target_fullscreen_state());
}

- (void)windowDidFailToExitFullScreen:(NSWindow*)window {
  // Unlike entering fullscreen, windowDidFailToExitFullScreen: is sent *before*
  // windowDidExitFullScreen:. Also, failing to exit fullscreen just dumps the
  // window out of fullscreen without an animation; still sending the expected,
  // windowDidExitFullScreen: notification. So, again, nothing to do here.
  DCHECK(!parent_->target_fullscreen_state());
}

- (void)windowDidResize:(NSNotification*)notification {
  parent_->OnSizeChanged();
}

- (void)windowDidBecomeKey:(NSNotification*)notification {
  parent_->OnWindowKeyStatusChangedTo(true);
}

- (void)windowDidResignKey:(NSNotification*)notification {
  parent_->OnWindowKeyStatusChangedTo(false);
}

- (void)windowWillClose:(NSNotification*)notification {
  DCHECK([parent_->ns_window() isEqual:[notification object]]);
  parent_->OnWindowWillClose();
}

- (void)windowDidMiniaturize:(NSNotification*)notification {
  parent_->OnVisibilityChanged();
}

- (void)windowDidDeminiaturize:(NSNotification*)notification {
  parent_->OnVisibilityChanged();
}

- (void)windowDidChangeBackingProperties:(NSNotification*)notification {
  parent_->OnBackingPropertiesChanged();
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
