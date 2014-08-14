// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/app_list_main_view.h"

#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/app_list/test/app_list_test_model.h"
#include "ui/app_list/test/app_list_test_view_delegate.h"
#include "ui/app_list/views/app_list_folder_view.h"
#include "ui/app_list/views/app_list_item_view.h"
#include "ui/app_list/views/apps_container_view.h"
#include "ui/app_list/views/apps_grid_view.h"
#include "ui/app_list/views/contents_view.h"
#include "ui/app_list/views/test/apps_grid_view_test_api.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view_model.h"
#include "ui/views/widget/widget.h"

namespace app_list {
namespace test {

namespace {

const int kInitialItems = 2;

class GridViewVisibleWaiter {
 public:
  explicit GridViewVisibleWaiter(AppsGridView* grid_view)
      : grid_view_(grid_view) {}
  ~GridViewVisibleWaiter() {}

  void Wait() {
    if (grid_view_->visible())
      return;

    check_timer_.Start(FROM_HERE,
                       base::TimeDelta::FromMilliseconds(50),
                       base::Bind(&GridViewVisibleWaiter::OnTimerCheck,
                                  base::Unretained(this)));
    run_loop_.reset(new base::RunLoop);
    run_loop_->Run();
    check_timer_.Stop();
  }

 private:
  void OnTimerCheck() {
    if (grid_view_->visible())
      run_loop_->Quit();
  }

  AppsGridView* grid_view_;
  scoped_ptr<base::RunLoop> run_loop_;
  base::RepeatingTimer<GridViewVisibleWaiter> check_timer_;

  DISALLOW_COPY_AND_ASSIGN(GridViewVisibleWaiter);
};

class AppListMainViewTest : public views::ViewsTestBase {
 public:
  AppListMainViewTest()
      : widget_(NULL),
        main_view_(NULL) {}

  virtual ~AppListMainViewTest() {}

  // testing::Test overrides:
  virtual void SetUp() OVERRIDE {
    views::ViewsTestBase::SetUp();
    delegate_.reset(new AppListTestViewDelegate);

    // In Ash, the third argument is a container aura::Window, but it is always
    // NULL on Windows, and not needed for tests. It is only used to determine
    // the scale factor for preloading icons.
    main_view_ = new AppListMainView(delegate_.get(), 0, NULL);
    main_view_->SetPaintToLayer(true);
    main_view_->model()->SetFoldersEnabled(true);

    widget_ = new views::Widget;
    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::TYPE_POPUP);
    params.bounds.set_size(main_view_->GetPreferredSize());
    widget_->Init(params);

    widget_->SetContentsView(main_view_);
  }

  virtual void TearDown() OVERRIDE {
    widget_->Close();
    views::ViewsTestBase::TearDown();
    delegate_.reset();
  }

  // |point| is in |grid_view|'s coordinates.
  AppListItemView* GetItemViewAtPointInGrid(AppsGridView* grid_view,
                                            const gfx::Point& point) {
    const views::ViewModel* view_model = grid_view->view_model_for_test();
    for (int i = 0; i < view_model->view_size(); ++i) {
      views::View* view = view_model->view_at(i);
      if (view->bounds().Contains(point)) {
        return static_cast<AppListItemView*>(view);
      }
    }

    return NULL;
  }

  void SimulateClick(views::View* view) {
    gfx::Point center = view->GetLocalBounds().CenterPoint();
    view->OnMousePressed(ui::MouseEvent(ui::ET_MOUSE_PRESSED,
                                        center,
                                        center,
                                        ui::EF_LEFT_MOUSE_BUTTON,
                                        ui::EF_LEFT_MOUSE_BUTTON));
    view->OnMouseReleased(ui::MouseEvent(ui::ET_MOUSE_RELEASED,
                                         center,
                                         center,
                                         ui::EF_LEFT_MOUSE_BUTTON,
                                         ui::EF_LEFT_MOUSE_BUTTON));
  }

  // |point| is in |grid_view|'s coordinates.
  AppListItemView* SimulateInitiateDrag(AppsGridView* grid_view,
                                        AppsGridView::Pointer pointer,
                                        const gfx::Point& point) {
    AppListItemView* view = GetItemViewAtPointInGrid(grid_view, point);
    DCHECK(view);

    gfx::Point translated =
        gfx::PointAtOffsetFromOrigin(point - view->bounds().origin());
    ui::MouseEvent pressed_event(ui::ET_MOUSE_PRESSED, translated, point, 0, 0);
    grid_view->InitiateDrag(view, pointer, pressed_event);
    return view;
  }

  // |point| is in |grid_view|'s coordinates.
  void SimulateUpdateDrag(AppsGridView* grid_view,
                          AppsGridView::Pointer pointer,
                          AppListItemView* drag_view,
                          const gfx::Point& point) {
    DCHECK(drag_view);
    gfx::Point translated =
        gfx::PointAtOffsetFromOrigin(point - drag_view->bounds().origin());
    ui::MouseEvent drag_event(ui::ET_MOUSE_DRAGGED, translated, point, 0, 0);
    grid_view->UpdateDragFromItem(pointer, drag_event);
  }

  AppsGridView* RootGridView() {
    return main_view_->contents_view()->apps_container_view()->apps_grid_view();
  }

  AppListFolderView* FolderView() {
    return main_view_->contents_view()
        ->apps_container_view()
        ->app_list_folder_view();
  }

  AppsGridView* FolderGridView() { return FolderView()->items_grid_view(); }

  const views::ViewModel* RootViewModel() {
    return RootGridView()->view_model_for_test();
  }

  const views::ViewModel* FolderViewModel() {
    return FolderGridView()->view_model_for_test();
  }

  AppListItemView* CreateAndOpenSingleItemFolder() {
    // Prepare single folder with a single item in it.
    AppListFolderItem* folder_item =
        delegate_->GetTestModel()->CreateSingleItemFolder("single_item_folder",
                                                          "single");
    EXPECT_EQ(folder_item,
              delegate_->GetTestModel()->FindFolderItem("single_item_folder"));
    EXPECT_EQ(AppListFolderItem::kItemType, folder_item->GetItemType());

    EXPECT_EQ(1, RootViewModel()->view_size());
    AppListItemView* folder_item_view =
        static_cast<AppListItemView*>(RootViewModel()->view_at(0));
    EXPECT_EQ(folder_item_view->item(), folder_item);

    // Click on the folder to open it.
    EXPECT_FALSE(FolderView()->visible());
    SimulateClick(folder_item_view);
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(FolderView()->visible());

#if defined(OS_WIN)
    AppsGridViewTestApi folder_grid_view_test_api(FolderGridView());
    folder_grid_view_test_api.DisableSynchronousDrag();
#endif
    return folder_item_view;
  }

  AppListItemView* StartDragForReparent(int index_in_folder) {
    // Start to drag the item in folder.
    views::View* item_view = FolderViewModel()->view_at(index_in_folder);
    gfx::Point point = item_view->bounds().CenterPoint();
    AppListItemView* dragged =
        SimulateInitiateDrag(FolderGridView(), AppsGridView::MOUSE, point);
    EXPECT_EQ(item_view, dragged);
    EXPECT_FALSE(RootGridView()->visible());
    EXPECT_TRUE(FolderView()->visible());

    // Drag it to top left corner.
    point = gfx::Point(0, 0);
    // Two update drags needed to actually drag the view. The first changes
    // state and the 2nd one actually moves the view. The 2nd call can be
    // removed when UpdateDrag is fixed.
    SimulateUpdateDrag(FolderGridView(), AppsGridView::MOUSE, dragged, point);
    SimulateUpdateDrag(FolderGridView(), AppsGridView::MOUSE, dragged, point);
    base::RunLoop().RunUntilIdle();

    // Wait until the folder view is invisible and root grid view shows up.
    GridViewVisibleWaiter(RootGridView()).Wait();
    EXPECT_TRUE(RootGridView()->visible());
    EXPECT_EQ(0, FolderView()->layer()->opacity());

    return dragged;
  }

 protected:
  views::Widget* widget_;  // Owned by native window.
  AppListMainView* main_view_;  // Owned by |widget_|.
  scoped_ptr<AppListTestViewDelegate> delegate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListMainViewTest);
};

}  // namespace

// Tests changing the AppListModel when switching profiles.
TEST_F(AppListMainViewTest, ModelChanged) {
  delegate_->GetTestModel()->PopulateApps(kInitialItems);
  EXPECT_EQ(kInitialItems, RootViewModel()->view_size());

  // The model is owned by a profile keyed service, which is never destroyed
  // until after profile switching.
  scoped_ptr<AppListModel> old_model(delegate_->ReleaseTestModel());

  const int kReplacementItems = 5;
  delegate_->ReplaceTestModel(kReplacementItems);
  main_view_->ModelChanged();
  EXPECT_EQ(kReplacementItems, RootViewModel()->view_size());
}

// Tests dragging an item out of a single item folder and drop it at the last
// slot.
TEST_F(AppListMainViewTest, DragLastItemFromFolderAndDropAtLastSlot) {
  AppListItemView* folder_item_view = CreateAndOpenSingleItemFolder();
  const gfx::Rect first_slot_tile = folder_item_view->bounds();

  EXPECT_EQ(1, FolderViewModel()->view_size());

  AppListItemView* dragged = StartDragForReparent(0);

  // Drop it to the slot on the right of first slot.
  gfx::Rect drop_target_tile(first_slot_tile);
  drop_target_tile.Offset(first_slot_tile.width(), 0);
  gfx::Point point = drop_target_tile.CenterPoint();
  SimulateUpdateDrag(FolderGridView(), AppsGridView::MOUSE, dragged, point);
  SimulateUpdateDrag(FolderGridView(), AppsGridView::MOUSE, dragged, point);
  base::RunLoop().RunUntilIdle();

  // Drop it.
  FolderGridView()->EndDrag(false);
  base::RunLoop().RunUntilIdle();

  // Folder icon view should be gone and there is only one item view.
  EXPECT_EQ(1, RootViewModel()->view_size());
  EXPECT_EQ(AppListItemView::kViewClassName,
            RootViewModel()->view_at(0)->GetClassName());

  // The item view should be in slot 1 instead of slot 2 where it is dropped.
  AppsGridViewTestApi root_grid_view_test_api(RootGridView());
  root_grid_view_test_api.LayoutToIdealBounds();
  EXPECT_EQ(first_slot_tile, RootViewModel()->view_at(0)->bounds());

  // Single item folder should be auto removed.
  EXPECT_EQ(NULL,
            delegate_->GetTestModel()->FindFolderItem("single_item_folder"));
}

// Test that an interrupted drag while reparenting an item from a folder, when
// canceled via the root grid, correctly forwards the cancelation to the drag
// ocurring from the folder.
TEST_F(AppListMainViewTest, MouseDragItemOutOfFolderWithCancel) {
  CreateAndOpenSingleItemFolder();
  AppListItemView* dragged = StartDragForReparent(0);

  // Now add an item to the model, not in any folder, e.g., as if by Sync.
  EXPECT_TRUE(RootGridView()->has_dragged_view());
  EXPECT_TRUE(FolderGridView()->has_dragged_view());
  delegate_->GetTestModel()->CreateAndAddItem("Extra");

  // The drag operation should get canceled.
  EXPECT_FALSE(RootGridView()->has_dragged_view());
  EXPECT_FALSE(FolderGridView()->has_dragged_view());

  // Additional mouse move operations should be ignored.
  gfx::Point point(1, 1);
  SimulateUpdateDrag(FolderGridView(), AppsGridView::MOUSE, dragged, point);
  EXPECT_FALSE(RootGridView()->has_dragged_view());
  EXPECT_FALSE(FolderGridView()->has_dragged_view());
}

}  // namespace test
}  // namespace app_list
