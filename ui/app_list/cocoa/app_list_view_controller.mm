// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/app_list/cocoa/app_list_view_controller.h"

#include "base/mac/foundation_util.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/app_list_view_delegate.h"
#import "ui/app_list/cocoa/app_list_pager_view.h"
#import "ui/app_list/cocoa/apps_grid_controller.h"
#import "ui/base/cocoa/flipped_view.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

namespace {

// The roundedness of the corners of the bubble.
const CGFloat kBubbleCornerRadius = 3;

// Height of the pager.
const CGFloat kPagerPreferredHeight = 57;

// Height of separator line drawn between the searchbox and grid view.
const CGFloat kTopSeparatorSize = 1;

// Height of the search input.
const CGFloat kSearchInputHeight = 48;

// Minimum margin on either side of the pager. If the pager grows beyond this,
// the segment size is reduced.
const CGFloat kMinPagerMargin = 40;
// Maximum width of a single segment.
const CGFloat kMaxSegmentWidth = 80;

}  // namespace

@interface BackgroundView : FlippedView;
@end

@implementation BackgroundView

- (void)drawRect:(NSRect)dirtyRect {
  gfx::ScopedNSGraphicsContextSaveGState context;
  NSRect boundsRect = [self bounds];
  NSRect searchAreaRect = NSMakeRect(0, 0,
                                     NSWidth(boundsRect), kSearchInputHeight);
  NSRect separatorRect = NSMakeRect(0, NSMaxY(searchAreaRect),
                                    NSWidth(boundsRect), kTopSeparatorSize);

  [[NSBezierPath bezierPathWithRoundedRect:boundsRect
                                   xRadius:kBubbleCornerRadius
                                   yRadius:kBubbleCornerRadius] addClip];

  [gfx::SkColorToCalibratedNSColor(app_list::kContentsBackgroundColor) set];
  NSRectFill(boundsRect);
  [gfx::SkColorToCalibratedNSColor(app_list::kSearchBoxBackground) set];
  NSRectFill(searchAreaRect);
  [gfx::SkColorToCalibratedNSColor(app_list::kTopSeparatorColor) set];
  NSRectFill(separatorRect);
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

- (void)setDelegate:(scoped_ptr<app_list::AppListViewDelegate>)newDelegate
      withTestModel:(scoped_ptr<app_list::AppListModel>)newModel {
  if (delegate_) {
    // First clean up, in reverse order.
    [appsSearchBoxController_ setDelegate:nil];
  }
  delegate_.reset(newDelegate.release());
  [appsGridController_ setDelegate:delegate_.get()];
  if (newModel.get())
    [appsGridController_ setModel:newModel.Pass()];
  [appsSearchBoxController_ setDelegate:self];
}

- (void)setDelegate:(scoped_ptr<app_list::AppListViewDelegate>)newDelegate {
  [self setDelegate:newDelegate.Pass()
      withTestModel:scoped_ptr<app_list::AppListModel>()];
}

-(void)loadAndSetView {
  pagerControl_.reset([[AppListPagerView alloc] init]);
  [pagerControl_ setTarget:appsGridController_];
  [pagerControl_ setAction:@selector(onPagerClicked:)];

  NSRect gridFrame = [[appsGridController_ view] frame];
  NSRect contentsRect = NSMakeRect(0, kSearchInputHeight + kTopSeparatorSize,
      NSWidth(gridFrame), NSHeight(gridFrame) + kPagerPreferredHeight -
          [AppsGridController scrollerPadding]);

  contentsView_.reset([[FlippedView alloc] initWithFrame:contentsRect]);
  scoped_nsobject<BackgroundView> backgroundView(
      [[BackgroundView alloc] initWithFrame:
          NSMakeRect(0, 0, NSMaxX(contentsRect), NSMaxY(contentsRect))]);
  appsSearchBoxController_.reset(
      [[AppsSearchBoxController alloc] initWithFrame:
          NSMakeRect(0, 0, NSWidth(contentsRect), kSearchInputHeight)]);

  [contentsView_ addSubview:[appsGridController_ view]];
  [contentsView_ addSubview:pagerControl_];
  [backgroundView addSubview:contentsView_];
  [backgroundView addSubview:[appsSearchBoxController_ view]];
  [self setView:backgroundView];
}

- (void)totalPagesChanged {
  size_t pageCount = [appsGridController_ pageCount];
  [pagerControl_ setSegmentCount:pageCount];

  NSRect viewFrame = [[pagerControl_ superview] bounds];
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
                 viewFrame.size.height - kPagerPreferredHeight,
                 [pagerControl_ bounds].size.width,
                 kPagerPreferredHeight)];
}

- (void)selectedPageChanged:(int)newSelected {
  [pagerControl_ selectSegmentWithTag:newSelected];
}

- (void)pageVisibilityChanged {
  [pagerControl_ setNeedsDisplay:YES];
}

- (NSInteger)pagerSegmentAtLocation:(NSPoint)locationInWindow {
  return [pagerControl_ findAndHighlightSegmentAtLocation:locationInWindow];
}

- (app_list::SearchBoxModel*)searchBoxModel {
  app_list::AppListModel* appListModel = [appsGridController_ model];
  return appListModel ? appListModel->search_box() : NULL;
}

- (app_list::AppListViewDelegate*)appListDelegate {
  return [self delegate];
}

- (BOOL)control:(NSControl*)control
               textView:(NSTextView*)textView
    doCommandBySelector:(SEL)command {
  // TODO(tapted): If showing search results, first pass up/down navigation to
  // the search results controller.

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

- (void)modelTextDidChange {
  app_list::SearchBoxModel* searchBoxModel = [self searchBoxModel];
  if (!searchBoxModel || !delegate_)
    return;

  // TODO(tapted): If there is a non-empty query in |searchBoxModel| reveal the
  // search results, and run delegate_->StartSearch(). Or, if the query is now
  // empty, hide results and run delegate_->StopSearch().
}

@end
