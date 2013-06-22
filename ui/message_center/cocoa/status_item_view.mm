// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/message_center/cocoa/status_item_view.h"

#include <cmath>

#include "base/format_macros.h"
#include "grit/ui_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// The width of the status bar item when it's just the icon.
const CGFloat kStatusItemLength = 26;

// The amount of space between the left and right edges and the content of the
// status item.
const CGFloat kMargin = 5;

// The amount of space between the icon and the unread count number.
const CGFloat kUnreadCountPadding = 3;

// The lower-left Y coordinate of the unread count number.
const CGFloat kUnreadCountMinY = 4;

}  // namespace

@interface MCStatusItemView (Private)
// Whether or not the status item should be drawn highlighted.
- (BOOL)shouldHighlight;

// Returns an autoreleased, styled string for the unread count.
- (NSAttributedString*)unreadCountString;
@end

@implementation MCStatusItemView

@synthesize unreadCount = unreadCount_;
@synthesize highlight = highlight_;

- (id)init {
  statusItem_.reset([[[NSStatusBar systemStatusBar] statusItemWithLength:
      NSVariableStatusItemLength] retain]);
  CGFloat thickness = [[statusItem_ statusBar] thickness];

  NSRect frame = NSMakeRect(0, 0, kStatusItemLength, thickness);
  if ((self = [super initWithFrame:frame])) {
    [statusItem_ setView:self];
  }
  return self;
}

- (void)removeItem {
  [[NSStatusBar systemStatusBar] removeStatusItem:statusItem_];
  statusItem_.reset();
}

- (message_center::StatusItemClickedCallack)callback {
  return callback_.get();
}

- (void)setCallback:(message_center::StatusItemClickedCallack)callback {
  callback_.reset(Block_copy(callback));
}

- (void)setUnreadCount:(size_t)unreadCount {
  unreadCount_ = unreadCount;

  NSRect frame = [self frame];
  frame.size.width = kStatusItemLength;
  NSAttributedString* countString = [self unreadCountString];
  if (countString) {
    // Get the subpixel bounding rectangle for the string. -size doesn't yield
    // correct results for pixel-precise drawing, since it doesn't use the
    // device metrics.
    NSRect boundingRect =
        [countString boundingRectWithSize:NSZeroSize
                                  options:NSStringDrawingUsesDeviceMetrics];
    frame.size.width += roundf(NSWidth(boundingRect)) + kMargin;
  }
  [self setFrame:frame];

  [self setNeedsDisplay:YES];
}

- (void)setHighlight:(BOOL)highlight {
  highlight_ = highlight;
  [self setNeedsDisplay:YES];
}

- (void)mouseDown:(NSEvent*)event {
  inMouseEventSequence_ = YES;
  [self setNeedsDisplay:YES];

  if (callback_)
    callback_.get()();
}

- (void)mouseUp:(NSEvent*)event {
  inMouseEventSequence_ = NO;
  [self setNeedsDisplay:YES];
}

- (void)rightMouseDown:(NSEvent*)event {
  [self mouseDown:event];
}

- (void)rightMouseUp:(NSEvent*)event {
  [self mouseUp:event];
}

- (void)otherMouseDown:(NSEvent*)event {
  [self mouseDown:event];
}

- (void)otherMouseUp:(NSEvent*)event {
  [self mouseUp:event];
}

- (void)drawRect:(NSRect)dirtyRect {
  NSRect frame = [self bounds];

  // Draw the background color.
  BOOL highlight = [self shouldHighlight];
  [statusItem_ drawStatusBarBackgroundInRect:frame
                               withHighlight:highlight];

  // Draw the icon.
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  NSImage* image = rb.GetNativeImageNamed(
      highlight ? IDR_TRAY_ICON_PRESSED : IDR_TRAY_ICON_REGULAR).ToNSImage();
  NSSize size = [image size];
  NSRect drawRect = NSMakeRect(kMargin,
                               floorf((NSHeight(frame) - size.height) / 2),
                               size.width,
                               size.height);
  [image drawInRect:drawRect
           fromRect:NSZeroRect
          operation:NSCompositeSourceOver
           fraction:1.0];

  // Draw the unread count.
  NSAttributedString* countString = [self unreadCountString];
  if (countString) {
    NSPoint countPoint = NSMakePoint(
        NSMaxX(drawRect) + kUnreadCountPadding, kUnreadCountMinY);
    [countString drawAtPoint:countPoint];
  }
}

- (NSArray*)accessibilityActionNames {
  return @[ NSAccessibilityPressAction ];
}

- (void)accessibilityPerformAction:(NSString*)action {
  if ([action isEqualToString:NSAccessibilityPressAction]) {
    if (callback_)
      callback_.get()();
    return;
  }
  [super accessibilityPerformAction:action];
}

// Private /////////////////////////////////////////////////////////////////////

- (BOOL)shouldHighlight {
  return highlight_ || inMouseEventSequence_;
}

- (NSAttributedString*)unreadCountString {
  if (unreadCount_ == 0)
    return nil;

  NSString* count = nil;
  if (unreadCount_ > 9)
    count = @"9+";
  else
    count = [NSString stringWithFormat:@"%"PRIuS, unreadCount_];

  NSColor* fontColor = [self shouldHighlight] ? [NSColor whiteColor]
                                              : [NSColor blackColor];
  NSDictionary* attributes = @{
      NSFontAttributeName: [NSFont fontWithName:@"Helvetica-Bold" size:12],
      NSForegroundColorAttributeName: fontColor,
  };
  return [[[NSAttributedString alloc] initWithString:count
                                          attributes:attributes] autorelease];
}

@end
