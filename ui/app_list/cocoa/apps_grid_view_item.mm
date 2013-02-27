// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/app_list/cocoa/apps_grid_view_item.h"

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_item_model.h"
#include "ui/app_list/app_list_item_model_observer.h"
#include "ui/gfx/image/image_skia_util_mac.h"

namespace app_list {

class ItemModelObserverBridge : public app_list::AppListItemModelObserver {
 public:
  ItemModelObserverBridge(AppsGridViewItem* parent, AppListItemModel* model);
  virtual ~ItemModelObserverBridge();

  AppListItemModel* model() { return model_; }

  virtual void ItemIconChanged() OVERRIDE;
  virtual void ItemTitleChanged() OVERRIDE;
  virtual void ItemHighlightedChanged() OVERRIDE;
  virtual void ItemIsInstallingChanged() OVERRIDE;
  virtual void ItemPercentDownloadedChanged() OVERRIDE;

 private:
  AppsGridViewItem* parent_;  // Weak. Owns us.
  AppListItemModel* model_;  // Weak. Owned by AppListModel::Apps.

  DISALLOW_COPY_AND_ASSIGN(ItemModelObserverBridge);
};

ItemModelObserverBridge::ItemModelObserverBridge(AppsGridViewItem* parent,
                                                 AppListItemModel* model)
    : parent_(parent),
      model_(model) {
  model_->AddObserver(this);
}

ItemModelObserverBridge::~ItemModelObserverBridge() {
  model_->RemoveObserver(this);
}

void ItemModelObserverBridge::ItemIconChanged() {
  [[parent_ button] setImage:gfx::NSImageFromImageSkia(model_->icon())];
}

void ItemModelObserverBridge::ItemTitleChanged() {
  [[parent_ button] setTitle:base::SysUTF8ToNSString(model_->title())];
}

void ItemModelObserverBridge::ItemHighlightedChanged() {
  //TODO(tapted): Ensure the item view is visible (requires pagination).
}

void ItemModelObserverBridge::ItemIsInstallingChanged() {
  //TODO(tapted): Hide the title while itemModel->is_installing().
}

void ItemModelObserverBridge::ItemPercentDownloadedChanged() {
  //TODO(tapted): Update install progress bar for this item.
}

}  // namespace app_list

@implementation AppsGridViewItem

- (void)setModel:(app_list::AppListItemModel*)itemModel {
  if (!itemModel) {
    observerBridge_.reset();
    return;
  }

  NSButton* button = [self button];
  [button setTitle:base::SysUTF8ToNSString(itemModel->title())];
  [button setImage:gfx::NSImageFromImageSkia(itemModel->icon())];
  observerBridge_.reset(new app_list::ItemModelObserverBridge(self, itemModel));
}

- (app_list::AppListItemModel*)model {
  return observerBridge_->model();
}

- (NSButton*)button {
  return base::mac::ObjCCastStrict<NSButton>([self view]);
}

- (void)setSelected:(BOOL)flag {
  if (flag) {
    [[[self button] cell] setBackgroundColor:[NSColor lightGrayColor]];
  } else {
    [[[self button] cell] setBackgroundColor:gfx::SkColorToCalibratedNSColor(
        app_list::kContentsBackgroundColor)];
  }
  [super setSelected:flag];
}

@end
