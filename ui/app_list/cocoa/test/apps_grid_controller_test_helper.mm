// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/app_list/cocoa/test/apps_grid_controller_test_helper.h"

#include "base/mac/foundation_util.h"
#import "ui/app_list/cocoa/apps_grid_controller.h"
#import "ui/app_list/cocoa/apps_grid_view_item.h"
#include "ui/app_list/test/app_list_test_model.h"
#include "ui/app_list/test/app_list_test_view_delegate.h"
#import "ui/base/test/cocoa_test_event_utils.h"

namespace app_list {
namespace test {

const size_t AppsGridControllerTestHelper::kItemsPerPage = 16;

AppsGridControllerTestHelper::AppsGridControllerTestHelper() {
  Init();
  delegate_.reset(new AppListTestViewDelegate);
}

AppsGridControllerTestHelper::~AppsGridControllerTestHelper() {}

void AppsGridControllerTestHelper::SetUpWithGridController(
    AppsGridController* grid_controller) {
  ui::CocoaTest::SetUp();
  apps_grid_controller_ = grid_controller;
  scoped_ptr<AppListModel> model(new AppListTestModel);
  [apps_grid_controller_ setModel:model.Pass()];
}

void AppsGridControllerTestHelper::SimulateClick(NSView* view) {
  std::pair<NSEvent*, NSEvent*> events(
      cocoa_test_event_utils::MouseClickInView(view, 1));
  [NSApp postEvent:events.first atStart:NO];
  [NSApp postEvent:events.second atStart:NO];
}

void AppsGridControllerTestHelper::SimulateKeyPress(unichar c) {
  [test_window() keyDown:cocoa_test_event_utils::KeyEventWithCharacter(c)];
}

void AppsGridControllerTestHelper::SimulateMouseEnterItemAt(size_t index) {
  [[apps_grid_controller_ itemAtIndex:index] mouseEntered:
      cocoa_test_event_utils::EnterExitEventWithType(NSMouseEntered)];
}

void AppsGridControllerTestHelper::SimulateMouseExitItemAt(size_t index) {
  [[apps_grid_controller_ itemAtIndex:index] mouseExited:
      cocoa_test_event_utils::EnterExitEventWithType(NSMouseExited)];
}

void AppsGridControllerTestHelper::ReplaceTestModel(int item_count) {
  scoped_ptr<AppListTestModel> new_model(new AppListTestModel);
  new_model->PopulateApps(item_count);
  [apps_grid_controller_ setModel:new_model.PassAs<AppListModel>()];
}

void AppsGridControllerTestHelper::DelayForCollectionView() {
  message_loop_.PostDelayedTask(FROM_HERE, MessageLoop::QuitClosure(),
                                base::TimeDelta::FromMilliseconds(100));
  message_loop_.Run();
}

void AppsGridControllerTestHelper::SinkEvents() {
  message_loop_.PostTask(FROM_HERE, MessageLoop::QuitClosure());
  message_loop_.Run();
}

NSButton* AppsGridControllerTestHelper::GetItemViewAt(size_t index) {
  return [[apps_grid_controller_ itemAtIndex:index] button];
}

NSCollectionView* AppsGridControllerTestHelper::GetPageAt(size_t index) {
  return [apps_grid_controller_ collectionViewAtPageIndex:index];
}

NSView* AppsGridControllerTestHelper::GetSelectedView() {
  // TODO(tapted): Update this to work for selections on other than the first
  // page.
  NSIndexSet* selection = [GetPageAt(0) selectionIndexes];
  if ([selection count]) {
    AppsGridViewItem* item = base::mac::ObjCCastStrict<AppsGridViewItem>(
        [GetPageAt(0) itemAtIndex:[selection firstIndex]]);
    return [item button];
  }

  return nil;
}

AppListTestViewDelegate* AppsGridControllerTestHelper::delegate() {
  return static_cast<AppListTestViewDelegate*>(delegate_.get());
}

AppListTestModel* AppsGridControllerTestHelper::model() {
  return static_cast<AppListTestModel*>([apps_grid_controller_ model]);
}

}  // namespace test
}  // namespace app_list
