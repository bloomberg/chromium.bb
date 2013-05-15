// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/message_center/cocoa/status_item_view.h"

#include <cmath>

#include "base/format_macros.h"
#include "grit/ui_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// The width of the status bar.
const CGFloat kStatusItemLength = 45;

// The amount of space between the edge of the status item and where the icon
// should start drawing.
const CGFloat kInset = 6;

}  // namespace

@implementation MCStatusItemView

@synthesize unreadCount = unreadCount_;
@synthesize highlight = highlight_;

- (id)initWithStatusItem:(NSStatusItem*)item {
  CGFloat thickness = [[item statusBar] thickness];
  NSRect frame = NSMakeRect(0, 0, kStatusItemLength, thickness);
  if ((self = [super initWithFrame:frame])) {
    statusItem_.reset([item retain]);
    [statusItem_ setView:self];
  }
  return self;
}

- (message_center::StatusItemClickedCallack)callback {
  return callback_.get();
}

- (void)setCallback:(message_center::StatusItemClickedCallack)callback {
  callback_.reset(Block_copy(callback));
}

- (void)setUnreadCount:(size_t)unreadCount {
  unreadCount_ = unreadCount;
  [self setNeedsDisplay:YES];
}

- (void)setHighlight:(BOOL)highlight {
  highlight_ = highlight;
  [self setNeedsDisplay:YES];
}

- (void)mouseDown:(NSEvent*)event {
  DCHECK(!inMouseEventSequence_);
  inMouseEventSequence_ = YES;
  [self setNeedsDisplay:YES];

  if (callback_)
    callback_.get()();
}

- (void)mouseUp:(NSEvent*)event {
  DCHECK(inMouseEventSequence_);
  inMouseEventSequence_ = NO;
  [self setNeedsDisplay:YES];
}

- (void)drawRect:(NSRect)dirtyRect {
  NSRect frame = [self bounds];

  // Draw the background color.
  BOOL highlight = highlight_ || inMouseEventSequence_;
  [statusItem_ drawStatusBarBackgroundInRect:frame
                               withHighlight:highlight];

  // Draw the icon.
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  NSImage* image = rb.GetNativeImageNamed(
      highlight ? IDR_TRAY_ICON_PRESSED : IDR_TRAY_ICON_REGULAR).ToNSImage();
  NSSize size = [image size];
  NSRect drawRect = NSMakeRect(kInset,
                               floorf((NSHeight(frame) - size.height) / 2),
                               size.width,
                               size.height);
  [image drawInRect:drawRect
           fromRect:NSZeroRect
          operation:NSCompositeSourceOver
           fraction:1.0];

  // Draw the unread count.
  if (unreadCount_ > 0) {
    NSString* count = nil;
    if (unreadCount_ > 9)
      count = @"9+";
    else
      count = [NSString stringWithFormat:@"%"PRIuS, unreadCount_];

    NSColor* fontColor = highlight ? [NSColor whiteColor]
                                   : [NSColor blackColor];
    NSDictionary* attributes = @{
        NSFontAttributeName: [NSFont fontWithName:@"Helvetica-Bold" size:12],
        NSForegroundColorAttributeName: fontColor,
    };

    // Center the string inside the remaining space of the status item.
    NSSize stringSize = [count sizeWithAttributes:attributes];
    NSRect iconSlice, textSlice;
    NSDivideRect(frame, &iconSlice, &textSlice, NSMaxX(drawRect), NSMinXEdge);
    NSPoint countPoint = NSMakePoint(
        floorf(NSMinX(textSlice) + (NSWidth(textSlice) - stringSize.width) / 2),
        floorf(NSMinY(textSlice) +
               (NSHeight(textSlice) - stringSize.height) / 2));

    [count drawAtPoint:countPoint withAttributes:attributes];
  }
}

@end
