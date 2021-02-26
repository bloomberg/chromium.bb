// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_SILENCE_PHONE_QUICK_ACTION_CONTROLLER_H_
#define ASH_SYSTEM_PHONEHUB_SILENCE_PHONE_QUICK_ACTION_CONTROLLER_H_

#include "ash/system/phonehub/quick_action_controller_base.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/components/phonehub/do_not_disturb_controller.h"

namespace base {
class OneShotTimer;
}  // namespace base

namespace ash {

// Controller of a quick action item that toggles silence phone mode.
class ASH_EXPORT SilencePhoneQuickActionController
    : public QuickActionControllerBase,
      public chromeos::phonehub::DoNotDisturbController::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Called when the state of the item has changed.
    virtual void OnSilencePhoneItemStateChanged() = 0;
  };

  explicit SilencePhoneQuickActionController(
      chromeos::phonehub::DoNotDisturbController* dnd_controller);
  ~SilencePhoneQuickActionController() override;
  SilencePhoneQuickActionController(SilencePhoneQuickActionController&) =
      delete;
  SilencePhoneQuickActionController operator=(
      SilencePhoneQuickActionController&) = delete;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Return true if the item is enabled/toggled.
  bool IsItemEnabled();

  // QuickActionControllerBase:
  QuickActionItem* CreateItem() override;
  void OnButtonPressed(bool is_now_enabled) override;

  // chromeos::phonehub::DoNotDisturbController::Observer:
  void OnDndStateChanged() override;

 private:
  friend class SilencePhoneQuickActionControllerTest;

  // All the possible states that the silence phone button can be viewed. Each
  // state has a corresponding icon, labels and tooltip view.
  enum class ActionState { kOff, kOn, kDisabled };

  // Set the item (including icon, label and tooltips) to a certain state.
  void SetItemState(ActionState state);

  // Retrieves the current state of the QuickActionItem. Used only for testing.
  ActionState GetItemState();

  // Check to see if the requested state is similar to current state of the
  // phone. Make changes to item's state if necessary.
  void CheckRequestedState();

  chromeos::phonehub::DoNotDisturbController* dnd_controller_ = nullptr;
  QuickActionItem* item_ = nullptr;

  // Keep track the current state of the item.
  ActionState state_;

  // State that user requests when clicking the button.
  base::Optional<ActionState> requested_state_;

  // Timer that fires to prevent showing wrong state in the item. It will check
  // if the requested state is similar to the current state after the button is
  // pressed for a certain time.
  std::unique_ptr<base::OneShotTimer> check_requested_state_timer_;

  // Registered observers.
  base::ObserverList<Observer> observer_list_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_SILENCE_PHONE_QUICK_ACTION_CONTROLLER_H_
