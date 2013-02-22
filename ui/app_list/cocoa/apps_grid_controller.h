// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_COCOA_APPS_GRID_CONTROLLER_H_
#define UI_APP_LIST_COCOA_APPS_GRID_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"

namespace app_list {
class AppListModel;
class AppListViewDelegate;
class AppsGridDelegateBridge;
}

// Controls a grid of views, representing AppListModel::Apps sub models.
@interface AppsGridController : NSViewController {
 @private
  scoped_ptr<app_list::AppListModel> model_;
  scoped_ptr<app_list::AppListViewDelegate> delegate_;
  scoped_ptr<app_list::AppsGridDelegateBridge> bridge_;

  scoped_nsobject<NSMutableArray> items_;
}

- (id)initWithViewDelegate:
    (scoped_ptr<app_list::AppListViewDelegate>)appListViewDelegate;

- (NSCollectionView*)collectionView;

- (app_list::AppListModel*)model;

- (app_list::AppListViewDelegate*)delegate;

- (void)setModel:(scoped_ptr<app_list::AppListModel>)model;

- (void)activateSelection;

@end

#endif  // UI_APP_LIST_COCOA_APPS_GRID_CONTROLLER_H_
