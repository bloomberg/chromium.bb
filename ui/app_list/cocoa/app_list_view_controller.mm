// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/app_list/cocoa/app_list_view_controller.h"

#include "base/mac/foundation_util.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_view_delegate.h"
#import "ui/app_list/cocoa/app_list_pager_view.h"
#import "ui/app_list/cocoa/apps_grid_controller.h"

namespace {

// The roundedness of the corners of the bubble.
const CGFloat kBubbleCornerRadius = 3;

// Height of the pager.
const CGFloat kPagerPreferredHeight = 57;
// Padding between the bottom of the grid and the bottom of the view.
const CGFloat kViewGridOffsetY = 38;

// Height of the search input. TODO(tapted): Make this visible when the search
// input UI is written.
const CGFloat kSearchInputHeight = 0;

// Minimum margin on either side of the pager. If the pager grows beyond this,
// the segment size is reduced.
const CGFloat kMinPagerMargin = 40;
// Maximum width of a single segment.
const CGFloat kMaxSegmentWidth = 80;

}  // namespace

@interface BackgroundView : NSView;
@end

@implementation BackgroundView

- (void)drawRect:(NSRect)dirtyRect {
  [NSGraphicsContext saveGraphicsState];
  [gfx::SkColorToCalibratedNSColor(app_list::kContentsBackgroundColor) set];
  [[NSBezierPath bezierPathWithRoundedRect:[self bounds]
                                   xRadius:kBubbleCornerRadius
                                   yRadius:kBubbleCornerRadius] addClip];
  NSRectFill([self bounds]);
  [NSGraphicsContext restoreGraphicsState];
}

@end

@interface AppListViewController ()

- (void)loadAndSetView;

@end

@implementation AppListViewController

- (id)init {
  if ((self = [super init])) {
    appsGridController_.reset([[AppsGridController alloc] init]);
    [self loadAndSetView];

    [self totalPagesChanged];
    [self selectedPageChanged:0];
    [appsGridController_ setPaginationObserver:self];
  }
  return self;
}

- (void)dealloc {
  // Ensure that setDelegate(NULL) has been called before destruction, because
  // dealloc can be called at odd times, and Objective C destruction order does
  // not properly tear down these dependencies.
  DCHECK(delegate_ == NULL);
  [appsGridController_ setPaginationObserver:nil];
  [super dealloc];
}

- (AppsGridController*)appsGridController {
  return appsGridController_;
}

- (NSSegmentedControl*)pagerControl {
  return pagerControl_;
}

- (app_list::AppListViewDelegate*)delegate {
  return delegate_.get();
}

- (void)setDelegate:(scoped_ptr<app_list::AppListViewDelegate>)newDelegate {
  delegate_.reset(newDelegate.release());
  [appsGridController_ setDelegate:delegate_.get()];
}

-(void)loadAndSetView {
  pagerControl_.reset([[AppListPagerView alloc] init]);
  [pagerControl_ setTarget:appsGridController_];
  [pagerControl_ setAction:@selector(onPagerClicked:)];

  [[appsGridController_ view] setFrameOrigin:NSMakePoint(0, kViewGridOffsetY)];

  NSRect backgroundRect = [[appsGridController_ view] bounds];
  backgroundRect.size.height += kViewGridOffsetY;
  scoped_nsobject<BackgroundView> backgroundView(
      [[BackgroundView alloc] initWithFrame:backgroundRect]);

  NSRect searchInputRect =
      NSMakeRect(0, NSMaxY(backgroundRect) - kSearchInputHeight,
                 backgroundRect.size.width, kSearchInputHeight);
  scoped_nsobject<NSTextField> searchInput(
      [[NSTextField alloc] initWithFrame:searchInputRect]);
  [searchInput setDelegate:self];

  [backgroundView addSubview:[appsGridController_ view]];
  [backgroundView addSubview:pagerControl_];
  [backgroundView addSubview:searchInput];
  [self setView:backgroundView];
}

- (void)totalPagesChanged {
  size_t pageCount = [appsGridController_ pageCount];
  [pagerControl_ setSegmentCount:pageCount];

  NSRect viewFrame = [[self view] bounds];
  CGFloat segmentWidth = std::min(
      kMaxSegmentWidth,
      (viewFrame.size.width - 2 * kMinPagerMargin) / pageCount);

  for (size_t i = 0; i < pageCount; ++i) {
    [pagerControl_ setWidth:segmentWidth
                 forSegment:i];
    [[pagerControl_ cell] setTag:i
                      forSegment:i];
  }

  // Center in view.
  [pagerControl_ sizeToFit];
  [pagerControl_ setFrame:
      NSMakeRect(NSMidX(viewFrame) - NSMidX([pagerControl_ bounds]),
                 0,
                 [pagerControl_ bounds].size.width,
                 kPagerPreferredHeight)];
}

- (void)selectedPageChanged:(int)newSelected {
  [pagerControl_ selectSegmentWithTag:newSelected];
}

- (void)pageVisibilityChanged {
  [pagerControl_ setNeedsDisplay:YES];
}

- (BOOL)control:(NSControl*)control
               textView:(NSTextView*)textView
    doCommandBySelector:(SEL)command {
  // If anything has been written, let the search view handle it.
  if ([[control stringValue] length] > 0)
    return NO;

  // Handle escape.
  if (command == @selector(complete:) ||
      command == @selector(cancel:) ||
      command == @selector(cancelOperation:)) {
    if (delegate_)
      delegate_->Dismiss();
    return YES;
  }

  // Possibly handle grid navigation.
  return [appsGridController_ handleCommandBySelector:command];
}

@end
