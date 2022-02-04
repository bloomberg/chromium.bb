// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/apps_grid_view_test_api.h"

#include <memory>
#include <vector>

#include "ash/app_list/paged_view_structure.h"
#include "ash/app_list/views/app_drag_icon_proxy.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/apps_grid_view.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "ui/events/event.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/animation/bounds_animator_observer.h"
#include "ui/views/view.h"

namespace ash {
namespace test {

namespace {

class BoundsAnimatorWaiter : public views::BoundsAnimatorObserver {
 public:
  explicit BoundsAnimatorWaiter(views::BoundsAnimator* animator)
      : animator_(animator) {
    animator->AddObserver(this);
  }

  BoundsAnimatorWaiter(const BoundsAnimatorWaiter&) = delete;
  BoundsAnimatorWaiter& operator=(const BoundsAnimatorWaiter&) = delete;

  ~BoundsAnimatorWaiter() override { animator_->RemoveObserver(this); }

  void Wait() {
    if (!animator_->IsAnimating())
      return;

    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

 private:
  // views::BoundsAnimatorObserver:
  void OnBoundsAnimatorProgressed(views::BoundsAnimator* animator) override {}
  void OnBoundsAnimatorDone(views::BoundsAnimator* animator) override {
    if (run_loop_)
      run_loop_->Quit();
  }

  views::BoundsAnimator* animator_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

}  // namespace

AppsGridViewTestApi::AppsGridViewTestApi(AppsGridView* view) : view_(view) {}

AppsGridViewTestApi::~AppsGridViewTestApi() = default;

views::View* AppsGridViewTestApi::GetViewAtModelIndex(int index) const {
  return view_->view_model_.view_at(index);
}

void AppsGridViewTestApi::LayoutToIdealBounds() {
  if (view_->reorder_timer_.IsRunning()) {
    view_->reorder_timer_.Stop();
    view_->OnReorderTimer();
  }
  view_->bounds_animator_->Cancel();
  view_->Layout();
}

gfx::Rect AppsGridViewTestApi::GetItemTileRectOnCurrentPageAt(int row,
                                                              int col) const {
  int slot = row * (view_->cols()) + col;
  gfx::Rect bounds_in_ltr =
      view_->GetExpectedTileBounds(GridIndex(view_->GetSelectedPage(), slot));
  // `GetExpectedTileBounds()` returns expected bounds for item at provided grid
  // index in LTR UI. Make sure this method returns mirrored bounds in RTL UI.
  return view_->GetMirroredRect(bounds_in_ltr);
}

void AppsGridViewTestApi::PressItemAt(int index) {
  GetViewAtModelIndex(index)->OnKeyPressed(
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_RETURN, ui::EF_NONE));
}

int AppsGridViewTestApi::TilesPerPage(int page) const {
  return view_->TilesPerPage(page);
}

int AppsGridViewTestApi::AppsOnPage(int page) const {
  return view_->GetNumberOfItemsOnPage(page);
}

AppListItemView* AppsGridViewTestApi::GetViewAtIndex(GridIndex index) const {
  return view_->GetViewAtIndex(index);
}

AppListItemView* AppsGridViewTestApi::GetViewAtVisualIndex(int page,
                                                           int slot) const {
  const std::vector<std::vector<AppListItemView*>>& view_structure =
      view_->view_structure_.pages();
  if (page >= static_cast<int>(view_structure.size()) ||
      slot >= static_cast<int>(view_structure[page].size())) {
    return nullptr;
  }
  return view_structure[page][slot];
}

const std::string& AppsGridViewTestApi::GetNameAtVisualIndex(int page,
                                                             int slot) const {
  return GetViewAtVisualIndex(page, slot)->item()->name();
}

gfx::Rect AppsGridViewTestApi::GetItemTileRectAtVisualIndex(int page,
                                                            int slot) const {
  // `GetExpectedTileBounds()` returns expected bounds for item at provided grid
  // index in LTR UI. Make sure this method returns mirrored bounds in RTL UI.
  return view_->GetMirroredRect(
      view_->GetExpectedTileBounds(GridIndex(page, slot)));
}

void AppsGridViewTestApi::WaitForItemMoveAnimationDone() {
  BoundsAnimatorWaiter waiter(view_->bounds_animator_.get());
  waiter.Wait();
}

void AppsGridViewTestApi::FireReorderTimerAndWaitForAnimationDone() {
  base::OneShotTimer* timer = &view_->reorder_timer_;
  if (timer->IsRunning())
    timer->FireNow();

  WaitForItemMoveAnimationDone();
}

void AppsGridViewTestApi::FireFolderItemReparentTimer() {
  view_->FireFolderItemReparentTimerForTest();
}

gfx::Rect AppsGridViewTestApi::GetDragIconBoundsInAppsGridView() {
  if (!view_->drag_icon_proxy_)
    return gfx::Rect();
  gfx::Rect icon_bounds_in_screen =
      view_->drag_icon_proxy_->GetBoundsInScreen();
  if (icon_bounds_in_screen.IsEmpty())
    return gfx::Rect();
  gfx::Point icon_origin = icon_bounds_in_screen.origin();
  views::View::ConvertPointFromScreen(view_, &icon_origin);
  return gfx::Rect(icon_origin, icon_bounds_in_screen.size());
}

ui::Layer* AppsGridViewTestApi::GetDragIconLayer() {
  if (!view_->drag_icon_proxy_)
    return nullptr;
  return view_->drag_icon_proxy_->GetImageLayerForTesting();
}

}  // namespace test
}  // namespace ash
