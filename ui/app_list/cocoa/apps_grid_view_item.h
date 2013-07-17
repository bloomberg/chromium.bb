// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_COCOA_APPS_GRID_VIEW_ITEM_H_
#define UI_APP_LIST_COCOA_APPS_GRID_VIEW_ITEM_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_ptr.h"
#include "ui/app_list/app_list_export.h"
#import "ui/base/cocoa/tracking_area.h"

namespace app_list {
class AppListItemModel;
class ItemModelObserverBridge;
}

// AppsGridViewItem is the controller for an NSButton representing an app item
// on an NSCollectionView controlled by an AppsGridController.
APP_LIST_EXPORT
@interface AppsGridViewItem : NSCollectionViewItem {
 @private
  scoped_ptr<app_list::ItemModelObserverBridge> observerBridge_;
  base::scoped_nsobject<NSProgressIndicator> progressIndicator_;

  // Used to highlight the background on hover.
  ui::ScopedCrTrackingArea trackingArea_;
}

@property(readonly, nonatomic) NSProgressIndicator* progressIndicator;

// Designated initializer. |tileSize| is the size of tiles in the grid.
- (id)initWithSize:(NSSize)tileSize;

// Set the represented model, updating views. Clears if |itemModel| is NULL.
- (void)setModel:(app_list::AppListItemModel*)itemModel;

// Model accessor, via the |observerBridge_|.
- (app_list::AppListItemModel*)model;

// Return the button portion of the item, showing the icon and title.
- (NSButton*)button;

// Generate and return a context menu, populated using the represented model.
- (NSMenu*)contextMenu;

// Take a snapshot of the grid cell with correct layout, then hide the button.
// If |isRestore| is true, the snapshot includes the label and items hidden for
// the initial snapshot are restored.
- (NSBitmapImageRep*)dragRepresentationForRestore:(BOOL)isRestore;

@end

#endif  // UI_APP_LIST_COCOA_APPS_GRID_VIEW_ITEM_H_
