// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_nsobject.h"
#import "testing/gtest_mac.h"
#include "ui/app_list/app_list_item_model.h"
#import "ui/app_list/cocoa/apps_grid_controller.h"
#import "ui/app_list/cocoa/apps_grid_view_item.h"
#import "ui/app_list/cocoa/apps_pagination_model_observer.h"
#import "ui/app_list/cocoa/test/apps_grid_controller_test_helper.h"
#include "ui/app_list/test/app_list_test_model.h"
#include "ui/app_list/test/app_list_test_view_delegate.h"

@interface TestPaginationObserver : NSObject<AppsPaginationModelObserver> {
 @private
  int totalPagesChangedCount_;
  int selectedPageChangedCount_;
  int lastNewSelectedPage_;
}

@property(assign, nonatomic) int totalPagesChangedCount;
@property(assign, nonatomic) int selectedPageChangedCount;
@property(assign, nonatomic) int lastNewSelectedPage;

@end

@implementation TestPaginationObserver

@synthesize totalPagesChangedCount = totalPagesChangedCount_;
@synthesize selectedPageChangedCount = selectedPageChangedCount_;
@synthesize lastNewSelectedPage = lastNewSelectedPage_;

- (void)totalPagesChanged {
  ++totalPagesChangedCount_;
}

- (void)selectedPageChanged:(int)newSelected {
  ++selectedPageChangedCount_;
  lastNewSelectedPage_ = newSelected;
}

@end

namespace app_list {
namespace test {

namespace {

class AppsGridControllerTest : public AppsGridControllerTestHelper {
 public:
  AppsGridControllerTest() {}

  virtual void SetUp() OVERRIDE {
    owned_apps_grid_controller_.reset([[AppsGridController alloc] init]);
    [owned_apps_grid_controller_ setDelegate:delegate_.get()];
    AppsGridControllerTestHelper::SetUpWithGridController(
        owned_apps_grid_controller_.get());

    [[test_window() contentView] addSubview:[apps_grid_controller_ view]];
    [test_window() makePretendKeyWindowAndSetFirstResponder:
        [apps_grid_controller_ collectionViewAtPageIndex:0]];
  }

  virtual void TearDown() OVERRIDE {
    owned_apps_grid_controller_.reset();
    AppsGridControllerTestHelper::TearDown();
  }

 private:
  scoped_nsobject<AppsGridController> owned_apps_grid_controller_;

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
  NSButton* button = GetItemViewAt(2);
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

// Test AppsGridPaginationObserver totalPagesChanged().
TEST_F(AppsGridControllerTest, PaginationObserverPagesChanged) {
  scoped_nsobject<TestPaginationObserver> observer(
      [[TestPaginationObserver alloc] init]);
  [apps_grid_controller_ setPaginationObserver:observer];

  // Test totalPagesChanged.
  model()->PopulateApps(kItemsPerPage);
  EXPECT_EQ(0, [observer totalPagesChangedCount]);
  EXPECT_EQ(1u, [apps_grid_controller_ pageCount]);
  model()->PopulateApps(1);
  EXPECT_EQ(1, [observer totalPagesChangedCount]);
  EXPECT_EQ(2u, [apps_grid_controller_ pageCount]);
  ReplaceTestModel(0);
  EXPECT_EQ(2, [observer totalPagesChangedCount]);
  EXPECT_EQ(1u, [apps_grid_controller_ pageCount]);
  ReplaceTestModel(kItemsPerPage * 3 + 1);
  EXPECT_EQ(3, [observer totalPagesChangedCount]);
  EXPECT_EQ(4u, [apps_grid_controller_ pageCount]);

  EXPECT_EQ(0, [observer selectedPageChangedCount]);

  [apps_grid_controller_ setPaginationObserver:nil];
}

// Test AppsGridPaginationObserver selectedPageChanged().
TEST_F(AppsGridControllerTest, PaginationObserverSelectedPageChanged) {
  scoped_nsobject<TestPaginationObserver> observer(
      [[TestPaginationObserver alloc] init]);
  [apps_grid_controller_ setPaginationObserver:observer];
  EXPECT_EQ(0, [[NSAnimationContext currentContext] duration]);

  ReplaceTestModel(kItemsPerPage * 3 + 1);
  EXPECT_EQ(1, [observer totalPagesChangedCount]);
  EXPECT_EQ(4u, [apps_grid_controller_ pageCount]);

  EXPECT_EQ(0, [observer selectedPageChangedCount]);

  [apps_grid_controller_ scrollToPage:1];
  EXPECT_EQ(1, [observer selectedPageChangedCount]);
  EXPECT_EQ(1, [observer lastNewSelectedPage]);

  [apps_grid_controller_ scrollToPage:0];
  EXPECT_EQ(2, [observer selectedPageChangedCount]);
  EXPECT_EQ(0, [observer lastNewSelectedPage]);

  [apps_grid_controller_ scrollToPage:3];
  // Note: with no animations, there is only a single page change. However, with
  // animations we expect multiple updates depending on the rate that the scroll
  // view updates and sends out NSViewBoundsDidChangeNotification.
  EXPECT_EQ(3, [observer selectedPageChangedCount]);
  EXPECT_EQ(3, [observer lastNewSelectedPage]);

  [apps_grid_controller_ setPaginationObserver:nil];
}

}  // namespace test
}  // namespace app_list
