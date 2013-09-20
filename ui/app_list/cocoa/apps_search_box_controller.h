// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_COCOA_APPS_SEARCH_BOX_CONTROLLER_H_
#define UI_APP_LIST_COCOA_APPS_SEARCH_BOX_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "ui/app_list/app_list_export.h"

namespace app_list {
class AppListMenu;
class AppListModel;
class AppListViewDelegate;
class SearchBoxModel;
class SearchBoxModelObserverBridge;
}

@class AppListMenuController;
@class HoverImageMenuButton;
@class SearchTextField;

@protocol AppsSearchBoxDelegate<NSTextFieldDelegate>

- (app_list::AppListViewDelegate*)appListDelegate;
- (app_list::SearchBoxModel*)searchBoxModel;
- (app_list::AppListModel*)appListModel;
- (void)modelTextDidChange;

@end

// Controller for the search box in the topmost portion of the app list.
APP_LIST_EXPORT
@interface AppsSearchBoxController : NSViewController<NSTextFieldDelegate> {
 @private
  base::scoped_nsobject<SearchTextField> searchTextField_;
  base::scoped_nsobject<NSImageView> searchImageView_;
  base::scoped_nsobject<HoverImageMenuButton> menuButton_;
  base::scoped_nsobject<AppListMenuController> menuController_;
  scoped_ptr<app_list::SearchBoxModelObserverBridge> bridge_;
  scoped_ptr<app_list::AppListMenu> appListMenu_;

  id<AppsSearchBoxDelegate> delegate_;  // Weak. Owns us.
}

@property(assign, nonatomic) id<AppsSearchBoxDelegate> delegate;

- (id)initWithFrame:(NSRect)frame;
- (void)clearSearch;

// Rebuild the menu due to changes from the AppListViewDelegate.
- (void)rebuildMenu;

@end

@interface AppsSearchBoxController (TestingAPI)

- (NSTextField*)searchTextField;
- (NSPopUpButton*)menuControl;
- (app_list::AppListMenu*)appListMenu;

@end

#endif  // UI_APP_LIST_COCOA_APPS_SEARCH_BOX_CONTROLLER_H_
