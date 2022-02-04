// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_bubble_presenter.h"

#include <memory>
#include <utility>

#include "ash/app_list/app_list_bubble_event_filter.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_event_targeter.h"
#include "ash/app_list/views/app_list_bubble_apps_page.h"
#include "ash/app_list/views/app_list_bubble_view.h"
#include "ash/app_list/views/app_list_drag_and_drop_host.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_client.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shell.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/wm/container_finder.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chromeos/services/assistant/public/cpp/assistant_enums.h"
#include "ui/aura/client/focus_client.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

using chromeos::assistant::AssistantExitPoint;

// Maximum amount of time to spend refreshing zero state search results before
// opening the launcher.
constexpr base::TimeDelta kZeroStateSearchTimeout = base::Milliseconds(16);

// Space between the edge of the bubble and the edge of the work area.
constexpr int kWorkAreaPadding = 8;

// Space between the AppListBubbleView and the top of the screen should be at
// least this value plus the shelf height.
constexpr int kExtraTopOfScreenSpacing = 16;

gfx::Rect GetWorkAreaForBubble(aura::Window* root_window) {
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_window);
  gfx::Rect work_area = display.work_area();

  // Subtract the shelf's bounds from the work area, since the shelf should
  // always be shown with the app list bubble. This is done because the work
  // area includes the area under the shelf when the shelf is set to auto-hide.
  work_area.Subtract(Shelf::ForWindow(root_window)->GetIdealBounds());

  return work_area;
}

// Returns the preferred size of the bubble widget in DIPs.
gfx::Size ComputeBubbleSize(aura::Window* root_window,
                            AppListBubbleView* bubble_view) {
  const int default_height = 688;
  // As of August 2021 the assistant cards require a minimum width of 640. If
  // the cards become narrower then this could be reduced.
  const int default_width = 640;
  const int shelf_size = ShelfConfig::Get()->shelf_size();
  const gfx::Rect work_area = GetWorkAreaForBubble(root_window);
  int height = default_height;

  // If the work area height is too small to fit the default size bubble, then
  // calculate a smaller height to fit in the work area. Otherwise, if the work
  // area height is tall enough to fit at least two default sized bubbles, then
  // calculate a taller bubble with height taking no more than half the work
  // area.
  if (work_area.height() <
      default_height + shelf_size + kExtraTopOfScreenSpacing) {
    height = work_area.height() - shelf_size - kExtraTopOfScreenSpacing;
  } else if (work_area.height() >
             default_height * 2 + shelf_size + kExtraTopOfScreenSpacing) {
    // Calculate the height required to fit the contents of the AppListBubble
    // with no scrolling.
    int height_to_fit_all_apps = bubble_view->GetHeightToFitAllApps();
    int max_height =
        (work_area.height() - shelf_size - kExtraTopOfScreenSpacing) / 2;
    DCHECK_GE(max_height, default_height);
    height = base::clamp(height_to_fit_all_apps, default_height, max_height);
  }

  return gfx::Size(default_width, height);
}

// Returns the bounds in root window coordinates for the bubble widget.
gfx::Rect ComputeBubbleBounds(aura::Window* root_window,
                              AppListBubbleView* bubble_view) {
  const gfx::Rect work_area = GetWorkAreaForBubble(root_window);
  const gfx::Size bubble_size = ComputeBubbleSize(root_window, bubble_view);
  const int padding = kWorkAreaPadding;  // Shorten name for readability.
  int x = 0;
  int y = 0;
  switch (Shelf::ForWindow(root_window)->alignment()) {
    case ShelfAlignment::kBottom:
    case ShelfAlignment::kBottomLocked:
      if (base::i18n::IsRTL())
        x = work_area.right() - padding - bubble_size.width();
      else
        x = work_area.x() + padding;
      y = work_area.bottom() - padding - bubble_size.height();
      break;
    case ShelfAlignment::kLeft:
      x = work_area.x() + padding;
      y = work_area.y() + padding;
      break;
    case ShelfAlignment::kRight:
      x = work_area.right() - padding - bubble_size.width();
      y = work_area.y() + padding;
      break;
  }
  return gfx::Rect(x, y, bubble_size.width(), bubble_size.height());
}

// Creates a bubble widget for the display with `root_window`. The widget is
// owned by its native widget.
views::Widget* CreateBubbleWidget(aura::Window* root_window) {
  views::Widget* widget = new views::Widget();
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.name = "AppListBubble";
  params.parent =
      Shell::GetContainer(root_window, kShellWindowId_AppListContainer);
  // AppListBubbleView handles round corners and blur via layers.
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.layer_type = ui::LAYER_NOT_DRAWN;
  widget->Init(std::move(params));
  return widget;
}

}  // namespace

AppListBubblePresenter::AppListBubblePresenter(
    AppListControllerImpl* controller)
    : controller_(controller) {
  DCHECK(controller_);
}

AppListBubblePresenter::~AppListBubblePresenter() {
  CHECK(!views::WidgetObserver::IsInObserverList());
}

void AppListBubblePresenter::Shutdown() {
  DVLOG(1) << __PRETTY_FUNCTION__;
  // Aborting in-progress animations will run their cleanup callbacks, which
  // might close the widget.
  if (bubble_view_)
    bubble_view_->AbortAllAnimations();
  if (bubble_widget_)
    bubble_widget_->CloseNow();  // Calls OnWidgetDestroying().
  DCHECK(!bubble_widget_);
  DCHECK(!bubble_view_);
}

void AppListBubblePresenter::Show(int64_t display_id) {
  DVLOG(1) << __PRETTY_FUNCTION__;
  DCHECK(features::IsProductivityLauncherEnabled());
  if (is_target_visibility_show_)
    return;

  if (bubble_view_)
    bubble_view_->AbortAllAnimations();

  is_target_visibility_show_ = true;
  target_page_ = AppListBubblePage::kApps;

  // Refresh the continue tasks before opening the launcher. If a file doesn't
  // exist on disk anymore then the launcher should not create or animate the
  // continue task view for that suggestion.
  controller_->GetClient()->StartZeroStateSearch(
      base::BindOnce(&AppListBubblePresenter::OnZeroStateSearchDone,
                     weak_factory_.GetWeakPtr(), display_id),
      kZeroStateSearchTimeout);
}

void AppListBubblePresenter::OnZeroStateSearchDone(int64_t display_id) {
  DVLOG(1) << __PRETTY_FUNCTION__;
  aura::Window* root_window = Shell::GetRootWindowForDisplayId(display_id);
  // Display might have disconnected during zero state refresh.
  if (!root_window)
    return;

  Shelf* shelf = Shelf::ForWindow(root_window);
  ApplicationDragAndDropHost* drag_and_drop_host =
      shelf->shelf_widget()->GetDragAndDropHostForAppList();
  HomeButton* home_button = shelf->navigation_widget()->GetHomeButton();

  if (!bubble_widget_) {
    // If the bubble widget is null, this is the first show. Construct views.
    base::TimeTicks time_shown = base::TimeTicks::Now();

    bubble_widget_ = CreateBubbleWidget(root_window);
    bubble_widget_->GetNativeWindow()->SetEventTargeter(
        std::make_unique<AppListEventTargeter>(controller_));
    bubble_view_ = bubble_widget_->SetContentsView(
        std::make_unique<AppListBubbleView>(controller_, drag_and_drop_host));
    // Arrow left/right and up/down triggers the same focus movement as
    // tab/shift+tab.
    bubble_widget_->widget_delegate()->SetEnableArrowKeyTraversal(true);

    bubble_widget_->AddObserver(this);
    // Set up event filter to close the bubble for clicks outside the bubble
    // that don't cause window activation changes (e.g. clicks on wallpaper or
    // blank areas of shelf).
    bubble_event_filter_ = std::make_unique<AppListBubbleEventFilter>(
        bubble_widget_, home_button,
        base::BindRepeating(&AppListBubblePresenter::OnPressOutsideBubble,
                            base::Unretained(this)));

    UmaHistogramTimes("Apps.AppListBubbleCreationTime",
                      base::TimeTicks::Now() - time_shown);
  } else {
    DCHECK(bubble_view_);
    // The bubble widget is cached, but it may change displays. Update pointers
    // that are tied to the display.
    bubble_view_->SetDragAndDropHostOfCurrentAppList(drag_and_drop_host);
    // Refresh suggestions now that zero-state search data is updated.
    bubble_view_->UpdateSuggestions();
    bubble_event_filter_->SetButton(home_button);
    // The observer for the correct display will be added below.
    aura::client::GetFocusClient(bubble_widget_->GetNativeWindow())
        ->RemoveObserver(this);
  }
  // The widget bounds sometimes depend on the height of the apps grid, so set
  // the bounds after creating and setting the contents. This may cause the
  // bubble to change displays.
  bubble_widget_->SetBounds(ComputeBubbleBounds(root_window, bubble_view_));

  // Bubble launcher is always keyboard traversable. Update every show in case
  // we are coming out of tablet mode.
  controller_->SetKeyboardTraversalMode(true);

  // The focus client is tied to the root window, so update the observer every
  // time the bubble is shown to make sure it tracks the right display.
  aura::client::GetFocusClient(bubble_widget_->GetNativeWindow())
      ->AddObserver(this);
  controller_->OnVisibilityWillChange(/*visible=*/true, display_id);
  bubble_widget_->Show();
  // The page must be set before triggering the show animation so the correct
  // animations are triggered.
  bubble_view_->ShowPage(target_page_);
  if (features::IsProductivityLauncherAnimationEnabled()) {
    bubble_view_->StartShowAnimation();
  }
  controller_->OnVisibilityChanged(/*visible=*/true, display_id);
}

ShelfAction AppListBubblePresenter::Toggle(int64_t display_id) {
  DVLOG(1) << __PRETTY_FUNCTION__;
  DCHECK(features::IsProductivityLauncherEnabled());
  if (is_target_visibility_show_) {
    Dismiss();
    return SHELF_ACTION_APP_LIST_DISMISSED;
  }
  Show(display_id);
  return SHELF_ACTION_APP_LIST_SHOWN;
}

void AppListBubblePresenter::Dismiss() {
  DVLOG(1) << __PRETTY_FUNCTION__;
  DCHECK(features::IsProductivityLauncherEnabled());
  if (!is_target_visibility_show_)
    return;

  // Check for view because the code could be waiting for zero-state search
  // results before first show.
  if (bubble_view_)
    bubble_view_->AbortAllAnimations();

  // Must call before setting `is_target_visibility_show_` to false.
  const int64_t display_id = GetDisplayId();

  is_target_visibility_show_ = false;

  // Reset keyboard traversal in case the user switches to tablet launcher.
  // Must happen before widget is destroyed.
  controller_->SetKeyboardTraversalMode(false);

  controller_->ViewClosing();
  controller_->OnVisibilityWillChange(/*visible=*/false, display_id);
  if (features::IsProductivityLauncherAnimationEnabled()) {
    if (bubble_view_) {
      bubble_view_->StartHideAnimation(
          base::BindOnce(&AppListBubblePresenter::OnHideAnimationEnded,
                         weak_factory_.GetWeakPtr()));
    }
  } else {
    // Check for widget because the code could be waiting for zero-state search
    // results before first show.
    if (bubble_widget_)
      OnHideAnimationEnded();
  }
  controller_->OnVisibilityChanged(/*visible=*/false, display_id);

  // Clean up assistant if it is showing.
  controller_->ScheduleCloseAssistant();
}

aura::Window* AppListBubblePresenter::GetWindow() const {
  return is_target_visibility_show_ && bubble_widget_
             ? bubble_widget_->GetNativeWindow()
             : nullptr;
}

bool AppListBubblePresenter::IsShowing() const {
  return is_target_visibility_show_;
}

bool AppListBubblePresenter::IsShowingEmbeddedAssistantUI() const {
  if (!is_target_visibility_show_)
    return false;
  DCHECK(bubble_widget_);
  return bubble_view_->IsShowingEmbeddedAssistantUI();
}

void AppListBubblePresenter::UpdateForNewSortingOrder(
    const absl::optional<AppListSortOrder>& new_order,
    bool animate,
    base::OnceClosure update_position_closure) {
  DCHECK_EQ(animate, !update_position_closure.is_null());

  if (!bubble_view_) {
    // A rare case. Still handle it for safety.
    if (update_position_closure)
      std::move(update_position_closure).Run();
    return;
  }

  bubble_view_->UpdateForNewSortingOrder(new_order, animate,
                                         std::move(update_position_closure));
}

void AppListBubblePresenter::ShowEmbeddedAssistantUI() {
  DVLOG(1) << __PRETTY_FUNCTION__;
  target_page_ = AppListBubblePage::kAssistant;
  // `bubble_view_` does not exist while waiting for zero-state results.
  // OnZeroStateSearchDone() sets the page in that case.
  if (bubble_view_) {
    DCHECK(bubble_widget_);
    bubble_view_->ShowEmbeddedAssistantUI();
  }
}

void AppListBubblePresenter::OnWidgetDestroying(views::Widget* widget) {
  DVLOG(1) << __PRETTY_FUNCTION__;
  // NOTE: While the widget is usually cached after Show(), this method can be
  // called on monitor disconnect. Clean up state.
  // `bubble_event_filter_` holds a pointer to the widget.
  bubble_event_filter_.reset();
  aura::client::GetFocusClient(bubble_widget_->GetNativeView())
      ->RemoveObserver(this);
  bubble_widget_->RemoveObserver(this);
  bubble_widget_ = nullptr;
  bubble_view_ = nullptr;
}

void AppListBubblePresenter::OnWindowFocused(aura::Window* gained_focus,
                                             aura::Window* lost_focus) {
  if (!is_target_visibility_show_)
    return;

  aura::Window* app_list_container =
      bubble_widget_->GetNativeWindow()->parent();

  // If the bubble or one of its children (e.g. an uninstall dialog) gained
  // focus, the bubble should stay open.
  if (gained_focus && app_list_container->Contains(gained_focus))
    return;

  // Otherwise, if the bubble or one of its children lost focus, the bubble
  // should close.
  if (lost_focus && app_list_container->Contains(lost_focus))
    Dismiss();
}

void AppListBubblePresenter::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  if (!IsShowing())
    return;
  // Ignore changes to displays that aren't showing the launcher.
  if (display.id() != GetDisplayId())
    return;
  aura::Window* root_window =
      bubble_widget_->GetNativeWindow()->GetRootWindow();
  bubble_widget_->SetBounds(ComputeBubbleBounds(root_window, bubble_view_));
}

void AppListBubblePresenter::OnPressOutsideBubble() {
  // Presses outside the bubble could be activating a shelf item. Record the
  // app list state prior to dismissal.
  controller_->RecordAppListState();
  Dismiss();
}

int64_t AppListBubblePresenter::GetDisplayId() const {
  if (!is_target_visibility_show_ || !bubble_widget_)
    return display::kInvalidDisplayId;
  return display::Screen::GetScreen()
      ->GetDisplayNearestView(bubble_widget_->GetNativeView())
      .id();
}

void AppListBubblePresenter::OnHideAnimationEnded() {
  // Hiding the launcher causes a window activation change. If the launcher is
  // hiding because the user opened a system tray bubble, don't immediately
  // close the bubble in response.
  auto lock = TrayBackgroundView::DisableCloseBubbleOnWindowActivated();
  bubble_widget_->Hide();

  controller_->MaybeCloseAssistant();
}

}  // namespace ash
