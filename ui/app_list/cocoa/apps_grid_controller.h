// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_COCOA_APPS_GRID_CONTROLLER_H_
#define UI_APP_LIST_COCOA_APPS_GRID_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "ui/app_list/app_list_export.h"
#import "ui/app_list/cocoa/app_list_pager_view.h"
#import "ui/app_list/cocoa/scroll_view_with_no_scrollbars.h"

namespace app_list {
class AppListModel;
class AppListViewDelegate;
class AppsGridDelegateBridge;
}

@class AppsGridViewItem;
@protocol AppsPaginationModelObserver;
@class AppsCollectionViewDragManager;

// Controls a grid of views, representing AppListItemList sub models.
APP_LIST_EXPORT
@interface AppsGridController : NSViewController<GestureScrollDelegate,
                                                 AppListPagerDelegate,
                                                 NSCollectionViewDelegate> {
 @private
  app_list::AppListViewDelegate* delegate_;  // Weak. Owned by view controller.
  scoped_ptr<app_list::AppsGridDelegateBridge> bridge_;

  base::scoped_nsobject<AppsCollectionViewDragManager> dragManager_;
  base::scoped_nsobject<NSMutableArray> pages_;
  base::scoped_nsobject<NSMutableArray> items_;
  base::scoped_nsobject<NSTimer> scrollWhileDraggingTimer_;

  id<AppsPaginationModelObserver> paginationObserver_;

  // Index of the currently visible page.
  size_t visiblePage_;
  // The page to which the view is currently animating a scroll.
  size_t targetScrollPage_;
  // The page to start scrolling to when the timer expires.
  size_t scheduledScrollPage_;

  // Whether we are currently animating a scroll to the nearest page.
  BOOL animatingScroll_;
}

@property(assign, nonatomic) id<AppsPaginationModelObserver> paginationObserver;

+ (void)setScrollAnimationDuration:(NSTimeInterval)duration;

// The amount the grid view has been extended to hold the sometimes present
// invisible scroller that allows for gesture scrolling.
+ (CGFloat)scrollerPadding;

- (NSCollectionView*)collectionViewAtPageIndex:(size_t)pageIndex;
- (size_t)pageIndexForCollectionView:(NSCollectionView*)page;

- (AppsGridViewItem*)itemAtIndex:(size_t)itemIndex;

- (app_list::AppListModel*)model;

- (void)setDelegate:(app_list::AppListViewDelegate*)newDelegate;

- (size_t)visiblePage;

// Calls item->Activate for the currently selected item by simulating a click.
- (void)activateSelection;

// Return the number of pages of icons in the grid.
- (size_t)pageCount;

// Return the number of items over all pages in the grid.
- (size_t)itemCount;

// Scroll to a page in the grid view with an animation.
- (void)scrollToPage:(size_t)pageIndex;

// Start a timer to scroll to a new page, if |locationInWindow| is to the left
// or the right of the view, or if it is over a pager segment. Cancels any
// existing timer if the target page changes.
- (void)maybeChangePageForPoint:(NSPoint)locationInWindow;

// Cancel a timer that may have been set by maybeChangePageForPoint().
- (void)cancelScrollTimer;

// Moves an item within the view only, for dragging or in response to model
// changes.
- (void)moveItemInView:(size_t)fromIndex
           toItemIndex:(size_t)toIndex;

// Moves an item in the item model. Does not adjust the view.
- (void)moveItemWithIndex:(size_t)itemIndex
             toModelIndex:(size_t)modelIndex;

// Return the index of the selected item.
- (NSUInteger)selectedItemIndex;

// Moves the selection to the given index.
- (void)selectItemAtIndex:(NSUInteger)index;

// Handle key actions. Similar to doCommandBySelector from NSResponder but that
// requires this class to be in the responder chain. Instead this method is
// invoked by the AppListViewController.
// Returns YES if this handled navigation or launched an app.
- (BOOL)handleCommandBySelector:(SEL)command;

@end

@interface AppsGridController(TestingAPI)

- (AppsCollectionViewDragManager*)dragManager;
- (size_t)scheduledScrollPage;

@end

#endif  // UI_APP_LIST_COCOA_APPS_GRID_CONTROLLER_H_
