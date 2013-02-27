// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/foundation_util.h"
#include "base/memory/scoped_nsobject.h"
#include "base/message_loop.h"
#import "testing/gtest_mac.h"
#include "ui/app_list/app_list_item_model.h"
#import "ui/app_list/cocoa/apps_grid_controller.h"
#include "ui/app_list/test/app_list_test_model.h"
#include "ui/app_list/test/app_list_test_view_delegate.h"
#import "ui/base/test/cocoa_test_event_utils.h"
#import "ui/base/test/ui_cocoa_test_helper.h"

namespace {

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
        [apps_grid_controller_ collectionView]];
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

  void DelayForCollectionView() {
    message_loop_.PostDelayedTask(FROM_HERE, MessageLoop::QuitClosure(),
                                  base::TimeDelta::FromMilliseconds(100));
    message_loop_.Run();
  }

  void SinkEvents() {
    message_loop_.PostTask(FROM_HERE, MessageLoop::QuitClosure());
    message_loop_.Run();
  }

  NSView* GetItemViewAt(size_t index) {
    if (index < [[[apps_grid_controller_ collectionView] content] count]) {
      NSCollectionViewItem* item = [[apps_grid_controller_ collectionView]
          itemAtIndex:index];
      return [item view];
    }

    return nil;
  }

  NSView* GetSelectedView() {
    NSIndexSet* selection =
        [[apps_grid_controller_ collectionView] selectionIndexes];
    if ([selection count]) {
      NSCollectionViewItem* item = [[apps_grid_controller_ collectionView]
          itemAtIndex:[selection firstIndex]];
      return [item view];
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
  EXPECT_TRUE([[apps_grid_controller_ collectionView] superview]);
  EXPECT_EQ(0u, [[[apps_grid_controller_ collectionView] content] count]);
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
  EXPECT_EQ(0u, [[[apps_grid_controller_ collectionView] content] count]);

  model()->PopulateApps(1);
  SinkEvents();
  EXPECT_FALSE([[apps_grid_controller_ collectionView] animations]);

  EXPECT_EQ(1u, [[[apps_grid_controller_ collectionView] content] count]);
  NSArray* subviews = [[apps_grid_controller_ collectionView] subviews];
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

// Test setModel.
TEST_F(AppsGridControllerTest, ReplaceModel) {
  const size_t kOrigItems = 1;
  const size_t kNewItems = 2;

  model()->PopulateApps(kOrigItems);
  EXPECT_EQ(kOrigItems,
            [[[apps_grid_controller_ collectionView] content] count]);

  scoped_ptr<app_list::test::AppListTestModel> new_model(
      new app_list::test::AppListTestModel);
  new_model->PopulateApps(kNewItems);
  [apps_grid_controller_ setModel:new_model.PassAs<app_list::AppListModel>()];
  EXPECT_EQ(kNewItems,
            [[[apps_grid_controller_ collectionView] content] count]);
}

// Tests basic left-right keyboard navigation on the first page, later tests
// will test keyboard navigation across pages and other corner cases.
TEST_F(AppsGridControllerTest, DISABLED_FirstPageKeyboardNavigation) {
  model()->PopulateApps(3);
  SinkEvents();
  EXPECT_EQ(3u, [[[apps_grid_controller_ collectionView] content] count]);

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
  EXPECT_EQ(2u, [[[apps_grid_controller_ collectionView] content] count]);

  // Add an item (PopulateApps will create a duplicate "Item 0").
  model()->PopulateApps(1);
  EXPECT_EQ(3u, [[[apps_grid_controller_ collectionView] content] count]);
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
  item_model->SetIcon(gfx::ImageSkia::CreateFrom1xBitmap(bitmap));
  icon_size = [[button image] size];
  EXPECT_EQ(kTestImageSize, icon_size.width);
  EXPECT_EQ(kTestImageSize, icon_size.height);
}
