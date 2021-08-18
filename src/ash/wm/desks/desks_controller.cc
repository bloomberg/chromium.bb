// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desks_controller.h"

#include <utility>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/desk_template.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desk_animation_base.h"
#include "ash/wm/desks/desk_animation_impl.h"
#include "ash/wm/desks/desks_animations.h"
#include "ash/wm/desks/desks_restore_util.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/full_restore/full_restore_util.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/switchable_windows.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_cycle/window_cycle_controller.h"
#include "ash/wm/window_util.h"
#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/i18n/number_formatting.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/numerics/ranges.h"
#include "base/timer/timer.h"
#include "components/full_restore/app_launch_info.h"
#include "components/full_restore/full_restore_utils.h"
#include "components/full_restore/restore_data.h"
#include "components/full_restore/window_info.h"
#include "components/user_manager/user_manager.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

constexpr char kNewDeskHistogramName[] = "Ash.Desks.NewDesk2";
constexpr char kDesksCountHistogramName[] = "Ash.Desks.DesksCount3";
constexpr char kWeeklyActiveDesksHistogramName[] =
    "Ash.Desks.WeeklyActiveDesks";
constexpr char kRemoveDeskHistogramName[] = "Ash.Desks.RemoveDesk";
constexpr char kDeskSwitchHistogramName[] = "Ash.Desks.DesksSwitch";
constexpr char kMoveWindowFromActiveDeskHistogramName[] =
    "Ash.Desks.MoveWindowFromActiveDesk";
constexpr char kNumberOfWindowsOnDesk_1_HistogramName[] =
    "Ash.Desks.NumberOfWindowsOnDesk_1";
constexpr char kNumberOfWindowsOnDesk_2_HistogramName[] =
    "Ash.Desks.NumberOfWindowsOnDesk_2";
constexpr char kNumberOfWindowsOnDesk_3_HistogramName[] =
    "Ash.Desks.NumberOfWindowsOnDesk_3";
constexpr char kNumberOfWindowsOnDesk_4_HistogramName[] =
    "Ash.Desks.NumberOfWindowsOnDesk_4";
constexpr char kNumberOfWindowsOnDesk_5_HistogramName[] =
    "Ash.Desks.NumberOfWindowsOnDesk_5";
constexpr char kNumberOfWindowsOnDesk_6_HistogramName[] =
    "Ash.Desks.NumberOfWindowsOnDesk_6";
constexpr char kNumberOfWindowsOnDesk_7_HistogramName[] =
    "Ash.Desks.NumberOfWindowsOnDesk_7";
constexpr char kNumberOfWindowsOnDesk_8_HistogramName[] =
    "Ash.Desks.NumberOfWindowsOnDesk_8";

constexpr char kNumberOfDeskTraversalsHistogramName[] =
    "Ash.Desks.NumberOfDeskTraversals";
constexpr int kDeskTraversalsMaxValue = 20;

// After an desk activation animation starts,
// |kNumberOfDeskTraversalsHistogramName| will be recorded after this time
// interval.
constexpr base::TimeDelta kDeskTraversalsTimeout =
    base::TimeDelta::FromSeconds(5);

constexpr int kDeskDefaultNameIds[] = {
    IDS_ASH_DESKS_DESK_1_MINI_VIEW_TITLE, IDS_ASH_DESKS_DESK_2_MINI_VIEW_TITLE,
    IDS_ASH_DESKS_DESK_3_MINI_VIEW_TITLE, IDS_ASH_DESKS_DESK_4_MINI_VIEW_TITLE,
    IDS_ASH_DESKS_DESK_5_MINI_VIEW_TITLE, IDS_ASH_DESKS_DESK_6_MINI_VIEW_TITLE,
    IDS_ASH_DESKS_DESK_7_MINI_VIEW_TITLE, IDS_ASH_DESKS_DESK_8_MINI_VIEW_TITLE};

// Appends the given |windows| to the end of the currently active overview mode
// session such that the most-recently used window is added first. If
// The windows will animate to their positions in the overview grid.
void AppendWindowsToOverview(const std::vector<aura::Window*>& windows) {
  DCHECK(Shell::Get()->overview_controller()->InOverviewSession());

  auto* overview_session =
      Shell::Get()->overview_controller()->overview_session();
  for (auto* window :
       Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk)) {
    if (!base::Contains(windows, window) ||
        window_util::ShouldExcludeForOverview(window)) {
      continue;
    }

    overview_session->AppendItem(window, /*reposition=*/true, /*animate=*/true);
  }
}

// Removes all the items that currently exist in overview.
void RemoveAllWindowsFromOverview() {
  DCHECK(Shell::Get()->overview_controller()->InOverviewSession());

  auto* overview_session =
      Shell::Get()->overview_controller()->overview_session();
  for (const auto& grid : overview_session->grid_list()) {
    while (!grid->empty())
      overview_session->RemoveItem(grid->window_list()[0].get());
  }
}

// Updates the |ShelfItem::is_on_active_desk| of the items associated with
// |windows_on_inactive_desk| and |windows_on_active_desk|. The items of the
// given windows will be updated, while the rest will remain unchanged. Either
// or both window lists can be empty.
void MaybeUpdateShelfItems(
    const std::vector<aura::Window*>& windows_on_inactive_desk,
    const std::vector<aura::Window*>& windows_on_active_desk) {
  if (!features::IsPerDeskShelfEnabled())
    return;

  auto* shelf_model = ShelfModel::Get();
  DCHECK(shelf_model);
  std::vector<ShelfModel::ItemDeskUpdate> shelf_items_updates;

  auto add_shelf_item_update = [&](aura::Window* window,
                                   bool is_on_active_desk) {
    const ShelfID shelf_id =
        ShelfID::Deserialize(window->GetProperty(kShelfIDKey));
    const int index = shelf_model->ItemIndexByID(shelf_id);

    if (index < 0)
      return;

    shelf_items_updates.push_back({index, is_on_active_desk});
  };

  for (auto* window : windows_on_inactive_desk)
    add_shelf_item_update(window, /*is_on_active_desk=*/false);
  for (auto* window : windows_on_active_desk)
    add_shelf_item_update(window, /*is_on_active_desk=*/true);

  shelf_model->UpdateItemsForDeskChange(shelf_items_updates);
}

bool IsParentSwitchableContainer(const aura::Window* window) {
  DCHECK(window);
  return window->parent() && IsSwitchableContainer(window->parent());
}

bool IsApplistActiveInTabletMode(const aura::Window* active_window) {
  DCHECK(active_window);
  Shell* shell = Shell::Get();
  if (!shell->tablet_mode_controller()->InTabletMode())
    return false;

  auto* app_list_controller = shell->app_list_controller();
  return active_window == app_list_controller->GetWindow();
}

// Observer to observe the desk switch animation and destroy itself when the
// animation is finished.
class DeskSwitchAnimationObserver : public DesksController::Observer {
 public:
  explicit DeskSwitchAnimationObserver(
      base::OnceCallback<void(bool)> complete_callback)
      : complete_callback_(std::move(complete_callback)) {
    DesksController::Get()->AddObserver(this);
  }
  DeskSwitchAnimationObserver(const DeskSwitchAnimationObserver& other) =
      delete;
  DeskSwitchAnimationObserver& operator=(
      const DeskSwitchAnimationObserver& rhs) = delete;

  ~DeskSwitchAnimationObserver() override {
    DesksController::Get()->RemoveObserver(this);
  }

  // DesksController::Observer:
  void OnDeskAdded(const Desk* desk) override {}
  void OnDeskRemoved(const Desk* desk) override {}
  void OnDeskReordered(int old_index, int new_index) override {}
  void OnDeskActivationChanged(const Desk* activated,
                               const Desk* deactivated) override {}
  void OnDeskSwitchAnimationLaunching() override {}
  void OnDeskSwitchAnimationFinished() override {
    std::move(complete_callback_).Run(/*success=*/true);
    delete this;
  }
  void OnDeskNameChanged(const Desk* desk,
                         const std::u16string& new_name) override {}

 private:
  base::OnceCallback<void(bool)> complete_callback_;
};

}  // namespace

// Helper class which wraps around a OneShotTimer and used for recording how
// many times the user has traversed desks. Here traversal means the amount of
// times the user has seen a visual desk change. This differs from desk
// activation as a desk is only activated as needed for a screenshot during an
// animation. The user may bounce back and forth on two desks that already
// have screenshots, and each bounce is recorded as a traversal. For touchpad
// swipes, the amount of traversals in one animation depends on the amount of
// changes in the most visible desk have been seen. For other desk changes,
// the amount of traversals in one animation is 1 + number of Replace() calls.
// Multiple animations may be recorded before the timer stops.
class DesksController::DeskTraversalsMetricsHelper {
 public:
  explicit DeskTraversalsMetricsHelper(DesksController* controller)
      : controller_(controller) {}
  DeskTraversalsMetricsHelper(const DeskTraversalsMetricsHelper&) = delete;
  DeskTraversalsMetricsHelper& operator=(const DeskTraversalsMetricsHelper&) =
      delete;
  ~DeskTraversalsMetricsHelper() = default;

  // Starts |timer_| unless it is already running.
  void MaybeStart() {
    if (timer_.IsRunning())
      return;

    count_ = 0;
    timer_.Start(FROM_HERE, kDeskTraversalsTimeout,
                 base::BindOnce(&DeskTraversalsMetricsHelper::OnTimerStop,
                                base::Unretained(this)));
  }

  // Called when a desk animation is finished. Adds all observed
  // |visible_desk_changes| to |count_|.
  void OnAnimationFinished(int visible_desk_changes) {
    if (timer_.IsRunning())
      count_ += visible_desk_changes;
  }

  // Fires |timer_| immediately.
  void FireTimerForTesting() {
    if (timer_.IsRunning())
      timer_.FireNow();
  }

 private:
  void OnTimerStop() {
    // If an animation is still running, add its current visible desk change
    // count to |count_|.
    DeskAnimationBase* current_animation = controller_->animation();
    if (current_animation)
      count_ += current_animation->visible_desk_changes();

    base::UmaHistogramExactLinear(kNumberOfDeskTraversalsHistogramName, count_,
                                  kDeskTraversalsMaxValue);
  }

  // Pointer to the DesksController that owns this. Guaranteed to be not
  // nullptr for the lifetime of |this|.
  DesksController* const controller_;

  base::OneShotTimer timer_;

  // Tracks the amount of traversals that have happened since |timer_| has
  // started.
  int count_ = 0;
};

DesksController::DesksController()
    : metrics_helper_(std::make_unique<DeskTraversalsMetricsHelper>(this)) {
  Shell::Get()->activation_client()->AddObserver(this);
  Shell::Get()->session_controller()->AddObserver(this);

  for (int id : desks_util::GetDesksContainersIds())
    available_container_ids_.push(id);

  // There's always one default desk. The DesksCreationRemovalSource used here
  // doesn't matter, since UMA reporting will be skipped for the first ever
  // default desk.
  NewDesk(DesksCreationRemovalSource::kButton);
  active_desk_ = desks_.back().get();
  active_desk_->Activate(/*update_window_activation=*/true);

  weekly_active_desks_scheduler_.Start(
      FROM_HERE, base::TimeDelta::FromDays(7), this,
      &DesksController::RecordAndResetNumberOfWeeklyActiveDesks);
}

DesksController::~DesksController() {
  Shell::Get()->session_controller()->RemoveObserver(this);
  Shell::Get()->activation_client()->RemoveObserver(this);
}

// static
DesksController* DesksController::Get() {
  // Sometimes it's necessary to get the instance even before the
  // constructor is done. For example,
  // |DesksController::NotifyDeskNameChanged())| could be called
  // during the construction of |DesksController|, and at this point
  // |Shell::desks_controller_| has not been assigned yet.
  return static_cast<DesksController*>(DesksHelper::Get());
}

// static
std::u16string DesksController::GetDeskDefaultName(size_t desk_index) {
  DCHECK_LT(desk_index, desks_util::kMaxNumberOfDesks);
  return l10n_util::GetStringUTF16(kDeskDefaultNameIds[desk_index]);
}

const Desk* DesksController::GetTargetActiveDesk() const {
  if (animation_)
    return desks_[animation_->ending_desk_index()].get();
  return active_desk();
}

base::flat_set<aura::Window*>
DesksController::GetVisibleOnAllDesksWindowsOnRoot(
    aura::Window* root_window) const {
  DCHECK(root_window->IsRootWindow());
  base::flat_set<aura::Window*> filtered_visible_on_all_desks_windows;
  for (auto* visible_on_all_desks_window : visible_on_all_desks_windows_) {
    if (visible_on_all_desks_window->GetRootWindow() == root_window)
      filtered_visible_on_all_desks_windows.insert(visible_on_all_desks_window);
  }
  return filtered_visible_on_all_desks_windows;
}

void DesksController::RestorePrimaryUserActiveDeskIndex(int active_desk_index) {
  DCHECK_GE(active_desk_index, 0);
  DCHECK_LT(active_desk_index, static_cast<int>(desks_.size()));
  user_to_active_desk_index_[Shell::Get()
                                 ->session_controller()
                                 ->GetPrimaryUserSession()
                                 ->user_info.account_id] = active_desk_index;
  // Following |OnActiveUserSessionChanged| approach, restoring uses
  // DesksSwitchSource::kUserSwitch as a desk switch source.
  // TODO(crbug.com/1145404): consider adding an UMA metric for desks
  // restoring to change the source to kDeskRestored.
  ActivateDesk(desks_[active_desk_index].get(), DesksSwitchSource::kUserSwitch);
}

void DesksController::OnNewUserShown() {
  RestackVisibleOnAllDesksWindowsOnActiveDesk();
  NotifyAllDesksForContentChanged();
}

void DesksController::Shutdown() {
  animation_.reset();
  desks_restore_util::UpdatePrimaryUserDeskMetricsPrefs();
}

void DesksController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DesksController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool DesksController::AreDesksBeingModified() const {
  return are_desks_being_modified_ || !!animation_;
}

bool DesksController::CanCreateDesks() const {
  return desks_.size() < desks_util::kMaxNumberOfDesks;
}

Desk* DesksController::GetNextDesk(bool use_target_active_desk) const {
  int next_index = use_target_active_desk ? GetDeskIndex(GetTargetActiveDesk())
                                          : GetActiveDeskIndex();
  if (++next_index >= static_cast<int>(desks_.size()))
    return nullptr;
  return desks_[next_index].get();
}

Desk* DesksController::GetPreviousDesk(bool use_target_active_desk) const {
  int previous_index = use_target_active_desk
                           ? GetDeskIndex(GetTargetActiveDesk())
                           : GetActiveDeskIndex();
  if (--previous_index < 0)
    return nullptr;
  return desks_[previous_index].get();
}

bool DesksController::CanRemoveDesks() const {
  return desks_.size() > 1;
}

void DesksController::NewDesk(DesksCreationRemovalSource source) {
  DCHECK(CanCreateDesks());
  DCHECK(!available_container_ids_.empty());

  base::AutoReset<bool> in_progress(&are_desks_being_modified_, true);

  // The first default desk should not overwrite any desks restore data, nor
  // should it trigger any UMA stats reports.
  const bool is_first_ever_desk = desks_.empty();

  desks_.push_back(std::make_unique<Desk>(
      available_container_ids_.front(),
      source == DesksCreationRemovalSource::kDesksRestore));
  available_container_ids_.pop();
  Desk* new_desk = desks_.back().get();

  // The new desk should have an empty name when the user creates a desk with
  // the button. This is done to encourage them to rename their desks.
  const bool empty_name =
      source == DesksCreationRemovalSource::kButton && desks_.size() > 1;
  if (!empty_name) {
    new_desk->SetName(GetDeskDefaultName(desks_.size() - 1),
                      /*set_by_user=*/false);
  }

  auto* shell = Shell::Get();
  shell->accessibility_controller()->TriggerAccessibilityAlertWithMessage(
      l10n_util::GetStringFUTF8(IDS_ASH_VIRTUAL_DESKS_ALERT_NEW_DESK_CREATED,
                                base::NumberToString16(desks_.size())));

  for (auto& observer : observers_)
    observer.OnDeskAdded(new_desk);

  if (!is_first_ever_desk) {
    desks_restore_util::UpdatePrimaryUserDeskNamesPrefs();
    desks_restore_util::UpdatePrimaryUserDeskMetricsPrefs();
    UMA_HISTOGRAM_ENUMERATION(kNewDeskHistogramName, source);
    ReportDesksCountHistogram();
  }
}

void DesksController::RemoveDesk(const Desk* desk,
                                 DesksCreationRemovalSource source) {
  DCHECK(CanRemoveDesks());
  DCHECK(HasDesk(desk));

  base::AutoReset<bool> in_progress(&are_desks_being_modified_, true);

  auto* overview_controller = Shell::Get()->overview_controller();
  const bool in_overview = overview_controller->InOverviewSession();
  if (!in_overview && active_desk_ == desk) {
    // When removing the active desk outside of overview, we trigger the remove
    // desk animation. We will activate the desk to its left if any, otherwise,
    // we activate one on the right.
    const int current_desk_index = GetDeskIndex(active_desk_);
    const int target_desk_index =
        current_desk_index + ((current_desk_index > 0) ? -1 : 1);
    DCHECK_GE(target_desk_index, 0);
    DCHECK_LT(target_desk_index, static_cast<int>(desks_.size()));
    animation_ = std::make_unique<DeskRemovalAnimation>(
        this, current_desk_index, target_desk_index, source);
    animation_->Launch();
    return;
  }

  RemoveDeskInternal(desk, source);
}

void DesksController::ReorderDesk(int old_index, int new_index) {
  DCHECK_NE(old_index, new_index);
  DCHECK_GE(old_index, 0);
  DCHECK_GE(new_index, 0);
  DCHECK_LT(old_index, static_cast<int>(desks_.size()));
  DCHECK_LT(new_index, static_cast<int>(desks_.size()));
  desks_util::ReorderItem(desks_, old_index, new_index);

  for (auto& observer : observers_)
    observer.OnDeskReordered(old_index, new_index);

  // Since multi-profile users share the same desks, the active user needs to
  // update the desk name list to maintain the right desk order for restore
  // and update workspaces of windows in all affected desks across all profiles.
  // Meanwhile, only the primary user needs to update the active desk, which is
  // independent across profiles but only recoverable for the primary user.

  // 1. Update desk name and metrics lists in the user prefs to maintain the
  // right order.
  desks_restore_util::UpdatePrimaryUserDeskNamesPrefs();
  desks_restore_util::UpdatePrimaryUserDeskMetricsPrefs();

  // 2. For multi-profile switching, update all affected active desk index in
  // |user_to_active_desk_index_|.
  const int starting_affected_index = std::min(old_index, new_index);
  const int ending_affected_index = std::max(old_index, new_index);
  // If the user move a desk to the back, other affected desks in between the
  // two positions shift left (-1), otherwiser shift right (+1).
  const int offset = new_index > old_index ? -1 : 1;

  for (auto& iter : user_to_active_desk_index_) {
    const int old_active_index = iter.second;
    if (old_active_index < starting_affected_index ||
        old_active_index > ending_affected_index) {
      // Skip unaffected desk index.
      continue;
    }
    // The moving desk changes from old_index to new_index, while other desks
    // between the two positions shift by one position.
    iter.second =
        old_active_index == old_index ? new_index : old_active_index + offset;
  }

  // 3. For primary user's active desks restore, update the active desk index.
  desks_restore_util::UpdatePrimaryUserActiveDeskPrefs(
      user_to_active_desk_index_[Shell::Get()
                                     ->session_controller()
                                     ->GetPrimaryUserSession()
                                     ->user_info.account_id]);

  // 4. For restoring windows to the right desks, update workspaces of all
  // windows in the affected desks for all simultaneously logged-in users.
  for (int i = starting_affected_index; i <= ending_affected_index; i++) {
    for (auto* window : desks_[i]->windows())
      window->SetProperty(aura::client::kWindowWorkspaceKey, i);
  }
}

void DesksController::ActivateDesk(const Desk* desk, DesksSwitchSource source) {
  DCHECK(HasDesk(desk));
  DCHECK(!animation_);

  // If we are switching users, we don't want to notify desks of content changes
  // until the user switch animation has shown the new user's windows.
  const bool is_user_switch = source == DesksSwitchSource::kUserSwitch;
  std::vector<base::AutoReset<bool>> desks_scoped_notify_disablers;
  if (is_user_switch) {
    for (const auto& desk : desks_) {
      desks_scoped_notify_disablers.push_back(
          desk->GetScopedNotifyContentChangedDisabler());
    }
  }

  OverviewController* overview_controller = Shell::Get()->overview_controller();
  const bool in_overview = overview_controller->InOverviewSession();
  if (desk == active_desk_) {
    if (in_overview) {
      // Selecting the active desk's mini_view in overview mode is allowed and
      // should just exit overview mode normally. Immediately exit overview if
      // switching to a new user, otherwise the multi user switch animation will
      // animate the same windows that overview watches to determine if the
      // overview shutdown animation is complete. See https://crbug.com/1001586.
      const bool immediate_exit = source == DesksSwitchSource::kUserSwitch;
      overview_controller->EndOverview(
          OverviewEndAction::kDeskActivation,
          immediate_exit ? OverviewEnterExitType::kImmediateExit
                         : OverviewEnterExitType::kNormal);
    }
    return;
  }

  UMA_HISTOGRAM_ENUMERATION(kDeskSwitchHistogramName, source);

  const int target_desk_index = GetDeskIndex(desk);
  if (source != DesksSwitchSource::kDeskRemoved) {
    // Desk removal has its own a11y alert.
    Shell::Get()
        ->accessibility_controller()
        ->TriggerAccessibilityAlertWithMessage(l10n_util::GetStringFUTF8(
            IDS_ASH_VIRTUAL_DESKS_ALERT_DESK_ACTIVATED, desk->name()));
  }

  if (source == DesksSwitchSource::kDeskRemoved || is_user_switch) {
    // Desk switches due to desks removal or user switches in a multi-profile
    // session result in immediate desk activation without animation.
    ActivateDeskInternal(desk, /*update_window_activation=*/!in_overview);
    return;
  }

  // When switching desks we want to update window activation when leaving
  // overview or if nothing was active prior to switching desks. This will
  // ensure that after switching desks, we will try to focus a candidate window.
  // We will also update window activation if the currently active window is one
  // in a switchable container. Otherwise, do not update the window activation.
  // This will prevent some ephemeral system UI surfaces such as the app list
  // and system tray from closing when switching desks. An exception is the app
  // list in tablet mode, which should gain activation when there are no
  // windows, as it is treated like a bottom stacked window.
  aura::Window* active_window = window_util::GetActiveWindow();
  const bool update_window_activation =
      in_overview || !active_window ||
      IsParentSwitchableContainer(active_window) ||
      IsApplistActiveInTabletMode(active_window);

  const int starting_desk_index = GetDeskIndex(active_desk());
  animation_ = std::make_unique<DeskActivationAnimation>(
      this, starting_desk_index, target_desk_index, source,
      update_window_activation);
  animation_->Launch();

  metrics_helper_->MaybeStart();
}

bool DesksController::ActivateAdjacentDesk(bool going_left,
                                           DesksSwitchSource source) {
  if (Shell::Get()->session_controller()->IsUserSessionBlocked())
    return false;

  // Try replacing an ongoing desk animation of the same source.
  if (animation_) {
    if (animation_->Replace(going_left, source))
      return true;

    // We arrive here if `DeskActivationAnimation::Replace()` fails
    // due to trying to replace an animation before the original animation has
    // finished taking their screenshots. We can continue with creating a new
    // animation in `ActivateDesk()`, but we need to clean up some desk state.
    ActivateDeskInternal(desks()[animation_->ending_desk_index()].get(),
                         /*update_window_activation=*/false);
    animation_.reset();
  }

  const Desk* desk_to_activate = going_left ? GetPreviousDesk() : GetNextDesk();
  if (desk_to_activate) {
    ActivateDesk(desk_to_activate, source);
  } else {
    for (auto* root : Shell::GetAllRootWindows())
      desks_animations::PerformHitTheWallAnimation(root, going_left);
  }

  return true;
}

bool DesksController::StartSwipeAnimation(bool move_left) {
  // Activate an adjacent desk. It will replace an ongoing touchpad animation if
  // one exists.
  return ActivateAdjacentDesk(move_left,
                              DesksSwitchSource::kDeskSwitchTouchpad);
}

void DesksController::UpdateSwipeAnimation(float scroll_delta_x) {
  if (animation_)
    animation_->UpdateSwipeAnimation(scroll_delta_x);
}

void DesksController::EndSwipeAnimation() {
  if (animation_)
    animation_->EndSwipeAnimation();
}

bool DesksController::MoveWindowFromActiveDeskTo(
    aura::Window* window,
    Desk* target_desk,
    aura::Window* target_root,
    DesksMoveWindowFromActiveDeskSource source) {
  DCHECK_NE(active_desk_, target_desk);

  // An active window might be an always-on-top or pip which doesn't belong to
  // the active desk, and hence cannot be removed.
  if (!base::Contains(active_desk_->windows(), window))
    return false;

  if (window->GetProperty(aura::client::kVisibleOnAllWorkspacesKey)) {
    if (source == DesksMoveWindowFromActiveDeskSource::kDragAndDrop) {
      // Since a visible on all desks window is on all desks, prevent users from
      // moving them manually in overview.
      return false;
    } else if (source == DesksMoveWindowFromActiveDeskSource::kShortcut) {
      window->SetProperty(aura::client::kVisibleOnAllWorkspacesKey, false);
    }
  }

  base::AutoReset<bool> in_progress(&are_desks_being_modified_, true);

  auto* overview_controller = Shell::Get()->overview_controller();
  const bool in_overview = overview_controller->InOverviewSession();

  // The below order matters:
  // If in overview, remove the item from overview first, before calling
  // MoveWindowToDesk(), since MoveWindowToDesk() unminimizes the window (if it
  // was minimized) before updating the mini views. We shouldn't change the
  // window's minimized state before removing it from overview, since overview
  // handles minimized windows differently.
  if (in_overview) {
    auto* overview_session = overview_controller->overview_session();
    auto* item = overview_session->GetOverviewItemForWindow(window);
    // |item| can be null when we are switching users and we're moving visible
    // on all desks windows, so skip if |item| is null.
    if (item) {
      item->OnMovingWindowToAnotherDesk();
      // The item no longer needs to be in the overview grid.
      overview_session->RemoveItem(item);
    }
  }

  active_desk_->MoveWindowToDesk(window, target_desk, target_root);

  MaybeUpdateShelfItems(/*windows_on_inactive_desk=*/{window},
                        /*windows_on_active_desk=*/{});

  Shell::Get()
      ->accessibility_controller()
      ->TriggerAccessibilityAlertWithMessage(l10n_util::GetStringFUTF8(
          IDS_ASH_VIRTUAL_DESKS_ALERT_WINDOW_MOVED_FROM_ACTIVE_DESK,
          window->GetTitle(), active_desk_->name(), target_desk->name()));

  if (source != DesksMoveWindowFromActiveDeskSource::kVisibleOnAllDesks)
    UMA_HISTOGRAM_ENUMERATION(kMoveWindowFromActiveDeskHistogramName, source);
  ReportNumberOfWindowsPerDeskHistogram();

  // A window moving out of the active desk cannot be active.
  // If we are in overview, we should not change the window activation as we do
  // below, since the dummy "OverviewModeFocusedWidget" should remain active
  // while overview mode is active.
  if (!in_overview)
    wm::DeactivateWindow(window);
  return true;
}

void DesksController::AddVisibleOnAllDesksWindow(aura::Window* window) {
  const bool added = visible_on_all_desks_windows_.emplace(window).second;
  DCHECK(added);
  NotifyAllDesksForContentChanged();
  UMA_HISTOGRAM_ENUMERATION(
      kMoveWindowFromActiveDeskHistogramName,
      DesksMoveWindowFromActiveDeskSource::kVisibleOnAllDesks);
}

void DesksController::MaybeRemoveVisibleOnAllDesksWindow(aura::Window* window) {
  if (visible_on_all_desks_windows_.erase(window))
    NotifyAllDesksForContentChanged();
}

void DesksController::NotifyAllDesksForContentChanged() {
  for (const auto& desk : desks_)
    desk->NotifyContentChanged();
}

void DesksController::NotifyDeskNameChanged(const Desk* desk,
                                            const std::u16string& new_name) {
  for (auto& observer : observers_)
    observer.OnDeskNameChanged(desk, new_name);
}

void DesksController::RevertDeskNameToDefault(Desk* desk) {
  DCHECK(HasDesk(desk));
  desk->SetName(GetDeskDefaultName(GetDeskIndex(desk)), /*set_by_user=*/false);
}

void DesksController::RestoreNameOfDeskAtIndex(std::u16string name,
                                               size_t index) {
  DCHECK(!name.empty());
  DCHECK_LT(index, desks_.size());

  desks_[index]->SetName(std::move(name), /*set_by_user=*/true);
}

void DesksController::RestoreCreationTimeOfDeskAtIndex(base::Time creation_time,
                                                       size_t index) {
  DCHECK_LT(index, desks_.size());

  desks_[index]->set_creation_time(creation_time);
}

void DesksController::RestoreVisitedMetricsOfDeskAtIndex(int first_day_visited,
                                                         int last_day_visited,
                                                         size_t index) {
  DCHECK_LT(index, desks_.size());
  DCHECK_GE(last_day_visited, first_day_visited);

  const auto& target_desk = desks_[index];
  target_desk->set_first_day_visited(first_day_visited);
  target_desk->set_last_day_visited(last_day_visited);
  if (!target_desk->IsConsecutiveDailyVisit())
    target_desk->RecordAndResetConsecutiveDailyVisits(/*being_removed=*/false);
}

void DesksController::RestoreWeeklyInteractionMetricOfDeskAtIndex(
    bool interacted_with_this_week,
    size_t index) {
  DCHECK_LT(index, desks_.size());

  desks_[index]->set_interacted_with_this_week(interacted_with_this_week);
}

void DesksController::RestoreWeeklyActiveDesksMetrics(int weekly_active_desks,
                                                      base::Time report_time) {
  DCHECK_GE(weekly_active_desks, 0);

  Desk::SetWeeklyActiveDesks(weekly_active_desks);

  base::TimeDelta report_time_delta(report_time - base::Time::Now());
  if (report_time_delta.InMinutes() < 0) {
    // The scheduled report time has passed so log the restored metrics and
    // reset related metrics.
    RecordAndResetNumberOfWeeklyActiveDesks();
  } else {
    // The scheduled report time has not passed so reset the existing timer to
    // go off at the scheduled report time.
    weekly_active_desks_scheduler_.Start(
        FROM_HERE, report_time_delta, this,
        &DesksController::RecordAndResetNumberOfWeeklyActiveDesks);
  }
}

base::Time DesksController::GetWeeklyActiveReportTime() const {
  return base::Time::Now() + weekly_active_desks_scheduler_.GetCurrentDelay();
}

void DesksController::OnRootWindowAdded(aura::Window* root_window) {
  for (auto& desk : desks_)
    desk->OnRootWindowAdded(root_window);
}

void DesksController::OnRootWindowClosing(aura::Window* root_window) {
  for (auto& desk : desks_)
    desk->OnRootWindowClosing(root_window);
}

int DesksController::GetDeskIndex(const Desk* desk) const {
  for (size_t i = 0; i < desks_.size(); ++i) {
    if (desk == desks_[i].get())
      return i;
  }

  NOTREACHED();
  return -1;
}

aura::Window* DesksController::GetDeskContainer(aura::Window* target_root,
                                                int desk_index) {
  if (desk_index < 0 || desk_index >= static_cast<int>(desks_.size()))
    return nullptr;
  return desks_[desk_index]->GetDeskContainerForRoot(target_root);
}

bool DesksController::BelongsToActiveDesk(aura::Window* window) {
  return desks_util::BelongsToActiveDesk(window);
}

int DesksController::GetActiveDeskIndex() const {
  return GetDeskIndex(active_desk_);
}

std::u16string DesksController::GetDeskName(int index) const {
  return index < static_cast<int>(desks_.size()) ? desks_[index]->name()
                                                 : std::u16string();
}

int DesksController::GetNumberOfDesks() const {
  return static_cast<int>(desks_.size());
}

void DesksController::SendToDeskAtIndex(aura::Window* window, int desk_index) {
  if (desk_index < 0 || desk_index >= static_cast<int>(desks_.size()))
    return;

  if (window->GetProperty(aura::client::kVisibleOnAllWorkspacesKey))
    window->SetProperty(aura::client::kVisibleOnAllWorkspacesKey, false);

  const int active_desk_index = GetDeskIndex(active_desk_);
  if (desk_index == active_desk_index)
    return;

  DCHECK(desks_.at(desk_index));

  desks_animations::PerformWindowMoveToDeskAnimation(
      window, /*going_left=*/desk_index < active_desk_index);
  MoveWindowFromActiveDeskTo(window, desks_[desk_index].get(),
                             window->GetRootWindow(),
                             DesksMoveWindowFromActiveDeskSource::kSendToDesk);
}

std::unique_ptr<DeskTemplate> DesksController::CaptureActiveDeskAsTemplate()
    const {
  DCHECK(current_account_id_.is_valid());

  std::unique_ptr<DeskTemplate> desk_template =
      std::make_unique<DeskTemplate>();
  desk_template->set_template_name(active_desk_->name());

  // Construct |restore_data| for |desk_template|.
  std::unique_ptr<full_restore::RestoreData> restore_data =
      std::make_unique<full_restore::RestoreData>();
  auto* shell = Shell::Get();
  auto mru_windows =
      shell->mru_window_tracker()->BuildMruWindowList(kActiveDesk);
  for (auto* window : mru_windows) {
    std::unique_ptr<full_restore::AppLaunchInfo> app_launch_info =
        shell->shell_delegate()->GetAppLaunchDataForDeskTemplate(window);
    if (!app_launch_info)
      continue;

    // We need to copy |app_launch_info->app_id| to |app_id| as the below
    // function AddAppLaunchInfo() will destroy |app_launch_info|.
    const std::string app_id = app_launch_info->app_id;
    const int32_t window_id = window->GetProperty(full_restore::kWindowIdKey);
    restore_data->AddAppLaunchInfo(std::move(app_launch_info));

    std::unique_ptr<full_restore::WindowInfo> window_info = BuildWindowInfo(
        window, /*activation_index=*/absl::nullopt, mru_windows);
    // Clear WindowInfo's |desk_id| as a window in template will always launch
    // to a newly created desk.
    window_info->desk_id.reset();
    // Clear WindowInfo's `visible_on_all_workspaces` as according to the PRD
    // we don't want the window that is created from desk template is visible
    // on other desks.
    window_info->visible_on_all_workspaces.reset();
    restore_data->ModifyWindowInfo(app_id, window_id, *window_info);
  }
  desk_template->set_desk_restore_data(std::move(restore_data));

  return desk_template;
}

void DesksController::CreateAndActivateNewDeskForTemplate(
    const std::u16string& template_name,
    base::OnceCallback<void(bool)> callback) {
  if (!CanCreateDesks()) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  // If there is an ongoing animation, we should stop it before creating and
  // activating the new desk, which triggers its own animation.
  if (animation_)
    animation_.reset();

  // Change the desk name if the current name already exists.
  int count = 1;
  std::u16string desk_name = template_name;
  while (HasDeskWithName(desk_name)) {
    desk_name = std::u16string(template_name)
                    .append(u" (" + base::FormatNumber(count) + u")");
    count++;
  }

  NewDesk(DesksCreationRemovalSource::kLaunchTemplate);
  Desk* desk = desks().back().get();
  desk->SetName(desk_name, /*set_by_user=*/true);
  new DeskSwitchAnimationObserver(std::move(callback));
  ActivateDesk(desk, DesksSwitchSource::kLaunchTemplate);
  DCHECK(animation_);
}

void DesksController::UpdateDesksDefaultNames() {
  size_t i = 0;
  for (auto& desk : desks_) {
    // Do not overwrite user-modified desks' names.
    if (!desk->is_name_set_by_user())
      desk->SetName(GetDeskDefaultName(i), /*set_by_user=*/false);
    i++;
  }
}

void DesksController::OnWindowActivating(ActivationReason reason,
                                         aura::Window* gaining_active,
                                         aura::Window* losing_active) {
  if (AreDesksBeingModified())
    return;

  // Browser session restore opens all restored windows, so it activates
  // every single window and activates the parent desk. Therefore, this check
  // prevents repetitive desk activation. Moreover, when Bento desks restore is
  // enabled, it avoid switching desk back and forth when windows are restored
  // to different desks.
  if (Shell::Get()->shell_delegate()->IsSessionRestoreInProgress())
    return;

  if (!gaining_active)
    return;

  const Desk* window_desk = FindDeskOfWindow(gaining_active);
  if (!window_desk || window_desk == active_desk_)
    return;

  ActivateDesk(window_desk, DesksSwitchSource::kWindowActivated);
}

void DesksController::OnWindowActivated(ActivationReason reason,
                                        aura::Window* gained_active,
                                        aura::Window* lost_active) {}

void DesksController::OnActiveUserSessionChanged(const AccountId& account_id) {
  // TODO(afakhry): Remove this when multi-profile support goes away.
  DCHECK(current_account_id_.is_valid());
  if (current_account_id_ == account_id) {
    return;
  }

  user_to_active_desk_index_[current_account_id_] = GetDeskIndex(active_desk_);
  current_account_id_ = account_id;

  // Note the following constraints for secondary users:
  // - Simultaneously logged-in users share the same number of desks.
  // - We don't sync and restore the number of desks nor the active desk
  //   position from previous login sessions.
  //
  // Given the above, we do the following for simplicity:
  // - If this user has never been seen before, we activate their first desk.
  // - If one of the simultaneously logged-in users remove desks, that other
  //   users' active-desk indices may become invalid. We won't create extra
  //   desks for this user, but rather we will simply activate their last desk
  //   on the right. Future user switches will update the pref for this user to
  //   the correct value.
  int new_user_active_desk_index =
      /* This is a default initialized index to 0 if the id doesn't exist. */
      user_to_active_desk_index_[current_account_id_];
  new_user_active_desk_index = base::ClampToRange(
      new_user_active_desk_index, 0, static_cast<int>(desks_.size()) - 1);

  ActivateDesk(desks_[new_user_active_desk_index].get(),
               DesksSwitchSource::kUserSwitch);
}

void DesksController::OnFirstSessionStarted() {
  current_account_id_ =
      Shell::Get()->session_controller()->GetActiveAccountId();
  desks_restore_util::RestorePrimaryUserDesks();
}

void DesksController::FireMetricsTimerForTesting() {
  metrics_helper_->FireTimerForTesting();
}

void DesksController::OnAnimationFinished(DeskAnimationBase* animation) {
  DCHECK_EQ(animation_.get(), animation);
  metrics_helper_->OnAnimationFinished(animation->visible_desk_changes());
  animation_.reset();
}

bool DesksController::HasDesk(const Desk* desk) const {
  auto iter = std::find_if(
      desks_.begin(), desks_.end(),
      [desk](const std::unique_ptr<Desk>& d) { return d.get() == desk; });
  return iter != desks_.end();
}

bool DesksController::HasDeskWithName(const std::u16string& desk_name) const {
  auto iter = std::find_if(desks_.begin(), desks_.end(),
                           [desk_name](const std::unique_ptr<Desk>& d) {
                             return d->name() == desk_name;
                           });
  return iter != desks_.end();
}

void DesksController::ActivateDeskInternal(const Desk* desk,
                                           bool update_window_activation) {
  DCHECK(HasDesk(desk));

  if (desk == active_desk_)
    return;

  base::AutoReset<bool> in_progress(&are_desks_being_modified_, true);

  // Mark the new desk as active first, so that deactivating windows on the
  // `old_active` desk do not activate other windows on the same desk. See
  // `ash::AshFocusRules::GetNextActivatableWindow()`.
  Desk* old_active = active_desk_;
  MoveVisibleOnAllDesksWindowsFromActiveDeskTo(const_cast<Desk*>(desk));
  active_desk_ = const_cast<Desk*>(desk);
  RestackVisibleOnAllDesksWindowsOnActiveDesk();

  // There should always be an active desk at any time.
  DCHECK(old_active);
  old_active->Deactivate(update_window_activation);
  active_desk_->Activate(update_window_activation);

  MaybeUpdateShelfItems(old_active->windows(), active_desk_->windows());

  // If in the middle of a window cycle gesture, reset the window cycle list
  // contents so it contains the new active desk's windows.
  auto* shell = Shell::Get();
  auto* window_cycle_controller = shell->window_cycle_controller();
  if (window_cycle_controller->IsAltTabPerActiveDesk())
    window_cycle_controller->MaybeResetCycleList();

  for (auto& observer : observers_)
    observer.OnDeskActivationChanged(active_desk_, old_active);

  // Only update active desk prefs when a primary user switches a desk.
  if (shell->session_controller()->IsUserPrimary()) {
    desks_restore_util::UpdatePrimaryUserActiveDeskPrefs(
        GetDeskIndex(active_desk_));
  }
}

void DesksController::RemoveDeskInternal(const Desk* desk,
                                         DesksCreationRemovalSource source) {
  DCHECK(CanRemoveDesks());

  base::AutoReset<bool> in_progress(&are_desks_being_modified_, true);

  auto iter = std::find_if(
      desks_.begin(), desks_.end(),
      [desk](const std::unique_ptr<Desk>& d) { return d.get() == desk; });
  DCHECK(iter != desks_.end());

  const int removed_desk_index = std::distance(desks_.begin(), iter);
  // Update workspaces of windows in desks that have higher indices than the
  // removed desk since indices of those desks shift by one.
  for (int i = removed_desk_index + 1; i < static_cast<int>(desks_.size());
       i++) {
    for (auto* window : desks_[i]->windows())
      window->SetProperty(aura::client::kWindowWorkspaceKey, i - 1);
  }

  // Record |desk|'s lifetime before it's removed from |desks_|.
  auto* non_const_desk = const_cast<Desk*>(desk);
  non_const_desk->RecordLifetimeHistogram();
  non_const_desk->RecordAndResetConsecutiveDailyVisits(/*being_removed=*/true);

  // Keep the removed desk alive until the end of this function.
  std::unique_ptr<Desk> removed_desk = std::move(*iter);
  removed_desk->SetDeskBeingRemoved();
  DCHECK_EQ(removed_desk.get(), desk);
  auto iter_after = desks_.erase(iter);

  DCHECK(!desks_.empty());

  auto* shell = Shell::Get();
  auto* overview_controller = shell->overview_controller();
  const bool in_overview = overview_controller->InOverviewSession();
  const std::vector<aura::Window*> removed_desk_windows =
      removed_desk->windows();

  // No need to spend time refreshing the mini_views of the removed desk.
  auto removed_desk_mini_views_pauser =
      removed_desk->GetScopedNotifyContentChangedDisabler();

  // - Move windows in removed desk (if any) to the currently active desk.
  // - If the active desk is the one being removed, activate the desk to its
  //   left, if no desk to the left, activate one on the right.
  const bool will_switch_desks = (removed_desk.get() == active_desk_);
  if (!will_switch_desks) {
    // We will refresh the mini_views of the active desk only once at the end.
    auto active_desk_mini_view_pauser =
        active_desk_->GetScopedNotifyContentChangedDisabler();

    removed_desk->MoveWindowsToDesk(active_desk_);

    MaybeUpdateShelfItems({}, removed_desk_windows);

    // If overview mode is active, we add the windows of the removed desk to the
    // overview grid in the order of the new MRU (which changes after removing a
    // desk by making the windows of the removed desk as the least recently used
    // across all desks). Note that this can only be done after the windows have
    // moved to the active desk in `MoveWindowsToDesk()` above, so that building
    // the window MRU list should contain those windows.
    if (in_overview)
      AppendWindowsToOverview(removed_desk_windows);
  } else {
    Desk* target_desk = nullptr;
    if (iter_after == desks_.begin()) {
      // Nothing before this desk.
      target_desk = (*iter_after).get();
    } else {
      // Back up to select the desk on the left.
      target_desk = (*(--iter_after)).get();
    }

    DCHECK(target_desk);

    // The target desk, which is about to become active, will have its
    // mini_views refreshed at the end.
    auto target_desk_mini_view_pauser =
        target_desk->GetScopedNotifyContentChangedDisabler();

    // Exit split view if active, before activating the new desk. We will
    // restore the split view state of the newly activated desk at the end.
    for (aura::Window* root_window : Shell::GetAllRootWindows()) {
      SplitViewController::Get(root_window)
          ->EndSplitView(SplitViewController::EndReason::kDesksChange);
    }

    // The removed desk is still the active desk, so temporarily remove its
    // windows from the overview grid which will result in removing the
    // "OverviewModeLabel" widgets created by overview mode for these windows.
    // This way the removed desk tracks only real windows, which are now ready
    // to be moved to the target desk.
    if (in_overview)
      RemoveAllWindowsFromOverview();

    // If overview mode is active, change desk activation without changing
    // window activation. Activation should remain on the dummy
    // "OverviewModeFocusedWidget" while overview mode is active.
    removed_desk->MoveWindowsToDesk(target_desk);
    ActivateDesk(target_desk, DesksSwitchSource::kDeskRemoved);

    // Desk activation should not change overview mode state.
    DCHECK_EQ(in_overview, overview_controller->InOverviewSession());

    // Now that |target_desk| is activated, we can restack the visible on all
    // desks windows that were moved from the old active desk.
    RestackVisibleOnAllDesksWindowsOnActiveDesk();

    // Now that the windows from the removed and target desks merged, add them
    // all to the grid in the order of the new MRU.
    if (in_overview)
      AppendWindowsToOverview(target_desk->windows());
  }

  // It's OK now to refresh the mini_views of *only* the active desk, and only
  // if windows from the removed desk moved to it.
  DCHECK(active_desk_->should_notify_content_changed());
  if (!removed_desk_windows.empty())
    active_desk_->NotifyContentChanged();

  UpdateDesksDefaultNames();

  for (auto& observer : observers_)
    observer.OnDeskRemoved(removed_desk.get());

  available_container_ids_.push(removed_desk->container_id());

  // Avoid having stale backdrop state as a desk is removed while in overview
  // mode, since the backdrop controller won't update the backdrop window as
  // the removed desk's windows move out from the container. Therefore, we need
  // to update it manually.
  if (in_overview)
    removed_desk->UpdateDeskBackdrops();

  // Restoring split view may start or end overview mode, therefore do this at
  // the end to avoid getting into a bad state.
  if (will_switch_desks)
    MaybeRestoreSplitView(/*refresh_snapped_windows=*/true);

  UMA_HISTOGRAM_ENUMERATION(kRemoveDeskHistogramName, source);
  ReportDesksCountHistogram();
  ReportNumberOfWindowsPerDeskHistogram();

  Shell::Get()
      ->accessibility_controller()
      ->TriggerAccessibilityAlertWithMessage(l10n_util::GetStringFUTF8(
          IDS_ASH_VIRTUAL_DESKS_ALERT_DESK_REMOVED, removed_desk->name(),
          active_desk_->name()));

  desks_restore_util::UpdatePrimaryUserDeskNamesPrefs();
  desks_restore_util::UpdatePrimaryUserDeskMetricsPrefs();

  DCHECK_LE(available_container_ids_.size(), desks_util::kMaxNumberOfDesks);
}

void DesksController::MoveVisibleOnAllDesksWindowsFromActiveDeskTo(
    Desk* new_desk) {
  // Ignore activations in the MRU tracker until we finish moving all visible on
  // all desks windows so we maintain global MRU order that is used later
  // for stacking visible on all desks windows.
  auto* mru_tracker = Shell::Get()->mru_window_tracker();
  mru_tracker->SetIgnoreActivations(true);

  for (auto* visible_on_all_desks_window : visible_on_all_desks_windows_) {
    MoveWindowFromActiveDeskTo(
        visible_on_all_desks_window, new_desk,
        visible_on_all_desks_window->GetRootWindow(),
        DesksMoveWindowFromActiveDeskSource::kVisibleOnAllDesks);
  }

  mru_tracker->SetIgnoreActivations(false);
}

void DesksController::RestackVisibleOnAllDesksWindowsOnActiveDesk() {
  auto mru_windows =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk);
  for (auto* visible_on_all_desks_window : visible_on_all_desks_windows_) {
    auto visible_on_all_desks_window_iter = std::find(
        mru_windows.begin(), mru_windows.end(), visible_on_all_desks_window);
    if (visible_on_all_desks_window_iter == mru_windows.end())
      continue;

    auto* desk_container =
        visible_on_all_desks_window->GetRootWindow()->GetChildById(
            active_desk_->container_id());
    DCHECK_EQ(desk_container, visible_on_all_desks_window->parent());

    // Search through the MRU list for the next element that shares the same
    // parent. This will be used to stack |visible_on_all_desks_window| in
    // the active desk so its stacking respects global MRU order.
    auto closest_window_below_iter =
        std::next(visible_on_all_desks_window_iter);
    while (closest_window_below_iter != mru_windows.end() &&
           (*closest_window_below_iter)->parent() !=
               visible_on_all_desks_window->parent()) {
      closest_window_below_iter = std::next(closest_window_below_iter);
    }

    if (closest_window_below_iter == mru_windows.end()) {
      // There was no element in the MRU list that was used after
      // |visible_on_all_desks_window| so stack it at the bottom.
      desk_container->StackChildAtBottom(visible_on_all_desks_window);
    } else {
      desk_container->StackChildAbove(visible_on_all_desks_window,
                                      *closest_window_below_iter);
    }
  }
}

const Desk* DesksController::FindDeskOfWindow(aura::Window* window) const {
  DCHECK(window);

  for (const auto& desk : desks_) {
    if (base::Contains(desk->windows(), window))
      return desk.get();
  }

  return nullptr;
}

void DesksController::ReportNumberOfWindowsPerDeskHistogram() const {
  for (size_t i = 0; i < desks_.size(); ++i) {
    const size_t windows_count = desks_[i]->windows().size();
    switch (i) {
      case 0:
        UMA_HISTOGRAM_COUNTS_100(kNumberOfWindowsOnDesk_1_HistogramName,
                                 windows_count);
        break;

      case 1:
        UMA_HISTOGRAM_COUNTS_100(kNumberOfWindowsOnDesk_2_HistogramName,
                                 windows_count);
        break;

      case 2:
        UMA_HISTOGRAM_COUNTS_100(kNumberOfWindowsOnDesk_3_HistogramName,
                                 windows_count);
        break;

      case 3:
        UMA_HISTOGRAM_COUNTS_100(kNumberOfWindowsOnDesk_4_HistogramName,
                                 windows_count);
        break;

      case 4:
        UMA_HISTOGRAM_COUNTS_100(kNumberOfWindowsOnDesk_5_HistogramName,
                                 windows_count);
        break;

      case 5:
        UMA_HISTOGRAM_COUNTS_100(kNumberOfWindowsOnDesk_6_HistogramName,
                                 windows_count);
        break;

      case 6:
        UMA_HISTOGRAM_COUNTS_100(kNumberOfWindowsOnDesk_7_HistogramName,
                                 windows_count);
        break;

      case 7:
        UMA_HISTOGRAM_COUNTS_100(kNumberOfWindowsOnDesk_8_HistogramName,
                                 windows_count);
        break;

      default:
        NOTREACHED();
        break;
    }
  }
}

void DesksController::ReportDesksCountHistogram() const {
  DCHECK_LE(desks_.size(), desks_util::kMaxNumberOfDesks);
  UMA_HISTOGRAM_EXACT_LINEAR(kDesksCountHistogramName, desks_.size(),
                             desks_util::kMaxNumberOfDesks);
}

void DesksController::RecordAndResetNumberOfWeeklyActiveDesks() {
  base::UmaHistogramCounts1000(kWeeklyActiveDesksHistogramName,
                               Desk::GetWeeklyActiveDesks());

  for (const auto& desk : desks_)
    desk->set_interacted_with_this_week(desk.get() == active_desk_);
  Desk::SetWeeklyActiveDesks(1);

  weekly_active_desks_scheduler_.Start(
      FROM_HERE, base::TimeDelta::FromDays(7), this,
      &DesksController::RecordAndResetNumberOfWeeklyActiveDesks);
}

}  // namespace ash
