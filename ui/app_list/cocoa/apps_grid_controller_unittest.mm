// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/foundation_util.h"
#include "base/memory/scoped_nsobject.h"
#include "base/message_loop.h"
#import "testing/gtest_mac.h"
#include "ui/app_list/app_list_item_model.h"
#import "ui/app_list/cocoa/apps_grid_controller.h"
#import "ui/app_list/cocoa/apps_grid_view_item.h"
#include "ui/app_list/test/app_list_test_model.h"
#include "ui/app_list/test/app_list_test_view_delegate.h"
#import "ui/base/test/cocoa_test_event_utils.h"
#import "ui/base/test/ui_cocoa_test_helper.h"

namespace {

const size_t kItemsPerPage = 16;

class AppsGridControllerTest : public ui::CocoaTest {
 public:
  AppsGridControllerTest() {
    Init();
  }

  virtual void SetUp() OVERRIDE {
    ui::CocoaTest::SetUp();
    scoped_ptr<app_list::AppListViewDelegate> delegate(
        new app_list::test::AppListTestViewDelegate);
    apps_grid_controller_.reset([[AppsGridController alloc]
        initWithViewDelegate:delegate.Pass()]);

    scoped_ptr<app_list::AppListModel> model(
        new app_list::test::AppListTestModel);
    [apps_grid_controller_ setModel:model.Pass()];

    [[test_window() contentView] addSubview:[apps_grid_controller_ view]];
    [test_window() makePretendKeyWindowAndSetFirstResponder:
        [apps_grid_controller_ collectionViewAtPageIndex:0]];
  }

  virtual void TearDown() OVERRIDE {
    apps_grid_controller_.reset();
    ui::CocoaTest::TearDown();
  }

 protected:
  // Send a click to the test window in the centre of |view|.
  void SimulateClick(NSView* view) {
    std::pair<NSEvent*, NSEvent*> events(
        cocoa_test_event_utils::MouseClickInView(view, 1));
    [NSApp postEvent:events.first atStart:NO];
    [NSApp postEvent:events.second atStart:NO];
  }

  // Send a key press to the first responder.
  void SimulateKeyPress(unichar c) {
    [test_window() keyDown:cocoa_test_event_utils::KeyEventWithCharacter(c)];
  }

  void SimulateMouseEnterItemAt(size_t index) {
    [[apps_grid_controller_ itemAtIndex:index] mouseEntered:
        cocoa_test_event_utils::EnterExitEventWithType(NSMouseEntered)];
  }

  void SimulateMouseExitItemAt(size_t index) {
    [[apps_grid_controller_ itemAtIndex:index] mouseExited:
        cocoa_test_event_utils::EnterExitEventWithType(NSMouseExited)];
  }

  // Do a bulk replacement of the items in the grid.
  void ReplaceTestModel(int item_count) {
    scoped_ptr<app_list::test::AppListTestModel> new_model(
        new app_list::test::AppListTestModel);
    new_model->PopulateApps(item_count);
    [apps_grid_controller_ setModel:new_model.PassAs<app_list::AppListModel>()];
  }

  void DelayForCollectionView() {
    message_loop_.PostDelayedTask(FROM_HERE, MessageLoop::QuitClosure(),
                                  base::TimeDelta::FromMilliseconds(100));
    message_loop_.Run();
  }

  void SinkEvents() {
    message_loop_.PostTask(FROM_HERE, MessageLoop::QuitClosure());
    message_loop_.Run();
  }

  NSButton* GetItemViewAt(size_t index) {
    return [[apps_grid_controller_ itemAtIndex:index] button];
  }

  NSCollectionView* GetPageAt(size_t index) {
    return [apps_grid_controller_ collectionViewAtPageIndex:index];
  }

  // TODO(tapted): Update this to work for selections on other than the first
  // page.
  NSView* GetSelectedView() {
    NSIndexSet* selection = [GetPageAt(0) selectionIndexes];
    if ([selection count]) {
      AppsGridViewItem* item = base::mac::ObjCCastStrict<AppsGridViewItem>(
          [GetPageAt(0) itemAtIndex:[selection firstIndex]]);
      return [item button];
    }

    return nil;
  }

  app_list::test::AppListTestViewDelegate* delegate() {
    return static_cast<app_list::test::AppListTestViewDelegate*>(
        [apps_grid_controller_ delegate]);
  }

  app_list::test::AppListTestModel* model() {
    return static_cast<app_list::test::AppListTestModel*>(
        [apps_grid_controller_ model]);
  }

  scoped_nsobject<AppsGridController> apps_grid_controller_;

 private:
  MessageLoopForUI message_loop_;

  DISALLOW_COPY_AND_ASSIGN(AppsGridControllerTest);
};

}  // namespace

TEST_VIEW(AppsGridControllerTest, [apps_grid_controller_ view]);

// Test showing with an empty model.
TEST_F(AppsGridControllerTest, EmptyModelAndShow) {
  EXPECT_TRUE([[apps_grid_controller_ view] superview]);

  // First page should always exist, even if empty.
  EXPECT_EQ(1u, [apps_grid_controller_ pageCount]);
  EXPECT_EQ(0u, [[GetPageAt(0) content] count]);
  EXPECT_TRUE([GetPageAt(0) superview]);  // The pages container.
  EXPECT_TRUE([[GetPageAt(0) superview] superview]);
}

// Test with a single item.
// This test is disabled in builders until the delay to wait for the collection
// view to load subviews can be removed, or some other solution is found.
TEST_F(AppsGridControllerTest, DISABLED_SingleEntryModel) {
  // We need to "wake up" the NSCollectionView, otherwise it does not
  // immediately update its subviews later in this function.
  // When this test is run by itself, it's enough just to send a keypress (and
  // this delay is not needed).
  DelayForCollectionView();
  EXPECT_EQ(1u, [apps_grid_controller_ pageCount]);
  EXPECT_EQ(0u, [[GetPageAt(0) content] count]);

  model()->PopulateApps(1);
  SinkEvents();
  EXPECT_FALSE([GetPageAt(0) animations]);

  EXPECT_EQ(1u, [[GetPageAt(0) content] count]);
  NSArray* subviews = [GetPageAt(0) subviews];
  EXPECT_EQ(1u, [subviews count]);

  // Note that using GetItemViewAt(0) here also works, and returns non-nil even
  // without the delay, but a "click" on it does not register without the delay.
  NSView* subview = [subviews objectAtIndex:0];

  // Launch the item.
  SimulateClick(subview);
  SinkEvents();
  EXPECT_EQ(1, delegate()->activate_count());
  EXPECT_EQ(std::string("Item 0"), delegate()->last_activated()->title());
}

// Test activating an item on the second page (the 17th item).
TEST_F(AppsGridControllerTest, DISABLED_TwoPageModel) {
  DelayForCollectionView();
  ReplaceTestModel(kItemsPerPage * 2);
  [apps_grid_controller_ scrollToPage:1];

  // The NSScrollView animator ignores the duration configured on the
  // NSAnimationContext (set by CocoaTest::Init), so we need to delay here.
  DelayForCollectionView();
  NSArray* subviews = [GetPageAt(1) subviews];
  NSView* subview = [subviews objectAtIndex:0];
  // Launch the item.
  SimulateClick(subview);
  SinkEvents();
  EXPECT_EQ(1, delegate()->activate_count());
  EXPECT_EQ(std::string("Item 16"), delegate()->last_activated()->title());
}

// Test setModel.
TEST_F(AppsGridControllerTest, ReplaceModel) {
  const size_t kOrigItems = 1;
  const size_t kNewItems = 2;

  model()->PopulateApps(kOrigItems);
  EXPECT_EQ(kOrigItems, [[GetPageAt(0) content] count]);

  ReplaceTestModel(kNewItems);
  EXPECT_EQ(kNewItems, [[GetPageAt(0) content] count]);
}

// Test pagination.
TEST_F(AppsGridControllerTest, Pagination) {
  model()->PopulateApps(1);
  EXPECT_EQ(1u, [apps_grid_controller_ pageCount]);
  EXPECT_EQ(1u, [[GetPageAt(0) content] count]);

  ReplaceTestModel(kItemsPerPage);
  EXPECT_EQ(1u, [apps_grid_controller_ pageCount]);
  EXPECT_EQ(kItemsPerPage, [[GetPageAt(0) content] count]);

  // Test adding an item onto the next page.
  model()->PopulateApps(1);  // Now 17 items.
  EXPECT_EQ(2u, [apps_grid_controller_ pageCount]);
  EXPECT_EQ(kItemsPerPage, [[GetPageAt(0) content] count]);
  EXPECT_EQ(1u, [[GetPageAt(1) content] count]);

  // Test N pages with the last page having one empty spot.
  const size_t kPagesToTest = 3;
  ReplaceTestModel(kPagesToTest * kItemsPerPage - 1);
  EXPECT_EQ(kPagesToTest, [apps_grid_controller_ pageCount]);
  for (size_t page_index = 0; page_index < kPagesToTest - 1; ++page_index) {
    EXPECT_EQ(kItemsPerPage, [[GetPageAt(page_index) content] count]);
  }
  EXPECT_EQ(kItemsPerPage - 1, [[GetPageAt(kPagesToTest - 1) content] count]);

  // Test removing pages.
  ReplaceTestModel(1);
  EXPECT_EQ(1u, [apps_grid_controller_ pageCount]);
  EXPECT_EQ(1u, [[GetPageAt(0) content] count]);
}

// Tests basic left-right keyboard navigation on the first page, later tests
// will test keyboard navigation across pages and other corner cases.
TEST_F(AppsGridControllerTest, DISABLED_FirstPageKeyboardNavigation) {
  model()->PopulateApps(3);
  SinkEvents();
  EXPECT_EQ(3u, [[GetPageAt(0) content] count]);

  SimulateKeyPress(NSRightArrowFunctionKey);
  SinkEvents();
  EXPECT_EQ(GetSelectedView(), GetItemViewAt(0));

  SimulateKeyPress(NSRightArrowFunctionKey);
  SinkEvents();
  EXPECT_EQ(GetSelectedView(), GetItemViewAt(1));

  SimulateKeyPress(NSLeftArrowFunctionKey);
  SinkEvents();
  EXPECT_EQ(GetSelectedView(), GetItemViewAt(0));

  // Go to the last item, and launch it.
  SimulateKeyPress(NSRightArrowFunctionKey);
  SimulateKeyPress(NSRightArrowFunctionKey);
  [apps_grid_controller_ activateSelection];
  SinkEvents();
  EXPECT_EQ(GetSelectedView(), GetItemViewAt(2));
  EXPECT_EQ(1, delegate()->activate_count());
  EXPECT_EQ(std::string("Item 2"), delegate()->last_activated()->title());
}

// Test runtime updates: adding items, changing titles and icons.
TEST_F(AppsGridControllerTest, ModelUpdates) {
  model()->PopulateApps(2);
  EXPECT_EQ(2u, [[GetPageAt(0) content] count]);

  // Add an item (PopulateApps will create a duplicate "Item 0").
  model()->PopulateApps(1);
  EXPECT_EQ(3u, [[GetPageAt(0) content] count]);
  NSButton* button = base::mac::ObjCCastStrict<NSButton>(GetItemViewAt(2));
  EXPECT_NSEQ(@"Item 0", [button title]);

  // Update the title via the ItemModelObserver.
  app_list::AppListItemModel* item_model = model()->apps()->GetItemAt(2);
  item_model->SetTitle("UpdatedItem");
  EXPECT_NSEQ(@"UpdatedItem", [button title]);

  // Update the icon, test by changing size.
  NSSize icon_size = [[button image] size];
  EXPECT_EQ(0, icon_size.width);
  EXPECT_EQ(0, icon_size.height);

  SkBitmap bitmap;
  const int kTestImageSize = 10;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, kTestImageSize, kTestImageSize);
  item_model->SetIcon(gfx::ImageSkia::CreateFrom1xBitmap(bitmap), false);
  icon_size = [[button image] size];
  EXPECT_EQ(kTestImageSize, icon_size.width);
  EXPECT_EQ(kTestImageSize, icon_size.height);
}

// Test mouseover selection.
TEST_F(AppsGridControllerTest, MouseoverSelects) {
  model()->PopulateApps(2);
  EXPECT_EQ(nil, GetSelectedView());

  // Test entering and exiting the first item.
  SimulateMouseEnterItemAt(0);
  EXPECT_EQ(GetItemViewAt(0), GetSelectedView());
  SimulateMouseExitItemAt(0);
  EXPECT_EQ(nil, GetSelectedView());

  // AppKit doesn't guarantee the order, so test moving between items.
  SimulateMouseEnterItemAt(0);
  EXPECT_EQ(GetItemViewAt(0), GetSelectedView());
  SimulateMouseEnterItemAt(1);
  EXPECT_EQ(GetItemViewAt(1), GetSelectedView());
  SimulateMouseExitItemAt(0);
  EXPECT_EQ(GetItemViewAt(1), GetSelectedView());
  SimulateMouseExitItemAt(1);
  EXPECT_EQ(nil, GetSelectedView());
}
