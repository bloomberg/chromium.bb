// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/app_list/cocoa/apps_search_results_model_bridge.h"

#import <Cocoa/Cocoa.h>

#include "base/strings/sys_string_conversions.h"
#include "ui/app_list/app_list_model.h"
#import "ui/app_list/cocoa/apps_search_results_controller.h"
#include "ui/app_list/search_result.h"
#include "ui/app_list/search_result_observer.h"
#import "ui/base/cocoa/menu_controller.h"

namespace app_list {

class AppsSearchResultsModelBridge::ItemObserver : public SearchResultObserver {
 public:
  ItemObserver(AppsSearchResultsModelBridge* bridge, size_t index)
      : bridge_(bridge), row_in_view_(index) {
    // Cache the result, because the results array is updated before notifying
    // observers (which happens before deleting the SearchResult).
    result_ = [bridge_->parent_ results]->GetItemAt(index);
    result_->AddObserver(this);
  }

  virtual ~ItemObserver() {
    result_->RemoveObserver(this);
  }

  NSMenu* GetContextMenu() {
    if (!context_menu_controller_) {
      ui::MenuModel* menu_model = result_->GetContextMenuModel();
      if (!menu_model)
        return nil;

      context_menu_controller_.reset(
          [[MenuController alloc] initWithModel:menu_model
                         useWithPopUpButtonCell:NO]);
    }
    return [context_menu_controller_ menu];
  }

  // SearchResultObserver overrides:
  virtual void OnIconChanged() OVERRIDE {
    bridge_->ReloadDataForItems(row_in_view_, 1);
  }
  virtual void OnActionsChanged() OVERRIDE {}
  virtual void OnIsInstallingChanged() OVERRIDE {}
  virtual void OnPercentDownloadedChanged() OVERRIDE {}
  virtual void OnItemInstalled() OVERRIDE {}
  virtual void OnItemUninstalled() OVERRIDE;

 private:
  AppsSearchResultsModelBridge* bridge_;  // Weak. Owns us.
  SearchResult* result_;  // Weak. Owned by AppListModel::SearchResults.
  size_t row_in_view_;
  base::scoped_nsobject<MenuController> context_menu_controller_;

  DISALLOW_COPY_AND_ASSIGN(ItemObserver);
};

void AppsSearchResultsModelBridge::ItemObserver::OnItemUninstalled() {
  // Performing the search again will destroy |this|, so post a task. This also
  // ensures that the AppSearchProvider has observed the uninstall before
  // performing the search again, otherwise it will provide a NULL result.
  [[bridge_->parent_ delegate] performSelector:@selector(redoSearch)
                                    withObject:nil
                                    afterDelay:0];
}

AppsSearchResultsModelBridge::AppsSearchResultsModelBridge(
    AppsSearchResultsController* results_controller)
    : parent_(results_controller) {
  UpdateItemObservers();
  [parent_ results]->AddObserver(this);
}

AppsSearchResultsModelBridge::~AppsSearchResultsModelBridge() {
  [parent_ results]->RemoveObserver(this);
}

NSMenu* AppsSearchResultsModelBridge::MenuForItem(size_t index) {
  DCHECK_LT(index, item_observers_.size());
  return item_observers_[index]->GetContextMenu();
}

void AppsSearchResultsModelBridge::UpdateItemObservers() {
  DCHECK(item_observers_.empty());
  const size_t itemCount = [parent_ results]->item_count();
  for (size_t i = 0 ; i < itemCount; ++i)
    item_observers_.push_back(new ItemObserver(this, i));
}

void AppsSearchResultsModelBridge::ReloadDataForItems(
    size_t start, size_t count) const {
  NSIndexSet* column = [NSIndexSet indexSetWithIndex:0];
  NSIndexSet* rows =
      [NSIndexSet indexSetWithIndexesInRange:NSMakeRange(start, count)];
  [[parent_ tableView] reloadDataForRowIndexes:rows
                                 columnIndexes:column];
}

void AppsSearchResultsModelBridge::ListItemsAdded(
    size_t start, size_t count) {
  item_observers_.clear();
  if (start == static_cast<size_t>([[parent_ tableView] numberOfRows]))
    [[parent_ tableView] noteNumberOfRowsChanged];
  else
    [[parent_ tableView] reloadData];
  UpdateItemObservers();
}

void AppsSearchResultsModelBridge::ListItemsRemoved(
    size_t start, size_t count) {
  item_observers_.clear();
  if (start == [parent_ results]->item_count())
    [[parent_ tableView] noteNumberOfRowsChanged];
  else
    [[parent_ tableView] reloadData];
  UpdateItemObservers();
}

void AppsSearchResultsModelBridge::ListItemMoved(
    size_t index, size_t target_index) {
  NOTREACHED();
}

void AppsSearchResultsModelBridge::ListItemsChanged(
    size_t start, size_t count) {
  item_observers_.clear();
  ReloadDataForItems(start, count);
  UpdateItemObservers();
}

}  // namespace app_list
