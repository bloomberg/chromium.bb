// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/app_list/cocoa/apps_search_results_controller.h"

#include "base/mac/scoped_nsobject.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "testing/gtest_mac.h"
#include "ui/app_list/search_result.h"
#include "ui/app_list/test/app_list_test_model.h"
#include "ui/base/models/simple_menu_model.h"
#import "ui/base/test/ui_cocoa_test_helper.h"
#include "ui/gfx/image/image_skia_util_mac.h"
#import "ui/base/test/cocoa_test_event_utils.h"

@interface TestAppsSearchResultsDelegate : NSObject<AppsSearchResultsDelegate> {
 @private
  app_list::test::AppListTestModel appListModel_;
  app_list::SearchResult* lastOpenedResult_;
  int redoSearchCount_;
}

@property(readonly, nonatomic) app_list::SearchResult* lastOpenedResult;
@property(readonly, nonatomic) int redoSearchCount;

@end

@implementation TestAppsSearchResultsDelegate

@synthesize lastOpenedResult = lastOpenedResult_;
@synthesize redoSearchCount = redoSearchCount_;

- (app_list::AppListModel*)appListModel {
  return &appListModel_;
}

- (void)openResult:(app_list::SearchResult*)result {
  lastOpenedResult_ = result;
}

- (void)redoSearch {
  ++redoSearchCount_;
}

@end

namespace app_list {
namespace test {
namespace {

const int kDefaultResultsCount = 3;

class SearchResultWithMenu : public SearchResult {
 public:
  SearchResultWithMenu(const std::string& title, const std::string& details)
      : menu_model_(NULL),
        menu_ready_(true) {
    set_title(ASCIIToUTF16(title));
    set_details(ASCIIToUTF16(details));
    menu_model_.AddItem(0, UTF8ToUTF16("Menu For: " + title));
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

  DISALLOW_COPY_AND_ASSIGN(SearchResultWithMenu);
};

class AppsSearchResultsControllerTest : public ui::CocoaTest {
 public:
  AppsSearchResultsControllerTest() {}

  void AddTestResultAtIndex(size_t index,
                            const std::string& title,
                            const std::string& details) {
    scoped_ptr<SearchResult> result(new SearchResultWithMenu(title, details));
    AppListModel::SearchResults* results = [delegate_ appListModel]->results();
    results->AddAt(index, result.release());
  }

  SearchResult* ModelResultAt(size_t index) {
    return [delegate_ appListModel]->results()->GetItemAt(index);
  }

  NSCell* ViewResultAt(NSInteger index) {
    NSTableView* table_view = [apps_search_results_controller_ tableView];
    return [table_view preparedCellAtColumn:0
                                        row:index];
  }

  void SetMenuReadyAt(size_t index, bool ready) {
    SearchResultWithMenu* result =
        static_cast<SearchResultWithMenu*>(ModelResultAt(index));
    result->SetMenuReadyForTesting(ready);
  }

  BOOL SimulateKeyAction(SEL c) {
    return [apps_search_results_controller_ handleCommandBySelector:c];
  }

  void ExpectConsistent();

  // ui::CocoaTest overrides:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

 protected:
  base::scoped_nsobject<TestAppsSearchResultsDelegate> delegate_;
  base::scoped_nsobject<AppsSearchResultsController>
      apps_search_results_controller_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppsSearchResultsControllerTest);
};

void AppsSearchResultsControllerTest::ExpectConsistent() {
  NSInteger item_count = [delegate_ appListModel]->results()->item_count();
  ASSERT_EQ(item_count,
            [[apps_search_results_controller_ tableView] numberOfRows]);

  // Compare content strings to ensure the order of items is consistent, and any
  // model data that should have been reloaded has been reloaded in the view.
  for (NSInteger i = 0; i < item_count; ++i) {
    SearchResult* result = ModelResultAt(i);
    base::string16 string_in_model = result->title();
    if (!result->details().empty())
      string_in_model += ASCIIToUTF16("\n") + result->details();
    EXPECT_NSEQ(base::SysUTF16ToNSString(string_in_model),
                [[ViewResultAt(i) attributedStringValue] string]);
  }
}

void AppsSearchResultsControllerTest::SetUp() {
  apps_search_results_controller_.reset(
      [[AppsSearchResultsController alloc] initWithAppsSearchResultsFrameSize:
          NSMakeSize(400, 400)]);
  // The view is initially hidden. Give it a non-zero height so it draws.
  [[apps_search_results_controller_ view] setFrameSize:NSMakeSize(400, 400)];

  delegate_.reset([[TestAppsSearchResultsDelegate alloc] init]);

  // Populate with some results so that TEST_VIEW does something non-trivial.
  for (int i = 0; i < kDefaultResultsCount; ++i)
    AddTestResultAtIndex(i, base::StringPrintf("Result %d", i), "ItemDetail");

  SearchResult::Tags test_tags;
  // Apply markup to the substring "Result" in the first item.
  test_tags.push_back(SearchResult::Tag(SearchResult::Tag::NONE, 0, 1));
  test_tags.push_back(SearchResult::Tag(SearchResult::Tag::URL, 1, 2));
  test_tags.push_back(SearchResult::Tag(SearchResult::Tag::MATCH, 2, 3));
  test_tags.push_back(SearchResult::Tag(SearchResult::Tag::DIM, 3, 4));
  test_tags.push_back(SearchResult::Tag(SearchResult::Tag::MATCH |
                                        SearchResult::Tag::URL, 4, 5));
  test_tags.push_back(SearchResult::Tag(SearchResult::Tag::MATCH |
                                        SearchResult::Tag::DIM, 5, 6));

  SearchResult* result = ModelResultAt(0);
  result->SetIcon(gfx::ImageSkiaFromNSImage(
      [NSImage imageNamed:NSImageNameStatusAvailable]));
  result->set_title_tags(test_tags);

  [apps_search_results_controller_ setDelegate:delegate_];

  ui::CocoaTest::SetUp();
  [[test_window() contentView] addSubview:
      [apps_search_results_controller_ view]];
}

void AppsSearchResultsControllerTest::TearDown() {
  [apps_search_results_controller_ setDelegate:nil];
  ui::CocoaTest::TearDown();
}

NSEvent* MouseEventInRow(NSTableView* table_view, NSInteger row_index) {
  NSRect row_rect = [table_view rectOfRow:row_index];
  NSPoint point_in_view = NSMakePoint(NSMidX(row_rect), NSMidY(row_rect));
  NSPoint point_in_window = [table_view convertPoint:point_in_view
                                              toView:nil];
  return cocoa_test_event_utils::LeftMouseDownAtPoint(point_in_window);
}

}  // namespace

TEST_VIEW(AppsSearchResultsControllerTest,
          [apps_search_results_controller_ view]);

TEST_F(AppsSearchResultsControllerTest, ModelObservers) {
  NSTableView* table_view = [apps_search_results_controller_ tableView];
  ExpectConsistent();

  EXPECT_EQ(1, [table_view numberOfColumns]);
  EXPECT_EQ(kDefaultResultsCount, [table_view numberOfRows]);

  // Insert at start.
  AddTestResultAtIndex(0, "One", std::string());
  EXPECT_EQ(kDefaultResultsCount + 1, [table_view numberOfRows]);
  ExpectConsistent();

  // Remove from end.
  [delegate_ appListModel]->results()->DeleteAt(kDefaultResultsCount);
  EXPECT_EQ(kDefaultResultsCount, [table_view numberOfRows]);
  ExpectConsistent();

  // Insert at end.
  AddTestResultAtIndex(kDefaultResultsCount, "Four", std::string());
  EXPECT_EQ(kDefaultResultsCount + 1, [table_view numberOfRows]);
  ExpectConsistent();

  // Delete from start.
  [delegate_ appListModel]->results()->DeleteAt(0);
  EXPECT_EQ(kDefaultResultsCount, [table_view numberOfRows]);
  ExpectConsistent();

  // Test clearing results.
  [delegate_ appListModel]->results()->DeleteAll();
  EXPECT_EQ(0, [table_view numberOfRows]);
  ExpectConsistent();
}

TEST_F(AppsSearchResultsControllerTest, KeyboardSelectAndActivate) {
  NSTableView* table_view = [apps_search_results_controller_ tableView];
  EXPECT_EQ(-1, [table_view selectedRow]);

  // Pressing up when nothing is selected should select the last item.
  EXPECT_TRUE(SimulateKeyAction(@selector(moveUp:)));
  EXPECT_EQ(kDefaultResultsCount - 1, [table_view selectedRow]);
  [table_view deselectAll:nil];
  EXPECT_EQ(-1, [table_view selectedRow]);

  // Pressing down when nothing is selected should select the first item.
  EXPECT_TRUE(SimulateKeyAction(@selector(moveDown:)));
  EXPECT_EQ(0, [table_view selectedRow]);

  // Pressing up should wrap around.
  EXPECT_TRUE(SimulateKeyAction(@selector(moveUp:)));
  EXPECT_EQ(kDefaultResultsCount - 1, [table_view selectedRow]);

  // Down should now also wrap, since the selection is at the end.
  EXPECT_TRUE(SimulateKeyAction(@selector(moveDown:)));
  EXPECT_EQ(0, [table_view selectedRow]);

  // Regular down and up movement, ensuring the cells have correct backgrounds.
  EXPECT_TRUE(SimulateKeyAction(@selector(moveDown:)));
  EXPECT_EQ(1, [table_view selectedRow]);
  EXPECT_EQ(NSBackgroundStyleDark, [ViewResultAt(1) backgroundStyle]);
  EXPECT_EQ(NSBackgroundStyleLight, [ViewResultAt(0) backgroundStyle]);

  EXPECT_TRUE(SimulateKeyAction(@selector(moveUp:)));
  EXPECT_EQ(0, [table_view selectedRow]);
  EXPECT_EQ(NSBackgroundStyleDark, [ViewResultAt(0) backgroundStyle]);
  EXPECT_EQ(NSBackgroundStyleLight, [ViewResultAt(1) backgroundStyle]);

  // Test activating items.
  EXPECT_TRUE(SimulateKeyAction(@selector(insertNewline:)));
  EXPECT_EQ(ModelResultAt(0), [delegate_ lastOpenedResult]);
  EXPECT_TRUE(SimulateKeyAction(@selector(moveDown:)));
  EXPECT_TRUE(SimulateKeyAction(@selector(insertNewline:)));
  EXPECT_EQ(ModelResultAt(1), [delegate_ lastOpenedResult]);
}

TEST_F(AppsSearchResultsControllerTest, ContextMenus) {
  NSTableView* table_view = [apps_search_results_controller_ tableView];
  NSEvent* mouse_in_row_0 = MouseEventInRow(table_view, 0);
  NSEvent* mouse_in_row_1 = MouseEventInRow(table_view, 1);

  NSMenu* menu = [table_view menuForEvent:mouse_in_row_0];
  EXPECT_EQ(1, [menu numberOfItems]);
  EXPECT_NSEQ(@"Menu For: Result 0", [[menu itemAtIndex:0] title]);

  // Test a context menu request while the item is still installing.
  SetMenuReadyAt(1, false);
  menu = [table_view menuForEvent:mouse_in_row_1];
  EXPECT_EQ(nil, menu);

  SetMenuReadyAt(1, true);
  menu = [table_view menuForEvent:mouse_in_row_1];
  EXPECT_EQ(1, [menu numberOfItems]);
  EXPECT_NSEQ(@"Menu For: Result 1", [[menu itemAtIndex:0] title]);
}

// Test that observing a search result item uninstall performs the search again.
TEST_F(AppsSearchResultsControllerTest, UninstallRedperformsSearch) {
  base::MessageLoopForUI message_loop;
  EXPECT_EQ(0, [delegate_ redoSearchCount]);
  ModelResultAt(0)->NotifyItemUninstalled();
  message_loop.PostTask(FROM_HERE, base::MessageLoop::QuitClosure());
  message_loop.Run();
  EXPECT_EQ(1, [delegate_ redoSearchCount]);
}

}  // namespace test
}  // namespace app_list
