// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/cocoa/scroll_view_with_no_scrollbars.h"

#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"

#if !defined(MAC_OS_X_VERSION_10_7) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_7

enum {
   NSEventPhaseNone       = 0,
   NSEventPhaseBegan      = 0x1 << 0,
   NSEventPhaseStationary = 0x1 << 1,
   NSEventPhaseChanged    = 0x1 << 2,
   NSEventPhaseEnded      = 0x1 << 3,
   NSEventPhaseCancelled  = 0x1 << 4,
};
typedef NSUInteger NSEventPhase;

@interface NSEvent (LionAPI)

- (NSEventPhase)momentumPhase;
- (NSEventPhase)phase;

@end

#endif  // 10.7

@interface InvisibleScroller : NSScroller;
@end

@implementation InvisibleScroller

// Makes it non-interactive (and invisible) on Lion with both 10.6 and 10.7
// SDKs. TODO(tapted): Find a way to make it non-interactive on Snow Leopard.
// TODO(tapted): Find a way to make it take up no space on Lion with a 10.6 SDK.
- (NSRect)rectForPart:(NSScrollerPart)aPart {
  return NSZeroRect;
}

@end

@implementation ScrollViewWithNoScrollbars

@synthesize delegate = delegate_;

- (id)initWithFrame:(NSRect)frame {
  if ((self = [super initWithFrame:frame])) {
    [self setHasHorizontalScroller:base::mac::IsOSLionOrLater()];
    NSRect horizontalScrollerRect = [self bounds];
    horizontalScrollerRect.size.height = 0;
    base::scoped_nsobject<InvisibleScroller> horizontalScroller(
        [[InvisibleScroller alloc] initWithFrame:horizontalScrollerRect]);
    [self setHorizontalScroller:horizontalScroller];
  }
  return self;
}

- (void)endGestureWithEvent:(NSEvent*)event {
  [super endGestureWithEvent:event];
  if (!base::mac::IsOSLionOrLater())
    [delegate_ userScrolling:NO];
}

- (void)scrollWheel:(NSEvent*)event {
  if ([event subtype] == NSMouseEventSubtype) {
    // Since the scroll view has no vertical scroller, regular up and down mouse
    // wheel events would be ignored. This maps mouse wheel events to a
    // horizontal scroll event of one line, to turn pages.
    BOOL downOrRight;
    if ([event deltaX] != 0)
      downOrRight = [event deltaX] > 0;
    else if ([event deltaY] != 0)
      downOrRight = [event deltaY] > 0;
    else
      return;

    base::ScopedCFTypeRef<CGEventRef> cgEvent(CGEventCreateScrollWheelEvent(
        NULL, kCGScrollEventUnitLine, 2, 0, downOrRight ? 1 : -1));
    [super scrollWheel:[NSEvent eventWithCGEvent:cgEvent]];
    return;
  }

  [super scrollWheel:event];
  if (![event respondsToSelector:@selector(momentumPhase)])
    return;

  BOOL scrollComplete = [event momentumPhase] == NSEventPhaseEnded ||
      ([event momentumPhase] == NSEventPhaseNone &&
          [event phase] == NSEventPhaseEnded);

  [delegate_ userScrolling:!scrollComplete];
}

@end
