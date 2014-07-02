// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/utf_string_conversions.h"
#include "skia/ext/skia_utils_mac.h"
#import "testing/gtest_mac.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_item.h"
#import "ui/app_list/cocoa/apps_collection_view_drag_manager.h"
#import "ui/app_list/cocoa/apps_grid_controller.h"
#import "ui/app_list/cocoa/apps_grid_view_item.h"
#import "ui/app_list/cocoa/apps_pagination_model_observer.h"
#import "ui/app_list/cocoa/test/apps_grid_controller_test_helper.h"
#include "ui/app_list/test/app_list_test_model.h"
#include "ui/app_list/test/app_list_test_view_delegate.h"
#include "ui/base/models/simple_menu_model.h"
#import "ui/events/test/cocoa_test_event_utils.h"

@interface TestPaginationObserver : NSObject<AppsPaginationModelObserver> {
 @private
  NSInteger hoveredSegmentForTest_;
  int totalPagesChangedCount_;
  int selectedPageChangedCount_;
  int lastNewSelectedPage_;
  bool visibilityDidChange_;
}

@property(assign, nonatomic) NSInteger hoveredSegmentForTest;
@property(assign, nonatomic) int totalPagesChangedCount;
@property(assign, nonatomic) int selectedPageChangedCount;
@property(assign, nonatomic) int lastNewSelectedPage;

- (bool)readVisibilityDidChange;

@end

@implementation TestPaginationObserver

@synthesize hoveredSegmentForTest = hoveredSegmentForTest_;
@synthesize totalPagesChangedCount = totalPagesChangedCount_;
@synthesize selectedPageChangedCount = selectedPageChangedCount_;
@synthesize lastNewSelectedPage = lastNewSelectedPage_;

- (id)init {
  if ((self = [super init]))
    hoveredSegmentForTest_ = -1;

  return self;
}

- (bool)readVisibilityDidChange {
  bool truth = visibilityDidChange_;
  visibilityDidChange_ = false;
  return truth;
}

- (void)totalPagesChanged {
  ++totalPagesChangedCount_;
}

- (void)selectedPageChanged:(int)newSelected {
  ++selectedPageChangedCount_;
  lastNewSelectedPage_ = newSelected;
}

- (void)pageVisibilityChanged {
  visibilityDidChange_ = true;
}

- (NSInteger)pagerSegmentAtLocation:(NSPoint)locationInWindow {
  return hoveredSegmentForTest_;
}

@end

namespace app_list {
namespace test {

namespace {

class AppsGridControllerTest : public AppsGridControllerTestHelper {
 public:
  AppsGridControllerTest() {}

  AppListTestViewDelegate* delegate() {
    return owned_delegate_.get();
  }

  NSColor* ButtonTitleColorAt(size_t index) {
    NSDictionary* attributes =
        [[[GetItemViewAt(index) cell] attributedTitle] attributesAtIndex:0
                                                          effectiveRange:NULL];
    return [attributes objectForKey:NSForegroundColorAttributeName];
  }

  virtual void SetUp() OVERRIDE {
    owned_apps_grid_controller_.reset([[AppsGridController alloc] init]);
    owned_delegate_.reset(new AppListTestViewDelegate);
    [owned_apps_grid_controller_ setDelegate:owned_delegate_.get()];
    AppsGridControllerTestHelper::SetUpWithGridController(
        owned_apps_grid_controller_.get());

    [[test_window() contentView] addSubview:[apps_grid_controller_ view]];
    [test_window() makePretendKeyWindowAndSetFirstResponder:
        [apps_grid_controller_ collectionViewAtPageIndex:0]];
  }

  virtual void TearDown() OVERRIDE {
    [owned_apps_grid_controller_ setDelegate:NULL];
    owned_apps_grid_controller_.reset();
    AppsGridControllerTestHelper::TearDown();
  }

  void ReplaceTestModel(int item_count) {
    // Clear the delegate before reseting and destroying the model.
    [owned_apps_grid_controller_ setDelegate:NULL];

    owned_delegate_->ReplaceTestModel(item_count);
    [owned_apps_grid_controller_ setDelegate:owned_delegate_.get()];
  }

  AppListTestModel* model() { return owned_delegate_->GetTestModel(); }

 private:
  base::scoped_nsobject<AppsGridController> owned_apps_grid_controller_;
  scoped_ptr<AppListTestViewDelegate> owned_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AppsGridControllerTest);
};

class AppListItemWithMenu : public AppListItem {
 public:
  explicit AppListItemWithMenu(const std::string& id)
      : AppListItem(id),
        menu_model_(NULL),
        menu_ready_(true) {
    SetName(id);
    menu_model_.AddItem(0, base::UTF8ToUTF16("Menu For: " + id));
  }

  void SetMenuReadyForTesting(bool ready) {
    menu_ready_ = ready;
  }

  virtual ui::MenuModel* GetContextMenuModel() OVERRIDE {
    if (!menu_ready_)
      return NULL;

    return &menu_model_;
  }

 private:
  ui::SimpleMenuModel menu_model_;
  bool menu_ready_;

  DISALLOW_COPY_AND_ASSIGN(AppListItemWithMenu);
};

// Generate a mouse event at the centre of the view in |page| with the given
// |index_in_page| that can be used to initiate, update and complete drag
// operations.
NSEvent* MouseEventInCell(NSCollectionView* page, size_t index_in_page) {
  NSRect cell_rect = [page frameForItemAtIndex:index_in_page];
  NSPoint point_in_view = NSMakePoint(NSMidX(cell_rect), NSMidY(cell_rect));
  NSPoint point_in_window = [page convertPoint:point_in_view
                                        toView:nil];
  return cocoa_test_event_utils::LeftMouseDownAtPoint(point_in_window);
}

NSEvent* MouseEventForScroll(NSView* view, CGFloat relative_x) {
  NSRect view_rect = [view frame];
  NSPoint point_in_view = NSMakePoint(NSMidX(view_rect), NSMidY(view_rect));
  point_in_view.x += point_in_view.x * relative_x;
  NSPoint point_in_window = [view convertPoint:point_in_view
                                        toView:nil];
  return cocoa_test_event_utils::LeftMouseDownAtPoint(point_in_window);
}

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
  EXPECT_EQ(1, model()->activate_count());
  ASSERT_TRUE(model()->last_activated());
  EXPECT_EQ(std::string("Item 0"), model()->last_activated()->name());
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
  EXPECT_EQ(1, model()->activate_count());
  ASSERT_TRUE(model()->last_activated());
  EXPECT_EQ(std::string("Item 16"), model()->last_activated()->name());
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

// Tests that selecting an item changes the text color correctly.
TEST_F(AppsGridControllerTest, SelectionChangesTextColor) {
  model()->PopulateApps(2);
  [apps_grid_controller_ selectItemAtIndex:0];
  EXPECT_NSEQ(ButtonTitleColorAt(0),
              gfx::SkColorToSRGBNSColor(app_list::kGridTitleHoverColor));
  EXPECT_NSEQ(ButtonTitleColorAt(1),
              gfx::SkColorToSRGBNSColor(app_list::kGridTitleColor));

  [apps_grid_controller_ selectItemAtIndex:1];
  EXPECT_NSEQ(ButtonTitleColorAt(0),
              gfx::SkColorToSRGBNSColor(app_list::kGridTitleColor));
  EXPECT_NSEQ(ButtonTitleColorAt(1),
              gfx::SkColorToSRGBNSColor(app_list::kGridTitleHoverColor));
}

// Tests basic keyboard navigation on the first page.
TEST_F(AppsGridControllerTest, FirstPageKeyboardNavigation) {
  model()->PopulateApps(kItemsPerPage - 2);
  EXPECT_EQ(kItemsPerPage - 2, [[GetPageAt(0) content] count]);

  SimulateKeyAction(@selector(moveRight:));
  EXPECT_EQ(0u, [apps_grid_controller_ selectedItemIndex]);

  SimulateKeyAction(@selector(moveRight:));
  EXPECT_EQ(1u, [apps_grid_controller_ selectedItemIndex]);

  SimulateKeyAction(@selector(moveDown:));
  EXPECT_EQ(5u, [apps_grid_controller_ selectedItemIndex]);

  SimulateKeyAction(@selector(moveLeft:));
  EXPECT_EQ(4u, [apps_grid_controller_ selectedItemIndex]);

  SimulateKeyAction(@selector(moveUp:));
  EXPECT_EQ(0u, [apps_grid_controller_ selectedItemIndex]);

  // Go to the third item, and launch it.
  SimulateKeyAction(@selector(moveRight:));
  SimulateKeyAction(@selector(moveRight:));
  EXPECT_EQ(2u, [apps_grid_controller_ selectedItemIndex]);
  SimulateKeyAction(@selector(insertNewline:));
  EXPECT_EQ(1, model()->activate_count());
  ASSERT_TRUE(model()->last_activated());
  EXPECT_EQ(std::string("Item 2"), model()->last_activated()->name());
}

// Tests keyboard navigation across pages.
TEST_F(AppsGridControllerTest, CrossPageKeyboardNavigation) {
  model()->PopulateApps(kItemsPerPage + 10);
  EXPECT_EQ(kItemsPerPage, [[GetPageAt(0) content] count]);
  EXPECT_EQ(10u, [[GetPageAt(1) content] count]);

  // Moving Left, Up, or PageUp from the top-left corner of the first page does
  // nothing.
  [apps_grid_controller_ selectItemAtIndex:0];
  SimulateKeyAction(@selector(moveLeft:));
  EXPECT_EQ(0u, [apps_grid_controller_ selectedItemIndex]);
  SimulateKeyAction(@selector(moveUp:));
  EXPECT_EQ(0u, [apps_grid_controller_ selectedItemIndex]);
  SimulateKeyAction(@selector(scrollPageUp:));
  EXPECT_EQ(0u, [apps_grid_controller_ selectedItemIndex]);

  // Moving Right from the right side goes to the next page. Moving Left goes
  // back to the first page.
  [apps_grid_controller_ selectItemAtIndex:3];
  SimulateKeyAction(@selector(moveRight:));
  EXPECT_EQ(1u, [apps_grid_controller_ visiblePage]);
  EXPECT_EQ(kItemsPerPage, [apps_grid_controller_ selectedItemIndex]);
  SimulateKeyAction(@selector(moveLeft:));
  EXPECT_EQ(0u, [apps_grid_controller_ visiblePage]);
  EXPECT_EQ(3u, [apps_grid_controller_ selectedItemIndex]);

  // Moving Down from the bottom does nothing.
  [apps_grid_controller_ selectItemAtIndex:13];
  EXPECT_EQ(0u, [apps_grid_controller_ visiblePage]);
  SimulateKeyAction(@selector(moveDown:));
  EXPECT_EQ(13u, [apps_grid_controller_ selectedItemIndex]);

  // Moving Right into a non-existent square on the next page will select the
  // last item.
  [apps_grid_controller_ selectItemAtIndex:15];
  EXPECT_EQ(0u, [apps_grid_controller_ visiblePage]);
  SimulateKeyAction(@selector(moveRight:));
  EXPECT_EQ(1u, [apps_grid_controller_ visiblePage]);
  EXPECT_EQ(kItemsPerPage + 9, [apps_grid_controller_ selectedItemIndex]);

  // PageDown and PageUp switches pages while maintaining the same selection
  // position.
  [apps_grid_controller_ selectItemAtIndex:6];
  EXPECT_EQ(0u, [apps_grid_controller_ visiblePage]);
  SimulateKeyAction(@selector(scrollPageDown:));
  EXPECT_EQ(1u, [apps_grid_controller_ visiblePage]);
  EXPECT_EQ(kItemsPerPage + 6, [apps_grid_controller_ selectedItemIndex]);
  SimulateKeyAction(@selector(scrollPageUp:));
  EXPECT_EQ(0u, [apps_grid_controller_ visiblePage]);
  EXPECT_EQ(6u, [apps_grid_controller_ selectedItemIndex]);

  // PageDown into a non-existent square on the next page will select the last
  // item.
  [apps_grid_controller_ selectItemAtIndex:11];
  EXPECT_EQ(0u, [apps_grid_controller_ visiblePage]);
  SimulateKeyAction(@selector(scrollPageDown:));
  EXPECT_EQ(1u, [apps_grid_controller_ visiblePage]);
  EXPECT_EQ(kItemsPerPage + 9, [apps_grid_controller_ selectedItemIndex]);

  // Moving Right, Down, or PageDown from the bottom-right corner of the last
  // page (not the last item) does nothing.
  [apps_grid_controller_ selectItemAtIndex:kItemsPerPage + 9];
  EXPECT_EQ(1u, [apps_grid_controller_ visiblePage]);
  SimulateKeyAction(@selector(moveRight:));
  EXPECT_EQ(kItemsPerPage + 9, [apps_grid_controller_ selectedItemIndex]);
  SimulateKeyAction(@selector(moveDown:));
  EXPECT_EQ(kItemsPerPage + 9, [apps_grid_controller_ selectedItemIndex]);
  SimulateKeyAction(@selector(scrollPageDown:));
  EXPECT_EQ(kItemsPerPage + 9, [apps_grid_controller_ selectedItemIndex]);

  // After page switch, arrow keys select first item on current page.
  [apps_grid_controller_ scrollToPage:0];
  [apps_grid_controller_ scrollToPage:1];
  EXPECT_EQ(NSNotFound, [apps_grid_controller_ selectedItemIndex]);
  SimulateKeyAction(@selector(moveUp:));
  EXPECT_EQ(kItemsPerPage, [apps_grid_controller_ selectedItemIndex]);
}

// Highlighting an item should cause the page it's on to be visible.
TEST_F(AppsGridControllerTest, EnsureHighlightedVisible) {
  model()->PopulateApps(3 * kItemsPerPage);
  EXPECT_EQ(kItemsPerPage, [[GetPageAt(2) content] count]);

  // First and last items of first page.
  [apps_grid_controller_ selectItemAtIndex:0];
  EXPECT_EQ(0u, [apps_grid_controller_ visiblePage]);
  [apps_grid_controller_ selectItemAtIndex:kItemsPerPage - 1];
  EXPECT_EQ(0u, [apps_grid_controller_ visiblePage]);

  // First item of second page.
  [apps_grid_controller_ selectItemAtIndex:kItemsPerPage + 1];
  EXPECT_EQ(1u, [apps_grid_controller_ visiblePage]);

  // Last item in model.
  [apps_grid_controller_ selectItemAtIndex:3 * kItemsPerPage - 1];
  EXPECT_EQ(2u, [apps_grid_controller_ visiblePage]);
}

// Test runtime updates: adding items, removing items, and moving items (e.g. in
// response to app install, uninstall, and chrome sync changes. Also test
// changing titles and icons.
TEST_F(AppsGridControllerTest, ModelUpdate) {
  model()->PopulateApps(2);
  EXPECT_EQ(2u, [[GetPageAt(0) content] count]);
  EXPECT_EQ(std::string("|Item 0,Item 1|"), GetViewContent());

  // Add an item (PopulateApps will create a new "Item 2").
  model()->PopulateApps(1);
  EXPECT_EQ(3u, [[GetPageAt(0) content] count]);
  NSButton* button = GetItemViewAt(2);
  EXPECT_NSEQ(@"Item 2", [button title]);
  EXPECT_EQ(std::string("|Item 0,Item 1,Item 2|"), GetViewContent());

  // Update the title via the ItemModelObserver.
  app_list::AppListItem* item_model =
      model()->top_level_item_list()->item_at(2);
  model()->SetItemName(item_model, "UpdatedItem");
  EXPECT_NSEQ(@"UpdatedItem", [button title]);

  // Test icon updates through the model observer by ensuring the icon changes.
  item_model->SetIcon(gfx::ImageSkia(), false);
  NSSize icon_size = [[button image] size];
  EXPECT_EQ(0, icon_size.width);
  EXPECT_EQ(0, icon_size.height);

  SkBitmap bitmap;
  const int kTestImageSize = 10;
  const int kTargetImageSize = 48;
  bitmap.setInfo(SkImageInfo::MakeN32Premul(kTestImageSize, kTestImageSize));
  item_model->SetIcon(gfx::ImageSkia::CreateFrom1xBitmap(bitmap), false);
  icon_size = [[button image] size];
  // Icon should always be resized to 48x48.
  EXPECT_EQ(kTargetImageSize, icon_size.width);
  EXPECT_EQ(kTargetImageSize, icon_size.height);
}

TEST_F(AppsGridControllerTest, ModelAdd) {
  model()->PopulateApps(2);
  EXPECT_EQ(2u, [[GetPageAt(0) content] count]);
  EXPECT_EQ(std::string("|Item 0,Item 1|"), GetViewContent());

  app_list::AppListItemList* item_list = model()->top_level_item_list();

  model()->CreateAndAddItem("Item 2");
  ASSERT_EQ(3u, item_list->item_count());
  EXPECT_EQ(3u, [apps_grid_controller_ itemCount]);
  EXPECT_EQ(std::string("|Item 0,Item 1,Item 2|"), GetViewContent());

  // Test adding an item whose position is in the middle.
  app_list::AppListItem* item0 = item_list->item_at(0);
  app_list::AppListItem* item1 = item_list->item_at(1);
  app_list::AppListItem* item3 = model()->CreateItem("Item Three");
  model()->AddItem(item3);
  item_list->SetItemPosition(
      item3, item0->position().CreateBetween(item1->position()));
  EXPECT_EQ(4u, [apps_grid_controller_ itemCount]);
  EXPECT_EQ(std::string("|Item 0,Item Three,Item 1,Item 2|"), GetViewContent());
}

TEST_F(AppsGridControllerTest, ModelMove) {
  model()->PopulateApps(3);
  EXPECT_EQ(3u, [[GetPageAt(0) content] count]);
  EXPECT_EQ(std::string("|Item 0,Item 1,Item 2|"), GetViewContent());

  // Test swapping items (e.g. rearranging via sync).
  model()->top_level_item_list()->MoveItem(1, 2);
  EXPECT_EQ(std::string("|Item 0,Item 2,Item 1|"), GetViewContent());
}

TEST_F(AppsGridControllerTest, ModelRemove) {
  model()->PopulateApps(3);
  EXPECT_EQ(3u, [[GetPageAt(0) content] count]);
  EXPECT_EQ(std::string("|Item 0,Item 1,Item 2|"), GetViewContent());

  // Test removing an item at the end.
  model()->DeleteItem("Item 2");
  EXPECT_EQ(2u, [apps_grid_controller_ itemCount]);
  EXPECT_EQ(std::string("|Item 0,Item 1|"), GetViewContent());

  // Test removing in the middle.
  model()->CreateAndAddItem("Item 2");
  EXPECT_EQ(3u, [apps_grid_controller_ itemCount]);
  EXPECT_EQ(std::string("|Item 0,Item 1,Item 2|"), GetViewContent());
  model()->DeleteItem("Item 1");
  EXPECT_EQ(2u, [apps_grid_controller_ itemCount]);
  EXPECT_EQ(std::string("|Item 0,Item 2|"), GetViewContent());
}

TEST_F(AppsGridControllerTest, ModelRemoveSeveral) {
  model()->PopulateApps(3);
  EXPECT_EQ(3u, [[GetPageAt(0) content] count]);
  EXPECT_EQ(std::string("|Item 0,Item 1,Item 2|"), GetViewContent());

  // Test removing multiple items via the model.
  model()->DeleteItem("Item 0");
  model()->DeleteItem("Item 1");
  model()->DeleteItem("Item 2");
  EXPECT_EQ(0u, [apps_grid_controller_ itemCount]);
  EXPECT_EQ(std::string("||"), GetViewContent());
}

TEST_F(AppsGridControllerTest, ModelRemovePage) {
  app_list::AppListItemList* item_list = model()->top_level_item_list();

  model()->PopulateApps(kItemsPerPage + 1);
  ASSERT_EQ(kItemsPerPage + 1, item_list->item_count());
  EXPECT_EQ(kItemsPerPage + 1, [apps_grid_controller_ itemCount]);
  EXPECT_EQ(2u, [apps_grid_controller_ pageCount]);

  // Test removing the last item when there is one item on the second page.
  app_list::AppListItem* last_item = item_list->item_at(kItemsPerPage);
  model()->DeleteItem(last_item->id());
  EXPECT_EQ(kItemsPerPage, item_list->item_count());
  EXPECT_EQ(kItemsPerPage, [apps_grid_controller_ itemCount]);
  EXPECT_EQ(1u, [apps_grid_controller_ pageCount]);
}

// Test install progress bars, and install flow with item highlighting.
TEST_F(AppsGridControllerTest, ItemInstallProgress) {
  ReplaceTestModel(kItemsPerPage + 1);
  EXPECT_EQ(2u, [apps_grid_controller_ pageCount]);
  EXPECT_EQ(0u, [apps_grid_controller_ visiblePage]);
  app_list::AppListItem* item_model =
      model()->top_level_item_list()->item_at(kItemsPerPage);

  // Highlighting an item should activate the page it is on.
  item_model->SetHighlighted(true);
  EXPECT_EQ(1u, [apps_grid_controller_ visiblePage]);

  // Clearing a highlight stays on the current page.
  [apps_grid_controller_ scrollToPage:0];
  EXPECT_EQ(0u, [apps_grid_controller_ visiblePage]);
  item_model->SetHighlighted(false);
  EXPECT_EQ(0u, [apps_grid_controller_ visiblePage]);

  // Starting install should add a progress bar, and temporarily clear the
  // button title.
  NSButton* button = GetItemViewAt(kItemsPerPage);
  NSView* containerView = [button superview];
  EXPECT_EQ(1u, [[containerView subviews] count]);
  EXPECT_NSEQ(@"Item 16", [button title]);
  item_model->SetHighlighted(true);
  item_model->SetIsInstalling(true);
  EXPECT_EQ(1u, [apps_grid_controller_ visiblePage]);

  EXPECT_EQ(2u, [[containerView subviews] count]);
  EXPECT_NSEQ(@"", [button title]);
  NSProgressIndicator* progressIndicator =
      [[containerView subviews] objectAtIndex:1];
  EXPECT_FALSE([progressIndicator isIndeterminate]);
  EXPECT_EQ(0.0, [progressIndicator doubleValue]);

  // Updating the progress in the model should update the progress bar.
  item_model->SetPercentDownloaded(50);
  EXPECT_EQ(50.0, [progressIndicator doubleValue]);

  // Two things can be installing simultaneously. When one starts or completes
  // the model builder will ask for the item to be highlighted.
  app_list::AppListItem* alternate_item_model =
      model()->top_level_item_list()->item_at(0);
  item_model->SetHighlighted(false);
  alternate_item_model->SetHighlighted(true);
  EXPECT_EQ(0u, [apps_grid_controller_ visiblePage]);

  // Update the first item (page doesn't change on updates).
  item_model->SetPercentDownloaded(100);
  EXPECT_EQ(100.0, [progressIndicator doubleValue]);
  EXPECT_EQ(0u, [apps_grid_controller_ visiblePage]);

  // A percent of -1 indicates the download is complete and the unpack/install
  // process has started.
  item_model->SetPercentDownloaded(-1);
  EXPECT_TRUE([progressIndicator isIndeterminate]);

  // Completing install removes the progress bar, and restores the title.
  // ExtensionAppModelBuilder will reload the ExtensionAppItem, which also
  // highlights. Do the same here.
  alternate_item_model->SetHighlighted(false);
  item_model->SetHighlighted(true);
  item_model->SetIsInstalling(false);
  EXPECT_EQ(1u, [[containerView subviews] count]);
  EXPECT_NSEQ(@"Item 16", [button title]);
  EXPECT_EQ(1u, [apps_grid_controller_ visiblePage]);

  // Things should cleanup OK with |alternate_item_model| left installing.
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
  base::scoped_nsobject<TestPaginationObserver> observer(
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

  EXPECT_FALSE([observer readVisibilityDidChange]);
  EXPECT_EQ(0, [observer selectedPageChangedCount]);

  [apps_grid_controller_ setPaginationObserver:nil];
}

// Test AppsGridPaginationObserver selectedPageChanged().
TEST_F(AppsGridControllerTest, PaginationObserverSelectedPageChanged) {
  base::scoped_nsobject<TestPaginationObserver> observer(
      [[TestPaginationObserver alloc] init]);
  [apps_grid_controller_ setPaginationObserver:observer];
  EXPECT_EQ(0, [[NSAnimationContext currentContext] duration]);

  ReplaceTestModel(kItemsPerPage * 3 + 1);
  EXPECT_EQ(1, [observer totalPagesChangedCount]);
  EXPECT_EQ(4u, [apps_grid_controller_ pageCount]);

  EXPECT_FALSE([observer readVisibilityDidChange]);
  EXPECT_EQ(0, [observer selectedPageChangedCount]);

  [apps_grid_controller_ scrollToPage:1];
  EXPECT_EQ(1, [observer selectedPageChangedCount]);
  EXPECT_EQ(1, [observer lastNewSelectedPage]);
  EXPECT_TRUE([observer readVisibilityDidChange]);
  EXPECT_FALSE([observer readVisibilityDidChange]);  // Testing test behaviour.
  EXPECT_EQ(0.0, [apps_grid_controller_ visiblePortionOfPage:0]);
  EXPECT_EQ(1.0, [apps_grid_controller_ visiblePortionOfPage:1]);
  EXPECT_EQ(0.0, [apps_grid_controller_ visiblePortionOfPage:2]);
  EXPECT_EQ(0.0, [apps_grid_controller_ visiblePortionOfPage:3]);

  [apps_grid_controller_ scrollToPage:0];
  EXPECT_EQ(2, [observer selectedPageChangedCount]);
  EXPECT_EQ(0, [observer lastNewSelectedPage]);
  EXPECT_TRUE([observer readVisibilityDidChange]);
  EXPECT_EQ(1.0, [apps_grid_controller_ visiblePortionOfPage:0]);
  EXPECT_EQ(0.0, [apps_grid_controller_ visiblePortionOfPage:1]);

  [apps_grid_controller_ scrollToPage:3];
  // Note: with no animations, there is only a single page change. However, with
  // animations we expect multiple updates depending on the rate that the scroll
  // view updates and sends out NSViewBoundsDidChangeNotification.
  EXPECT_EQ(3, [observer selectedPageChangedCount]);
  EXPECT_EQ(3, [observer lastNewSelectedPage]);
  EXPECT_TRUE([observer readVisibilityDidChange]);
  EXPECT_EQ(0.0, [apps_grid_controller_ visiblePortionOfPage:0]);
  EXPECT_EQ(1.0, [apps_grid_controller_ visiblePortionOfPage:3]);

  [apps_grid_controller_ setPaginationObserver:nil];
}

// Test basic item moves with two items; swapping them around, dragging outside
// of the view bounds, and dragging on the background.
TEST_F(AppsGridControllerTest, DragAndDropSimple) {
  model()->PopulateApps(2);
  NSCollectionView* page = [apps_grid_controller_ collectionViewAtPageIndex:0];
  NSEvent* mouse_at_cell_0 = MouseEventInCell(page, 0);
  NSEvent* mouse_at_cell_1 = MouseEventInCell(page, 1);
  NSEvent* mouse_at_page_centre = MouseEventInCell(page, 6);
  NSEvent* mouse_off_page = MouseEventInCell(page, kItemsPerPage * 2);

  const std::string kOrdered = "Item 0,Item 1";
  const std::string kSwapped = "Item 1,Item 0";
  const std::string kOrderedView = "|Item 0,Item 1|";
  const std::string kSwappedView = "|Item 1,Item 0|";

  EXPECT_EQ(kOrdered, model()->GetModelContent());
  EXPECT_EQ(kOrderedView, GetViewContent());
  AppsCollectionViewDragManager* drag_manager =
      [apps_grid_controller_ dragManager];

  // Drag first item over the second item and release.
  [drag_manager onMouseDownInPage:page
                        withEvent:mouse_at_cell_0];
  [drag_manager onMouseDragged:mouse_at_cell_1];
  EXPECT_EQ(kOrdered, model()->GetModelContent());
  EXPECT_EQ(kSwappedView, GetViewContent());  // View swaps first.
  [drag_manager onMouseUp:mouse_at_cell_1];
  EXPECT_EQ(kSwapped, model()->GetModelContent());
  EXPECT_EQ(kSwappedView, GetViewContent());

  // Drag item back.
  [drag_manager onMouseDownInPage:page
                        withEvent:mouse_at_cell_1];
  [drag_manager onMouseDragged:mouse_at_cell_0];
  EXPECT_EQ(kSwapped, model()->GetModelContent());
  EXPECT_EQ(kOrderedView, GetViewContent());
  [drag_manager onMouseUp:mouse_at_cell_0];
  EXPECT_EQ(kOrdered, model()->GetModelContent());
  EXPECT_EQ(kOrderedView, GetViewContent());

  // Drag first item to centre of view (should put in last place).
  [drag_manager onMouseDownInPage:page
                        withEvent:mouse_at_cell_0];
  [drag_manager onMouseDragged:mouse_at_page_centre];
  EXPECT_EQ(kOrdered, model()->GetModelContent());
  EXPECT_EQ(kSwappedView, GetViewContent());
  [drag_manager onMouseUp:mouse_at_page_centre];
  EXPECT_EQ(kSwapped, model()->GetModelContent());
  EXPECT_EQ(kSwappedView, GetViewContent());

  // Drag item to centre again (should leave it in the last place).
  [drag_manager onMouseDownInPage:page
                        withEvent:mouse_at_cell_1];
  [drag_manager onMouseDragged:mouse_at_page_centre];
  EXPECT_EQ(kSwapped, model()->GetModelContent());
  EXPECT_EQ(kSwappedView, GetViewContent());
  [drag_manager onMouseUp:mouse_at_page_centre];
  EXPECT_EQ(kSwapped, model()->GetModelContent());
  EXPECT_EQ(kSwappedView, GetViewContent());

  // Drag starting in the centre of the view, should do nothing.
  [drag_manager onMouseDownInPage:page
                        withEvent:mouse_at_page_centre];
  [drag_manager onMouseDragged:mouse_at_cell_0];
  EXPECT_EQ(kSwapped, model()->GetModelContent());
  EXPECT_EQ(kSwappedView, GetViewContent());
  [drag_manager onMouseUp:mouse_at_cell_0];
  EXPECT_EQ(kSwapped, model()->GetModelContent());
  EXPECT_EQ(kSwappedView, GetViewContent());

  // Click off page.
  [drag_manager onMouseDownInPage:page
                        withEvent:mouse_off_page];
  [drag_manager onMouseDragged:mouse_at_cell_0];
  EXPECT_EQ(kSwapped, model()->GetModelContent());
  EXPECT_EQ(kSwappedView, GetViewContent());
  [drag_manager onMouseUp:mouse_at_cell_0];
  EXPECT_EQ(kSwapped, model()->GetModelContent());
  EXPECT_EQ(kSwappedView, GetViewContent());

  // Drag to first over second item, then off page.
  [drag_manager onMouseDownInPage:page
                        withEvent:mouse_at_cell_0];
  [drag_manager onMouseDragged:mouse_at_cell_1];
  EXPECT_EQ(kSwapped, model()->GetModelContent());
  EXPECT_EQ(kOrderedView, GetViewContent());
  [drag_manager onMouseDragged:mouse_off_page];
  EXPECT_EQ(kSwapped, model()->GetModelContent());
  EXPECT_EQ(kOrderedView, GetViewContent());
  [drag_manager onMouseUp:mouse_off_page];
  EXPECT_EQ(kOrdered, model()->GetModelContent());
  EXPECT_EQ(kOrderedView, GetViewContent());

  // Replace with an empty model, and ensure we do not break.
  ReplaceTestModel(0);
  EXPECT_EQ(std::string(), model()->GetModelContent());
  EXPECT_EQ(std::string("||"), GetViewContent());
  [drag_manager onMouseDownInPage:page
                        withEvent:mouse_at_cell_0];
  [drag_manager onMouseDragged:mouse_at_cell_1];
  [drag_manager onMouseUp:mouse_at_cell_1];
  EXPECT_EQ(std::string(), model()->GetModelContent());
  EXPECT_EQ(std::string("||"), GetViewContent());
}

// Test item moves between pages.
TEST_F(AppsGridControllerTest, DragAndDropMultiPage) {
  const size_t kPagesToTest = 3;
  // Put one item on the last page to hit more edge cases.
  ReplaceTestModel(kItemsPerPage * (kPagesToTest - 1) + 1);
  NSCollectionView* page[kPagesToTest];
  for (size_t i = 0; i < kPagesToTest; ++i)
    page[i] = [apps_grid_controller_ collectionViewAtPageIndex:i];

  const std::string kSecondItemMovedToSecondPage =
      "|Item 0,Item 2,Item 3,Item 4,Item 5,Item 6,Item 7,Item 8,"
      "Item 9,Item 10,Item 11,Item 12,Item 13,Item 14,Item 15,Item 16|"
      "|Item 17,Item 1,Item 18,Item 19,Item 20,Item 21,Item 22,Item 23,"
      "Item 24,Item 25,Item 26,Item 27,Item 28,Item 29,Item 30,Item 31|"
      "|Item 32|";

  NSEvent* mouse_at_cell_0 = MouseEventInCell(page[0], 0);
  NSEvent* mouse_at_cell_1 = MouseEventInCell(page[0], 1);
  AppsCollectionViewDragManager* drag_manager =
      [apps_grid_controller_ dragManager];
  [drag_manager onMouseDownInPage:page[0]
                        withEvent:mouse_at_cell_1];

  // Initiate dragging before changing pages.
  [drag_manager onMouseDragged:mouse_at_cell_0];

  // Scroll to the second page.
  [apps_grid_controller_ scrollToPage:1];
  [drag_manager onMouseDragged:mouse_at_cell_1];

  // Do one exhaustive check, and then spot-check corner cases.
  EXPECT_EQ(kSecondItemMovedToSecondPage, GetViewContent());
  EXPECT_EQ(0u, GetPageIndexForItem(0));
  EXPECT_EQ(1u, GetPageIndexForItem(1));
  EXPECT_EQ(0u, GetPageIndexForItem(2));
  EXPECT_EQ(0u, GetPageIndexForItem(16));
  EXPECT_EQ(1u, GetPageIndexForItem(17));
  EXPECT_EQ(1u, GetPageIndexForItem(31));
  EXPECT_EQ(2u, GetPageIndexForItem(32));

  // Scroll to the third page and drag some more.
  [apps_grid_controller_ scrollToPage:2];
  [drag_manager onMouseDragged:mouse_at_cell_1];
  EXPECT_EQ(2u, GetPageIndexForItem(1));
  EXPECT_EQ(1u, GetPageIndexForItem(31));
  EXPECT_EQ(1u, GetPageIndexForItem(32));

  // Scroll backwards.
  [apps_grid_controller_ scrollToPage:1];
  [drag_manager onMouseDragged:mouse_at_cell_1];
  EXPECT_EQ(kSecondItemMovedToSecondPage, GetViewContent());
  EXPECT_EQ(1u, GetPageIndexForItem(1));
  EXPECT_EQ(1u, GetPageIndexForItem(31));
  EXPECT_EQ(2u, GetPageIndexForItem(32));

  // Simulate installing an item while dragging (or have it appear during sync).
  model()->PopulateAppWithId(33);
  // Item should go back to its position before the drag.
  EXPECT_EQ(0u, GetPageIndexForItem(1));
  EXPECT_EQ(1u, GetPageIndexForItem(31));
  EXPECT_EQ(2u, GetPageIndexForItem(32));
  // New item should appear at end.
  EXPECT_EQ(2u, GetPageIndexForItem(33));

  // Scroll to end again, and keep dragging (should be ignored).
  [apps_grid_controller_ scrollToPage:2];
  [drag_manager onMouseDragged:mouse_at_cell_0];
  EXPECT_EQ(0u, GetPageIndexForItem(1));
  [drag_manager onMouseUp:mouse_at_cell_0];
  EXPECT_EQ(0u, GetPageIndexForItem(1));
}

// Test scrolling when dragging past edge or over the pager.
TEST_F(AppsGridControllerTest, ScrollingWhileDragging) {
  base::scoped_nsobject<TestPaginationObserver> observer(
      [[TestPaginationObserver alloc] init]);
  [apps_grid_controller_ setPaginationObserver:observer];

  ReplaceTestModel(kItemsPerPage * 3);
  // Start on the middle page.
  [apps_grid_controller_ scrollToPage:1];
  NSCollectionView* page = [apps_grid_controller_ collectionViewAtPageIndex:1];
  NSEvent* mouse_at_cell_0 = MouseEventInCell(page, 0);

  NSEvent* at_center = MouseEventForScroll([apps_grid_controller_ view], 0.0);
  NSEvent* at_left = MouseEventForScroll([apps_grid_controller_ view], -1.1);
  NSEvent* at_right = MouseEventForScroll([apps_grid_controller_ view], 1.1);

  AppsCollectionViewDragManager* drag_manager =
      [apps_grid_controller_ dragManager];
  [drag_manager onMouseDownInPage:page
                        withEvent:mouse_at_cell_0];
  [drag_manager onMouseDragged:at_center];

  // Nothing should be scheduled: target page is visible page.
  EXPECT_EQ(1u, [apps_grid_controller_ visiblePage]);
  EXPECT_EQ(1u, [apps_grid_controller_ scheduledScrollPage]);

  // Drag to the left, should go to first page and no further.
  [drag_manager onMouseDragged:at_left];
  EXPECT_EQ(1u, [apps_grid_controller_ visiblePage]);
  EXPECT_EQ(0u, [apps_grid_controller_ scheduledScrollPage]);
  [apps_grid_controller_ scrollToPage:0];  // Commit without timer for testing.
  [drag_manager onMouseDragged:at_left];
  EXPECT_EQ(0u, [apps_grid_controller_ visiblePage]);
  EXPECT_EQ(0u, [apps_grid_controller_ scheduledScrollPage]);

  // Drag to the right, should go to last page and no futher.
  [drag_manager onMouseDragged:at_right];
  EXPECT_EQ(0u, [apps_grid_controller_ visiblePage]);
  EXPECT_EQ(1u, [apps_grid_controller_ scheduledScrollPage]);
  [apps_grid_controller_ scrollToPage:1];
  [drag_manager onMouseDragged:at_right];
  EXPECT_EQ(1u, [apps_grid_controller_ visiblePage]);
  EXPECT_EQ(2u, [apps_grid_controller_ scheduledScrollPage]);
  [apps_grid_controller_ scrollToPage:2];
  [drag_manager onMouseDragged:at_right];
  EXPECT_EQ(2u, [apps_grid_controller_ visiblePage]);
  EXPECT_EQ(2u, [apps_grid_controller_ scheduledScrollPage]);

  // Simulate a hover over the first pager segment.
  [observer setHoveredSegmentForTest:0];
  [drag_manager onMouseDragged:at_center];
  EXPECT_EQ(2u, [apps_grid_controller_ visiblePage]);
  EXPECT_EQ(0u, [apps_grid_controller_ scheduledScrollPage]);

  // Drag it back, should cancel schedule.
  [observer setHoveredSegmentForTest:-1];
  [drag_manager onMouseDragged:at_center];
  EXPECT_EQ(2u, [apps_grid_controller_ visiblePage]);
  EXPECT_EQ(2u, [apps_grid_controller_ scheduledScrollPage]);

  // Hover again, now over middle segment, and ensure a release also cancels.
  [observer setHoveredSegmentForTest:1];
  [drag_manager onMouseDragged:at_center];
  EXPECT_EQ(2u, [apps_grid_controller_ visiblePage]);
  EXPECT_EQ(1u, [apps_grid_controller_ scheduledScrollPage]);
  [drag_manager onMouseUp:at_center];
  EXPECT_EQ(2u, [apps_grid_controller_ visiblePage]);
  EXPECT_EQ(2u, [apps_grid_controller_ scheduledScrollPage]);

  [apps_grid_controller_ setPaginationObserver:nil];
}

TEST_F(AppsGridControllerTest, ContextMenus) {
  AppListItemWithMenu* item_two_model = new AppListItemWithMenu("Item Two");
  model()->AddItem(new AppListItemWithMenu("Item One"));
  model()->AddItem(item_two_model);
  EXPECT_EQ(2u, [apps_grid_controller_ itemCount]);

  NSCollectionView* page = [apps_grid_controller_ collectionViewAtPageIndex:0];
  NSEvent* mouse_at_cell_0 = MouseEventInCell(page, 0);
  NSEvent* mouse_at_cell_1 = MouseEventInCell(page, 1);

  NSMenu* menu = [page menuForEvent:mouse_at_cell_0];
  EXPECT_EQ(1, [menu numberOfItems]);
  EXPECT_NSEQ(@"Menu For: Item One", [[menu itemAtIndex:0] title]);

  // Test a context menu request while the item is still installing.
  item_two_model->SetMenuReadyForTesting(false);
  menu = [page menuForEvent:mouse_at_cell_1];
  EXPECT_EQ(nil, menu);

  item_two_model->SetMenuReadyForTesting(true);
  menu = [page menuForEvent:mouse_at_cell_1];
  EXPECT_EQ(1, [menu numberOfItems]);
  EXPECT_NSEQ(@"Menu For: Item Two", [[menu itemAtIndex:0] title]);

  // Test that a button being held down with the left button does not also show
  // a context menu.
  [GetItemViewAt(0) highlight:YES];
  EXPECT_FALSE([page menuForEvent:mouse_at_cell_0]);
  [GetItemViewAt(0) highlight:NO];
  EXPECT_TRUE([page menuForEvent:mouse_at_cell_0]);
}

}  // namespace test
}  // namespace app_list
