// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_session.h"

#include <algorithm>
#include <functional>
#include <utility>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/screen_util.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_delegate.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/overview/overview_window_drag_controller.h"
#include "ash/wm/splitview/split_view_drag_indicators.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/switchable_windows.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/auto_reset.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// Triggers a shelf visibility update on all root window controllers.
void UpdateShelfVisibility() {
  for (aura::Window* root : Shell::GetAllRootWindows())
    Shelf::ForWindow(root)->UpdateVisibilityState();
}

// Returns the bounds for the overview window grid according to the split view
// state. If split view mode is active, the overview window should open on the
// opposite side of the default snap window. If |divider_changed| is true, maybe
// clamp the bounds to a minimum size and shift the bounds offscreen.
gfx::Rect GetGridBoundsInScreen(aura::Window* root_window,
                                bool divider_changed) {
  gfx::Rect work_area =
      screen_util::GetDisplayWorkAreaBoundsInScreenForDefaultContainer(
          root_window);

  // If the shelf is in auto hide, overview will force it to be in auto hide
  // shown, but we want to place the thumbnails as if the shelf was shown, so
  // manually update the work area.
  if (Shelf::ForWindow(root_window)->GetVisibilityState() == SHELF_AUTO_HIDE) {
    const int inset = kShelfSize;
    switch (Shelf::ForWindow(root_window)->alignment()) {
      case SHELF_ALIGNMENT_BOTTOM:
      case SHELF_ALIGNMENT_BOTTOM_LOCKED:
        work_area.Inset(0, 0, 0, inset);
        break;
      case SHELF_ALIGNMENT_LEFT:
        work_area.Inset(inset, 0, 0, 0);
        break;
      case SHELF_ALIGNMENT_RIGHT:
        work_area.Inset(0, 0, inset, 0);
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  SplitViewController* split_view_controller =
      Shell::Get()->split_view_controller();
  if (!split_view_controller->IsSplitViewModeActive())
    return work_area;

  SplitViewController::SnapPosition opposite_position =
      (split_view_controller->default_snap_position() ==
       SplitViewController::LEFT)
          ? SplitViewController::RIGHT
          : SplitViewController::LEFT;
  gfx::Rect bounds = split_view_controller->GetSnappedWindowBoundsInScreen(
      root_window, opposite_position);
  if (!divider_changed)
    return bounds;

  const bool landscape = IsCurrentScreenOrientationLandscape();
  const int min_length =
      (landscape ? work_area.width() : work_area.height()) / 3;
  const int current_length = landscape ? bounds.width() : bounds.height();

  if (current_length > min_length)
    return bounds;

  // Clamp bounds' length to the minimum length.
  if (landscape)
    bounds.set_width(min_length);
  else
    bounds.set_height(min_length);

  // The |opposite_position| will be physically on the left or top of the screen
  // (depending on whether the orientation is landscape or portrait
  //  respectively), if |opposite_position| is left AND current orientation is
  // primary, OR |opposite_position| is right AND current orientation is not
  // primary. This is an X-NOR condition.
  const bool primary = IsCurrentScreenOrientationPrimary();
  const bool left_or_top =
      (primary == (opposite_position == SplitViewController::LEFT));
  if (left_or_top) {
    // If we are shifting to the left or top we need to update the origin as
    // well.
    const int offset = min_length - current_length;
    bounds.Offset(landscape ? gfx::Vector2d(-offset, 0)
                            : gfx::Vector2d(0, -offset));
  }

  return bounds;
}

}  // namespace

OverviewSession::OverviewSession(OverviewDelegate* delegate)
    : delegate_(delegate),
      restore_focus_window_(wm::GetFocusedWindow()),
      overview_start_time_(base::Time::Now()) {
  DCHECK(delegate_);
  Shell::Get()->AddPreTargetHandler(this);
}

OverviewSession::~OverviewSession() {
  DCHECK(observed_windows_.empty());
  // Don't delete |window_drag_controller_| yet since the stack might be still
  // using it.
  if (window_drag_controller_) {
    window_drag_controller_->ResetOverviewSession();
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(
        FROM_HERE, window_drag_controller_.release());
  }
}

// NOTE: The work done in Init() is not done in the constructor because it may
// cause other, unrelated classes, to make indirect method calls on a partially
// constructed object.
void OverviewSession::Init(const WindowList& windows,
                           const WindowList& hide_windows) {
  hide_overview_windows_ =
      std::make_unique<ScopedOverviewHideWindows>(std::move(hide_windows));
  if (restore_focus_window_)
    restore_focus_window_->AddObserver(this);

  if (ShouldAllowSplitView())
    split_view_drag_indicators_ = std::make_unique<SplitViewDragIndicators>();

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  std::sort(root_windows.begin(), root_windows.end(),
            [](const aura::Window* a, const aura::Window* b) {
              // Since we don't know if windows are vertically or horizontally
              // oriented we use both x and y position. This may be confusing
              // if you have 3 or more monitors which are not strictly
              // horizontal or vertical but that case is not yet supported.
              return (a->GetBoundsInScreen().x() + a->GetBoundsInScreen().y()) <
                     (b->GetBoundsInScreen().x() + b->GetBoundsInScreen().y());
            });

  for (auto* root : root_windows) {
    // Observed switchable containers for newly created windows on all root
    // windows.
    for (size_t i = 0; i < wm::kSwitchableWindowContainerIdsLength; ++i) {
      aura::Window* container =
          root->GetChildById(wm::kSwitchableWindowContainerIds[i]);
      container->AddObserver(this);
      observed_windows_.insert(container);
    }

    auto grid = std::make_unique<OverviewGrid>(
        root, windows, this,
        GetGridBoundsInScreen(root, /*divider_changed=*/false));
    num_items_ += grid->size();
    grid_list_.push_back(std::move(grid));
  }

  // The calls to OverviewGrid::PrepareForOverview() requires some
  // LayoutManagers to perform layouts so that windows are correctly visible and
  // properly animated in overview mode. Otherwise these layouts should be
  // suppressed during overview mode so they don't conflict with overview mode
  // animations.

  // Do not call PrepareForOverview until all items are added to window_list_
  // as we don't want to cause any window updates until all windows in
  // overview are observed. See http://crbug.com/384495.
  for (std::unique_ptr<OverviewGrid>& overview_grid : grid_list_) {
    overview_grid->PrepareForOverview();

    // Do not animate if there is any window that is being dragged in the
    // grid.
    if (enter_exit_overview_type_ == EnterExitOverviewType::kWindowDragged) {
      overview_grid->PositionWindows(/*animate=*/false);
    } else if (enter_exit_overview_type_ ==
               EnterExitOverviewType::kWindowsMinimized) {
      overview_grid->PositionWindows(/*animate=*/false);
      overview_grid->SlideWindowsIn();
    } else {
      // EnterExitOverviewType::kSwipeFromShelf is an exit only type, so it
      // should not appear here.
      DCHECK_NE(enter_exit_overview_type_,
                EnterExitOverviewType::kSwipeFromShelf);
      overview_grid->CalculateWindowListAnimationStates(
          /*selected_item=*/nullptr, OverviewTransition::kEnter);
      overview_grid->PositionWindows(/*animate=*/true, /*ignore_item=*/nullptr,
                                     OverviewTransition::kEnter);
    }
  }

  // Create the widget that will receive focus while in overview mode for
  // accessiblity purposes.
  overview_focus_widget_ = std::make_unique<views::Widget>();
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.accept_events = false;
  params.bounds = gfx::Rect(0, 0, 2, 2);
  params.layer_type = ui::LAYER_NOT_DRAWN;
  params.name = "OverviewModeFocusedWidget";
  params.parent = Shell::GetPrimaryRootWindow()->GetChildById(
      kShellWindowId_StatusContainer);
  overview_focus_widget_->Init(params);

  UMA_HISTOGRAM_COUNTS_100("Ash.WindowSelector.Items", num_items_);

  Shell::Get()->split_view_controller()->AddObserver(this);

  display::Screen::GetScreen()->AddObserver(this);
  base::RecordAction(base::UserMetricsAction("WindowSelector_Overview"));
  // Send an a11y alert.
  Shell::Get()->accessibility_controller()->TriggerAccessibilityAlert(
      mojom::AccessibilityAlert::WINDOW_OVERVIEW_MODE_ENTERED);

  UpdateShelfVisibility();

  ignore_activations_ = false;
}

// NOTE: The work done in Shutdown() is not done in the destructor because it
// may cause other, unrelated classes, to make indirect calls to
// restoring_minimized_windows() on a partially destructed object.
void OverviewSession::Shutdown() {
  Shell::Get()->RemovePreTargetHandler(this);

  // Stop observing screen metrics changes first to avoid auto-positioning
  // windows in response to work area changes from window activation.
  display::Screen::GetScreen()->RemoveObserver(this);

  // Stop observing split view state changes before restoring window focus.
  // Otherwise the activation of the window triggers OnSplitViewStateChanged()
  // that will call into this function again.
  Shell::Get()->split_view_controller()->RemoveObserver(this);

  size_t remaining_items = 0;
  for (std::unique_ptr<OverviewGrid>& overview_grid : grid_list_) {
    // During shutdown, do not animate all windows in overview if we need to
    // animate the snapped window.
    if (overview_grid->should_animate_when_exiting()) {
      overview_grid->CalculateWindowListAnimationStates(
          selected_item_ &&
                  selected_item_->overview_grid() == overview_grid.get()
              ? selected_item_
              : nullptr,
          OverviewTransition::kExit);
    }
    for (const auto& overview_item : overview_grid->window_list())
      overview_item->RestoreWindow(/*reset_transform=*/true);
    remaining_items += overview_grid->size();
  }

  // Setting focus after restoring windows' state avoids unnecessary animations.
  // No need to restore if we are sliding to the home launcher screen, as all
  // windows will be minimized.
  ResetFocusRestoreWindow(enter_exit_overview_type_ ==
                          EnterExitOverviewType::kNormal);
  RemoveAllObservers();

  for (std::unique_ptr<OverviewGrid>& overview_grid : grid_list_)
    overview_grid->Shutdown();

  DCHECK(num_items_ >= remaining_items);
  UMA_HISTOGRAM_COUNTS_100("Ash.WindowSelector.OverviewClosedItems",
                           num_items_ - remaining_items);
  UMA_HISTOGRAM_MEDIUM_TIMES("Ash.WindowSelector.TimeInOverview",
                             base::Time::Now() - overview_start_time_);

  // Clearing the window list resets the ignored_by_shelf flag on the windows.
  grid_list_.clear();
  UpdateShelfVisibility();
}

void OverviewSession::CancelSelection() {
  delegate_->OnSelectionEnded();
}

void OverviewSession::OnGridEmpty(OverviewGrid* grid) {
  size_t index = 0;
  // If there are no longer any items on any of the grids, shutdown,
  // otherwise the empty grids will remain blurred but will have no items.
  if (IsEmpty()) {
    // Shutdown all grids if no grids have any items and split view mode is
    // not active. Set |index| to -1 so that it does not attempt to select any
    // items.
    index = -1;
    if (!Shell::Get()->IsSplitViewModeActive()) {
      for (const auto& grid : grid_list_)
        grid->Shutdown();
      grid_list_.clear();
    }
  } else {
    for (auto iter = grid_list_.begin(); iter != grid_list_.end(); ++iter) {
      if (grid == (*iter).get()) {
        index = iter - grid_list_.begin();
        break;
      }
    }
  }
  if (index > 0 && selected_grid_index_ >= index) {
    // If the grid closed is not the one with the selected item, we do not need
    // to move the selected item.
    if (selected_grid_index_ == index)
      --selected_grid_index_;
    // If the grid which became empty was the one with the selected window, we
    // need to select a window on the newly selected grid.
    if (selected_grid_index_ == index - 1)
      Move(LEFT, true);
  }
  if (grid_list_.empty())
    CancelSelection();
  else
    PositionWindows(/*animate=*/false);
}

void OverviewSession::IncrementSelection(int increment) {
  const Direction direction = increment > 0 ? RIGHT : LEFT;
  for (int step = 0; step < abs(increment); ++step)
    Move(direction, true);
}

bool OverviewSession::AcceptSelection() {
  if (!grid_list_[selected_grid_index_]->is_selecting())
    return false;
  SelectWindow(grid_list_[selected_grid_index_]->SelectedWindow());
  return true;
}

void OverviewSession::SelectWindow(OverviewItem* item) {
  aura::Window* window = item->GetWindow();
  aura::Window::Windows window_list =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList();
  if (!window_list.empty()) {
    // Record WindowSelector_ActiveWindowChanged if the user is selecting a
    // window other than the window that was active prior to entering overview
    // mode (i.e., the window at the front of the MRU list).
    if (window_list[0] != window) {
      base::RecordAction(
          base::UserMetricsAction("WindowSelector_ActiveWindowChanged"));
      Shell::Get()->metrics()->task_switch_metrics_recorder().OnTaskSwitch(
          TaskSwitchSource::OVERVIEW_MODE);
    }
    const auto it = std::find(window_list.begin(), window_list.end(), window);
    if (it != window_list.end()) {
      // Record 1-based index so that selecting a top MRU window will record 1.
      UMA_HISTOGRAM_COUNTS_100("Ash.WindowSelector.SelectionDepth",
                               1 + it - window_list.begin());
    }
  }
  item->EnsureVisible();
  ::wm::ActivateWindow(window);
}

void OverviewSession::SetBoundsForOverviewGridsInScreenIgnoringWindow(
    const gfx::Rect& bounds,
    OverviewItem* ignored_item) {
  for (std::unique_ptr<OverviewGrid>& grid : grid_list_)
    grid->SetBoundsAndUpdatePositionsIgnoringWindow(bounds, ignored_item);
}

void OverviewSession::SetSplitViewDragIndicatorsIndicatorState(
    IndicatorState indicator_state,
    const gfx::Point& event_location) {
  DCHECK(split_view_drag_indicators_);
  split_view_drag_indicators_->SetIndicatorState(indicator_state,
                                                 event_location);
}

OverviewGrid* OverviewSession::GetGridWithRootWindow(
    aura::Window* root_window) {
  for (std::unique_ptr<OverviewGrid>& grid : grid_list_) {
    if (grid->root_window() == root_window)
      return grid.get();
  }

  return nullptr;
}

void OverviewSession::AddItem(aura::Window* window,
                              bool reposition,
                              bool animate) {
  // Early exit if a grid already contains |window|.
  OverviewGrid* grid = GetGridWithRootWindow(window->GetRootWindow());
  if (!grid || grid->GetOverviewItemContaining(window))
    return;

  grid->AddItem(window, reposition, animate);
  ++num_items_;

  // Transfer focus from |window| to |overview_focus_widget_| to match the
  // behavior of entering overview mode in the beginning.
  DCHECK(overview_focus_widget_);
  ::wm::ActivateWindow(GetOverviewFocusWindow());
}

void OverviewSession::RemoveOverviewItem(OverviewItem* item, bool reposition) {
  if (item->GetWindow()->HasObserver(this)) {
    item->GetWindow()->RemoveObserver(this);
    observed_windows_.erase(item->GetWindow());
    if (item->GetWindow() == restore_focus_window_)
      restore_focus_window_ = nullptr;
  }

  // Remove |item| from the corresponding grid.
  for (std::unique_ptr<OverviewGrid>& grid : grid_list_) {
    if (grid->GetOverviewItemContaining(item->GetWindow())) {
      grid->RemoveItem(item, reposition);
      --num_items_;
      break;
    }
  }
}

void OverviewSession::InitiateDrag(OverviewItem* item,
                                   const gfx::PointF& location_in_screen) {
  window_drag_controller_ =
      std::make_unique<OverviewWindowDragController>(this);
  window_drag_controller_->InitiateDrag(item, location_in_screen);

  for (std::unique_ptr<OverviewGrid>& grid : grid_list_)
    grid->OnSelectorItemDragStarted(item);
}

void OverviewSession::Drag(OverviewItem* item,
                           const gfx::PointF& location_in_screen) {
  DCHECK(window_drag_controller_);
  DCHECK_EQ(item, window_drag_controller_->item());
  window_drag_controller_->Drag(location_in_screen);
}

void OverviewSession::CompleteDrag(OverviewItem* item,
                                   const gfx::PointF& location_in_screen) {
  DCHECK(window_drag_controller_);
  DCHECK_EQ(item, window_drag_controller_->item());
  window_drag_controller_->CompleteDrag(location_in_screen);

  for (std::unique_ptr<OverviewGrid>& grid : grid_list_)
    grid->OnSelectorItemDragEnded();
}

void OverviewSession::StartSplitViewDragMode(
    const gfx::PointF& location_in_screen) {
  window_drag_controller_->StartSplitViewDragMode(location_in_screen);
}

void OverviewSession::Fling(OverviewItem* item,
                            const gfx::PointF& location_in_screen,
                            float velocity_x,
                            float velocity_y) {
  // Its possible a fling event is not paired with a tap down event. Ignore
  // these flings.
  if (!window_drag_controller_ || item != window_drag_controller_->item())
    return;

  window_drag_controller_->Fling(location_in_screen, velocity_x, velocity_y);
  for (std::unique_ptr<OverviewGrid>& grid : grid_list_)
    grid->OnSelectorItemDragEnded();
}

void OverviewSession::ActivateDraggedWindow() {
  window_drag_controller_->ActivateDraggedWindow();
}

void OverviewSession::ResetDraggedWindowGesture() {
  window_drag_controller_->ResetGesture();
}

void OverviewSession::OnWindowDragStarted(aura::Window* dragged_window,
                                          bool animate) {
  OverviewGrid* target_grid =
      GetGridWithRootWindow(dragged_window->GetRootWindow());
  if (!target_grid)
    return;
  target_grid->OnWindowDragStarted(dragged_window, animate);
}

void OverviewSession::OnWindowDragContinued(
    aura::Window* dragged_window,
    const gfx::Point& location_in_screen,
    IndicatorState indicator_state) {
  OverviewGrid* target_grid =
      GetGridWithRootWindow(dragged_window->GetRootWindow());
  if (!target_grid)
    return;
  target_grid->OnWindowDragContinued(dragged_window, location_in_screen,
                                     indicator_state);
}
void OverviewSession::OnWindowDragEnded(aura::Window* dragged_window,
                                        const gfx::Point& location_in_screen,
                                        bool should_drop_window_into_overview) {
  OverviewGrid* target_grid =
      GetGridWithRootWindow(dragged_window->GetRootWindow());
  if (!target_grid)
    return;
  target_grid->OnWindowDragEnded(dragged_window, location_in_screen,
                                 should_drop_window_into_overview);
}

void OverviewSession::PositionWindows(bool animate,
                                      OverviewItem* ignored_item) {
  for (std::unique_ptr<OverviewGrid>& grid : grid_list_)
    grid->PositionWindows(animate, ignored_item);
}

bool OverviewSession::IsWindowInOverview(const aura::Window* window) {
  for (const std::unique_ptr<OverviewGrid>& grid : grid_list_) {
    if (grid->GetOverviewItemContaining(window))
      return true;
  }
  return false;
}

void OverviewSession::SetWindowListNotAnimatedWhenExiting(
    aura::Window* root_window) {
  // Find the grid accociated with |root_window|.
  OverviewGrid* grid = GetGridWithRootWindow(root_window);
  if (grid)
    grid->SetWindowListNotAnimatedWhenExiting();
}

void OverviewSession::UpdateGridAtLocationYPositionAndOpacity(
    int64_t display_id,
    int new_y,
    float opacity,
    const gfx::Rect& work_area,
    UpdateAnimationSettingsCallback callback) {
  OverviewGrid* grid = GetGridWithRootWindow(
      ash::Shell::Get()->GetRootWindowForDisplayId(display_id));
  if (grid)
    grid->UpdateYPositionAndOpacity(new_y, opacity, work_area, callback);
}

void OverviewSession::UpdateMaskAndShadow() {
  for (auto& grid : grid_list_)
    for (auto& window : grid->window_list())
      window->UpdateMaskAndShadow();
}

void OverviewSession::OnStartingAnimationComplete(bool canceled) {
  for (auto& grid : grid_list_)
    grid->OnStartingAnimationComplete(canceled);

  if (!canceled) {
    if (overview_focus_widget_)
      overview_focus_widget_->Show();
    Shell::Get()->overview_controller()->DelayedUpdateMaskAndShadow();
  }
}

bool OverviewSession::IsOverviewGridAnimating() {
  for (auto& grid : grid_list_) {
    if (grid->shield_widget()
            ->GetNativeWindow()
            ->layer()
            ->GetAnimator()
            ->is_animating()) {
      return true;
    }
  }
  return false;
}

void OverviewSession::OnWindowActivating(
    ::wm::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  if (ignore_activations_ || !gained_active ||
      gained_active == GetOverviewFocusWindow()) {
    return;
  }

  auto* grid = GetGridWithRootWindow(gained_active->GetRootWindow());
  if (!grid)
    return;

  // Do not cancel overview mode if the window activation was caused by
  // snapping window to one side of the screen.
  if (Shell::Get()->IsSplitViewModeActive())
    return;

  // Do not cancel overview mode if the window activation was caused while
  // dragging overview mode offscreen.
  if (IsSlidingOutOverviewFromShelf())
    return;

  // Don't restore focus on exit if a window was just activated.
  ResetFocusRestoreWindow(false);
  const auto& windows = grid->window_list();
  auto iter = std::find_if(
      windows.begin(), windows.end(),
      [gained_active](const std::unique_ptr<OverviewItem>& window) {
        return window->Contains(gained_active);
      });

  if (iter != windows.end())
    selected_item_ = iter->get();
  CancelSelection();
}

aura::Window* OverviewSession::GetOverviewFocusWindow() {
  if (overview_focus_widget_)
    return overview_focus_widget_->GetNativeWindow();

  return nullptr;
}

void OverviewSession::SuspendReposition() {
  for (auto& grid : grid_list_)
    grid->set_suspend_reposition(true);
}

void OverviewSession::ResumeReposition() {
  for (auto& grid : grid_list_)
    grid->set_suspend_reposition(false);
}

void OverviewSession::OnDisplayRemoved(const display::Display& display) {
  // TODO(flackr): Keep window selection active on remaining displays.
  CancelSelection();
}

void OverviewSession::OnDisplayMetricsChanged(const display::Display& display,
                                              uint32_t metrics) {
  // For metrics changes that happen when the split view mode is active, the
  // display bounds will be adjusted in OnSplitViewDividerPositionChanged().
  if (Shell::Get()->IsSplitViewModeActive())
    return;
  OnDisplayBoundsChanged();
}

void OverviewSession::OnWindowHierarchyChanged(
    const HierarchyChangeParams& params) {
  // Only care about newly added children of |observed_windows_|.
  if (!observed_windows_.count(params.receiver) ||
      !observed_windows_.count(params.new_parent)) {
    return;
  }

  aura::Window* new_window = params.target;
  wm::WindowState* state = wm::GetWindowState(new_window);
  if (!state->IsUserPositionable() || state->IsPip())
    return;

  // If the new window is added when splitscreen is active, do nothing.
  // SplitViewController will do the right thing to snap the window or end
  // overview mode.
  if (Shell::Get()->IsSplitViewModeActive() &&
      new_window->GetRootWindow() == Shell::Get()
                                         ->split_view_controller()
                                         ->GetDefaultSnappedWindow()
                                         ->GetRootWindow()) {
    return;
  }

  for (size_t i = 0; i < wm::kSwitchableWindowContainerIdsLength; ++i) {
    if (new_window->parent()->id() == wm::kSwitchableWindowContainerIds[i] &&
        !::wm::GetTransientParent(new_window)) {
      // The new window is in one of the switchable containers, abort overview.
      CancelSelection();
      return;
    }
  }
}

void OverviewSession::OnWindowDestroying(aura::Window* window) {
  window->RemoveObserver(this);
  observed_windows_.erase(window);
  if (window == restore_focus_window_)
    restore_focus_window_ = nullptr;
}

void OverviewSession::OnKeyEvent(ui::KeyEvent* event) {
  if (event->type() != ui::ET_KEY_PRESSED)
    return;

  switch (event->key_code()) {
    case ui::VKEY_BROWSER_BACK:
      FALLTHROUGH;
    case ui::VKEY_ESCAPE:
      CancelSelection();
      break;
    case ui::VKEY_UP:
      num_key_presses_++;
      Move(UP, true);
      break;
    case ui::VKEY_DOWN:
      num_key_presses_++;
      Move(DOWN, true);
      break;
    case ui::VKEY_RIGHT:
    case ui::VKEY_TAB:
      if (event->key_code() == ui::VKEY_RIGHT ||
          !(event->flags() & ui::EF_SHIFT_DOWN)) {
        num_key_presses_++;
        Move(RIGHT, true);
        break;
      }
      FALLTHROUGH;
    case ui::VKEY_LEFT:
      num_key_presses_++;
      Move(LEFT, true);
      break;
    case ui::VKEY_W:
      if (!(event->flags() & ui::EF_CONTROL_DOWN) ||
          !grid_list_[selected_grid_index_]->is_selecting()) {
        return;
      }
      base::RecordAction(
          base::UserMetricsAction("WindowSelector_OverviewCloseKey"));
      grid_list_[selected_grid_index_]->SelectedWindow()->CloseWindow();
      break;
    case ui::VKEY_RETURN:
      // Ignore if no item is selected.
      if (!grid_list_[selected_grid_index_]->is_selecting())
        return;
      UMA_HISTOGRAM_COUNTS_100("Ash.WindowSelector.ArrowKeyPresses",
                               num_key_presses_);
      UMA_HISTOGRAM_CUSTOM_COUNTS("Ash.WindowSelector.KeyPressesOverItemsRatio",
                                  (num_key_presses_ * 100) / num_items_, 1, 300,
                                  30);
      base::RecordAction(
          base::UserMetricsAction("WindowSelector_OverviewEnterKey"));
      SelectWindow(grid_list_[selected_grid_index_]->SelectedWindow());
      break;
    default:
      return;
  }

  event->SetHandled();
  event->StopPropagation();
}

void OverviewSession::OnSplitViewStateChanged(
    SplitViewController::State previous_state,
    SplitViewController::State state) {
  const bool unsnappable_window_activated =
      state == SplitViewController::NO_SNAP &&
      Shell::Get()->split_view_controller()->end_reason() ==
          SplitViewController::EndReason::kUnsnappableWindowActivated;

  if (state != SplitViewController::NO_SNAP || unsnappable_window_activated) {
    // Do not restore focus if a window was just snapped and activated or
    // splitview mode is ended by activating an unsnappable window.
    ResetFocusRestoreWindow(false);
  }

  if (state == SplitViewController::BOTH_SNAPPED ||
      unsnappable_window_activated) {
    // If two windows were snapped to both sides of the screen or an unsnappable
    // window was just activated, end overview mode.
    CancelSelection();
  } else {
    // Otherwise adjust the overview window grid bounds if overview mode is
    // active at the moment.
    OnDisplayBoundsChanged();
    for (auto& grid : grid_list_)
      grid->UpdateCannotSnapWarningVisibility();
  }
}

void OverviewSession::OnSplitViewDividerPositionChanged() {
  DCHECK(Shell::Get()->IsSplitViewModeActive());
  // Re-calculate the bounds for the window grids and position all the windows.
  for (std::unique_ptr<OverviewGrid>& grid : grid_list_) {
    grid->SetBoundsAndUpdatePositions(
        GetGridBoundsInScreen(const_cast<aura::Window*>(grid->root_window()),
                              /*divider_changed=*/true));
  }
  PositionWindows(/*animate=*/false);
}

bool OverviewSession::IsEmpty() const {
  for (const auto& grid : grid_list_) {
    if (!grid->empty())
      return false;
  }
  return true;
}

void OverviewSession::ResetFocusRestoreWindow(bool focus) {
  if (!restore_focus_window_)
    return;

  // Ensure the window is still in the window hierarchy and not in the middle
  // of teardown.
  if (focus && restore_focus_window_->GetRootWindow()) {
    base::AutoReset<bool> restoring_focus(&ignore_activations_, true);
    wm::ActivateWindow(restore_focus_window_);
  }
  // If the window is in the observed_windows_ list it needs to continue to be
  // observed.
  if (observed_windows_.find(restore_focus_window_) ==
      observed_windows_.end()) {
    restore_focus_window_->RemoveObserver(this);
  }
  restore_focus_window_ = nullptr;
}

void OverviewSession::Move(Direction direction, bool animate) {
  // Direction to move if moving past the end of a display.
  int display_direction = (direction == RIGHT || direction == DOWN) ? 1 : -1;

  // If this is the first move and it's going backwards, start on the last
  // display.
  if (display_direction == -1 && !grid_list_.empty() &&
      !grid_list_[selected_grid_index_]->is_selecting()) {
    selected_grid_index_ = grid_list_.size() - 1;
  }

  // Keep calling Move() on the grids until one of them reports no overflow or
  // we made a full cycle on all the grids.
  for (size_t i = 0; i <= grid_list_.size() &&
                     grid_list_[selected_grid_index_]->Move(direction, animate);
       i++) {
    selected_grid_index_ =
        (selected_grid_index_ + display_direction + grid_list_.size()) %
        grid_list_.size();
  }
}

void OverviewSession::RemoveAllObservers() {
  for (auto* window : observed_windows_)
    window->RemoveObserver(this);
  observed_windows_.clear();

  display::Screen::GetScreen()->RemoveObserver(this);
  if (restore_focus_window_)
    restore_focus_window_->RemoveObserver(this);
}

void OverviewSession::OnDisplayBoundsChanged() {
  // Re-calculate the bounds for the window grids and position all the windows.
  for (std::unique_ptr<OverviewGrid>& grid : grid_list_) {
    grid->SetBoundsAndUpdatePositions(
        GetGridBoundsInScreen(const_cast<aura::Window*>(grid->root_window()),
                              /*divider_changed=*/false));
  }
  PositionWindows(/*animate=*/false);
  if (split_view_drag_indicators_)
    split_view_drag_indicators_->OnDisplayBoundsChanged();
}

}  // namespace ash
