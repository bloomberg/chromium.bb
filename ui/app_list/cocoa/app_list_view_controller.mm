// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/app_list/cocoa/app_list_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/app_list_view_delegate_observer.h"
#import "ui/app_list/cocoa/app_list_pager_view.h"
#import "ui/app_list/cocoa/apps_grid_controller.h"
#import "ui/base/cocoa/flipped_view.h"
#include "ui/app_list/search_box_model.h"
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

// Duration of the animation for sliding in and out search results.
const NSTimeInterval kResultsAnimationDuration = 0.2;

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

  [gfx::SkColorToSRGBNSColor(app_list::kContentsBackgroundColor) set];
  NSRectFill(boundsRect);
  [gfx::SkColorToSRGBNSColor(app_list::kSearchBoxBackground) set];
  NSRectFill(searchAreaRect);
  [gfx::SkColorToSRGBNSColor(app_list::kTopSeparatorColor) set];
  NSRectFill(separatorRect);
}

@end

@interface AppListViewController ()

- (void)loadAndSetView;
- (void)revealSearchResults:(BOOL)show;

@end

namespace app_list {

class AppListModelObserverBridge : public AppListViewDelegateObserver {
 public:
  AppListModelObserverBridge(AppListViewController* parent);
  virtual ~AppListModelObserverBridge();

 private:
  // Overridden from app_list::AppListViewDelegateObserver:
  virtual void OnProfilesChanged() OVERRIDE;

  AppListViewController* parent_;  // Weak. Owns us.

  DISALLOW_COPY_AND_ASSIGN(AppListModelObserverBridge);
};

AppListModelObserverBridge::AppListModelObserverBridge(
    AppListViewController* parent)
    : parent_(parent) {
  [parent_ delegate]->AddObserver(this);
}

AppListModelObserverBridge::~AppListModelObserverBridge() {
  [parent_ delegate]->RemoveObserver(this);
}

void AppListModelObserverBridge::OnProfilesChanged() {
  [parent_ onProfilesChanged];
}

}  // namespace app_list

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

- (AppsSearchBoxController*)searchBoxController {
  return appsSearchBoxController_;
}

- (BOOL)showingSearchResults {
  return showingSearchResults_;
}

- (AppsGridController*)appsGridController {
  return appsGridController_;
}

- (NSSegmentedControl*)pagerControl {
  return pagerControl_;
}

- (NSView*)backgroundView {
  return backgroundView_;
}

- (app_list::AppListViewDelegate*)delegate {
  return delegate_.get();
}

- (void)setDelegate:(scoped_ptr<app_list::AppListViewDelegate>)newDelegate {
  if (delegate_) {
    // Ensure the search box is cleared when switching profiles.
    if ([self searchBoxModel])
      [self searchBoxModel]->SetText(base::string16());

    // First clean up, in reverse order.
    app_list_model_observer_bridge_.reset();
    [appsSearchResultsController_ setDelegate:nil];
    [appsSearchBoxController_ setDelegate:nil];
    [appsGridController_ setDelegate:nil];
  }
  delegate_.reset(newDelegate.release());
  if (delegate_) {
    [loadingIndicator_ stopAnimation:self];
  } else {
    [loadingIndicator_ startAnimation:self];
    return;
  }

  [appsGridController_ setDelegate:delegate_.get()];
  [appsSearchBoxController_ setDelegate:self];
  [appsSearchResultsController_ setDelegate:self];
  app_list_model_observer_bridge_.reset(
      new app_list::AppListModelObserverBridge(self));
  [self onProfilesChanged];
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

  // The contents view contains animations both from an NSCollectionView and the
  // app list's own transitive drag layers. On Mavericks, the subviews need to
  // have access to a compositing layer they can share. Otherwise the compositor
  // makes tearing artifacts. However, doing this on Mountain Lion or earler
  // results in flickering whilst an item is installing.
  if (base::mac::IsOSMavericksOrLater())
    [contentsView_ setWantsLayer:YES];

  backgroundView_.reset(
      [[BackgroundView alloc] initWithFrame:
              NSMakeRect(0, 0, NSMaxX(contentsRect), NSMaxY(contentsRect))]);
  appsSearchBoxController_.reset(
      [[AppsSearchBoxController alloc] initWithFrame:
          NSMakeRect(0, 0, NSWidth(contentsRect), kSearchInputHeight)]);
  appsSearchResultsController_.reset(
      [[AppsSearchResultsController alloc] initWithAppsSearchResultsFrameSize:
          [contentsView_ bounds].size]);
  base::scoped_nsobject<NSView> containerView(
      [[NSView alloc] initWithFrame:[backgroundView_ frame]]);

  loadingIndicator_.reset(
      [[NSProgressIndicator alloc] initWithFrame:NSZeroRect]);
  [loadingIndicator_ setStyle:NSProgressIndicatorSpinningStyle];
  [loadingIndicator_ sizeToFit];
  NSRect indicatorRect = [loadingIndicator_ frame];
  indicatorRect.origin.x = NSWidth(contentsRect) / 2 - NSMidX(indicatorRect);
  indicatorRect.origin.y = NSHeight(contentsRect) / 2 - NSMidY(indicatorRect);
  [loadingIndicator_ setFrame:indicatorRect];
  [loadingIndicator_ setDisplayedWhenStopped:NO];
  [loadingIndicator_ startAnimation:self];

  [contentsView_ addSubview:[appsGridController_ view]];
  [contentsView_ addSubview:pagerControl_];
  [contentsView_ addSubview:loadingIndicator_];
  [backgroundView_ addSubview:contentsView_];
  [backgroundView_ addSubview:[appsSearchResultsController_ view]];
  [backgroundView_ addSubview:[appsSearchBoxController_ view]];
  [containerView addSubview:backgroundView_];
  [self setView:containerView];
}

- (void)revealSearchResults:(BOOL)show {
  if (show == showingSearchResults_)
    return;

  showingSearchResults_ = show;
  NSSize contentsSize = [contentsView_ frame].size;
  NSRect resultsTargetRect = NSMakeRect(
      0, kSearchInputHeight + kTopSeparatorSize,
      contentsSize.width, contentsSize.height);
  NSRect contentsTargetRect = resultsTargetRect;

  // Shows results by sliding the grid and pager down to the bottom of the view.
  // Hides results by collapsing the search results container to a height of 0.
  if (show)
    contentsTargetRect.origin.y += NSHeight(contentsTargetRect);
  else
    resultsTargetRect.size.height = 0;

  [[NSAnimationContext currentContext] setDuration:kResultsAnimationDuration];
  [[contentsView_ animator] setFrame:contentsTargetRect];
  [[[appsSearchResultsController_ view] animator] setFrame:resultsTargetRect];
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
  if (showingSearchResults_)
    return [appsSearchResultsController_ handleCommandBySelector:command];

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

  base::string16 query;
  base::TrimWhitespace(searchBoxModel->text(), base::TRIM_ALL, &query);
  BOOL shouldShowSearch = !query.empty();
  [self revealSearchResults:shouldShowSearch];
  if (shouldShowSearch)
    delegate_->StartSearch();
  else
    delegate_->StopSearch();
}

- (app_list::AppListModel*)appListModel {
  return [appsGridController_ model];
}

- (void)openResult:(app_list::SearchResult*)result {
  if (delegate_) {
    delegate_->OpenSearchResult(
        result, false /* auto_launch */, 0 /* event flags */);
  }
}

- (void)redoSearch {
  [self modelTextDidChange];
}

- (void)onProfilesChanged {
  [appsSearchBoxController_ rebuildMenu];
}

@end
