// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_COCOA_APPS_GRID_VIEW_ITEM_H_
#define UI_APP_LIST_COCOA_APPS_GRID_VIEW_ITEM_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_ptr.h"

namespace app_list {
class AppListItemModel;
class ItemModelObserverBridge;
}

// AppsGridViewItem is the controller for an NSButton representing an app item
// on an NSCollectionView controlled by an AppsGridController.
@interface AppsGridViewItem : NSCollectionViewItem {
 @private
  scoped_ptr<app_list::ItemModelObserverBridge> observerBridge_;
}

- (void)setModel:(app_list::AppListItemModel*)itemModel;

- (app_list::AppListItemModel*)model;

- (NSButton*)button;

@end

#endif  // UI_APP_LIST_COCOA_APPS_GRID_VIEW_ITEM_H_
