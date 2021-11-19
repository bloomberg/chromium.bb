// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_test_base.h"

#include "ash/public/cpp/presentation_time_recorder.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/test_shell_delegate.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_item_view.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/overview/overview_wallpaper_controller.h"
#include "ash/wm/overview/scoped_overview_transform_window.h"
#include "ash/wm/window_preview_view.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/test/test_utils.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

class CustomTestShellDelegate : public TestShellDelegate {
 public:
  explicit CustomTestShellDelegate(desks_storage::DeskModel* desk_model)
      : desk_model_(desk_model) {}
  CustomTestShellDelegate(const CustomTestShellDelegate&) = delete;
  CustomTestShellDelegate& operator=(const CustomTestShellDelegate&) = delete;
  ~CustomTestShellDelegate() override = default;

  // TestShellDelegate:
  desks_storage::DeskModel* GetDeskModel() override { return desk_model_; }

 private:
  // The desk model for the desks templates feature.
  desks_storage::DeskModel* const desk_model_;
};

}  // namespace

OverviewTestBase::~OverviewTestBase() = default;

void OverviewTestBase::EnterTabletMode() {
  // Ensure calls to SetEnabledForTest complete.
  base::RunLoop().RunUntilIdle();
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  base::RunLoop().RunUntilIdle();
}

bool OverviewTestBase::InOverviewSession() {
  return GetOverviewController()->InOverviewSession();
}

bool OverviewTestBase::WindowsOverlapping(aura::Window* window1,
                                          aura::Window* window2) {
  const gfx::Rect window1_bounds = GetTransformedTargetBounds(window1);
  const gfx::Rect window2_bounds = GetTransformedTargetBounds(window2);
  return window1_bounds.Intersects(window2_bounds);
}

std::unique_ptr<aura::Window> OverviewTestBase::CreateUnsnappableWindow(
    const gfx::Rect& bounds) {
  std::unique_ptr<aura::Window> window = CreateTestWindow(bounds);
  window->SetProperty(aura::client::kResizeBehaviorKey,
                      aura::client::kResizeBehaviorNone);
  return window;
}

void OverviewTestBase::ClickWindow(aura::Window* window) {
  ui::test::EventGenerator event_generator(window->GetRootWindow(), window);
  event_generator.ClickLeftButton();
}

OverviewController* OverviewTestBase::GetOverviewController() {
  return Shell::Get()->overview_controller();
}

OverviewSession* OverviewTestBase::GetOverviewSession() {
  return GetOverviewController()->overview_session();
}

SplitViewController* OverviewTestBase::GetSplitViewController() {
  return SplitViewController::Get(Shell::GetPrimaryRootWindow());
}

gfx::Rect OverviewTestBase::GetTransformedBounds(aura::Window* window) {
  gfx::Rect bounds_in_screen = window->layer()->bounds();
  wm::ConvertRectToScreen(window->parent(), &bounds_in_screen);
  gfx::RectF bounds(bounds_in_screen);
  gfx::Transform transform(gfx::TransformAboutPivot(
      gfx::ToFlooredPoint(bounds.origin()), window->layer()->transform()));
  transform.TransformRect(&bounds);
  return ToStableSizeRoundedRect(bounds);
}

gfx::Rect OverviewTestBase::GetTransformedTargetBounds(aura::Window* window) {
  gfx::Rect bounds_in_screen = window->layer()->GetTargetBounds();
  wm::ConvertRectToScreen(window->parent(), &bounds_in_screen);
  gfx::RectF bounds(bounds_in_screen);
  gfx::Transform transform(
      gfx::TransformAboutPivot(gfx::ToFlooredPoint(bounds.origin()),
                               window->layer()->GetTargetTransform()));
  transform.TransformRect(&bounds);
  return ToStableSizeRoundedRect(bounds);
}

gfx::Rect OverviewTestBase::GetTransformedBoundsInRootWindow(
    aura::Window* window) {
  gfx::RectF bounds = gfx::RectF(gfx::SizeF(window->bounds().size()));
  aura::Window* root = window->GetRootWindow();
  CHECK(window->layer());
  CHECK(root->layer());
  gfx::Transform transform;
  if (!window->layer()->GetTargetTransformRelativeTo(root->layer(),
                                                     &transform)) {
    return gfx::Rect();
  }
  transform.TransformRect(&bounds);
  return gfx::ToEnclosingRect(bounds);
}

OverviewItem* OverviewTestBase::GetDropTarget(int grid_index) {
  return GetOverviewSession()->grid_list_[grid_index]->GetDropTarget();
}

views::ImageButton* OverviewTestBase::GetCloseButton(OverviewItem* item) {
  return item->overview_item_view_->close_button();
}

views::Label* OverviewTestBase::GetLabelView(OverviewItem* item) {
  return item->overview_item_view_->title_label();
}

views::View* OverviewTestBase::GetBackdropView(OverviewItem* item) {
  return item->overview_item_view_->backdrop_view();
}

WindowPreviewView* OverviewTestBase::GetPreviewView(OverviewItem* item) {
  return item->overview_item_view_->preview_view();
}

float OverviewTestBase::GetCloseButtonOpacity(OverviewItem* item) {
  return GetCloseButton(item)->layer()->opacity();
}

float OverviewTestBase::GetTitlebarOpacity(OverviewItem* item) {
  return item->overview_item_view_->header_view()->layer()->opacity();
}

const ScopedOverviewTransformWindow& OverviewTestBase::GetTransformWindow(
    OverviewItem* item) const {
  return item->transform_window_;
}

bool OverviewTestBase::HasRoundedCorner(OverviewItem* item) {
  const ui::Layer* layer = item->transform_window_.IsMinimized()
                               ? GetPreviewView(item)->layer()
                               : GetTransformWindow(item).window()->layer();
  return !layer->rounded_corner_radii().IsEmpty();
}

void OverviewTestBase::CheckWindowAndCloseButtonInScreen(
    aura::Window* window,
    OverviewItem* window_item) {
  const gfx::Rect screen_bounds =
      window_item->root_window()->GetBoundsInScreen();
  EXPECT_TRUE(window_item->Contains(window));
  EXPECT_TRUE(screen_bounds.Contains(GetTransformedTargetBounds(window)));
  EXPECT_TRUE(
      screen_bounds.Contains(GetCloseButton(window_item)->GetBoundsInScreen()));
}

void OverviewTestBase::SetUp() {
  EXPECT_TRUE(desk_model_temp_dir_.CreateUniqueTempDir());
  desk_model_ = std::make_unique<desks_storage::LocalDeskDataManager>(
      desk_model_temp_dir_.GetPath());
  desk_model_->EnsureCacheIsLoaded();

  AshTestBase::SetUp(
      std::make_unique<CustomTestShellDelegate>(desk_model_.get()));

  aura::Env::GetInstance()->set_throttle_input_on_resize_for_testing(false);
  shelf_view_test_api_ = std::make_unique<ShelfViewTestAPI>(
      GetPrimaryShelf()->GetShelfViewForTesting());
  shelf_view_test_api_->SetAnimationDuration(base::Milliseconds(1));
  ScopedOverviewTransformWindow::SetImmediateCloseForTests(
      /*immediate=*/true);
  OverviewWallpaperController::SetDisableChangeWallpaperForTest(true);
  PresentationTimeRecorder::SetReportPresentationTimeImmediatelyForTest(true);
}

void OverviewTestBase::TearDown() {
  OverviewWallpaperController::SetDisableChangeWallpaperForTest(false);
  PresentationTimeRecorder::SetReportPresentationTimeImmediatelyForTest(false);
  trace_names_.clear();
  AshTestBase::TearDown();
}

void OverviewTestBase::CheckForDuplicateTraceName(const std::string& trace) {
  DCHECK(!base::Contains(trace_names_, trace)) << trace;
  trace_names_.push_back(trace);
}

void OverviewTestBase::CheckOverviewEnterExitHistogram(
    const std::string& trace,
    const std::vector<int>& enter_counts,
    const std::vector<int>& exit_counts) {
  CheckForDuplicateTraceName(trace);

  // Overview histograms recorded via ui::ThroughputTracker is reported
  // on the next frame presented after animation stops. Wait for the next
  // frame with a 100ms timeout for the report, regardless of whether there
  // is a next frame.
  ignore_result(ui::WaitForNextFrameToBePresented(
      Shell::GetPrimaryRootWindow()->layer()->GetCompositor(),
      base::Milliseconds(500)));

  {
    SCOPED_TRACE(trace + ".Enter");
    CheckOverviewHistogram("Ash.Overview.AnimationSmoothness.Enter",
                           enter_counts);
  }
  {
    SCOPED_TRACE(trace + ".Exit");
    CheckOverviewHistogram("Ash.Overview.AnimationSmoothness.Exit",
                           exit_counts);
  }
}

gfx::Rect OverviewTestBase::GetGridBounds() {
  if (GetOverviewSession())
    return GetOverviewSession()->grid_list_[0]->bounds_;

  return gfx::Rect();
}

void OverviewTestBase::SetGridBounds(OverviewGrid* grid,
                                     const gfx::Rect& bounds) {
  grid->bounds_ = bounds;
}

void OverviewTestBase::CheckOverviewHistogram(const std::string& histogram,
                                              const std::vector<int>& counts) {
  ASSERT_EQ(5u, counts.size());

  histograms_.ExpectTotalCount(histogram + ".ClamshellMode", counts[0]);
  histograms_.ExpectTotalCount(histogram + ".SingleClamshellMode", counts[1]);
  histograms_.ExpectTotalCount(histogram + ".TabletMode", counts[2]);
  histograms_.ExpectTotalCount(histogram + ".MinimizedTabletMode", counts[3]);
  histograms_.ExpectTotalCount(histogram + ".SplitView", counts[4]);
}
}  // namespace ash
