// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_COCOA_TEST_APPS_GRID_CONTROLLER_TEST_HELPER_H_
#define UI_APP_LIST_COCOA_TEST_APPS_GRID_CONTROLLER_TEST_HELPER_H_

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#import "ui/base/test/ui_cocoa_test_helper.h"

@class AppsGridController;

namespace app_list {
namespace test {

class AppListTestViewDelegate;
class AppListTestModel;

class AppsGridControllerTestHelper : public ui::CocoaTest {
 public:
  static const size_t kItemsPerPage;

  AppsGridControllerTestHelper();
  virtual ~AppsGridControllerTestHelper();

  void SetUpWithGridController(AppsGridController* grid_controller);

 protected:
  // Send a click to the test window in the centre of |view|.
  void SimulateClick(NSView* view);

  // Send a key press to the first responder.
  void SimulateKeyPress(unichar c);

  void SimulateMouseEnterItemAt(size_t index);
  void SimulateMouseExitItemAt(size_t index);

  // Do a bulk replacement of the items in the grid.
  void ReplaceTestModel(int item_count);

  void DelayForCollectionView();
  void SinkEvents();

  NSButton* GetItemViewAt(size_t index);
  NSCollectionView* GetPageAt(size_t index);
  NSView* GetSelectedView();

  AppListTestViewDelegate* delegate();
  AppListTestModel* model();

  scoped_ptr<AppListTestViewDelegate> delegate_;
  AppsGridController* apps_grid_controller_;

 private:
  MessageLoopForUI message_loop_;

  DISALLOW_COPY_AND_ASSIGN(AppsGridControllerTestHelper);
};

}  // namespace test
}  // namespace app_list

#endif  // UI_APP_LIST_COCOA_TEST_APPS_GRID_CONTROLLER_TEST_HELPER_H_
