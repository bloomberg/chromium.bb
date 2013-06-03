// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/cocoa/tray_controller.h"

#include "ui/base/cocoa/window_size_constants.h"
#include "ui/base/resource/resource_bundle.h"
#import "ui/message_center/cocoa/popup_collection.h"
#import "ui/message_center/cocoa/tray_view_controller.h"
#include "ui/message_center/message_center_tray.h"
#include "ui/message_center/message_center_tray_delegate.h"

@interface MCTrayWindow : NSPanel
@end

@implementation MCTrayWindow

- (BOOL)canBecomeKeyWindow {
  return YES;
}

@end

@implementation MCTrayController

- (id)initWithMessageCenterTray:(message_center::MessageCenterTray*)tray {
  scoped_nsobject<MCTrayWindow> window(
      [[MCTrayWindow alloc] initWithContentRect:ui::kWindowSizeDeterminedLater
                                      styleMask:NSBorderlessWindowMask
                                        backing:NSBackingStoreBuffered
                                          defer:NO]);
  if ((self = [super initWithWindow:window])) {
    tray_ = tray;

    [window setDelegate:self];
    [window setHasShadow:YES];
    [window setHidesOnDeactivate:NO];
    [window setLevel:NSFloatingWindowLevel];

    viewController_.reset([[MCTrayViewController alloc] initWithMessageCenter:
        tray_->message_center()]);
    NSView* contentView = [viewController_ view];
    [window setFrame:[contentView frame] display:NO];
    [window setContentView:contentView];

    // The global event monitor will close the tray in response to events
    // delivered to other applications, and -windowDidResignKey: will catch
    // events within the application.
    clickEventMonitor_ =
        [NSEvent addGlobalMonitorForEventsMatchingMask:NSLeftMouseDownMask |
                                                       NSRightMouseDownMask |
                                                       NSOtherMouseDownMask
            handler:^(NSEvent* event) {
                tray_->HideMessageCenterBubble();
            }];
  }
  return self;
}

- (void)dealloc {
  [NSEvent removeMonitor:clickEventMonitor_];
  [super dealloc];
}

- (void)showTrayAt:(NSPoint)point {
  NSRect frame = [[viewController_ view] frame];
  frame.origin.x = point.x;
  frame.origin.y = point.y - NSHeight(frame);
  [[self window] setFrame:frame display:YES];
  [self showWindow:nil];
}

- (void)onMessageCenterTrayChanged {
  CGFloat oldHeight = NSHeight([[viewController_ view] frame]);
  [viewController_ onMessageCenterTrayChanged];
  CGFloat newHeight = NSHeight([[viewController_ view] frame]);

  NSRect windowFrame = [[self window] frame];
  CGFloat delta = newHeight - oldHeight;
  windowFrame.origin.y -= delta;
  windowFrame.size.height += delta;
  [[self window] setFrame:windowFrame display:YES];
}

- (void)windowDidResignKey:(NSNotification*)notification {
  tray_->HideMessageCenterBubble();
}

@end
