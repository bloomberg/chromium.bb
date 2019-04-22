// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_window_manager.h"

#include <memory>

#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/scoped_animation_disabler.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/tablet_mode/scoped_skip_user_session_blocked_check.h"
#include "ash/wm/tablet_mode/tablet_mode_backdrop_delegate_impl.h"
#include "ash/wm/tablet_mode/tablet_mode_event_handler.h"
#include "ash/wm/tablet_mode/tablet_mode_window_state.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/workspace_controller.h"
#include "base/command_line.h"
#include "base/stl_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/display/screen.h"

namespace ash {

namespace {

// Exits overview mode if it is currently active. Returns true if overview is
// active before cancelling it.
bool CancelOverview() {
  OverviewController* controller = Shell::Get()->overview_controller();
  if (controller->IsSelecting()) {
    controller->OnSelectionEnded();
    return true;
  }
  return false;
}

// Returns true if windows[i] exists, can snap in split view, and is not ARC. A
// window snapped in clamshell mode must meet these criteria to potentially
// carry over into tablet mode split view. We want ARC windows to be included,
// but there is an obstacle (see https://crbug.com/922282 and
// https://buganizer.corp.google.com/issues/123432223).
bool IsCandidateForSplitView(const MruWindowTracker::WindowList& windows,
                             size_t i) {
  return windows.size() > i && CanSnapInSplitview(windows[i]) &&
         static_cast<ash::AppType>(windows[i]->GetProperty(
             aura::client::kAppType)) != AppType::ARC_APP;
}

// Iterates over |windows| and |positions| in parallel, up to the length of
// |positions| (assumed not to exceed the length of |windows|), snapping the
// windows in the positions.
void DoSplitView(
    const MruWindowTracker::WindowList& windows,
    const std::vector<SplitViewController::SnapPosition>& positions) {
  DCHECK_GE(windows.size(), positions.size());

  SplitViewController* split_view_controller =
      Shell::Get()->split_view_controller();
  for (size_t i = 0u; i < positions.size(); ++i)
    split_view_controller->SnapWindow(windows[i], positions[i]);
}

}  // namespace

TabletModeWindowManager::~TabletModeWindowManager() {
  // Overview mode needs to be ended before exiting tablet mode to prevent
  // transforming windows which are currently in
  // overview: http://crbug.com/366605
  const bool was_in_overview = CancelOverview();
  for (aura::Window* window : added_windows_)
    window->RemoveObserver(this);
  added_windows_.clear();
  Shell::Get()->RemoveShellObserver(this);
  Shell::Get()->session_controller()->RemoveObserver(this);
  Shell::Get()->overview_controller()->RemoveObserver(this);
  display::Screen::GetScreen()->RemoveObserver(this);
  EnableBackdropBehindTopWindowOnEachDisplay(false);
  RemoveWindowCreationObservers();
  ArrangeWindowsForDesktopMode(was_in_overview);
}

int TabletModeWindowManager::GetNumberOfManagedWindows() {
  return window_state_map_.size();
}

void TabletModeWindowManager::AddWindow(aura::Window* window) {
  // Only add the window if it is a direct dependent of a container window
  // and not yet tracked.
  if (base::ContainsKey(window_state_map_, window) ||
      !IsContainerWindow(window->parent())) {
    return;
  }

  TrackWindow(window);
}

void TabletModeWindowManager::WindowStateDestroyed(aura::Window* window) {
  // We come here because the tablet window state object was destroyed. It was
  // destroyed either because ForgetWindow() was called, or because its
  // associated window was destroyed. In both cases, the window must has removed
  // TabletModeWindowManager as an observer.
  DCHECK(!window->HasObserver(this));

  // The window state object might have been removed in OnWindowDestroying().
  auto it = window_state_map_.find(window);
  if (it != window_state_map_.end())
    window_state_map_.erase(it);
}

void TabletModeWindowManager::OnOverviewModeEndingAnimationComplete(
    bool canceled) {
  if (canceled)
    return;

  auto* split_view_controller = Shell::Get()->split_view_controller();

  // Maximize all snapped windows upon exiting overview mode except snapped
  // windows in splitview mode. Note the snapped window might not be tracked in
  // our |window_state_map_|.
  MruWindowTracker::WindowList windows =
      Shell::Get()->mru_window_tracker()->BuildWindowListIgnoreModal();
  for (auto* window : windows) {
    if (split_view_controller->left_window() != window &&
        split_view_controller->right_window() != window) {
      MaximizeIfSnapped(window);
    }
  }
}

// ShellObserver:
void TabletModeWindowManager::OnSplitViewModeEnded() {
  switch (Shell::Get()->split_view_controller()->end_reason()) {
    case SplitViewController::EndReason::kNormal:
    case SplitViewController::EndReason::kUnsnappableWindowActivated:
    case SplitViewController::EndReason::kPipExpanded:
      break;
    case SplitViewController::EndReason::kHomeLauncherPressed:
    case SplitViewController::EndReason::kActiveUserChanged:
    case SplitViewController::EndReason::kWindowDragStarted:
      // For the case of kHomeLauncherPressed, the home launcher will minimize
      // the snapped windows after ending splitview, so avoid maximizing them
      // here. For the case of kActiveUserChanged, the snapped windows will be
      // used to restore the splitview layout when switching back, and it is
      // already too late to maximize them anyway (the for loop below would
      // iterate over windows in the newly activated user session).
      return;
  }

  // Maximize all snapped windows upon exiting split view mode. Note the snapped
  // window might not be tracked in our |window_state_map_|.
  MruWindowTracker::WindowList windows =
      Shell::Get()->mru_window_tracker()->BuildWindowListIgnoreModal();
  for (auto* window : windows)
    MaximizeIfSnapped(window);
}

void TabletModeWindowManager::OnWindowDestroying(aura::Window* window) {
  if (IsContainerWindow(window)) {
    // container window can be removed on display destruction.
    window->RemoveObserver(this);
    observed_container_windows_.erase(window);
  } else if (base::ContainsKey(added_windows_, window)) {
    // Added window was destroyed before being shown.
    added_windows_.erase(window);
    window->RemoveObserver(this);
  } else {
    // If a known window gets destroyed we need to remove all knowledge about
    // it.
    ForgetWindow(window, true /* destroyed */);
  }
}

void TabletModeWindowManager::OnWindowHierarchyChanged(
    const HierarchyChangeParams& params) {
  // A window can get removed and then re-added by a drag and drop operation.
  if (params.new_parent && IsContainerWindow(params.new_parent) &&
      !base::ContainsKey(window_state_map_, params.target)) {
    // Don't register the window if the window is invisible. Instead,
    // wait until it becomes visible because the client may update the
    // flag to control if the window should be added.
    if (!params.target->IsVisible()) {
      if (!base::ContainsKey(added_windows_, params.target)) {
        added_windows_.insert(params.target);
        params.target->AddObserver(this);
      }
      return;
    }
    TrackWindow(params.target);
    // When the state got added, the "WM_EVENT_ADDED_TO_WORKSPACE" event got
    // already sent and we have to notify our state again.
    if (base::ContainsKey(window_state_map_, params.target)) {
      wm::WMEvent event(wm::WM_EVENT_ADDED_TO_WORKSPACE);
      wm::GetWindowState(params.target)->OnWMEvent(&event);
    }
  }
}

void TabletModeWindowManager::OnWindowPropertyChanged(aura::Window* window,
                                                      const void* key,
                                                      intptr_t old) {
  // Stop managing |window| if the always-on-top property is added.
  if (key == aura::client::kAlwaysOnTopKey &&
      window->GetProperty(aura::client::kAlwaysOnTopKey)) {
    ForgetWindow(window, false /* destroyed */);
  }
}

void TabletModeWindowManager::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  if (!IsContainerWindow(window))
    return;

  auto* session = Shell::Get()->overview_controller()->overview_session();
  if (session)
    session->SuspendReposition();

  // Reposition all non maximizeable windows.
  for (auto& pair : window_state_map_) {
    pair.second->UpdateWindowPosition(wm::GetWindowState(pair.first),
                                      /*animate=*/false);
  }
  if (session)
    session->ResumeReposition();
}

void TabletModeWindowManager::OnWindowVisibilityChanged(aura::Window* window,
                                                        bool visible) {
  // Skip if it's already managed.
  if (base::ContainsKey(window_state_map_, window))
    return;

  if (IsContainerWindow(window->parent()) &&
      base::ContainsKey(added_windows_, window) && visible) {
    added_windows_.erase(window);
    window->RemoveObserver(this);
    TrackWindow(window);
    // When the state got added, the "WM_EVENT_ADDED_TO_WORKSPACE" event got
    // already sent and we have to notify our state again.
    if (base::ContainsKey(window_state_map_, window)) {
      wm::WMEvent event(wm::WM_EVENT_ADDED_TO_WORKSPACE);
      wm::GetWindowState(window)->OnWMEvent(&event);
    }
  }
}

void TabletModeWindowManager::OnDisplayAdded(const display::Display& display) {
  DisplayConfigurationChanged();
}

void TabletModeWindowManager::OnDisplayRemoved(
    const display::Display& display) {
  DisplayConfigurationChanged();
}

void TabletModeWindowManager::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  SplitViewController* split_view_controller =
      Shell::Get()->split_view_controller();

  // There is only one SplitViewController object for all user sessions, but
  // functionally, each user session independently can be in split view or not.
  // Here, a new user session has just been switched to, and if split view mode
  // is active then it was for the previous user session.
  // SplitViewController::EndSplitView() will perform some cleanup, including
  // setting |SplitViewController::left_window_| and
  // |SplitViewController::right_window_| to null, but the aura::Window objects
  // will be left unchanged to facilitate switching back.
  split_view_controller->EndSplitView(
      SplitViewController::EndReason::kActiveUserChanged);

  // If a user session is now active for the first time since clamshell mode,
  // then do the logic for carrying over snapped windows. Else recreate the
  // split view layout from the last time the current user session was active.
  if (accounts_since_entering_tablet_.count(account_id) == 0u) {
    MruWindowTracker::WindowList windows =
        Shell::Get()->mru_window_tracker()->BuildWindowForCycleList();
    DoSplitView(windows, GetSnapPositions(windows));
    accounts_since_entering_tablet_.insert(account_id);
  } else {
    // Search for snapped windows to detect if the now active user session was
    // in split view. In case multiple windows were snapped to one side, one
    // window after another, there may be multiple windows in a LEFT_SNAPPED
    // state or multiple windows in a RIGHT_SNAPPED state. For each of those two
    // state types that belongs to multiple windows, the relevant window will be
    // listed first among those windows, and a null check in the loop body below
    // will filter out the rest of them.
    // TODO(amusbach): The windows that were in split view may have later been
    // destroyed or changed to non-snapped states. Then the following for loop
    // could snap windows that were not in split view. Also, a window may have
    // become full screen, and if so, then it would be better not to reactivate
    // split view. See https://crbug.com/944134.
    MruWindowTracker::WindowList windows =
        Shell::Get()->mru_window_tracker()->BuildWindowListIgnoreModal();
    for (aura::Window* window : windows) {
      switch (wm::GetWindowState(window)->GetStateType()) {
        case mojom::WindowStateType::LEFT_SNAPPED:
          if (split_view_controller->left_window() == nullptr) {
            split_view_controller->SnapWindow(window,
                                              SplitViewController::LEFT);
          }
          break;
        case mojom::WindowStateType::RIGHT_SNAPPED:
          if (split_view_controller->right_window() == nullptr) {
            split_view_controller->SnapWindow(window,
                                              SplitViewController::RIGHT);
          }
          break;
        default:
          break;
      }
      if (split_view_controller->state() == SplitViewController::BOTH_SNAPPED)
        break;
    }
  }

  // Ensure that overview mode is active if and only if there is a window
  // snapped to one side but no window snapped to the other side.
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  SplitViewController::State state = split_view_controller->state();
  if (overview_controller->IsSelecting() !=
      (state == SplitViewController::LEFT_SNAPPED ||
       state == SplitViewController::RIGHT_SNAPPED)) {
    overview_controller->ToggleOverview();
  }
}

void TabletModeWindowManager::SetIgnoreWmEventsForExit() {
  for (auto& pair : window_state_map_)
    pair.second->set_ignore_wm_events(true);
}

TabletModeWindowManager::TabletModeWindowManager() {
  // The overview mode needs to be ended before the tablet mode is started. To
  // guarantee the proper order, it will be turned off from here.
  CancelOverview();

  ArrangeWindowsForTabletMode();
  AddWindowCreationObservers();
  EnableBackdropBehindTopWindowOnEachDisplay(true);
  display::Screen::GetScreen()->AddObserver(this);
  Shell::Get()->AddShellObserver(this);
  Shell::Get()->session_controller()->AddObserver(this);
  Shell::Get()->overview_controller()->AddObserver(this);
  accounts_since_entering_tablet_.insert(
      Shell::Get()->session_controller()->GetActiveAccountId());
  event_handler_ = std::make_unique<wm::TabletModeEventHandler>();
}

mojom::WindowStateType TabletModeWindowManager::GetDesktopWindowStateType(
    aura::Window* window) const {
  auto iter = window_state_map_.find(window);
  return iter == window_state_map_.end()
             ? wm::GetWindowState(window)->GetStateType()
             : iter->second->old_state()->GetType();
}

std::vector<SplitViewController::SnapPosition>
TabletModeWindowManager::GetSnapPositions(
    const MruWindowTracker::WindowList& windows) const {
  std::vector<SplitViewController::SnapPosition> result;
  if (!IsCandidateForSplitView(windows, 0u))
    return result;
  switch (GetDesktopWindowStateType(windows[0])) {
    case mojom::WindowStateType::LEFT_SNAPPED:
      // windows[0] was snapped on the left in desktop mode. Snap windows[0] on
      // the left in split view. If windows[1] was snapped on the right in
      // desktop mode, then snap windows[1] on the right in split view.
      result.push_back(SplitViewController::LEFT);
      if (IsCandidateForSplitView(windows, 1u) &&
          GetDesktopWindowStateType(windows[1]) ==
              mojom::WindowStateType::RIGHT_SNAPPED) {
        result.push_back(SplitViewController::RIGHT);
      }
      return result;
    case mojom::WindowStateType::RIGHT_SNAPPED:
      // windows[0] was snapped on the right in desktop mode. Snap windows[0] on
      // the right in split view. If windows[1] was snapped on the left in
      // desktop mode, then snap windows[1] on the left in split view.
      result.push_back(SplitViewController::RIGHT);
      if (IsCandidateForSplitView(windows, 1u) &&
          GetDesktopWindowStateType(windows[1]) ==
              mojom::WindowStateType::LEFT_SNAPPED) {
        result.push_back(SplitViewController::LEFT);
      }
      return result;
    default:
      return result;
  }
}

void TabletModeWindowManager::ArrangeWindowsForTabletMode() {
  // |split_view_eligible_windows| is for determining split view layout.
  // |activatable_windows| includes all windows to be tracked, and that includes
  // windows on the lock screen via |scoped_skip_user_session_blocked_check|.
  MruWindowTracker::WindowList split_view_eligible_windows =
      Shell::Get()->mru_window_tracker()->BuildWindowForCycleList();
  ScopedSkipUserSessionBlockedCheck scoped_skip_user_session_blocked_check;
  MruWindowTracker::WindowList activatable_windows =
      Shell::Get()->mru_window_tracker()->BuildWindowListIgnoreModal();

  // Determine the desired split view layout.
  const std::vector<SplitViewController::SnapPosition> snap_positions =
      GetSnapPositions(split_view_eligible_windows);

  // If split view is not appropriate, then maximize all windows and bail out.
  if (snap_positions.empty()) {
    for (auto* window : activatable_windows)
      TrackWindow(window, /*entering_tablet_mode=*/true);
    return;
  }

  // Carry over the state types of the windows that shall be in split view.
  // Maximize all other windows. Do not animate any window bounds updates.
  for (auto* window : activatable_windows) {
    bool snap = false;
    for (size_t i = 0u; i < snap_positions.size(); ++i) {
      if (window == split_view_eligible_windows[i]) {
        snap = true;
        break;
      }
    }
    TrackWindow(window, /*entering_tablet_mode=*/true, snap,
                /*animate_bounds_on_attach=*/false);
  }

  // Enter split view mode.
  DoSplitView(split_view_eligible_windows, snap_positions);
}

void TabletModeWindowManager::ArrangeWindowsForDesktopMode(
    bool was_in_overview) {
  while (window_state_map_.size())
    ForgetWindow(window_state_map_.begin()->first, false /* destroyed */,
                 was_in_overview);
}

void TabletModeWindowManager::TrackWindow(aura::Window* window,
                                          bool entering_tablet_mode,
                                          bool snap,
                                          bool animate_bounds_on_attach) {
  if (!ShouldHandleWindow(window))
    return;

  DCHECK(!base::ContainsKey(window_state_map_, window));
  window->AddObserver(this);

  // We create and remember a tablet mode state which will attach itself to
  // the provided state object.
  window_state_map_[window] = new TabletModeWindowState(
      window, this, snap, animate_bounds_on_attach, entering_tablet_mode);
}

void TabletModeWindowManager::ForgetWindow(aura::Window* window,
                                           bool destroyed,
                                           bool was_in_overview) {
  added_windows_.erase(window);
  window->RemoveObserver(this);

  WindowToState::iterator it = window_state_map_.find(window);
  // A window may not be registered yet if the observer was
  // registered in OnWindowHierarchyChanged.
  if (it == window_state_map_.end())
    return;

  if (destroyed) {
    // If the window is to-be-destroyed, remove it from |window_state_map_|
    // immidietely. Otherwise it's possible to send a WMEvent to the to-be-
    // destroyed window.  Note we should not restore its old previous window
    // state object here since it will send unnecessary window state change
    // events. The tablet window state object and the old window state object
    // will be both deleted when the window is destroyed.
    window_state_map_.erase(it);
  } else {
    // By telling the state object to revert, it will switch back the old
    // State object and destroy itself, calling WindowStateDestroyed().
    it->second->LeaveTabletMode(wm::GetWindowState(it->first), was_in_overview);
    DCHECK(!base::ContainsKey(window_state_map_, window));
  }
}

bool TabletModeWindowManager::ShouldHandleWindow(aura::Window* window) {
  DCHECK(window);

  // Windows with the always-on-top property should be free-floating and thus
  // not managed by us.
  if (window->GetProperty(aura::client::kAlwaysOnTopKey))
    return false;

  // If the changing bounds in the maximized/fullscreen is allowed, then
  // let the client manage it even in tablet mode.
  if (!wm::GetWindowState(window) ||
      wm::GetWindowState(window)->allow_set_bounds_direct()) {
    return false;
  }

  return window->type() == aura::client::WINDOW_TYPE_NORMAL;
}

void TabletModeWindowManager::AddWindowCreationObservers() {
  DCHECK(observed_container_windows_.empty());
  // Observe window activations/creations in the default containers on all root
  // windows.
  for (aura::Window* root : Shell::GetAllRootWindows()) {
    for (auto* desk_container : desks_util::GetDesksContainers(root)) {
      DCHECK(!base::ContainsKey(observed_container_windows_, desk_container));
      desk_container->AddObserver(this);
      observed_container_windows_.insert(desk_container);
    }
  }
}

void TabletModeWindowManager::RemoveWindowCreationObservers() {
  for (aura::Window* window : observed_container_windows_)
    window->RemoveObserver(this);
  observed_container_windows_.clear();
}

void TabletModeWindowManager::DisplayConfigurationChanged() {
  EnableBackdropBehindTopWindowOnEachDisplay(false);
  RemoveWindowCreationObservers();
  AddWindowCreationObservers();
  EnableBackdropBehindTopWindowOnEachDisplay(true);
}

bool TabletModeWindowManager::IsContainerWindow(aura::Window* window) {
  return base::ContainsKey(observed_container_windows_, window);
}

void TabletModeWindowManager::EnableBackdropBehindTopWindowOnEachDisplay(
    bool enable) {
  // Inform the WorkspaceLayoutManager that we want to show a backdrop behind
  // the topmost window of its container.
  for (auto* root : Shell::GetAllRootWindows()) {
    for (auto* desk_container : desks_util::GetDesksContainers(root)) {
      WorkspaceController* controller = GetWorkspaceController(desk_container);
      DCHECK(controller);

      controller->SetBackdropDelegate(
          enable ? std::make_unique<TabletModeBackdropDelegateImpl>()
                 : nullptr);
    }
  }
}

}  // namespace ash
