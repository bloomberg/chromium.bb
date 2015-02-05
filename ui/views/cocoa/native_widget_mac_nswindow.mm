// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views/cocoa/native_widget_mac_nswindow.h"

#include "base/mac/foundation_util.h"
#import "ui/views/cocoa/views_nswindow_delegate.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/widget/native_widget_mac.h"

@interface NativeWidgetMacNSWindow ()
- (ViewsNSWindowDelegate*)viewsNSWindowDelegate;
- (views::Widget*)viewsWidget;
- (BOOL)hasViewsMenuActive;
@end

@implementation NativeWidgetMacNSWindow

- (ViewsNSWindowDelegate*)viewsNSWindowDelegate {
  return base::mac::ObjCCastStrict<ViewsNSWindowDelegate>([self delegate]);
}

- (views::Widget*)viewsWidget {
  return [[self viewsNSWindowDelegate] nativeWidgetMac]->GetWidget();
}

- (BOOL)hasViewsMenuActive {
  views::MenuController* menuController =
      views::MenuController::GetActiveInstance();
  return menuController && menuController->owner() == [self viewsWidget];
}

// Ignore [super canBecome{Key,Main}Window]. The default is NO for windows with
// NSBorderlessWindowMask, which is not the desired behavior.
// Note these can be called via -[NSWindow close] while the widget is being torn
// down, so check for a delegate.
- (BOOL)canBecomeKeyWindow {
  return [self delegate] && [self viewsWidget]->CanActivate();
}

- (BOOL)canBecomeMainWindow {
  return [self delegate] && [self viewsWidget]->CanActivate();
}

// Override sendEvent to allow key events to be forwarded to a toolkit-views
// menu while it is active, and while still allowing any native subview to
// retain firstResponder status.
- (void)sendEvent:(NSEvent*)event {
  NSEventType type = [event type];
  if ((type != NSKeyDown && type != NSKeyUp) || ![self hasViewsMenuActive]) {
    [super sendEvent:event];
    return;
  }

  // Send to the menu, after converting the event into an action message using
  // the content view.
  if (type == NSKeyDown)
    [[self contentView] keyDown:event];
  else
    [[self contentView] keyUp:event];
}

// Override display, since this is the first opportunity Cocoa gives to detect
// a visibility change in some cases. For example, restoring from the dock first
// calls -[NSWindow display] before any NSWindowDelegate functions and before
// ordering the window (and without actually calling -[NSWindow deminiaturize]).
// By notifying the delegate that a display is about to occur, it can apply a
// correct visibility state, before [super display] requests a draw of the
// contentView. -[NSWindow isVisible] can still report NO at this point, so this
// gives the delegate time to apply correct visibility before the draw occurs.
- (void)display {
  [[self viewsNSWindowDelegate] onWindowWillDisplay];
  [super display];
}

// Override window order functions to intercept other visibility changes. This
// is needed in addition to the -[NSWindow display] override because Cocoa
// hardly ever calls display, and reports -[NSWindow isVisible] incorrectly
// when ordering in a window for the first time.
- (void)orderWindow:(NSWindowOrderingMode)orderingMode
         relativeTo:(NSInteger)otherWindowNumber {
  [[self viewsNSWindowDelegate] onWindowOrderWillChange:orderingMode];
  [super orderWindow:orderingMode relativeTo:otherWindowNumber];
  [[self viewsNSWindowDelegate] onWindowOrderChanged:nil];
}

// NSResponder implementation.

- (void)cursorUpdate:(NSEvent*)theEvent {
  // The cursor provided by the delegate should only be applied within the
  // content area. This is because we rely on the contentView to track the
  // mouse cursor and forward cursorUpdate: messages up the responder chain.
  // The cursorUpdate: isn't handled in BridgedContentView because views-style
  // SetCapture() conflicts with the way tracking events are processed for
  // the view during a drag. Since the NSWindow is still in the responder chain
  // overriding cursorUpdate: here handles both cases.
  if (!NSPointInRect([theEvent locationInWindow], [[self contentView] frame])) {
    [super cursorUpdate:theEvent];
    return;
  }

  NSCursor* cursor = [[self viewsNSWindowDelegate] cursor];
  if (cursor)
    [cursor set];
  else
    [super cursorUpdate:theEvent];
}

@end
