// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views/cocoa/widget_owner_nswindow_adapter.h"

#import <Cocoa/Cocoa.h>

#include "base/logging.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#import "ui/gfx/mac/coordinate_conversion.h"
#import "ui/views/cocoa/bridged_native_widget.h"

// Bridges an AppKit observer to observe when the (non-views) NSWindow owning a
// views::Widget will close.
@interface WidgetOwnerNSWindowAdapterBridge : NSObject {
 @private
  views::WidgetOwnerNSWindowAdapter* adapter_;  // Weak. Owns us.
}
- (instancetype)initWithAdapter:(views::WidgetOwnerNSWindowAdapter*)adapter;
- (void)windowWillClose:(NSNotification*)notification;
@end

@implementation WidgetOwnerNSWindowAdapterBridge

- (instancetype)initWithAdapter:(views::WidgetOwnerNSWindowAdapter*)adapter {
  if ((self = [super init]))
    adapter_ = adapter;
  return self;
}

- (void)windowWillClose:(NSNotification*)notification {
  adapter_->OnWindowWillClose();
}

@end

namespace views {

WidgetOwnerNSWindowAdapter::WidgetOwnerNSWindowAdapter(
    BridgedNativeWidget* child,
    NSView* anchor_view)
    : child_(child),
      anchor_view_([anchor_view retain]),
      observer_bridge_(
          [[WidgetOwnerNSWindowAdapterBridge alloc] initWithAdapter:this]) {
  DCHECK([anchor_view_ window]);
  [[NSNotificationCenter defaultCenter]
      addObserver:observer_bridge_
         selector:@selector(windowWillClose:)
             name:NSWindowWillCloseNotification
           object:[anchor_view_ window]];
}

void WidgetOwnerNSWindowAdapter::OnWindowWillClose() {
  [child_->ns_window() close];
  // Note: |this| will be deleted here.
}

NSWindow* WidgetOwnerNSWindowAdapter::GetNSWindow() {
  return [anchor_view_ window];
}

gfx::Vector2d WidgetOwnerNSWindowAdapter::GetChildWindowOffset() const {
  NSWindow* window = [anchor_view_ window];
  NSRect rect_in_window =
      [anchor_view_ convertRect:[anchor_view_ bounds] toView:nil];
  // Ensure we anchor off the top-left of |anchor_view_| (rect_in_window.origin
  // is the bottom-left of the view).
  // TODO(tapted): Use -[NSWindow convertRectToScreen:] when we ditch 10.6.
  NSRect rect_in_screen = NSZeroRect;
  rect_in_screen.origin =
      [window convertBaseToScreen:NSMakePoint(NSMinX(rect_in_window),
                                              NSMaxY(rect_in_window))];
  return gfx::ScreenRectFromNSRect(rect_in_screen).OffsetFromOrigin();
}

bool WidgetOwnerNSWindowAdapter::IsVisibleParent() const {
  return [[anchor_view_ window] isVisible];
}

void WidgetOwnerNSWindowAdapter::RemoveChildWindow(BridgedNativeWidget* child) {
  DCHECK_EQ(child, child_);
  [GetNSWindow() removeChildWindow:child->ns_window()];
  delete this;
}

WidgetOwnerNSWindowAdapter::~WidgetOwnerNSWindowAdapter() {
  [[NSNotificationCenter defaultCenter] removeObserver:observer_bridge_];
}

}  // namespace views
