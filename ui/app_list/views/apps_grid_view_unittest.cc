// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/apps_grid_view.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_features.h"
#include "ui/app_list/app_list_folder_item.h"
#include "ui/app_list/app_list_item.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/pagination_model.h"
#include "ui/app_list/test/app_list_test_model.h"
#include "ui/app_list/test/app_list_test_view_delegate.h"
#include "ui/app_list/test/test_search_result.h"
#include "ui/app_list/views/app_list_item_view.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/app_list/views/apps_container_view.h"
#include "ui/app_list/views/apps_grid_view_folder_delegate.h"
#include "ui/app_list/views/contents_view.h"
#include "ui/app_list/views/suggestions_container_view.h"
#include "ui/app_list/views/test/apps_grid_view_test_api.h"
#include "ui/events/event_utils.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/views_test_base.h"

namespace app_list {
namespace test {

namespace {

constexpr int kNumOfSuggestedApps = 3;

class PageFlipWaiter : public PaginationModelObserver {
 public:
  explicit PageFlipWaiter(PaginationModel* model) : model_(model) {
    model_->AddObserver(this);
  }

  ~PageFlipWaiter() override { model_->RemoveObserver(this); }

  void Wait() {
    DCHECK(!wait_);
    wait_ = true;

    ui_run_loop_.reset(new base::RunLoop);
    ui_run_loop_->Run();
    wait_ = false;
  }

  void Reset() { selected_pages_.clear(); }

  const std::string& selected_pages() const { return selected_pages_; }

 private:
  // PaginationModelObserver overrides:
  void TotalPagesChanged() override {}
  void SelectedPageChanged(int old_selected, int new_selected) override {
    if (!selected_pages_.empty())
      selected_pages_ += ',';
    selected_pages_ += base::IntToString(new_selected);

    if (wait_)
      ui_run_loop_->QuitWhenIdle();
  }
  void TransitionStarted() override {}
  void TransitionChanged() override {}

  std::unique_ptr<base::RunLoop> ui_run_loop_;
  PaginationModel* model_ = nullptr;
  bool wait_ = false;
  std::string selected_pages_;

  DISALLOW_COPY_AND_ASSIGN(PageFlipWaiter);
};

class TestSuggestedSearchResult : public TestSearchResult {
 public:
  TestSuggestedSearchResult() { set_display_type(DISPLAY_RECOMMENDATION); }
  ~TestSuggestedSearchResult() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestSuggestedSearchResult);
};

}  // namespace

class AppsGridViewTest : public views::ViewsTestBase,
                         public testing::WithParamInterface<bool> {
 public:
  AppsGridViewTest() = default;
  ~AppsGridViewTest() override = default;

  // testing::Test overrides:
  void SetUp() override {
    views::ViewsTestBase::SetUp();

    // If the current test is parameterized.
    if (testing::UnitTest::GetInstance()->current_test_info()->value_param()) {
      test_with_fullscreen_ = GetParam();
      if (test_with_fullscreen_)
        EnableFullscreenAppList();
    }

    gfx::NativeView parent = GetContext();
    delegate_.reset(new AppListTestViewDelegate);
    app_list_view_ = new AppListView(delegate_.get());
    app_list_view_->set_app_list_animation_duration_ms_for_testing(0);

    app_list_view_->Initialize(parent, 0, false, false);
    ContentsView* contents_view =
        app_list_view_->app_list_main_view()->contents_view();
    apps_grid_view_ = contents_view->apps_container_view()->apps_grid_view();
    // Initialize around a point that ensures the window is wholly shown. It
    // bails out early with |test_with_fullscreen_|.
    // TODO(warx): remove MaybeSetAnchorPoint setup here when bubble launcher is
    // removed from code base.
    gfx::Size size = apps_grid_view_->GetPreferredSize();
    app_list_view_->MaybeSetAnchorPoint(
        gfx::Point(size.width() / 2, size.height() / 2));
    app_list_view_->GetWidget()->Show();

    model_ = delegate_->GetTestModel();
    suggestions_container_ = apps_grid_view_->suggestions_container_for_test();
    for (size_t i = 0; i < kNumOfSuggestedApps; ++i)
      model_->results()->Add(base::MakeUnique<TestSuggestedSearchResult>());
    // Needed to update suggestions from |model_|.
    apps_grid_view_->ResetForShowApps();

    if (test_with_fullscreen_) {
      app_list_view_->SetState(AppListView::FULLSCREEN_ALL_APPS);
      app_list_view_->Layout();
    } else {
      // Set app list view to show all apps page to test AppsGridView.
      contents_view->SetActiveState(AppListModel::STATE_APPS);
      contents_view->Layout();
    }

    test_api_.reset(new AppsGridViewTestApi(apps_grid_view_));
  }
  void TearDown() override {
    app_list_view_->GetWidget()->Close();
    views::ViewsTestBase::TearDown();
  }

 protected:
  void EnableFullscreenAppList() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kEnableFullscreenAppList);
  }

  AppListItemView* GetItemViewAt(int index) const {
    return static_cast<AppListItemView*>(test_api_->GetViewAtModelIndex(index));
  }

  AppListItemView* GetItemViewForPoint(const gfx::Point& point) const {
    for (size_t i = 0; i < model_->top_level_item_list()->item_count(); ++i) {
      AppListItemView* view = GetItemViewAt(i);
      if (view->bounds().Contains(point))
        return view;
    }
    return nullptr;
  }

  gfx::Rect GetItemTileRectAt(int row, int col) const {
    DCHECK_GT(model_->top_level_item_list()->item_count(), 0u);
    return test_api_->GetItemTileRectAt(row, col);
  }

  int GetTilesPerPage(int page) const { return test_api_->TilesPerPage(page); }

  PaginationModel* GetPaginationModel() const {
    return apps_grid_view_->pagination_model();
  }

  // Points are in |apps_grid_view_|'s coordinates.
  AppListItemView* SimulateDrag(AppsGridView::Pointer pointer,
                                const gfx::Point& from,
                                const gfx::Point& to) {
    AppListItemView* view = GetItemViewForPoint(from);
    DCHECK(view);

    gfx::Point translated_from =
        gfx::PointAtOffsetFromOrigin(from - view->origin());
    gfx::Point translated_to =
        gfx::PointAtOffsetFromOrigin(to - view->origin());
    apps_grid_view_->InitiateDrag(view, pointer, translated_from, from);

    ui::MouseEvent drag_event(ui::ET_MOUSE_DRAGGED, translated_to, to,
                              ui::EventTimeForNow(), 0, 0);
    apps_grid_view_->UpdateDragFromItem(pointer, drag_event);
    return view;
  }

  void SimulateKeyPress(ui::KeyboardCode key_code) {
    ui::KeyEvent key_event(ui::ET_KEY_PRESSED, key_code, ui::EF_NONE);
    apps_grid_view_->OnKeyPressed(key_event);
  }

  AppListView* app_list_view_ = nullptr;    // Owned by native widget.
  AppsGridView* apps_grid_view_ = nullptr;  // Owned by |app_list_view_|.
  SuggestionsContainerView* suggestions_container_ =
      nullptr;  // Owned by |apps_grid_view_|.
  std::unique_ptr<AppListTestViewDelegate> delegate_;
  AppListTestModel* model_ = nullptr;  // Owned by |delegate_|.
  std::unique_ptr<AppsGridViewTestApi> test_api_;
  bool test_with_fullscreen_ = false;
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppsGridViewTest);
};

// Instantiate the Boolean which is used to toggle the Fullscreen app list in
// the parameterized tests.
INSTANTIATE_TEST_CASE_P(, AppsGridViewTest, testing::Bool());

class TestAppsGridViewFolderDelegate : public AppsGridViewFolderDelegate {
 public:
  TestAppsGridViewFolderDelegate() = default;
  ~TestAppsGridViewFolderDelegate() override = default;

  // Overridden from AppsGridViewFolderDelegate:
  void UpdateFolderViewBackground(bool show_bubble) override {
    show_bubble_ = show_bubble;
  }

  void ReparentItem(AppListItemView* original_drag_view,
                    const gfx::Point& drag_point_in_folder_grid,
                    bool has_native_drag) override {}

  void DispatchDragEventForReparent(
      AppsGridView::Pointer pointer,
      const gfx::Point& drag_point_in_folder_grid) override {}

  void DispatchEndDragEventForReparent(bool events_forwarded_to_drag_drop_host,
                                       bool cancel_drag) override {}

  bool IsPointOutsideOfFolderBoundary(const gfx::Point& point) override {
    return false;
  }

  bool IsOEMFolder() const override { return false; }

  void SetRootLevelDragViewVisible(bool visible) override {}

  bool show_bubble() { return show_bubble_; }

 private:
  bool show_bubble_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestAppsGridViewFolderDelegate);
};

TEST_P(AppsGridViewTest, CreatePage) {
  // Fully populates a page.
  const int kPages = 1;
  if (test_with_fullscreen_) {
    EXPECT_EQ(kNumOfSuggestedApps, suggestions_container_->num_results());
    int kExpectedTilesOnFirstPage =
        apps_grid_view_->cols() * (apps_grid_view_->rows_per_page() - 1);
    EXPECT_EQ(kExpectedTilesOnFirstPage, GetTilesPerPage(kPages - 1));
  }
  model_->PopulateApps(kPages * GetTilesPerPage(kPages - 1));
  EXPECT_EQ(kPages, GetPaginationModel()->total_pages());

  // Adds one more and gets a new page created.
  model_->CreateAndAddItem("Extra");
  EXPECT_EQ(kPages + 1, GetPaginationModel()->total_pages());
}

TEST_P(AppsGridViewTest, EnsureHighlightedVisible) {
  const int kPages = 3;
  model_->PopulateApps(GetTilesPerPage(0) + (kPages - 1) * GetTilesPerPage(1));
  EXPECT_EQ(kPages, GetPaginationModel()->total_pages());
  EXPECT_EQ(0, GetPaginationModel()->selected_page());

  // Highlight first one and last one one first page and first page should be
  // selected.
  model_->HighlightItemAt(0);
  EXPECT_EQ(0, GetPaginationModel()->selected_page());
  model_->HighlightItemAt(GetTilesPerPage(0) - 1);
  EXPECT_EQ(0, GetPaginationModel()->selected_page());

  // Highlight first one on 2nd page and 2nd page should be selected.
  model_->HighlightItemAt(GetTilesPerPage(1) + 1);
  EXPECT_EQ(1, GetPaginationModel()->selected_page());

  // Highlight last one in the model and last page should be selected.
  model_->HighlightItemAt(model_->top_level_item_list()->item_count() - 1);
  EXPECT_EQ(kPages - 1, GetPaginationModel()->selected_page());
}

TEST_P(AppsGridViewTest, RemoveSelectedLastApp) {
  const int kTotalItems = 2;
  const int kLastItemIndex = kTotalItems - 1;

  model_->PopulateApps(kTotalItems);

  AppListItemView* last_view = GetItemViewAt(kLastItemIndex);
  apps_grid_view_->SetSelectedView(last_view);
  model_->DeleteItem(model_->GetItemName(kLastItemIndex));

  EXPECT_FALSE(apps_grid_view_->IsSelectedView(last_view));

  // No crash happens.
  AppListItemView* view = GetItemViewAt(0);
  apps_grid_view_->SetSelectedView(view);
  EXPECT_TRUE(apps_grid_view_->IsSelectedView(view));
}

TEST_P(AppsGridViewTest, MouseDragWithFolderDisabled) {
  model_->SetFoldersEnabled(false);
  const int kTotalItems = 4;
  model_->PopulateApps(kTotalItems);
  EXPECT_EQ(std::string("Item 0,Item 1,Item 2,Item 3"),
            model_->GetModelContent());

  gfx::Point from = GetItemTileRectAt(0, 0).CenterPoint();
  gfx::Point to = GetItemTileRectAt(0, 1).CenterPoint();

  // Dragging changes model order.
  SimulateDrag(AppsGridView::MOUSE, from, to);
  apps_grid_view_->EndDrag(false);
  EXPECT_EQ(std::string("Item 1,Item 0,Item 2,Item 3"),
            model_->GetModelContent());
  test_api_->LayoutToIdealBounds();

  // Canceling drag should keep existing order.
  SimulateDrag(AppsGridView::MOUSE, from, to);
  apps_grid_view_->EndDrag(true);
  EXPECT_EQ(std::string("Item 1,Item 0,Item 2,Item 3"),
            model_->GetModelContent());
  test_api_->LayoutToIdealBounds();

  // Deleting an item keeps remaining intact.
  SimulateDrag(AppsGridView::MOUSE, from, to);
  model_->DeleteItem(model_->GetItemName(0));
  apps_grid_view_->EndDrag(false);
  EXPECT_EQ(std::string("Item 1,Item 2,Item 3"), model_->GetModelContent());
  test_api_->LayoutToIdealBounds();

  // Adding a launcher item cancels the drag and respects the order.
  SimulateDrag(AppsGridView::MOUSE, from, to);
  EXPECT_TRUE(apps_grid_view_->has_dragged_view());
  model_->CreateAndAddItem("Extra");
  // No need to EndDrag explicitly - adding an item should do this.
  EXPECT_FALSE(apps_grid_view_->has_dragged_view());
  // Even though cancelled, mouse move events can still arrive via the item
  // view. Ensure that behaves sanely, and doesn't start a new drag.
  ui::MouseEvent drag_event(ui::ET_MOUSE_DRAGGED, gfx::Point(1, 1),
                            gfx::Point(2, 2), ui::EventTimeForNow(), 0, 0);
  apps_grid_view_->UpdateDragFromItem(AppsGridView::MOUSE, drag_event);
  EXPECT_FALSE(apps_grid_view_->has_dragged_view());

  EXPECT_EQ(std::string("Item 1,Item 2,Item 3,Extra"),
            model_->GetModelContent());
  test_api_->LayoutToIdealBounds();
}

TEST_P(AppsGridViewTest, MouseDragItemIntoFolder) {
  size_t kTotalItems = 3;
  model_->PopulateApps(kTotalItems);
  EXPECT_EQ(model_->top_level_item_list()->item_count(), kTotalItems);
  EXPECT_EQ(std::string("Item 0,Item 1,Item 2"), model_->GetModelContent());

  gfx::Point from = GetItemTileRectAt(0, 1).CenterPoint();
  gfx::Point to = GetItemTileRectAt(0, 0).CenterPoint();

  // Dragging item_1 over item_0 creates a folder.
  SimulateDrag(AppsGridView::MOUSE, from, to);
  apps_grid_view_->EndDrag(false);
  EXPECT_EQ(kTotalItems - 1, model_->top_level_item_list()->item_count());
  EXPECT_EQ(AppListFolderItem::kItemType,
            model_->top_level_item_list()->item_at(0)->GetItemType());
  AppListFolderItem* folder_item = static_cast<AppListFolderItem*>(
      model_->top_level_item_list()->item_at(0));
  EXPECT_EQ(2u, folder_item->ChildItemCount());
  AppListItem* item_0 = model_->FindItem("Item 0");
  EXPECT_TRUE(item_0->IsInFolder());
  EXPECT_EQ(folder_item->id(), item_0->folder_id());
  AppListItem* item_1 = model_->FindItem("Item 1");
  EXPECT_TRUE(item_1->IsInFolder());
  EXPECT_EQ(folder_item->id(), item_1->folder_id());
  std::string expected_items = folder_item->id() + ",Item 2";
  EXPECT_EQ(expected_items, model_->GetModelContent());
  test_api_->LayoutToIdealBounds();

  // Dragging item_2 to the folder adds item_2 to the folder.
  SimulateDrag(AppsGridView::MOUSE, from, to);
  apps_grid_view_->EndDrag(false);

  EXPECT_EQ(kTotalItems - 2, model_->top_level_item_list()->item_count());
  EXPECT_EQ(folder_item->id(), model_->GetModelContent());
  EXPECT_EQ(3u, folder_item->ChildItemCount());
  item_0 = model_->FindItem("Item 0");
  EXPECT_TRUE(item_0->IsInFolder());
  EXPECT_EQ(folder_item->id(), item_0->folder_id());
  item_1 = model_->FindItem("Item 1");
  EXPECT_TRUE(item_1->IsInFolder());
  EXPECT_EQ(folder_item->id(), item_1->folder_id());
  AppListItem* item_2 = model_->FindItem("Item 2");
  EXPECT_TRUE(item_2->IsInFolder());
  EXPECT_EQ(folder_item->id(), item_2->folder_id());
  test_api_->LayoutToIdealBounds();
}

TEST_P(AppsGridViewTest, MouseDragMaxItemsInFolder) {
  // Create and add a folder with 15 items in it.
  size_t kTotalItems = kMaxFolderItems - 1;
  model_->CreateAndPopulateFolderWithApps(kTotalItems);
  EXPECT_EQ(1u, model_->top_level_item_list()->item_count());
  EXPECT_EQ(AppListFolderItem::kItemType,
            model_->top_level_item_list()->item_at(0)->GetItemType());
  AppListFolderItem* folder_item = static_cast<AppListFolderItem*>(
      model_->top_level_item_list()->item_at(0));
  EXPECT_EQ(kTotalItems, folder_item->ChildItemCount());

  // Create and add another 2 items.
  model_->PopulateAppWithId(kTotalItems);
  model_->PopulateAppWithId(kTotalItems + 1);
  EXPECT_EQ(3u, model_->top_level_item_list()->item_count());
  EXPECT_EQ(folder_item->id(), model_->top_level_item_list()->item_at(0)->id());
  EXPECT_EQ(model_->GetItemName(kMaxFolderItems - 1),
            model_->top_level_item_list()->item_at(1)->id());
  EXPECT_EQ(model_->GetItemName(kMaxFolderItems),
            model_->top_level_item_list()->item_at(2)->id());

  gfx::Point from = GetItemTileRectAt(0, 1).CenterPoint();
  gfx::Point to = GetItemTileRectAt(0, 0).CenterPoint();

  // Dragging one item into the folder, the folder should accept the item.
  SimulateDrag(AppsGridView::MOUSE, from, to);
  apps_grid_view_->EndDrag(false);
  EXPECT_EQ(2u, model_->top_level_item_list()->item_count());
  EXPECT_EQ(folder_item->id(), model_->top_level_item_list()->item_at(0)->id());
  EXPECT_EQ(kMaxFolderItems, folder_item->ChildItemCount());
  EXPECT_EQ(model_->GetItemName(kMaxFolderItems),
            model_->top_level_item_list()->item_at(1)->id());
  test_api_->LayoutToIdealBounds();

  // Dragging the last item over the folder, the folder won't accept the new
  // item.
  SimulateDrag(AppsGridView::MOUSE, from, to);
  apps_grid_view_->EndDrag(false);
  EXPECT_EQ(2u, model_->top_level_item_list()->item_count());
  EXPECT_EQ(kMaxFolderItems, folder_item->ChildItemCount());
  test_api_->LayoutToIdealBounds();
}

// Check that moving items around doesn't allow a drop to happen into a full
// folder.
TEST_P(AppsGridViewTest, MouseDragMaxItemsInFolderWithMovement) {
  // Create and add a folder with 16 items in it.
  size_t kTotalItems = kMaxFolderItems;
  model_->CreateAndPopulateFolderWithApps(kTotalItems);
  EXPECT_EQ(1u, model_->top_level_item_list()->item_count());
  EXPECT_EQ(AppListFolderItem::kItemType,
            model_->top_level_item_list()->item_at(0)->GetItemType());
  AppListFolderItem* folder_item = static_cast<AppListFolderItem*>(
      model_->top_level_item_list()->item_at(0));
  EXPECT_EQ(kTotalItems, folder_item->ChildItemCount());

  // Create and add another item.
  model_->PopulateAppWithId(kTotalItems);
  EXPECT_EQ(2u, model_->top_level_item_list()->item_count());
  EXPECT_EQ(folder_item->id(), model_->top_level_item_list()->item_at(0)->id());
  EXPECT_EQ(model_->GetItemName(kMaxFolderItems),
            model_->top_level_item_list()->item_at(1)->id());

  AppListItemView* folder_view =
      GetItemViewForPoint(GetItemTileRectAt(0, 0).CenterPoint());

  // Drag the new item to the left so that the grid reorders.
  gfx::Point from = GetItemTileRectAt(0, 1).CenterPoint();
  gfx::Point to = GetItemTileRectAt(0, 0).bottom_left();
  to.Offset(0, -1);  // Get a point inside the rect.
  AppListItemView* dragged_view = SimulateDrag(AppsGridView::MOUSE, from, to);
  test_api_->LayoutToIdealBounds();

  // The grid now looks like | blank | folder |.
  EXPECT_EQ(nullptr,
            GetItemViewForPoint(GetItemTileRectAt(0, 0).CenterPoint()));
  EXPECT_EQ(folder_view,
            GetItemViewForPoint(GetItemTileRectAt(0, 1).CenterPoint()));

  // Move onto the folder and end the drag.
  to = GetItemTileRectAt(0, 1).CenterPoint();
  gfx::Point translated_to =
      gfx::PointAtOffsetFromOrigin(to - dragged_view->origin());
  ui::MouseEvent drag_event(ui::ET_MOUSE_DRAGGED, translated_to, to,
                            ui::EventTimeForNow(), 0, 0);
  apps_grid_view_->UpdateDragFromItem(AppsGridView::MOUSE, drag_event);
  apps_grid_view_->EndDrag(false);

  // The item should not have moved into the folder.
  EXPECT_EQ(2u, model_->top_level_item_list()->item_count());
  EXPECT_EQ(kMaxFolderItems, folder_item->ChildItemCount());
  test_api_->LayoutToIdealBounds();
}

TEST_P(AppsGridViewTest, MouseDragItemReorder) {
  // Using a simulated 2x2 layout for the test. If fullscreen app list is
  // enabled, rows_per_page passed should be 3 as the first row is occupied by
  // suggested apps.
  apps_grid_view_->SetLayout(2, test_with_fullscreen_ ? 3 : 2);
  model_->PopulateApps(4);
  EXPECT_EQ(4u, model_->top_level_item_list()->item_count());
  EXPECT_EQ(std::string("Item 0,Item 1,Item 2,Item 3"),
            model_->GetModelContent());

  // Dragging an item towards its neighbours should not reorder until the drag
  // is past the folder drop point.
  gfx::Point top_right = GetItemTileRectAt(0, 1).CenterPoint();
  gfx::Vector2d drag_vector;
  int half_tile_width =
      (GetItemTileRectAt(0, 1).x() - GetItemTileRectAt(0, 0).x()) / 2;
  int tile_height = GetItemTileRectAt(1, 0).y() - GetItemTileRectAt(0, 0).y();

  // Drag left but stop before the folder dropping circle.
  drag_vector.set_x(-half_tile_width - 4);
  SimulateDrag(AppsGridView::MOUSE, top_right, top_right + drag_vector);
  apps_grid_view_->EndDrag(false);
  EXPECT_EQ(std::string("Item 0,Item 1,Item 2,Item 3"),
            model_->GetModelContent());

  // Drag left, past the folder dropping circle.
  gfx::Vector2d last_drag_vector(drag_vector);
  drag_vector.set_x(-3 * half_tile_width + 4);
  SimulateDrag(AppsGridView::MOUSE, top_right + last_drag_vector,
               top_right + drag_vector);
  apps_grid_view_->EndDrag(false);
  EXPECT_EQ(std::string("Item 1,Item 0,Item 2,Item 3"),
            model_->GetModelContent());

  // Drag down, between apps 2 and 3. The gap should open up, making space for
  // app 1 in the bottom left.
  last_drag_vector = drag_vector;
  drag_vector.set_x(-half_tile_width);
  drag_vector.set_y(tile_height);
  SimulateDrag(AppsGridView::MOUSE, top_right + last_drag_vector,
               top_right + drag_vector);
  apps_grid_view_->EndDrag(false);
  EXPECT_EQ(std::string("Item 0,Item 2,Item 1,Item 3"),
            model_->GetModelContent());

  // Drag up, between apps 0 and 2. The gap should open up, making space for app
  // 1 in the top right.
  last_drag_vector = drag_vector;
  drag_vector.set_x(-half_tile_width);
  drag_vector.set_y(0);
  SimulateDrag(AppsGridView::MOUSE, top_right + last_drag_vector,
               top_right + drag_vector);
  apps_grid_view_->EndDrag(false);
  EXPECT_EQ(std::string("Item 0,Item 1,Item 2,Item 3"),
            model_->GetModelContent());

  // Dragging down past the last app should reorder to the last position.
  last_drag_vector = drag_vector;
  drag_vector.set_x(half_tile_width);
  drag_vector.set_y(2 * tile_height);
  SimulateDrag(AppsGridView::MOUSE, top_right + last_drag_vector,
               top_right + drag_vector);
  apps_grid_view_->EndDrag(false);
  EXPECT_EQ(std::string("Item 0,Item 2,Item 3,Item 1"),
            model_->GetModelContent());
}

TEST_P(AppsGridViewTest, MouseDragFolderReorder) {
  size_t kTotalItems = 2;
  model_->CreateAndPopulateFolderWithApps(kTotalItems);
  model_->PopulateAppWithId(kTotalItems);
  EXPECT_EQ(2u, model_->top_level_item_list()->item_count());
  EXPECT_EQ(AppListFolderItem::kItemType,
            model_->top_level_item_list()->item_at(0)->GetItemType());
  AppListFolderItem* folder_item = static_cast<AppListFolderItem*>(
      model_->top_level_item_list()->item_at(0));
  EXPECT_EQ("Item 2", model_->top_level_item_list()->item_at(1)->id());

  gfx::Point from = GetItemTileRectAt(0, 0).CenterPoint();
  gfx::Point to = GetItemTileRectAt(0, 1).CenterPoint();

  // Dragging folder over item_1 should leads to re-ordering these two
  // items.
  SimulateDrag(AppsGridView::MOUSE, from, to);
  apps_grid_view_->EndDrag(false);
  EXPECT_EQ(2u, model_->top_level_item_list()->item_count());
  EXPECT_EQ("Item 2", model_->top_level_item_list()->item_at(0)->id());
  EXPECT_EQ(folder_item->id(), model_->top_level_item_list()->item_at(1)->id());
  test_api_->LayoutToIdealBounds();
}

TEST_P(AppsGridViewTest, MouseDragWithCancelDeleteAddItem) {
  size_t kTotalItems = 4;
  model_->PopulateApps(kTotalItems);
  EXPECT_EQ(model_->top_level_item_list()->item_count(), kTotalItems);
  EXPECT_EQ(std::string("Item 0,Item 1,Item 2,Item 3"),
            model_->GetModelContent());

  gfx::Point from = GetItemTileRectAt(0, 0).CenterPoint();
  gfx::Point to = GetItemTileRectAt(0, 1).CenterPoint();

  // Canceling drag should keep existing order.
  SimulateDrag(AppsGridView::MOUSE, from, to);
  apps_grid_view_->EndDrag(true);
  EXPECT_EQ(std::string("Item 0,Item 1,Item 2,Item 3"),
            model_->GetModelContent());
  test_api_->LayoutToIdealBounds();

  // Deleting an item keeps remaining intact.
  SimulateDrag(AppsGridView::MOUSE, from, to);
  model_->DeleteItem(model_->GetItemName(2));
  apps_grid_view_->EndDrag(false);
  EXPECT_EQ(std::string("Item 0,Item 1,Item 3"), model_->GetModelContent());
  test_api_->LayoutToIdealBounds();

  // Adding a launcher item cancels the drag and respects the order.
  SimulateDrag(AppsGridView::MOUSE, from, to);
  model_->CreateAndAddItem("Extra");
  apps_grid_view_->EndDrag(false);
  EXPECT_EQ(std::string("Item 0,Item 1,Item 3,Extra"),
            model_->GetModelContent());
  test_api_->LayoutToIdealBounds();
}

// TODO(warx): enable this test for |test_with_fullscreen_|, crbug.com/742581.
TEST_F(AppsGridViewTest, MouseDragFlipPage) {
  apps_grid_view_->set_page_flip_delay_in_ms_for_testing(10);
  GetPaginationModel()->SetTransitionDurations(10, 10);

  PageFlipWaiter page_flip_waiter(GetPaginationModel());

  const int kPages = 3;
  model_->PopulateApps(GetTilesPerPage(0) + (kPages - 1) * GetTilesPerPage(1));
  EXPECT_EQ(kPages, GetPaginationModel()->total_pages());
  EXPECT_EQ(0, GetPaginationModel()->selected_page());

  gfx::Point from = GetItemTileRectAt(0, 0).CenterPoint();
  gfx::Point to =
      gfx::Point(apps_grid_view_->width(), apps_grid_view_->height() / 2);

  // Drag to right edge.
  page_flip_waiter.Reset();
  SimulateDrag(AppsGridView::MOUSE, from, to);

  // Page should be flipped after sometime to hit page 1 and 2 then stop.
  while (test_api_->HasPendingPageFlip()) {
    page_flip_waiter.Wait();
  }
  EXPECT_EQ("1,2", page_flip_waiter.selected_pages());
  EXPECT_EQ(2, GetPaginationModel()->selected_page());

  apps_grid_view_->EndDrag(true);

  // Now drag to the left edge and test the other direction.
  to.set_x(0);

  page_flip_waiter.Reset();
  SimulateDrag(AppsGridView::MOUSE, from, to);

  while (test_api_->HasPendingPageFlip()) {
    page_flip_waiter.Wait();
  }
  EXPECT_EQ("1,0", page_flip_waiter.selected_pages());
  EXPECT_EQ(0, GetPaginationModel()->selected_page());

  apps_grid_view_->EndDrag(true);
}

TEST_P(AppsGridViewTest, SimultaneousDragWithFolderDisabled) {
  model_->SetFoldersEnabled(false);
  const int kTotalItems = 4;
  model_->PopulateApps(kTotalItems);
  EXPECT_EQ(std::string("Item 0,Item 1,Item 2,Item 3"),
            model_->GetModelContent());

  gfx::Point mouse_from = GetItemTileRectAt(0, 0).CenterPoint();
  gfx::Point mouse_to = GetItemTileRectAt(0, 1).CenterPoint();

  gfx::Point touch_from = GetItemTileRectAt(0, 2).CenterPoint();
  gfx::Point touch_to = GetItemTileRectAt(0, 3).CenterPoint();

  // Starts a mouse drag first then a touch drag.
  SimulateDrag(AppsGridView::MOUSE, mouse_from, mouse_to);
  SimulateDrag(AppsGridView::TOUCH, touch_from, touch_to);
  // Finishes the drag and mouse drag wins.
  apps_grid_view_->EndDrag(false);
  EXPECT_EQ(std::string("Item 1,Item 0,Item 2,Item 3"),
            model_->GetModelContent());
  test_api_->LayoutToIdealBounds();

  // Starts a touch drag first then a mouse drag.
  SimulateDrag(AppsGridView::TOUCH, touch_from, touch_to);
  SimulateDrag(AppsGridView::MOUSE, mouse_from, mouse_to);
  // Finishes the drag and touch drag wins.
  apps_grid_view_->EndDrag(false);
  EXPECT_EQ(std::string("Item 1,Item 0,Item 3,Item 2"),
            model_->GetModelContent());
  test_api_->LayoutToIdealBounds();
}

TEST_P(AppsGridViewTest, UpdateFolderBackgroundOnCancelDrag) {
  const int kTotalItems = 4;
  TestAppsGridViewFolderDelegate folder_delegate;
  apps_grid_view_->set_folder_delegate(&folder_delegate);
  model_->PopulateApps(kTotalItems);
  EXPECT_EQ(std::string("Item 0,Item 1,Item 2,Item 3"),
            model_->GetModelContent());

  gfx::Point mouse_from = GetItemTileRectAt(0, 0).CenterPoint();
  gfx::Point mouse_to = GetItemTileRectAt(0, 1).CenterPoint();

  // Starts a mouse drag and then cancels it.
  SimulateDrag(AppsGridView::MOUSE, mouse_from, mouse_to);
  EXPECT_TRUE(folder_delegate.show_bubble());
  apps_grid_view_->EndDrag(true);
  EXPECT_FALSE(folder_delegate.show_bubble());
  EXPECT_EQ(std::string("Item 0,Item 1,Item 2,Item 3"),
            model_->GetModelContent());
}

TEST_P(AppsGridViewTest, HighlightWithKeyboard) {
  if (test_with_fullscreen_)
    return;

  const int kPages = 3;
  const int kItems = GetTilesPerPage(0) + GetTilesPerPage(1) + 1;
  model_->PopulateApps(kItems);
  EXPECT_EQ(kPages, GetPaginationModel()->total_pages());

  const int first_index = 0;
  const int last_index = kItems - 1;
  const int last_index_on_page1_first_row =
      apps_grid_view_->rows_per_page() - 1;
  const int last_index_on_page1 = GetTilesPerPage(0) - 1;
  const int first_index_on_page2 = GetTilesPerPage(1);
  const int first_index_on_page2_last_row =
      2 * GetTilesPerPage(1) - apps_grid_view_->cols();
  const int last_index_on_page2_last_row = 2 * GetTilesPerPage(1) - 1;

  // Try moving off the item beyond the first one.
  apps_grid_view_->SetSelectedView(GetItemViewAt(first_index));
  SimulateKeyPress(ui::VKEY_UP);
  EXPECT_TRUE(apps_grid_view_->IsSelectedView(GetItemViewAt(first_index)));
  SimulateKeyPress(ui::VKEY_LEFT);
  EXPECT_TRUE(apps_grid_view_->IsSelectedView(GetItemViewAt(first_index)));

  // Move to the last item and try to go past it.
  apps_grid_view_->SetSelectedView(GetItemViewAt(last_index));
  SimulateKeyPress(ui::VKEY_DOWN);
  EXPECT_TRUE(apps_grid_view_->IsSelectedView(GetItemViewAt(last_index)));
  SimulateKeyPress(ui::VKEY_RIGHT);
  EXPECT_TRUE(apps_grid_view_->IsSelectedView(GetItemViewAt(last_index)));

  // Move right on last item on page 1 should get to first item on page 2's last
  // row and vice versa.
  apps_grid_view_->SetSelectedView(GetItemViewAt(last_index_on_page1));
  SimulateKeyPress(ui::VKEY_RIGHT);
  EXPECT_TRUE(apps_grid_view_->IsSelectedView(
      GetItemViewAt(first_index_on_page2_last_row)));
  SimulateKeyPress(ui::VKEY_LEFT);
  EXPECT_TRUE(
      apps_grid_view_->IsSelectedView(GetItemViewAt(last_index_on_page1)));

  // Up/down on page boundary does nothing.
  apps_grid_view_->SetSelectedView(GetItemViewAt(last_index_on_page1));
  SimulateKeyPress(ui::VKEY_DOWN);
  EXPECT_TRUE(
      apps_grid_view_->IsSelectedView(GetItemViewAt(last_index_on_page1)));
  apps_grid_view_->SetSelectedView(
      GetItemViewAt(first_index_on_page2_last_row));
  apps_grid_view_->SetSelectedView(
      GetItemViewAt(last_index_on_page1_first_row));
  SimulateKeyPress(ui::VKEY_UP);
  EXPECT_TRUE(apps_grid_view_->IsSelectedView(
      GetItemViewAt(last_index_on_page1_first_row)));

  // Page up and down should go to the same item on the next and last page.
  apps_grid_view_->SetSelectedView(GetItemViewAt(first_index_on_page2));
  SimulateKeyPress(ui::VKEY_PRIOR);
  EXPECT_TRUE(apps_grid_view_->IsSelectedView(GetItemViewAt(first_index)));
  SimulateKeyPress(ui::VKEY_NEXT);
  EXPECT_TRUE(
      apps_grid_view_->IsSelectedView(GetItemViewAt(first_index_on_page2)));

  // Moving onto a page with too few apps to support the expected index snaps
  // to the last available index.
  apps_grid_view_->SetSelectedView(GetItemViewAt(last_index_on_page2_last_row));
  SimulateKeyPress(ui::VKEY_RIGHT);
  EXPECT_TRUE(apps_grid_view_->IsSelectedView(GetItemViewAt(last_index)));
  apps_grid_view_->SetSelectedView(GetItemViewAt(last_index_on_page2_last_row));
  SimulateKeyPress(ui::VKEY_NEXT);
  EXPECT_TRUE(apps_grid_view_->IsSelectedView(GetItemViewAt(last_index)));

  // After page switch, arrow keys select first item on current page.
  apps_grid_view_->SetSelectedView(GetItemViewAt(first_index));
  GetPaginationModel()->SelectPage(1, false);
  SimulateKeyPress(ui::VKEY_LEFT);
  EXPECT_TRUE(
      apps_grid_view_->IsSelectedView(GetItemViewAt(first_index_on_page2)));
}

TEST_P(AppsGridViewTest, MoveSelectedOnAllAppsTiles) {
  if (!test_with_fullscreen_)
    return;

  const int kItemsOnSecondPage = 3;
  const int kAllAppsItems = GetTilesPerPage(0) + kItemsOnSecondPage;
  const int kLastIndexOfFirstPage = GetTilesPerPage(0) - 1;
  const int kFirstIndexOfLastRowFirstPage =
      GetTilesPerPage(0) - apps_grid_view_->cols();
  model_->PopulateApps(kAllAppsItems);

  // Tests moving left from the first tile on the first page.
  apps_grid_view_->SetSelectedView(GetItemViewAt(0));
  SimulateKeyPress(ui::VKEY_LEFT);
  EXPECT_FALSE(apps_grid_view_->has_selected_view());
  EXPECT_EQ(suggestions_container_->num_results() - 1,
            suggestions_container_->selected_index());
  suggestions_container_->ClearSelectedIndex();

  // Tests moving left from the first tile on the second page.
  apps_grid_view_->SetSelectedView(GetItemViewAt(GetTilesPerPage(0)));
  SimulateKeyPress(ui::VKEY_LEFT);
  EXPECT_TRUE(
      apps_grid_view_->IsSelectedView(GetItemViewAt(kLastIndexOfFirstPage)));

  // Tests moving right from the last slot on the first page.
  apps_grid_view_->SetSelectedView(GetItemViewAt(kLastIndexOfFirstPage));
  SimulateKeyPress(ui::VKEY_RIGHT);
  EXPECT_TRUE(
      apps_grid_view_->IsSelectedView(GetItemViewAt(GetTilesPerPage(0))));

  // Tests moving up from the first row on the first page.
  apps_grid_view_->SetSelectedView(GetItemViewAt(1));
  SimulateKeyPress(ui::VKEY_UP);
  EXPECT_FALSE(apps_grid_view_->has_selected_view());
  EXPECT_EQ(1, suggestions_container_->selected_index());
  suggestions_container_->ClearSelectedIndex();

  // Tests moving up from the first row on the second page.
  apps_grid_view_->SetSelectedView(GetItemViewAt(kAllAppsItems - 1));
  SimulateKeyPress(ui::VKEY_UP);
  int expected_index =
      GetTilesPerPage(0) - 1 - (apps_grid_view_->cols() - kItemsOnSecondPage);
  EXPECT_TRUE(apps_grid_view_->IsSelectedView(GetItemViewAt(expected_index)));

  // Tests moving down from the last row on the first page.
  apps_grid_view_->SetSelectedView(
      GetItemViewAt(kFirstIndexOfLastRowFirstPage));
  SimulateKeyPress(ui::VKEY_DOWN);
  EXPECT_TRUE(
      apps_grid_view_->IsSelectedView(GetItemViewAt(GetTilesPerPage(0))));

  // Tests moving down if no direct tile exists, clamp to the last item.
  apps_grid_view_->SetSelectedView(GetItemViewAt(kLastIndexOfFirstPage));
  SimulateKeyPress(ui::VKEY_DOWN);
  EXPECT_TRUE(
      apps_grid_view_->IsSelectedView(GetItemViewAt(kAllAppsItems - 1)));
}

TEST_P(AppsGridViewTest, HandleSuggestionsMove) {
  if (!test_with_fullscreen_)
    return;

  // Tests that a non-up arrow key would move focus to the first tile of
  // suggestions container, and all apps grid view doesn't have focus.
  const int kPages = 2;
  const int kAllAppsItems = GetTilesPerPage(0) + 1;
  model_->PopulateApps(kAllAppsItems);
  SimulateKeyPress(ui::VKEY_UP);
  EXPECT_EQ(-1, suggestions_container_->selected_index());
  EXPECT_FALSE(apps_grid_view_->has_selected_view());
  SimulateKeyPress(ui::VKEY_DOWN);
  EXPECT_EQ(0, suggestions_container_->selected_index());
  EXPECT_FALSE(apps_grid_view_->has_selected_view());

  // Tests moving to previous page and moving up.
  SimulateKeyPress(ui::VKEY_PRIOR);
  EXPECT_EQ(0, suggestions_container_->selected_index());
  SimulateKeyPress(ui::VKEY_UP);
  EXPECT_EQ(0, suggestions_container_->selected_index());

  // Tests moving left, moving right and moving right out of suggestions.
  SimulateKeyPress(ui::VKEY_LEFT);
  EXPECT_EQ(0, suggestions_container_->selected_index());
  SimulateKeyPress(ui::VKEY_RIGHT);
  SimulateKeyPress(ui::VKEY_RIGHT);
  EXPECT_EQ(kNumOfSuggestedApps - 1, suggestions_container_->selected_index());
  SimulateKeyPress(ui::VKEY_RIGHT);
  EXPECT_EQ(-1, suggestions_container_->selected_index());
  EXPECT_TRUE(apps_grid_view_->IsSelectedView(GetItemViewAt(0)));

  // Tests moving down from |suggestions_container_|.
  apps_grid_view_->ClearAnySelectedView();
  SimulateKeyPress(ui::VKEY_RIGHT);
  SimulateKeyPress(ui::VKEY_RIGHT);
  EXPECT_EQ(1, suggestions_container_->selected_index());
  SimulateKeyPress(ui::VKEY_DOWN);
  EXPECT_EQ(-1, suggestions_container_->selected_index());
  EXPECT_TRUE(apps_grid_view_->IsSelectedView(GetItemViewAt(1)));

  // Tests moving to next page.
  apps_grid_view_->ClearAnySelectedView();
  SimulateKeyPress(ui::VKEY_LEFT);
  EXPECT_EQ(0, suggestions_container_->selected_index());
  SimulateKeyPress(ui::VKEY_NEXT);
  EXPECT_EQ(-1, suggestions_container_->selected_index());
  EXPECT_TRUE(
      apps_grid_view_->IsSelectedView(GetItemViewAt(kAllAppsItems - 1)));
  EXPECT_EQ(kPages - 1, GetPaginationModel()->selected_page());
}

TEST_P(AppsGridViewTest, ItemLabelShortNameOverride) {
  // If the app's full name and short name differ, the title label's tooltip
  // should always be the full name of the app.
  std::string expected_text("xyz");
  std::string expected_tooltip("tooltip");
  AppListItem* item = model_->CreateAndAddItem("Item with short name");
  model_->SetItemNameAndShortName(item, expected_tooltip, expected_text);

  base::string16 actual_tooltip;
  AppListItemView* item_view = GetItemViewAt(0);
  ASSERT_TRUE(item_view);
  const views::Label* title_label = item_view->title();
  EXPECT_TRUE(item_view->GetTooltipText(title_label->bounds().CenterPoint(),
                                        &actual_tooltip));
  EXPECT_EQ(expected_tooltip, base::UTF16ToUTF8(actual_tooltip));
  EXPECT_EQ(expected_text, base::UTF16ToUTF8(title_label->text()));
}

TEST_P(AppsGridViewTest, ItemLabelNoShortName) {
  // If the app's full name and short name are the same, use the default tooltip
  // behavior of the label (only show a tooltip if the title is truncated).
  std::string title("a");
  AppListItem* item = model_->CreateAndAddItem(title);
  model_->SetItemNameAndShortName(item, title, "");

  base::string16 actual_tooltip;
  AppListItemView* item_view = GetItemViewAt(0);
  ASSERT_TRUE(item_view);
  const views::Label* title_label = item_view->title();
  EXPECT_FALSE(title_label->GetTooltipText(title_label->bounds().CenterPoint(),
                                           &actual_tooltip));
  EXPECT_EQ(title, base::UTF16ToUTF8(title_label->text()));
}

}  // namespace test
}  // namespace app_list
