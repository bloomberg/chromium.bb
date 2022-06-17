// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk.h"

#include <algorithm>
#include <utility>

#include "ash/constants/app_types.h"
#include "ash/public/cpp/desks_templates_delegate.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_restore_util.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/window_positioner.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_transient_descendant_iterator.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/backdrop_controller.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "ash/wm/workspace_controller.h"
#include "base/bind.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/app_restore/full_restore_utils.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window_tracker.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// The name of the histogram for consecutive daily visits.
constexpr char kConsecutiveDailyVisitsHistogramName[] =
    "Ash.Desks.ConsecutiveDailyVisits";

// Prefix for the desks lifetime histogram.
constexpr char kDeskLifetimeHistogramNamePrefix[] = "Ash.Desks.DeskLifetime_";

// The amount of time a user has to stay on a recently activated desk for it to
// be considered interacted with. Used for tracking weekly active desks metric.
constexpr base::TimeDelta kDeskInteractedWithTime = base::Seconds(3);

// A counter for tracking the number of desks interacted with this week. A
// desk is considered interacted with if a window is moved to it, it is
// created, its name is changed or it is activated and stayed on for a brief
// period of time. This value can go beyond the max number of desks as it
// counts deleted desks that have been previously interacted with.
int g_weekly_active_desks = 0;

void UpdateBackdropController(aura::Window* desk_container) {
  auto* workspace_controller = GetWorkspaceController(desk_container);
  // Work might have already been cleared when the display is removed. See
  // |RootWindowController::MoveWindowsTo()|.
  if (!workspace_controller)
    return;

  WorkspaceLayoutManager* layout_manager =
      workspace_controller->layout_manager();
  BackdropController* backdrop_controller =
      layout_manager->backdrop_controller();
  backdrop_controller->OnDeskContentChanged();
}

bool IsOverviewUiWindow(aura::Window* window) {
  return window->GetId() == kShellWindowId_DesksBarWindow ||
         window->GetId() == kShellWindowId_SaveDeskButtonContainer ||
         window->GetId() == kShellWindowId_OverviewNoWindowsLabelWindow;
}

// Returns true if |window| can be managed by the desk, and therefore can be
// moved out of the desk when the desk is removed.
bool CanMoveWindowOutOfDeskContainer(aura::Window* window) {
  // The desks bar widget is an activatable window placed in the active desk's
  // container, therefore it should be allowed to move outside of its desk when
  // its desk is removed. The save desk as template widget is not activatable
  // but should also be moved to the next active desk.
  if (IsOverviewUiWindow(window))
    return true;

  // We never move transient descendants directly, this is taken care of by
  // `wm::TransientWindowManager::OnWindowHierarchyChanged()`.
  auto* transient_root = ::wm::GetTransientRoot(window);
  if (transient_root != window)
    return false;

  // Only allow app windows to move to other desks.
  return window->GetProperty(aura::client::kAppType) !=
         static_cast<int>(AppType::NON_APP);
}

// Adjusts the z-order stacking of |window_to_fix| in its parent to match its
// order in the MRU window list. This is done after the window is moved from one
// desk container to another by means of calling AddChild() which adds it as the
// top-most window, which doesn't necessarily match the MRU order.
// |window_to_fix| must be a child of a desk container, and the root of a
// transient hierarchy (if it belongs to one).
// This function must be called AddChild() was called to add the |window_to_fix|
// (i.e. |window_to_fix| is the top-most window or the top-most window is a
// transient child of |window_to_fix|).
void FixWindowStackingAccordingToGlobalMru(aura::Window* window_to_fix) {
  aura::Window* container = window_to_fix->parent();
  DCHECK(desks_util::IsDeskContainer(container));
  DCHECK_EQ(window_to_fix, wm::GetTransientRoot(window_to_fix));
  DCHECK(window_to_fix == container->children().back() ||
         window_to_fix == wm::GetTransientRoot(container->children().back()));

  const auto mru_windows =
      Shell::Get()->mru_window_tracker()->BuildWindowListIgnoreModal(
          DesksMruType::kAllDesks);
  // Find the closest sibling that is not a transient descendant, which
  // |window_to_fix| should be stacked below.
  aura::Window* closest_sibling_above_window = nullptr;
  for (auto* window : mru_windows) {
    if (window == window_to_fix) {
      if (closest_sibling_above_window)
        container->StackChildBelow(window_to_fix, closest_sibling_above_window);
      return;
    }

    if (window->parent() == container &&
        !wm::HasTransientAncestor(window, window_to_fix)) {
      closest_sibling_above_window = window;
    }
  }
}

// Used to temporarily turn off the automatic window positioning while windows
// are being moved between desks.
class ScopedWindowPositionerDisabler {
 public:
  ScopedWindowPositionerDisabler() {
    WindowPositioner::DisableAutoPositioning(true);
  }

  ScopedWindowPositionerDisabler(const ScopedWindowPositionerDisabler&) =
      delete;
  ScopedWindowPositionerDisabler& operator=(
      const ScopedWindowPositionerDisabler&) = delete;

  ~ScopedWindowPositionerDisabler() {
    WindowPositioner::DisableAutoPositioning(false);
  }
};

}  // namespace

class DeskContainerObserver : public aura::WindowObserver {
 public:
  DeskContainerObserver(Desk* owner, aura::Window* container)
      : owner_(owner), container_(container) {
    DCHECK_EQ(container_->GetId(), owner_->container_id());
    container->AddObserver(this);
  }

  DeskContainerObserver(const DeskContainerObserver&) = delete;
  DeskContainerObserver& operator=(const DeskContainerObserver&) = delete;

  ~DeskContainerObserver() override { container_->RemoveObserver(this); }

  // aura::WindowObserver:
  void OnWindowAdded(aura::Window* new_window) override {
    // TODO(afakhry): Overview mode creates a new widget for each window under
    // the same parent for the OverviewItemView. We will be notified with
    // this window addition here. Consider ignoring these windows if they cause
    // problems.
    owner_->AddWindowToDesk(new_window);

    if (Shell::Get()->overview_controller()->InOverviewSession() &&
        !new_window->GetProperty(kHideInDeskMiniViewKey) &&
        desks_util::IsWindowVisibleOnAllWorkspaces(new_window)) {
      // If we're in overview and an all desks window has been added to a new
      // container, that means the user has moved the window to another display
      // so we need to refresh all the desk previews.
      Shell::Get()->desks_controller()->NotifyAllDesksForContentChanged();
    }
  }

  void OnWindowRemoved(aura::Window* removed_window) override {
    // We listen to `OnWindowRemoved()` as opposed to `OnWillRemoveWindow()`
    // since we want to refresh the mini_views only after the window has been
    // removed from the window tree hierarchy.
    owner_->RemoveWindowFromDesk(removed_window);
  }

  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override {
    // We need this for desks templates, where new app windows can be created
    // while in overview. The window may not be visible when `OnWindowAdded` is
    // called so updating the previews then wouldn't show the new window
    // preview.

    if (!Shell::Get()->overview_controller()->InOverviewSession())
      return;

    // `OnWindowVisibilityChanged()` will be run for all windows in the tree of
    // `container_`. We are only interested in direct children.
    if (!window->parent() || window->parent() != container_)
      return;

    // No need to update transient children as the update will handle them.
    if (wm::GetTransientRoot(window) != window)
      return;

    // Minimized windows may be force shown to be mirrored. They won't be
    // visible on the desk preview however, so no need to update.
    if (!WindowState::Get(window) || WindowState::Get(window)->IsMinimized())
      return;

    // Do not update windows shown or hidden for overview as they will not be
    // shown in the desk previews anyways.
    if (window->GetProperty(kHideInDeskMiniViewKey))
      return;

    owner_->NotifyContentChanged();
  }

  void OnWindowDestroyed(aura::Window* window) override {
    // We should never get here. We should be notified in
    // `OnRootWindowClosing()` before the child containers of the root window
    // are destroyed, and this object should have already been destroyed.
    NOTREACHED();
  }

 private:
  Desk* const owner_;
  aura::Window* const container_;
};

// -----------------------------------------------------------------------------
// Desk:

Desk::Desk(int associated_container_id, bool desk_being_restored)
    : uuid_(base::GUID::GenerateRandomV4()),
      container_id_(associated_container_id),
      creation_time_(base::Time::Now()) {
  // For the very first default desk added during initialization, there won't be
  // any root windows yet. That's OK, OnRootWindowAdded() will be called
  // explicitly by the RootWindowController when they're initialized.
  for (aura::Window* root : Shell::GetAllRootWindows())
    OnRootWindowAdded(root);

  if (!desk_being_restored)
    MaybeIncrementWeeklyActiveDesks();
}

Desk::~Desk() {
#if DCHECK_IS_ON()
  for (auto* window : windows_) {
    DCHECK(!CanMoveWindowOutOfDeskContainer(window))
        << "DesksController should remove this desk's application windows "
           "first.";
  }
#endif

  for (auto& observer : observers_) {
    observers_.RemoveObserver(&observer);
    observer.OnDeskDestroyed(this);
  }
}

// static
void Desk::SetWeeklyActiveDesks(int weekly_active_desks) {
  g_weekly_active_desks = weekly_active_desks;
}

// static
int Desk::GetWeeklyActiveDesks() {
  return g_weekly_active_desks;
}

void Desk::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void Desk::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void Desk::OnRootWindowAdded(aura::Window* root) {
  DCHECK(!roots_to_containers_observers_.count(root));

  // No windows should be added to the desk container on |root| prior to
  // tracking it by the desk.
  aura::Window* desk_container = root->GetChildById(container_id_);
  DCHECK(desk_container->children().empty());
  auto container_observer =
      std::make_unique<DeskContainerObserver>(this, desk_container);
  roots_to_containers_observers_.emplace(root, std::move(container_observer));
}

void Desk::OnRootWindowClosing(aura::Window* root) {
  const size_t count = roots_to_containers_observers_.erase(root);
  DCHECK(count);

  // The windows on this root are about to be destroyed. We already stopped
  // observing the container above, so we won't get a call to
  // DeskContainerObserver::OnWindowRemoved(). Therefore, we must remove those
  // windows manually. If this is part of shutdown (i.e. when the
  // RootWindowController is being destroyed), then we're done with those
  // windows. If this is due to a display being removed, then the
  // WindowTreeHostManager will move those windows to another host/root, and
  // they will be added again to the desk container on the new root.
  const auto windows = windows_;
  for (auto* window : windows) {
    if (window->GetRootWindow() == root)
      base::Erase(windows_, window);
  }
}

void Desk::AddWindowToDesk(aura::Window* window) {
  DCHECK(!base::Contains(windows_, window));

  windows_.push_back(window);
  // No need to refresh the mini_views if the destroyed window doesn't show up
  // there in the first place. Also don't refresh for visible on all desks
  // windows since they're already refreshed in OnWindowAdded().
  if (!window->GetProperty(kHideInDeskMiniViewKey) &&
      !desks_util::IsWindowVisibleOnAllWorkspaces(window)) {
    NotifyContentChanged();
  }

  // Update the window's workspace to this parent desk.
  auto* desks_controller = DesksController::Get();
  if (!is_desk_being_removed_ &&
      !desks_util::IsWindowVisibleOnAllWorkspaces(window)) {
    window->SetProperty(aura::client::kWindowWorkspaceKey,
                        desks_controller->GetDeskIndex(this));
  }

  MaybeIncrementWeeklyActiveDesks();
}

void Desk::RemoveWindowFromDesk(aura::Window* window) {
  DCHECK(base::Contains(windows_, window));

  base::Erase(windows_, window);
  // No need to refresh the mini_views if the destroyed window doesn't show up
  // there in the first place. Also don't refresh for visible on all desks
  // windows since they're already refreshed in OnWindowRemoved().
  if (!window->GetProperty(kHideInDeskMiniViewKey) &&
      !desks_util::IsWindowVisibleOnAllWorkspaces(window)) {
    NotifyContentChanged();
  }
}

base::AutoReset<bool> Desk::GetScopedNotifyContentChangedDisabler() {
  return base::AutoReset<bool>(&should_notify_content_changed_, false);
}

bool Desk::ContainsAppWindows() const {
  return std::find_if(windows_.begin(), windows_.end(),
                      [](aura::Window* window) {
                        return window->GetProperty(aura::client::kAppType) !=
                               static_cast<int>(AppType::NON_APP);
                      }) != windows_.end();
}

void Desk::SetName(std::u16string new_name, bool set_by_user) {
  // Even if the user focuses the DeskNameView for the first time and hits enter
  // without changing the desk's name (i.e. |new_name| is the same,
  // |is_name_set_by_user_| is false, and |set_by_user| is true), we don't
  // change |is_name_set_by_user_| and keep considering the name as a default
  // name.
  if (name_ == new_name)
    return;

  name_ = std::move(new_name);
  is_name_set_by_user_ = set_by_user;

  if (set_by_user)
    MaybeIncrementWeeklyActiveDesks();

  for (auto& observer : observers_)
    observer.OnDeskNameChanged(name_);

  DesksController::Get()->NotifyDeskNameChanged(this, name_);
}

void Desk::PrepareForActivationAnimation() {
  DCHECK(!is_active_);

  for (aura::Window* root : Shell::GetAllRootWindows()) {
    auto* container = root->GetChildById(container_id_);
    container->layer()->SetOpacity(0);
    container->Show();
  }
  started_activation_animation_ = true;
}

void Desk::Activate(bool update_window_activation) {
  DCHECK(!is_active_);

  if (!MaybeResetContainersOpacities()) {
    for (aura::Window* root : Shell::GetAllRootWindows())
      root->GetChildById(container_id_)->Show();
  }

  is_active_ = true;

  if (!IsConsecutiveDailyVisit())
    RecordAndResetConsecutiveDailyVisits(/*being_removed=*/false);

  int current_date = desks_restore_util::GetDaysFromLocalEpoch();
  if (current_date < last_day_visited_ || first_day_visited_ == -1) {
    // If |current_date| < |last_day_visited_| then the user has moved timezones
    // or the stored data has been corrupted so reset |first_day_visited_|.
    first_day_visited_ = current_date;
  }
  last_day_visited_ = current_date;

  active_desk_timer_.Start(
      FROM_HERE, kDeskInteractedWithTime,
      base::BindOnce(&Desk::MaybeIncrementWeeklyActiveDesks,
                     base::Unretained(this)));

  if (!update_window_activation || windows_.empty())
    return;

  // Activate the window on this desk that was most recently used right before
  // the user switched to another desk, so as not to break the user's workflow.
  for (auto* window :
       Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk)) {
    if (!base::Contains(windows_, window))
      continue;

    // Do not activate minimized windows, otherwise they will unminimize.
    if (WindowState::Get(window)->IsMinimized())
      continue;

    wm::ActivateWindow(window);
    return;
  }
}

void Desk::Deactivate(bool update_window_activation) {
  DCHECK(is_active_);

  auto* active_window = window_util::GetActiveWindow();

  // Hide the associated containers on all roots.
  for (aura::Window* root : Shell::GetAllRootWindows())
    root->GetChildById(container_id_)->Hide();

  is_active_ = false;
  last_day_visited_ = desks_restore_util::GetDaysFromLocalEpoch();

  active_desk_timer_.Stop();

  if (!update_window_activation)
    return;

  // Deactivate the active window (if it belongs to this desk; active window may
  // be on a different container, or one of the widgets created by overview mode
  // which are not considered desk windows) after this desk's associated
  // containers have been hidden. This is to prevent the focus controller from
  // activating another window on the same desk when the active window loses
  // focus.
  if (active_window && base::Contains(windows_, active_window))
    wm::DeactivateWindow(active_window);
}

void Desk::MoveNonAppOverviewWindowsToDesk(Desk* target_desk) {
  DCHECK(Shell::Get()->overview_controller()->InOverviewSession());

  {
    // Wait until the end to allow notifying the observers of either desk.
    auto this_desk_throttled = GetScopedNotifyContentChangedDisabler();
    auto target_desk_throttled =
        target_desk->GetScopedNotifyContentChangedDisabler();

    // Create a `aura::WindowTracker` to hold `windows_`'s windows so that we do
    // not edit `windows_` in place.
    aura::WindowTracker window_tracker(windows_);

    // Move only the non-app overview windows.
    while (!window_tracker.windows().empty()) {
      auto* window = window_tracker.Pop();
      if (IsOverviewUiWindow(window))
        MoveWindowToDeskInternal(window, target_desk, window->GetRootWindow());
    }
  }

  target_desk->NotifyContentChanged();
}

void Desk::MoveWindowsToDesk(Desk* target_desk) {
  DCHECK(target_desk);

  {
    ScopedWindowPositionerDisabler window_positioner_disabler;

    // Throttle notifying the observers, while we move those windows and notify
    // them only once when done.
    auto this_desk_throttled = GetScopedNotifyContentChangedDisabler();
    auto target_desk_throttled =
        target_desk->GetScopedNotifyContentChangedDisabler();

    // Moving windows will change the hierarchy and hence |windows_|, and has to
    // be done without changing the relative z-order. So we make a copy of all
    // the top-level windows on all the containers of this desk, such that
    // windows in each container are copied from top-most (z-order) to
    // bottom-most.
    // Note that moving windows out of the container and restacking them
    // differently may trigger events that lead to destroying a window on the
    // list. For example moving the top-most window which has a backdrop will
    // cause the backdrop to be destroyed. Therefore observe such events using
    // an |aura::WindowTracker|.
    aura::WindowTracker windows_to_move;
    for (aura::Window* root : Shell::GetAllRootWindows()) {
      const aura::Window* container = GetDeskContainerForRoot(root);
      for (auto* window : base::Reversed(container->children()))
        windows_to_move.Add(window);
    }

    auto* mru_tracker = Shell::Get()->mru_window_tracker();
    while (!windows_to_move.windows().empty()) {
      auto* window = windows_to_move.Pop();
      if (!CanMoveWindowOutOfDeskContainer(window))
        continue;

      // Note that windows that belong to the same container in
      // |windows_to_move| are sorted from top-most to bottom-most, hence
      // calling |StackChildAtBottom()| on each in this order will maintain that
      // same order in the |target_desk|'s container.
      MoveWindowToDeskInternal(window, target_desk, window->GetRootWindow());
      window->parent()->StackChildAtBottom(window);
      mru_tracker->OnWindowMovedOutFromRemovingDesk(window);
    }
  }

  NotifyContentChanged();
  target_desk->NotifyContentChanged();
}

void Desk::MoveWindowToDesk(aura::Window* window,
                            Desk* target_desk,
                            aura::Window* target_root,
                            bool unminimize) {
  DCHECK(window);
  DCHECK(target_desk);
  DCHECK(target_root);
  DCHECK(base::Contains(windows_, window));
  DCHECK(this != target_desk);

  {
    ScopedWindowPositionerDisabler window_positioner_disabler;

    // Throttling here is necessary even though we're attempting to move a
    // single window. This is because that window might exist in a transient
    // window tree, which will result in actually moving multiple windows if the
    // transient children used to be on the same container.
    // See `wm::TransientWindowManager::OnWindowHierarchyChanged()`.
    auto this_desk_throttled = GetScopedNotifyContentChangedDisabler();
    auto target_desk_throttled =
        target_desk->GetScopedNotifyContentChangedDisabler();

    // Always move the root of the transient window tree. We should never move a
    // transient child and leave its parent behind. Moving the transient
    // descendants that exist on the same desk container will be taken care of
    //  by `wm::TransientWindowManager::OnWindowHierarchyChanged()`.
    aura::Window* transient_root = ::wm::GetTransientRoot(window);
    MoveWindowToDeskInternal(transient_root, target_desk, target_root);
    FixWindowStackingAccordingToGlobalMru(transient_root);

    // Unminimize the window so that it shows up in the mini_view after it had
    // been dragged and moved to another desk. Don't unminimize if the window is
    // visible on all desks since it's being moved during desk activation.
    auto* window_state = WindowState::Get(transient_root);
    if (unminimize && window_state->IsMinimized() &&
        !desks_util::IsWindowVisibleOnAllWorkspaces(window)) {
      window_state->Unminimize();
    }
  }

  NotifyContentChanged();
  target_desk->NotifyContentChanged();
}

aura::Window* Desk::GetDeskContainerForRoot(aura::Window* root) const {
  DCHECK(root);

  return root->GetChildById(container_id_);
}

void Desk::NotifyContentChanged() {
  if (!should_notify_content_changed_)
    return;

  // Updating the backdrops below may lead to the removal or creation of
  // backdrop windows in this desk, which can cause us to recurse back here.
  // Disable this.
  auto disable_recursion = GetScopedNotifyContentChangedDisabler();

  // The availability and visibility of backdrops of all containers associated
  // with this desk will be updated *before* notifying observer, so that the
  // mini_views update *after* the backdrops do.
  // This is *only* needed if the WorkspaceLayoutManager won't take care of this
  // for us while overview is active.
  if (Shell::Get()->overview_controller()->InOverviewSession())
    UpdateDeskBackdrops();

  for (auto& observer : observers_)
    observer.OnContentChanged();
}

void Desk::UpdateDeskBackdrops() {
  for (auto* root : Shell::GetAllRootWindows())
    UpdateBackdropController(GetDeskContainerForRoot(root));
}

void Desk::RecordLifetimeHistogram(int index) {
  // Desk index is 1-indexed in histograms.
  const int desk_index = index + 1;
  base::UmaHistogramCounts1000(
      base::StringPrintf("%s%i", kDeskLifetimeHistogramNamePrefix, desk_index),
      (base::Time::Now() - creation_time_).InHours());
}

bool Desk::IsConsecutiveDailyVisit() const {
  if (last_day_visited_ == -1)
    return true;

  const int days_since_last_visit =
      desks_restore_util::GetDaysFromLocalEpoch() - last_day_visited_;
  return days_since_last_visit <= 1;
}

void Desk::RecordAndResetConsecutiveDailyVisits(bool being_removed) {
  if (being_removed && is_active_) {
    // When the user removes the active desk, update |last_day_visited_| to the
    // current day to account for the time they spent on this desk.
    last_day_visited_ = desks_restore_util::GetDaysFromLocalEpoch();
  }

  const int consecutive_daily_visits =
      last_day_visited_ - first_day_visited_ + 1;
  DCHECK_GE(consecutive_daily_visits, 1);
  base::UmaHistogramCounts1000(kConsecutiveDailyVisitsHistogramName,
                               consecutive_daily_visits);

  last_day_visited_ = -1;
  first_day_visited_ = -1;
}

std::vector<aura::Window*> Desk::GetAllAppWindows() {
  // We need to copy the app windows from `windows_` into `app_windows` so
  // that we do not modify `windows_` in place. This also gives us a filtered
  // list with all of the app windows that we need to remove.
  std::vector<aura::Window*> app_windows;
  base::ranges::copy_if(windows_, std::back_inserter(app_windows),
                        [](aura::Window* window) {
                          return window->GetProperty(aura::client::kAppType) !=
                                 static_cast<int>(AppType::NON_APP);
                        });

  return app_windows;
}

void Desk::MoveWindowToDeskInternal(aura::Window* window,
                                    Desk* target_desk,
                                    aura::Window* target_root) {
  DCHECK(base::Contains(windows_, window));
  DCHECK(CanMoveWindowOutOfDeskContainer(window))
      << "Non-desk windows are not allowed to move out of the container.";

  // When |target_root| is different than the current window's |root|, this can
  // only happen when dragging and dropping a window on mini desk view on
  // another display. Therefore |target_desk| is an inactive desk (i.e.
  // invisible). The order doesn't really matter, but we move the window to the
  // target desk's container first (so that it becomes hidden), then move it to
  // the target display (while it's hidden).
  aura::Window* root = window->GetRootWindow();
  aura::Window* source_container = GetDeskContainerForRoot(root);
  aura::Window* target_container = target_desk->GetDeskContainerForRoot(root);
  DCHECK(window->parent() == source_container);
  target_container->AddChild(window);

  if (root != target_root) {
    // Move the window to the container with the same ID on the target display's
    // root (i.e. container that belongs to the same desk), and adjust its
    // bounds to fit in the new display's work area.
    window_util::MoveWindowToDisplay(window,
                                     display::Screen::GetScreen()
                                         ->GetDisplayNearestWindow(target_root)
                                         .id());
    DCHECK_EQ(target_desk->container_id_, window->parent()->GetId());
  }
}

bool Desk::MaybeResetContainersOpacities() {
  if (!started_activation_animation_)
    return false;

  for (aura::Window* root : Shell::GetAllRootWindows()) {
    auto* container = root->GetChildById(container_id_);
    container->layer()->SetOpacity(1);
  }
  started_activation_animation_ = false;
  return true;
}

void Desk::MaybeIncrementWeeklyActiveDesks() {
  if (interacted_with_this_week_)
    return;
  interacted_with_this_week_ = true;
  ++g_weekly_active_desks;
}

}  // namespace ash
