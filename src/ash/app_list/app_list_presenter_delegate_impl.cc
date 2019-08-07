// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_presenter_delegate_impl.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_util.h"
#include "ash/app_list/presenter/app_list_presenter_impl.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/keyboard/ui/keyboard_controller.h"
#include "ash/public/cpp/app_list/app_list_switches.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/shelf/app_list_button.h"
#include "ash/shelf/back_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/command_line.h"
#include "chromeos/constants/chromeos_switches.h"
#include "ui/aura/window.h"
#include "ui/display/manager/display_manager.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {
namespace {

// Whether the shelf is oriented on the side, not on the bottom.
bool IsSideShelf(aura::Window* root_window) {
  Shelf* shelf = Shelf::ForWindow(root_window);
  switch (shelf->alignment()) {
    case SHELF_ALIGNMENT_BOTTOM:
    case SHELF_ALIGNMENT_BOTTOM_LOCKED:
      return false;
    case SHELF_ALIGNMENT_LEFT:
    case SHELF_ALIGNMENT_RIGHT:
      return true;
  }
  return false;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterDelegateImpl, public:

AppListPresenterDelegateImpl::AppListPresenterDelegateImpl(
    AppListControllerImpl* controller)
    : controller_(controller), display_observer_(this) {
  display_observer_.Add(display::Screen::GetScreen());
}

AppListPresenterDelegateImpl::~AppListPresenterDelegateImpl() {
  Shell::Get()->RemovePreTargetHandler(this);
}

void AppListPresenterDelegateImpl::SetPresenter(
    app_list::AppListPresenterImpl* presenter) {
  presenter_ = presenter;
}

void AppListPresenterDelegateImpl::Init(app_list::AppListView* view,
                                        int64_t display_id,
                                        int current_apps_page) {
  // App list needs to know the new shelf layout in order to calculate its
  // UI layout when AppListView visibility changes.
  Shell::GetPrimaryRootWindowController()
      ->GetShelfLayoutManager()
      ->UpdateAutoHideState();
  view_ = view;
  aura::Window* root_window = Shell::GetRootWindowForDisplayId(display_id);

  app_list::AppListView::InitParams params;
  const bool is_tablet_mode = IsTabletMode();
  aura::Window* parent_window =
      RootWindowController::ForWindow(root_window)
          ->GetContainer(is_tablet_mode ? kShellWindowId_HomeScreenContainer
                                        : kShellWindowId_AppListContainer);
  params.parent = parent_window;
  params.initial_apps_page = current_apps_page;
  params.is_tablet_mode = is_tablet_mode;
  params.is_side_shelf = IsSideShelf(root_window);
  view->Initialize(params);

  SnapAppListBoundsToDisplayEdge();
  Shell::Get()->AddPreTargetHandler(this);

  // By setting us as DnD recipient, the app list knows that we can
  // handle items.
  Shelf* shelf = Shelf::ForWindow(root_window);
  view->SetDragAndDropHostOfCurrentAppList(
      shelf->shelf_widget()->GetDragAndDropHostForAppList());
}

void AppListPresenterDelegateImpl::OnShown(int64_t display_id) {
  is_visible_ = true;
  controller_->ViewShown(display_id);
}

void AppListPresenterDelegateImpl::OnClosing() {
  DCHECK(is_visible_);
  DCHECK(view_);
  is_visible_ = false;
  Shell::Get()->RemovePreTargetHandler(this);
  controller_->ViewClosing();
}

void AppListPresenterDelegateImpl::OnClosed() {
  controller_->ViewClosed();
}

gfx::Vector2d AppListPresenterDelegateImpl::GetVisibilityAnimationOffset(
    aura::Window* root_window) {
  DCHECK(Shell::HasInstance());

  Shelf* shelf = Shelf::ForWindow(root_window);

  // App list needs to know the new shelf layout in order to calculate its
  // UI layout when AppListView visibility changes.
  int app_list_y = view_->GetBoundsInScreen().y();
  return gfx::Vector2d(0, IsSideShelf(root_window)
                              ? 0
                              : shelf->GetIdealBounds().y() - app_list_y);
}

base::TimeDelta AppListPresenterDelegateImpl::GetVisibilityAnimationDuration(
    aura::Window* root_window,
    bool is_visible) {
  // If the view is below the shelf, just hide immediately.
  if (view_->GetBoundsInScreen().y() >
      Shelf::ForWindow(root_window)->GetIdealBounds().y()) {
    return base::TimeDelta::FromMilliseconds(0);
  }
  return GetAnimationDurationFullscreen(IsSideShelf(root_window),
                                        view_->is_fullscreen());
}

bool AppListPresenterDelegateImpl::IsTabletMode() const {
  return Shell::Get()
      ->tablet_mode_controller()
      ->IsTabletModeWindowManagerEnabled();
}

app_list::AppListViewDelegate*
AppListPresenterDelegateImpl::GetAppListViewDelegate() {
  return controller_;
}

bool AppListPresenterDelegateImpl::GetOnScreenKeyboardShown() {
  return controller_->onscreen_keyboard_shown();
}

aura::Window* AppListPresenterDelegateImpl::GetRootWindowForDisplayId(
    int64_t display_id) {
  return ash::Shell::Get()->GetRootWindowForDisplayId(display_id);
}

void AppListPresenterDelegateImpl::OnVisibilityChanged(bool visible,
                                                       int64_t display_id) {
  controller_->NotifyAppListVisibilityChanged(visible, display_id);
}

void AppListPresenterDelegateImpl::OnTargetVisibilityChanged(bool visible) {
  controller_->NotifyAppListTargetVisibilityChanged(visible);
}

void AppListPresenterDelegateImpl::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  if (!presenter_->GetWindow())
    return;

  view_->OnParentWindowBoundsChanged();
  SnapAppListBoundsToDisplayEdge();
}

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterDelegateImpl, private:

void AppListPresenterDelegateImpl::ProcessLocatedEvent(
    ui::LocatedEvent* event) {
  if (!view_ || !is_visible_)
    return;

  aura::Window* target = static_cast<aura::Window*>(event->target());
  if (!target)
    return;
  // If the event happened on a menu, then the event should not close the app
  // list.
  RootWindowController* root_controller =
      RootWindowController::ForWindow(target);
  if (root_controller) {
    aura::Window* menu_container =
        root_controller->GetContainer(kShellWindowId_MenuContainer);
    if (menu_container->Contains(target))
      return;
    aura::Window* keyboard_container =
        root_controller->GetContainer(kShellWindowId_VirtualKeyboardContainer);
    if (keyboard_container->Contains(target))
      return;
  }

  // If the event happened on the app list button, it'll get handled by the
  // button.
  Shelf* shelf = Shelf::ForWindow(target);
  AppListButton* app_list_button = shelf->shelf_widget()->GetAppListButton();
  if (app_list_button && app_list_button->GetWidget() &&
      target == app_list_button->GetWidget()->GetNativeWindow() &&
      app_list_button->bounds().Contains(event->location())) {
    return;
  }

  // If the event happened on the back button, it'll get handled by the
  // button.
  BackButton* back_button = shelf->shelf_widget()->GetBackButton();
  if (back_button && back_button->GetWidget() &&
      target == back_button->GetWidget()->GetNativeWindow() &&
      back_button->bounds().Contains(event->location())) {
    return;
  }

  aura::Window* window = view_->GetWidget()->GetNativeView()->parent();
  if (!window->Contains(target) && !presenter_->HandleCloseOpenFolder() &&
      !app_list::switches::ShouldNotDismissOnBlur() && !IsTabletMode()) {
    const aura::Window* status_window =
        shelf->shelf_widget()->status_area_widget()->GetNativeWindow();
    const aura::Window* shelf_window = shelf->shelf_widget()->GetNativeWindow();
    // Don't dismiss the auto-hide shelf if event happened in status area. Then
    // the event can still be propagated to the status area tray to open the
    // corresponding tray bubble.
    base::Optional<Shelf::ScopedAutoHideLock> auto_hide_lock;
    if (status_window && status_window->Contains(target))
      auto_hide_lock.emplace(shelf);

    // Keep app list opened if event happened in the shelf area.
    if (!shelf_window || !shelf_window->Contains(target))
      presenter_->Dismiss(event->time_stamp());
  }

  if (IsTabletMode() && presenter_->IsShowingEmbeddedAssistantUI()) {
    auto* contents_view =
        presenter_->GetView()->app_list_main_view()->contents_view();
    if (target == contents_view->GetWidget()->GetNativeWindow() &&
        contents_view->bounds().Contains(event->location())) {
      // Keep Assistant open if event happen inside.
      return;
    }

    // Touching anywhere else closes Assistant.
    view_->Back();
    view_->search_box_view()->ClearSearch();
    view_->search_box_view()->SetSearchBoxActive(false, ui::ET_UNKNOWN);
  }
}

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterDelegateImpl, aura::EventFilter implementation:

void AppListPresenterDelegateImpl::OnMouseEvent(ui::MouseEvent* event) {
  // Moving the mouse shouldn't hide focus rings.
  if (event->IsAnyButton())
    controller_->SetKeyboardTraversalMode(false);

  if (event->type() == ui::ET_MOUSE_PRESSED)
    ProcessLocatedEvent(event);
}

void AppListPresenterDelegateImpl::OnGestureEvent(ui::GestureEvent* event) {
  controller_->SetKeyboardTraversalMode(false);

  if (event->type() == ui::ET_GESTURE_TAP ||
      event->type() == ui::ET_GESTURE_TWO_FINGER_TAP ||
      event->type() == ui::ET_GESTURE_LONG_PRESS) {
    ProcessLocatedEvent(event);
  }
}

void AppListPresenterDelegateImpl::OnKeyEvent(ui::KeyEvent* event) {
  // If keyboard traversal is already engaged, no-op.
  if (controller_->KeyboardTraversalEngaged())
    return;

  // If the home launcher is not shown in tablet mode, ignore events.
  if (IsTabletMode() && !presenter_->home_launcher_shown())
    return;

  // Don't absorb the first event for the search box while it is open
  if (view_->search_box_view()->is_search_box_active())
    return;

  // Arrow keys or Tab will engage the traversal mode.
  if ((app_list::IsUnhandledArrowKeyEvent(*event) ||
       event->key_code() == ui::VKEY_TAB)) {
    // Handle the first arrow key event to just show the focus rings.
    event->SetHandled();
    controller_->SetKeyboardTraversalMode(true);
  }
}

void AppListPresenterDelegateImpl::SnapAppListBoundsToDisplayEdge() {
  CHECK(view_ && view_->GetWidget());
  aura::Window* window = view_->GetWidget()->GetNativeView();
  const gfx::Rect bounds =
      ash::screen_util::SnapBoundsToDisplayEdge(window->bounds(), window);
  window->SetBounds(bounds);
}

}  // namespace ash
