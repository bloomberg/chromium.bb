// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/scrollable_apps_grid_view.h"

#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_item_list.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/model/app_list_test_model.h"
#include "ash/app_list/paged_view_structure.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/test_app_list_client.h"
#include "ash/app_list/views/app_list_folder_view.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/apps_grid_view_test_api.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/test/test_shelf_item_delegate.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

class ShelfItemFactoryFake : public ShelfModel::ShelfItemFactory {
 public:
  virtual ~ShelfItemFactoryFake() = default;
  bool CreateShelfItemForAppId(
      const std::string& app_id,
      ShelfItem* item,
      std::unique_ptr<ShelfItemDelegate>* delegate) override {
    *item = ShelfItem();
    item->id = ShelfID(app_id);
    *delegate = std::make_unique<TestShelfItemDelegate>(item->id);
    return true;
  }
};

}  // namespace

class ScrollableAppsGridViewTest : public AshTestBase {
 public:
  ScrollableAppsGridViewTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitAndEnableFeature(features::kProductivityLauncher);
  }
  ~ScrollableAppsGridViewTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    app_list_test_model_ = std::make_unique<test::AppListTestModel>();
    search_model_ = std::make_unique<SearchModel>();
    Shell::Get()->app_list_controller()->SetActiveModel(
        /*profile_id=*/1, app_list_test_model_.get(), search_model_.get());

    shelf_item_factory_ = std::make_unique<ShelfItemFactoryFake>();
    ShelfModel::Get()->SetShelfItemFactory(shelf_item_factory_.get());
  }

  void TearDown() override {
    ShelfModel::Get()->SetShelfItemFactory(nullptr);
    AshTestBase::TearDown();
  }

  test::AppListTestModel::AppListTestItem* AddAppListItem(
      const std::string& id) {
    return app_list_test_model_->CreateAndAddItem(id);
  }

  void PopulateApps(int n) { app_list_test_model_->PopulateApps(n); }

  void DeleteApps(int n) {
    AppListItemList* item_list = app_list_test_model_->top_level_item_list();
    for (int i = 0; i < n; i++) {
      app_list_test_model_->DeleteItem(item_list->item_at(0)->id());
    }
  }

  AppListFolderItem* CreateAndPopulateFolderWithApps(int n) {
    return app_list_test_model_->CreateAndPopulateFolderWithApps(n);
  }

  void SimulateKeyPress(ui::KeyboardCode key_code, int flags = ui::EF_NONE) {
    GetEventGenerator()->PressKey(key_code, flags);
  }

  void SimulateKeyReleased(ui::KeyboardCode key_code, int flags = ui::EF_NONE) {
    GetEventGenerator()->ReleaseKey(key_code, flags);
  }

  void ShowAppList() {
    GetAppListTestHelper()->ShowAppList();

    apps_grid_view_ = GetAppListTestHelper()->GetScrollableAppsGridView();
    scroll_view_ = apps_grid_view_->scroll_view_for_test();
  }

  AppListItemView* StartDragOnItemViewAt(int item_index) {
    AppListItemView* item = apps_grid_view_->GetItemViewAt(item_index);
    auto* generator = GetEventGenerator();
    generator->MoveMouseTo(item->GetBoundsInScreen().CenterPoint());
    generator->PressLeftButton();
    item->FireMouseDragTimerForTest();
    return item;
  }

  AppListItemView* StartDragOnItemInFolderAt(int item_index) {
    DCHECK(GetAppListTestHelper()->IsInFolderView());
    auto* folder_view = GetAppListTestHelper()->GetBubbleFolderView();
    AppListItemView* item =
        folder_view->items_grid_view()->GetItemViewAt(item_index);
    auto* generator = GetEventGenerator();
    generator->MoveMouseTo(item->GetBoundsInScreen().CenterPoint());
    generator->PressLeftButton();
    item->FireMouseDragTimerForTest();
    return item;
  }

  void DragItemOutOfFolder() {
    ASSERT_TRUE(GetAppListTestHelper()->IsInFolderView());
    auto* folder_view = GetAppListTestHelper()->GetBubbleFolderView();
    ASSERT_TRUE(folder_view->items_grid_view()->has_dragged_item());
    gfx::Point outside_view =
        folder_view->GetBoundsInScreen().bottom_right() + gfx::Vector2d(10, 10);
    GetEventGenerator()->MoveMouseTo(outside_view);
    folder_view->items_grid_view()->FireFolderItemReparentTimerForTest();
  }

  ScrollableAppsGridView* GetScrollableAppsGridView() {
    return GetAppListTestHelper()->GetScrollableAppsGridView();
  }

  void AddPageBreakItem() { GetAppListTestHelper()->AddPageBreakItem(); }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<test::AppListTestModel> app_list_test_model_;
  std::unique_ptr<SearchModel> search_model_;
  std::unique_ptr<ShelfItemFactoryFake> shelf_item_factory_;

  // Cache some view pointers to make the tests more concise.
  ScrollableAppsGridView* apps_grid_view_ = nullptr;
  views::ScrollView* scroll_view_ = nullptr;
};

TEST_F(ScrollableAppsGridViewTest, PageBreaksDoNotCauseExtraRowsInLayout) {
  AddAppListItem("1");
  AddAppListItem("2");
  AddAppListItem("3");
  AddAppListItem("4");
  AddPageBreakItem();
  AddAppListItem("5");
  ShowAppList();

  ScrollableAppsGridView* view = GetScrollableAppsGridView();
  const int tile_height = view->app_list_config()->grid_tile_height();
  const gfx::Size grid_size = view->GetTileGridSize();
  // The layout is one tile tall because it has only one row.
  EXPECT_EQ(grid_size.height(), tile_height);
}

TEST_F(ScrollableAppsGridViewTest, ClickOnApp) {
  AddAppListItem("id");

  ShowAppList();

  // Click on the first icon.
  ScrollableAppsGridView* view = GetScrollableAppsGridView();
  views::View* icon = view->GetItemViewAt(0);
  GetEventGenerator()->MoveMouseTo(icon->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();

  // The item was activated.
  EXPECT_EQ(1, GetTestAppListClient()->activate_item_count());
  EXPECT_EQ("id", GetTestAppListClient()->activate_item_last_id());
}

TEST_F(ScrollableAppsGridViewTest, DragApp) {
  base::HistogramTester histogram_tester;
  AddAppListItem("id1");
  AddAppListItem("id2");
  ShowAppList();

  // Start dragging the first item.
  StartDragOnItemViewAt(0);

  // Drag to the right of the second item.
  gfx::Size tile_size = apps_grid_view_->GetTotalTileSize(/*page=*/0);
  auto* generator = GetEventGenerator();
  generator->MoveMouseBy(tile_size.width() * 2, 0);
  generator->ReleaseLeftButton();

  // The item was not activated.
  EXPECT_EQ(0, GetTestAppListClient()->activate_item_count());

  // Items were reordered.
  AppListItemList* item_list = app_list_test_model_->top_level_item_list();
  ASSERT_EQ(2u, item_list->item_count());
  EXPECT_EQ("id2", item_list->item_at(0)->id());
  EXPECT_EQ("id1", item_list->item_at(1)->id());

  // Reordering apps is recorded in the histogram tester.
  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kReorderByDragInTopLevel, 1);
}

TEST_F(ScrollableAppsGridViewTest, SearchBoxHasFocusAfterDrag) {
  PopulateApps(2);
  ShowAppList();

  // Drag the first item to the right.
  AppListItemView* item = StartDragOnItemViewAt(0);
  GetEventGenerator()->MoveMouseBy(250, 0);
  GetEventGenerator()->ReleaseLeftButton();

  // The item does not have focus.
  EXPECT_FALSE(item->HasFocus());
  EXPECT_FALSE(apps_grid_view_->IsSelectedView(item));

  // The search box has focus.
  auto* search_box_view = GetAppListTestHelper()->GetBubbleSearchBoxView();
  EXPECT_TRUE(search_box_view->search_box()->HasFocus());
  EXPECT_TRUE(search_box_view->is_search_box_active());
}

TEST_F(ScrollableAppsGridViewTest, ItemIndicesForMove) {
  AddAppListItem("aaa");  // App list item index 0, visual index 0,0.
  AddPageBreakItem();     // Not visible.
  AddAppListItem("bbb");  // App list item index 2, visual index 0,1.
  AddPageBreakItem();     // Not visible.
  AddAppListItem("ccc");  // App list item index 4, visual index 0,2.
  ShowAppList();

  auto* view = GetScrollableAppsGridView();
  PagedViewStructure* structure =
      test::AppsGridViewTestApi(view).GetPagedViewStructure();

  // The last visual index to add an item is 0,3.
  EXPECT_EQ(GridIndex(0, 3), structure->GetLastTargetIndex());
  EXPECT_EQ(GridIndex(0, 3), structure->GetLastTargetIndexOfPage(0));

  // During a drag, the last visual index to add an item is 0,2.
  StartDragOnItemViewAt(0);
  EXPECT_EQ(GridIndex(0, 2), structure->GetLastTargetIndex());
  EXPECT_EQ(GridIndex(0, 2), structure->GetLastTargetIndexOfPage(0));
  GetEventGenerator()->ReleaseLeftButton();

  // Visual index directly maps to target index in "view model".
  EXPECT_EQ(0, structure->GetTargetModelIndexForMove(nullptr, GridIndex(0, 0)));
  EXPECT_EQ(1, structure->GetTargetModelIndexForMove(nullptr, GridIndex(0, 1)));
  EXPECT_EQ(2, structure->GetTargetModelIndexForMove(nullptr, GridIndex(0, 2)));
  EXPECT_EQ(3, structure->GetTargetModelIndexForMove(nullptr, GridIndex(0, 3)));

  // Target is the front.
  EXPECT_EQ(0,
            structure->GetTargetItemListIndexForMove(nullptr, GridIndex(0, 0)));
  // Target is after "aaa".
  EXPECT_EQ(1,
            structure->GetTargetItemListIndexForMove(nullptr, GridIndex(0, 1)));
  // Target is after "aaa" + break + "bbb".
  EXPECT_EQ(3,
            structure->GetTargetItemListIndexForMove(nullptr, GridIndex(0, 2)));
  // Target is after "aaa" + break + "bbb" + break + "ccc".
  EXPECT_EQ(5,
            structure->GetTargetItemListIndexForMove(nullptr, GridIndex(0, 3)));
}

TEST_F(ScrollableAppsGridViewTest, DragAppAfterScrollingDown) {
  // Simulate data from another device that has a page break after 20 items.
  PopulateApps(20);
  AddPageBreakItem();
  AddAppListItem("aaa");
  AddAppListItem("bbb");
  ShowAppList();

  // "aaa" and "bbb" are the last two items.
  AppListItemList* item_list = app_list_test_model_->top_level_item_list();
  ASSERT_EQ(23u, item_list->item_count());
  ASSERT_EQ("aaa", item_list->item_at(21)->id());
  ASSERT_EQ("bbb", item_list->item_at(22)->id());

  // Scroll down to the "aaa" item.
  auto* apps_grid_view = GetScrollableAppsGridView();
  AppListItemView* item = apps_grid_view->GetItemViewAt(20);
  ASSERT_EQ("aaa", item->item()->id());
  item->ScrollViewToVisible();

  // Drag the "aaa" item to the right.
  auto* generator = GetEventGenerator();
  generator->MoveMouseTo(item->GetBoundsInScreen().CenterPoint());
  generator->PressLeftButton();
  item->FireMouseDragTimerForTest();
  gfx::Size tile_size = apps_grid_view->GetTotalTileSize(/*page=*/0);
  generator->MoveMouseBy(tile_size.width() * 2, 0);
  generator->ReleaseLeftButton();

  // The last 2 items were reordered.
  EXPECT_EQ("bbb", item_list->item_at(21)->id()) << item_list->ToString();
  EXPECT_EQ("aaa", item_list->item_at(22)->id()) << item_list->ToString();
}

TEST_F(ScrollableAppsGridViewTest, AutoScrollDown) {
  PopulateApps(30);
  ShowAppList();

  // Scroll view starts at the top.
  const int initial_scroll_offset = scroll_view_->GetVisibleRect().y();
  EXPECT_EQ(initial_scroll_offset, 0);

  // Drag an item into the bottom auto-scroll margin.
  StartDragOnItemViewAt(0);
  auto* generator = GetEventGenerator();
  generator->MoveMouseTo(scroll_view_->GetBoundsInScreen().bottom_center());

  // The scroll view scrolls immediately.
  const int scroll_offset1 = scroll_view_->GetVisibleRect().y();
  EXPECT_GT(scroll_offset1, 0);

  // Scroll timer is running.
  EXPECT_TRUE(apps_grid_view_->auto_scroll_timer_for_test()->IsRunning());

  // Reordering is paused.
  EXPECT_FALSE(apps_grid_view_->reorder_timer_for_test()->IsRunning());

  // Holding the mouse in place for a while scrolls down more.
  task_environment()->FastForwardBy(base::Milliseconds(100));
  const int scroll_offset2 = scroll_view_->GetVisibleRect().y();
  EXPECT_GT(scroll_offset2, scroll_offset1);

  // Move the mouse back into the (vertical) center of the view (not in the
  // scroll margin). Use a point close to a horizontal edge to avoid hitting an
  // item bounds (which would trigger reparent instead of reorder timer).
  generator->MoveMouseTo(scroll_view_->GetBoundsInScreen().left_center() +
                         gfx::Vector2d(5, 0));

  // Scroll position didn't change, auto-scrolling is stopped, and reordering
  // started again.
  EXPECT_EQ(scroll_offset2, scroll_view_->GetVisibleRect().y());
  EXPECT_FALSE(apps_grid_view_->auto_scroll_timer_for_test()->IsRunning());
  EXPECT_TRUE(apps_grid_view_->reorder_timer_for_test()->IsRunning());
}

TEST_F(ScrollableAppsGridViewTest, DoesNotAutoScrollUpWhenAtTop) {
  PopulateApps(30);
  ShowAppList();

  // Drag an item into the top auto-scroll margin and wait a while.
  StartDragOnItemViewAt(0);
  GetEventGenerator()->MoveMouseTo(
      scroll_view_->GetBoundsInScreen().top_center());
  task_environment()->FastForwardBy(base::Milliseconds(500));

  // View did not scroll.
  int scroll_offset = scroll_view_->GetVisibleRect().y();
  EXPECT_EQ(scroll_offset, 0);
  EXPECT_FALSE(apps_grid_view_->auto_scroll_timer_for_test()->IsRunning());
}

TEST_F(ScrollableAppsGridViewTest, DoesNotAutoScrollDownWhenAtBottom) {
  PopulateApps(30);
  ShowAppList();

  // Scroll the view to the bottom.
  scroll_view_->ScrollToPosition(scroll_view_->vertical_scroll_bar(),
                                 std::numeric_limits<int>::max());
  int initial_scroll_offset = scroll_view_->GetVisibleRect().y();

  // Drag an item into the bottom auto-scroll margin and wait a while.
  StartDragOnItemViewAt(29);
  GetEventGenerator()->MoveMouseTo(
      scroll_view_->GetBoundsInScreen().bottom_center());
  task_environment()->FastForwardBy(base::Milliseconds(500));

  // View did not scroll.
  int scroll_offset = scroll_view_->GetVisibleRect().y();
  EXPECT_EQ(scroll_offset, initial_scroll_offset);
  EXPECT_FALSE(apps_grid_view_->auto_scroll_timer_for_test()->IsRunning());
}

TEST_F(ScrollableAppsGridViewTest, DoesNotAutoScrollWhenDraggedToTheRight) {
  PopulateApps(30);
  ShowAppList();

  // Drag an item outside the bottom-right corner of the scroll view (i.e.
  // towards the shelf).
  StartDragOnItemViewAt(0);
  gfx::Point point = scroll_view_->GetBoundsInScreen().bottom_right();
  point.Offset(10, 10);
  GetEventGenerator()->MoveMouseTo(point);
  task_environment()->FastForwardBy(base::Milliseconds(500));

  // View did not scroll.
  int scroll_offset = scroll_view_->GetVisibleRect().y();
  EXPECT_EQ(scroll_offset, 0);
  EXPECT_FALSE(apps_grid_view_->auto_scroll_timer_for_test()->IsRunning());
}

TEST_F(ScrollableAppsGridViewTest, DoesNotAutoScrollWhenAboveWidget) {
  PopulateApps(30);
  ShowAppList();

  // Scroll the view to the bottom.
  scroll_view_->ScrollToPosition(scroll_view_->vertical_scroll_bar(),
                                 std::numeric_limits<int>::max());
  int initial_scroll_offset = scroll_view_->GetVisibleRect().y();

  // Drag an item above the widget scroll margin.
  StartDragOnItemViewAt(29);
  gfx::Point point =
      scroll_view_->GetWidget()->GetWindowBoundsInScreen().top_center();
  point.Offset(0, -10);
  GetEventGenerator()->MoveMouseTo(point);
  task_environment()->FastForwardBy(base::Milliseconds(500));

  // View did not scroll.
  int scroll_offset = scroll_view_->GetVisibleRect().y();
  EXPECT_EQ(scroll_offset, initial_scroll_offset);
  EXPECT_FALSE(apps_grid_view_->auto_scroll_timer_for_test()->IsRunning());
}

TEST_F(ScrollableAppsGridViewTest, DoesNotAutoScrollWhenBelowWidget) {
  PopulateApps(30);
  ShowAppList();

  // Drag an item below the widget scroll margin.
  StartDragOnItemViewAt(0);
  gfx::Point point =
      scroll_view_->GetWidget()->GetWindowBoundsInScreen().bottom_center();
  point.Offset(0, 10);
  GetEventGenerator()->MoveMouseTo(point);
  task_environment()->FastForwardBy(base::Milliseconds(500));

  // View did not scroll.
  int scroll_offset = scroll_view_->GetVisibleRect().y();
  EXPECT_EQ(scroll_offset, 0);
  EXPECT_FALSE(apps_grid_view_->auto_scroll_timer_for_test()->IsRunning());
}

// Regression test for https://crbug.com/1258954
TEST_F(ScrollableAppsGridViewTest, DragItemIntoEmptySpaceWillReorderToEnd) {
  AddAppListItem("id1");
  AddAppListItem("id2");
  AddAppListItem("id3");
  ShowAppList();

  // The grid view is taller than the single row of apps, so it can handle drops
  // in the empty region.
  EXPECT_GT(apps_grid_view_->height(),
            apps_grid_view_->GetTileGridSize().height());

  // Drag and drop the first item straight down below the first row.
  StartDragOnItemViewAt(0);
  gfx::Size tile_size = apps_grid_view_->GetTotalTileSize(/*page=*/0);
  auto* generator = GetEventGenerator();
  generator->MoveMouseBy(0, tile_size.height());
  generator->ReleaseLeftButton();

  // The first item was reordered to the end.
  AppListItemList* item_list = app_list_test_model_->top_level_item_list();
  ASSERT_EQ(3u, item_list->item_count());
  EXPECT_EQ("id2", item_list->item_at(0)->id());
  EXPECT_EQ("id3", item_list->item_at(1)->id());
  EXPECT_EQ("id1", item_list->item_at(2)->id());
}

TEST_F(ScrollableAppsGridViewTest, ChangingAppListModelUpdatesAppsGridHeight) {
  // Start with 4 rows of 5.
  PopulateApps(20);
  ShowAppList();

  // Adding one row of 5 causes the grid size to expand.
  const int height_before_adding = apps_grid_view_->height();
  PopulateApps(5);
  apps_grid_view_->GetWidget()->LayoutRootViewIfNecessary();
  EXPECT_GT(apps_grid_view_->height(), height_before_adding);

  // Removing one row of 5 causes the grid size to contract.
  const int height_before_removing = apps_grid_view_->height();
  DeleteApps(5);
  apps_grid_view_->GetWidget()->LayoutRootViewIfNecessary();
  EXPECT_LT(apps_grid_view_->height(), height_before_removing);
}

TEST_F(ScrollableAppsGridViewTest, SmallFolderHasCorrectWidth) {
  CreateAndPopulateFolderWithApps(2);
  ShowAppList();

  // Enter the folder view.
  auto* generator = GetEventGenerator();
  SimulateMouseClickAt(generator, apps_grid_view_->GetItemViewAt(0));
  ASSERT_TRUE(GetAppListTestHelper()->IsInFolderView());

  auto* folder_view = GetAppListTestHelper()->GetBubbleFolderView();
  auto* items_grid_view = folder_view->items_grid_view();
  const int tile_width = items_grid_view->app_list_config()->grid_tile_width();

  // Spec calls for 8 dips of padding at edges and between tiles.
  EXPECT_EQ(folder_view->width(), 8 + tile_width + 8 + tile_width + 8);

  // The leftmost item is flush to the left of the grid.
  EXPECT_EQ(items_grid_view->GetItemViewAt(0)->bounds().x(), 0);

  // The rightmost item is flush to the right of the grid.
  EXPECT_EQ(items_grid_view->GetItemViewAt(1)->bounds().right(),
            items_grid_view->GetLocalBounds().right());
}

TEST_F(ScrollableAppsGridViewTest, DragItemToReorderInFolderRecordsHistogram) {
  base::HistogramTester histogram_tester;
  // Create a folder with 3 apps.
  AppListFolderItem* folder_item = CreateAndPopulateFolderWithApps(3);
  ShowAppList();

  // Enter the folder view.
  auto* generator = GetEventGenerator();
  SimulateMouseClickAt(generator, apps_grid_view_->GetItemViewAt(0));
  ASSERT_TRUE(GetAppListTestHelper()->IsInFolderView());

  // Drag the first app in the folder.
  AppListItemView* item_view = StartDragOnItemInFolderAt(0);

  // Drag the item to the third position in the folder.
  gfx::Size tile_size = apps_grid_view_->GetTileViewSize();
  generator->MoveMouseBy(0, tile_size.height());
  generator->ReleaseLeftButton();

  // The item is now reordered in the folder and the reordering is recorded.
  EXPECT_EQ(3u, folder_item->ChildItemCount());
  EXPECT_EQ(folder_item->item_list()->item_at(2)->id(),
            item_view->item()->id());
  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kReorderByDragInFolder, 1);
}

TEST_F(ScrollableAppsGridViewTest, DragItemIntoFolderRecordsHistogram) {
  base::HistogramTester histogram_tester;
  // Create a folder and an app.
  AppListFolderItem* folder_item = CreateAndPopulateFolderWithApps(3);
  AddAppListItem("dragged_item");
  ShowAppList();

  // Drag the app in the top level app list into the folder.
  StartDragOnItemViewAt(1);
  ASSERT_TRUE(apps_grid_view_->GetItemViewAt(0)->item()->is_folder());
  auto* generator = GetEventGenerator();
  generator->MoveMouseTo(
      apps_grid_view_->GetItemViewAt(0)->GetBoundsInScreen().CenterPoint());
  generator->ReleaseLeftButton();

  // The dragged app is now in the folder and the reordering is recorded.
  EXPECT_EQ(4u, folder_item->ChildItemCount());
  EXPECT_EQ(folder_item->item_list()->item_at(3)->id(), "dragged_item");
  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kMoveByDragIntoFolder, 1);
}

TEST_F(ScrollableAppsGridViewTest, DragItemOutOfFolderRecordsHistogram) {
  base::HistogramTester histogram_tester;
  // Create a folder with 3 apps.
  AppListFolderItem* folder_item = CreateAndPopulateFolderWithApps(3);
  ShowAppList();

  // Enter the folder view.
  auto* generator = GetEventGenerator();
  SimulateMouseClickAt(generator, apps_grid_view_->GetItemViewAt(0));
  ASSERT_TRUE(GetAppListTestHelper()->IsInFolderView());

  // Drag the first app in the folder and move it out of the folder.
  AppListItemView* item_view = StartDragOnItemInFolderAt(0);
  std::string item_id = item_view->item()->id();
  DragItemOutOfFolder();

  // Drag the app item to near the expected end position and end the drag.
  generator->MoveMouseTo(
      apps_grid_view_->GetItemViewAt(0)->GetBoundsInScreen().right_center() +
      gfx::Vector2d(20, 0));
  generator->ReleaseLeftButton();

  // The folder view should be closed and invisible after releasing the drag.
  EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());
  EXPECT_FALSE(GetAppListTestHelper()->GetBubbleFolderView()->GetVisible());

  // The dragged item is now in the top level item list and the reordering is
  // recorded.
  AppListItemList* item_list = app_list_test_model_->top_level_item_list();
  EXPECT_EQ(2u, item_list->item_count());
  EXPECT_EQ(item_list->item_at(1)->id(), item_id);
  EXPECT_EQ(2u, folder_item->item_list()->item_count());
  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kMoveByDragOutOfFolder, 1);
}

TEST_F(ScrollableAppsGridViewTest,
       DragItemFromOneFolderToAnotherRecordsHistogram) {
  base::HistogramTester histogram_tester;
  // Create two folders.
  AppListFolderItem* folder_item_1 = CreateAndPopulateFolderWithApps(3);
  AppListFolderItem* folder_item_2 = CreateAndPopulateFolderWithApps(2);
  ShowAppList();

  // Enter the view of the first folder.
  auto* generator = GetEventGenerator();
  SimulateMouseClickAt(generator, apps_grid_view_->GetItemViewAt(0));
  ASSERT_TRUE(GetAppListTestHelper()->IsInFolderView());

  // Drag the first app in the folder and move it out of the folder.
  StartDragOnItemInFolderAt(0);
  DragItemOutOfFolder();

  // Move the app item into the other folder and end the drag.
  generator->MoveMouseTo(
      apps_grid_view_->GetItemViewAt(1)->GetBoundsInScreen().CenterPoint());
  generator->ReleaseLeftButton();

  // No folder view is showing now.
  EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());
  EXPECT_FALSE(GetAppListTestHelper()->GetBubbleFolderView()->GetVisible());

  // The dragged item was moved to another folder and the reordering is
  // recorded.
  EXPECT_EQ(2u, folder_item_1->item_list()->item_count());
  EXPECT_EQ(3u, folder_item_2->item_list()->item_count());
  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kMoveIntoAnotherFolder, 1);
}

TEST_F(ScrollableAppsGridViewTest, LeftAndRightArrowKeysMoveSelection) {
  PopulateApps(2);
  ShowAppList();

  auto* apps_grid_view = GetScrollableAppsGridView();
  AppListItemView* item1 = apps_grid_view->GetItemViewAt(0);
  AppListItemView* item2 = apps_grid_view->GetItemViewAt(1);

  apps_grid_view->GetFocusManager()->SetFocusedView(item1);
  EXPECT_TRUE(item1->HasFocus());

  PressAndReleaseKey(ui::VKEY_RIGHT);
  EXPECT_FALSE(item1->HasFocus());
  EXPECT_TRUE(item2->HasFocus());

  PressAndReleaseKey(ui::VKEY_LEFT);
  EXPECT_TRUE(item1->HasFocus());
  EXPECT_FALSE(item2->HasFocus());
}

TEST_F(ScrollableAppsGridViewTest, ArrowKeysCanMoveFocusOutOfGrid) {
  PopulateApps(2);
  ShowAppList();

  auto* apps_grid_view = GetScrollableAppsGridView();
  AppListItemView* item1 = apps_grid_view->GetItemViewAt(0);
  AppListItemView* item2 = apps_grid_view->GetItemViewAt(1);

  // Moving left from the first item removes focus from the grid.
  apps_grid_view->GetFocusManager()->SetFocusedView(item1);
  PressAndReleaseKey(ui::VKEY_LEFT);
  EXPECT_FALSE(item1->HasFocus());
  EXPECT_FALSE(item2->HasFocus());

  // Moving up from the first item removes focus from the grid.
  apps_grid_view->GetFocusManager()->SetFocusedView(item1);
  PressAndReleaseKey(ui::VKEY_UP);
  EXPECT_FALSE(item1->HasFocus());
  EXPECT_FALSE(item2->HasFocus());

  // Moving right from the last item removes focus from the grid.
  apps_grid_view->GetFocusManager()->SetFocusedView(item2);
  PressAndReleaseKey(ui::VKEY_RIGHT);
  EXPECT_FALSE(item1->HasFocus());
  EXPECT_FALSE(item2->HasFocus());

  // Moving down from the last item removes focus from the grid.
  apps_grid_view->GetFocusManager()->SetFocusedView(item2);
  PressAndReleaseKey(ui::VKEY_DOWN);
  EXPECT_FALSE(item1->HasFocus());
  EXPECT_FALSE(item2->HasFocus());
}

// Tests that histograms are recorded when apps are moved with control+arrow.
TEST_F(ScrollableAppsGridViewTest, ControlArrowRecordsHistogramBasic) {
  base::HistogramTester histogram_tester;
  PopulateApps(20);
  ShowAppList();
  ScrollableAppsGridView* apps_grid_view = GetScrollableAppsGridView();

  AppListItemView* moving_item = apps_grid_view->GetItemViewAt(0);
  apps_grid_view->GetFocusManager()->SetFocusedView(moving_item);

  // Make one move right and expect a histogram is recorded.
  SimulateKeyPress(ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_RIGHT, ui::EF_NONE);

  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kReorderByKeyboardInTopLevel, 1);

  // Make one move down and expect a histogram is recorded.
  SimulateKeyPress(ui::VKEY_DOWN, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_DOWN, ui::EF_NONE);

  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kReorderByKeyboardInTopLevel, 2);

  // Make one move up and expect a histogram is recorded.
  SimulateKeyPress(ui::VKEY_UP, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_UP, ui::EF_NONE);

  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kReorderByKeyboardInTopLevel, 3);

  // Make one move left and expect a histogram is recorded.
  SimulateKeyPress(ui::VKEY_LEFT, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_LEFT, ui::EF_NONE);

  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kReorderByKeyboardInTopLevel, 4);
}

// Test that histograms do not record when the keyboard move is a no-op.
TEST_F(ScrollableAppsGridViewTest,
       ControlArrowDoesNotRecordHistogramWithNoOpMove) {
  base::HistogramTester histogram_tester;
  PopulateApps(20);
  ShowAppList();
  ScrollableAppsGridView* apps_grid_view = GetScrollableAppsGridView();

  AppListItemView* moving_item = apps_grid_view->GetItemViewAt(0);
  apps_grid_view->GetFocusManager()->SetFocusedView(moving_item);

  // Make 2 no-op moves and one successful move from 0,0 and expect a histogram
  // is recorded only once.
  SimulateKeyPress(ui::VKEY_LEFT, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_LEFT, ui::EF_NONE);

  SimulateKeyPress(ui::VKEY_UP, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_UP, ui::EF_NONE);

  SimulateKeyPress(ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_RIGHT, ui::EF_NONE);

  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kReorderByKeyboardInTopLevel, 1);
}

// Tests that histograms are recorded in folder view when apps are moved with
// control+arrow.
TEST_F(ScrollableAppsGridViewTest, ControlArrowRecordsHistogramInFolderBasic) {
  base::HistogramTester histogram_tester;
  CreateAndPopulateFolderWithApps(4);
  ShowAppList();
  ScrollableAppsGridView* apps_grid_view = GetScrollableAppsGridView();

  // Select the folder item in the grid.
  AppListItemView* folder_item_view = apps_grid_view->GetItemViewAt(0);
  EXPECT_TRUE(folder_item_view->item()->is_folder());

  // Enter the folder view.
  apps_grid_view->GetFocusManager()->SetFocusedView(folder_item_view);
  SimulateKeyPress(ui::VKEY_RETURN);
  ASSERT_TRUE(GetAppListTestHelper()->IsInFolderView());

  // If the folder view is entered by pressing return key while the focus is on
  // the folder, the focus will move to the first item inside the folder view.
  AppsGridView* folder_grid_view =
      GetAppListTestHelper()->GetBubbleFolderView()->items_grid_view();
  EXPECT_EQ(apps_grid_view->GetFocusManager()->GetFocusedView(),
            folder_grid_view->GetItemViewAt(0));

  // Make one move right and expect a histogram is recorded.
  SimulateKeyPress(ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_RIGHT, ui::EF_NONE);
  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kReorderByKeyboardInFolder, 1);

  // Make one move down and expect a histogram is recorded.
  SimulateKeyPress(ui::VKEY_DOWN, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_DOWN, ui::EF_NONE);
  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kReorderByKeyboardInFolder, 2);

  // Make one move left and expect a histogram is recorded.
  SimulateKeyPress(ui::VKEY_LEFT, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_LEFT, ui::EF_NONE);
  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kReorderByKeyboardInFolder, 3);

  // Make one move up and expect a histogram is recorded.
  SimulateKeyPress(ui::VKEY_UP, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_UP, ui::EF_NONE);
  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kReorderByKeyboardInFolder, 4);
}

// Tests that histograms do not record when the keyboard move is a no-op in the
// folder view.
TEST_F(ScrollableAppsGridViewTest,
       ControlArrowDoesNotRecordHistogramWithNoOpMoveInFolder) {
  base::HistogramTester histogram_tester;
  CreateAndPopulateFolderWithApps(4);
  ShowAppList();
  ScrollableAppsGridView* apps_grid_view = GetScrollableAppsGridView();

  // Select the folder item in the grid.
  AppListItemView* folder_item_view = apps_grid_view->GetItemViewAt(0);
  EXPECT_TRUE(folder_item_view->item()->is_folder());

  // Enter the folder view.
  apps_grid_view->GetFocusManager()->SetFocusedView(folder_item_view);
  SimulateKeyPress(ui::VKEY_RETURN);
  ASSERT_TRUE(GetAppListTestHelper()->IsInFolderView());

  // If the folder view is entered by pressing return key while the focus is on
  // the folder, the focus will move to the first item inside the folder view.
  AppsGridView* folder_grid_view =
      GetAppListTestHelper()->GetBubbleFolderView()->items_grid_view();
  EXPECT_EQ(apps_grid_view->GetFocusManager()->GetFocusedView(),
            folder_grid_view->GetItemViewAt(0));

  // Make 2 no-op moves and one successful move from 0,0 and expect a histogram
  // is recorded only once.
  SimulateKeyPress(ui::VKEY_LEFT, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_LEFT, ui::EF_NONE);

  SimulateKeyPress(ui::VKEY_UP, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_UP, ui::EF_NONE);

  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kReorderByKeyboardInFolder, 0);

  SimulateKeyPress(ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_RIGHT, ui::EF_NONE);

  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kReorderByKeyboardInFolder, 1);
}

// Tests that control + shift + arrow puts selected item into a folder or
// creates a folder if one does not exist.
TEST_F(ScrollableAppsGridViewTest, ControlShiftArrowFoldersItem) {
  base::HistogramTester histogram_tester;
  PopulateApps(20);
  ShowAppList();
  ScrollableAppsGridView* apps_grid_view = GetScrollableAppsGridView();

  // Select the first item in the grid, folder it with the item to the right.
  AppListItemView* first_item = apps_grid_view->GetItemViewAt(0);
  apps_grid_view->GetFocusManager()->SetFocusedView(first_item);
  const std::string first_item_id = first_item->item()->id();
  const std::string second_item_id =
      apps_grid_view->GetItemViewAt(1)->item()->id();
  SimulateKeyPress(ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);

  // Test that the first item in the grid is now a folder with the first and
  // second items, and that the folder is the selected view.
  AppListItemView* new_folder = apps_grid_view->GetItemViewAt(0);
  ASSERT_TRUE(apps_grid_view->IsSelectedView(new_folder));
  EXPECT_TRUE(new_folder->item()->is_folder());
  AppListFolderItem* folder_item =
      static_cast<AppListFolderItem*>(new_folder->item());
  EXPECT_EQ(2u, folder_item->ChildItemCount());
  EXPECT_TRUE(folder_item->FindChildItem(first_item_id));
  EXPECT_TRUE(folder_item->FindChildItem(second_item_id));
  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kMoveByKeyboardIntoFolder, 1);

  // Test that when a folder is selected, control+shift+arrow does nothing.
  SimulateKeyPress(ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(apps_grid_view->IsSelectedView(new_folder));
  EXPECT_EQ(2u, folder_item->ChildItemCount());
  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kMoveByKeyboardIntoFolder, 1);

  // Move selection to the item to the right of the folder and put it in the
  // folder.
  apps_grid_view->GetFocusManager()->SetFocusedView(
      apps_grid_view->GetItemViewAt(1));

  SimulateKeyPress(ui::VKEY_LEFT, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);

  EXPECT_TRUE(apps_grid_view->IsSelectedView(new_folder));
  EXPECT_EQ(3u, folder_item->ChildItemCount());
  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kMoveByKeyboardIntoFolder, 2);

  // Move selection to the item below the folder and put it in the folder.
  SimulateKeyPress(ui::VKEY_DOWN);
  SimulateKeyPress(ui::VKEY_UP, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);

  EXPECT_TRUE(apps_grid_view->IsSelectedView(new_folder));
  EXPECT_EQ(4u, folder_item->ChildItemCount());
  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kMoveByKeyboardIntoFolder, 3);

  // Move the folder to the second row, then put the item above the folder in
  // the folder.
  SimulateKeyPress(ui::VKEY_DOWN, ui::EF_CONTROL_DOWN);
  SimulateKeyPress(ui::VKEY_UP);
  SimulateKeyPress(ui::VKEY_DOWN, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);

  EXPECT_TRUE(apps_grid_view->IsSelectedView(new_folder));
  EXPECT_EQ(5u, folder_item->ChildItemCount());
  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kMoveByKeyboardIntoFolder, 4);
}

// Tests that control + shift + arrow moves selected item out of a folder.
TEST_F(ScrollableAppsGridViewTest, ControlShiftArrowMovesItemOutOfFolder) {
  base::HistogramTester histogram_tester;
  AppListFolderItem* folder_item = CreateAndPopulateFolderWithApps(5);
  ShowAppList();
  ScrollableAppsGridView* apps_grid_view = GetScrollableAppsGridView();

  // Select the folder item in the grid.
  AppListItemView* folder_item_view = apps_grid_view->GetItemViewAt(0);
  EXPECT_TRUE(folder_item_view->item()->is_folder());

  // Enter the folder view and move the item out of and to the left of the
  // folder.
  apps_grid_view->GetFocusManager()->SetFocusedView(folder_item_view);
  SimulateKeyPress(ui::VKEY_RETURN);
  ASSERT_TRUE(GetAppListTestHelper()->IsInFolderView());
  SimulateKeyPress(ui::VKEY_LEFT, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  EXPECT_EQ(apps_grid_view->selected_view(), apps_grid_view->GetItemViewAt(0));
  EXPECT_EQ(4u, folder_item->ChildItemCount());
  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kMoveByKeyboardOutOfFolder, 1);

  // Enter the folder view and move the item out of and to the right of the
  // folder.
  apps_grid_view->GetFocusManager()->SetFocusedView(folder_item_view);
  SimulateKeyPress(ui::VKEY_RETURN);
  ASSERT_TRUE(GetAppListTestHelper()->IsInFolderView());
  SimulateKeyPress(ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  EXPECT_EQ(apps_grid_view->selected_view(), apps_grid_view->GetItemViewAt(2));
  EXPECT_EQ(3u, folder_item->ChildItemCount());
  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kMoveByKeyboardOutOfFolder, 2);

  // Enter the folder view and move the item out of and to the above of the
  // folder.
  apps_grid_view->GetFocusManager()->SetFocusedView(folder_item_view);
  SimulateKeyPress(ui::VKEY_RETURN);
  SimulateKeyPress(ui::VKEY_UP, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  EXPECT_EQ(apps_grid_view->selected_view(), apps_grid_view->GetItemViewAt(1));
  EXPECT_EQ(2u, folder_item->ChildItemCount());
  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kMoveByKeyboardOutOfFolder, 3);

  // Enter the folder view and move the item out of and to the below of the
  // folder.
  apps_grid_view->GetFocusManager()->SetFocusedView(folder_item_view);
  SimulateKeyPress(ui::VKEY_RETURN);
  ASSERT_TRUE(GetAppListTestHelper()->IsInFolderView());
  SimulateKeyPress(ui::VKEY_DOWN, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  EXPECT_EQ(apps_grid_view->selected_view(), apps_grid_view->GetItemViewAt(3));
  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kMoveByKeyboardOutOfFolder, 4);
  EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());
}

}  // namespace ash
