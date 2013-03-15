// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_COCOA_APPS_GRID_CONTROLLER_H_
#define UI_APP_LIST_COCOA_APPS_GRID_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "ui/app_list/cocoa/scroll_view_with_no_scrollbars.h"

namespace app_list {
class AppListModel;
class AppListViewDelegate;
class AppsGridDelegateBridge;
}

@class AppsGridViewItem;

// Controls a grid of views, representing AppListModel::Apps sub models.
@interface AppsGridController : NSViewController<GestureScrollDelegate> {
 @private
  scoped_ptr<app_list::AppListModel> model_;
  scoped_ptr<app_list::AppListViewDelegate> delegate_;
  scoped_ptr<app_list::AppsGridDelegateBridge> bridge_;

  scoped_nsobject<NSMutableArray> pages_;
  scoped_nsobject<NSMutableArray> items_;

  // Whether we are currently animating a scroll to the nearest page.
  BOOL animatingScroll_;
}

- (id)initWithViewDelegate:
    (scoped_ptr<app_list::AppListViewDelegate>)appListViewDelegate;

- (NSCollectionView*)collectionViewAtPageIndex:(size_t)pageIndex;

- (AppsGridViewItem*)itemAtIndex:(size_t)itemIndex;

- (app_list::AppListModel*)model;

- (app_list::AppListViewDelegate*)delegate;

- (void)setModel:(scoped_ptr<app_list::AppListModel>)model;

// Calls delegate_->ActivateAppListItem for the currently selected item by
// simulating a click.
- (void)activateSelection;

// Return the number of pages of icons in the grid.
- (size_t)pageCount;

// Scroll to a page in the grid view with an animation.
- (void)scrollToPage:(size_t)pageIndex;

@end

#endif  // UI_APP_LIST_COCOA_APPS_GRID_CONTROLLER_H_
