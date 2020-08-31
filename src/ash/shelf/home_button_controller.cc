// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/home_button_controller.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/home_screen/home_screen_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/assistant_overlay.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/shelf_button.h"
#include "ash/shell.h"
#include "ash/shell_state.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/bind.h"
#include "base/check_op.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/account_id/account_id.h"
#include "ui/display/screen.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

constexpr base::TimeDelta kAssistantAnimationDelay =
    base::TimeDelta::FromMilliseconds(200);

// Returns true if the button should appear activatable.
bool CanActivate(int64_t display_id) {
  return Shell::Get()->IsInTabletMode() ||
         !Shell::Get()->app_list_controller()->IsVisible(display_id);
}

}  // namespace

HomeButtonController::HomeButtonController(HomeButton* button)
    : button_(button) {
  DCHECK(button_);

  InitializeAssistantOverlay();
  DCHECK(assistant_overlay_);

  Shell* shell = Shell::Get();
  shell->app_list_controller()->AddObserver(this);
  shell->tablet_mode_controller()->AddObserver(this);
  AssistantUiController::Get()->AddModelObserver(this);
  AssistantState::Get()->AddObserver(this);
}

HomeButtonController::~HomeButtonController() {
  Shell* shell = Shell::Get();

  // AppListController and TabletModeController are destroyed early when Shell
  // is being destroyed, so they may not exist.
  if (AssistantUiController::Get())
    AssistantUiController::Get()->RemoveModelObserver(this);
  if (shell->app_list_controller())
    shell->app_list_controller()->RemoveObserver(this);
  if (shell->tablet_mode_controller())
    shell->tablet_mode_controller()->RemoveObserver(this);
  if (AssistantState::Get())
    AssistantState::Get()->RemoveObserver(this);
}

bool HomeButtonController::MaybeHandleGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_TAP:
    case ui::ET_GESTURE_TAP_CANCEL:
      if (IsAssistantAvailable()) {
        assistant_overlay_->EndAnimation();
        assistant_animation_delay_timer_->Stop();
      }

      if (CanActivate(button_->GetDisplayId()))
        button_->AnimateInkDrop(views::InkDropState::ACTION_TRIGGERED, event);

      // After animating the ripple, let the button handle the event.
      return false;
    case ui::ET_GESTURE_TAP_DOWN:
      if (IsAssistantAvailable()) {
        assistant_animation_delay_timer_->Start(
            FROM_HERE, kAssistantAnimationDelay,
            base::BindOnce(&HomeButtonController::StartAssistantAnimation,
                           base::Unretained(this)));
      }

      if (CanActivate(button_->GetDisplayId()))
        button_->AnimateInkDrop(views::InkDropState::ACTION_PENDING, event);

      return false;
    case ui::ET_GESTURE_LONG_PRESS:
      // Only consume the long press event if the Assistant is available.
      if (!IsAssistantAvailable())
        return false;

      base::RecordAction(base::UserMetricsAction(
          "VoiceInteraction.Started.HomeButtonLongPress"));
      assistant_overlay_->BurstAnimation();
      event->SetHandled();
      Shell::Get()->shell_state()->SetRootWindowForNewWindows(
          button_->GetWidget()->GetNativeWindow()->GetRootWindow());
      AssistantUiController::Get()->ShowUi(
          AssistantEntryPoint::kLongPressLauncher);
      return true;
    case ui::ET_GESTURE_LONG_TAP:
      // Only consume the long tap event if the Assistant is available.
      if (!IsAssistantAvailable())
        return false;

      // This event happens after the user long presses and lifts the finger.
      button_->AnimateInkDrop(views::InkDropState::HIDDEN, event);

      // We already handled the long press; consume the long tap to avoid
      // bringing up the context menu again.
      event->SetHandled();
      return true;
    default:
      return false;
  }
}

bool HomeButtonController::IsAssistantAvailable() {
  AssistantStateBase* state = AssistantState::Get();
  return state->allowed_state() ==
             chromeos::assistant::AssistantAllowedState::ALLOWED &&
         state->settings_enabled().value_or(false);
}

bool HomeButtonController::IsAssistantVisible() {
  return AssistantUiController::Get()->GetModel()->visibility() ==
         AssistantVisibility::kVisible;
}

void HomeButtonController::OnAppListVisibilityWillChange(bool shown,
                                                         int64_t display_id) {
  if (button_->GetDisplayId() != display_id)
    return;
  if (shown)
    OnAppListShown();
  else
    OnAppListDismissed();
}

void HomeButtonController::OnTabletModeStarted() {
  button_->AnimateInkDrop(views::InkDropState::DEACTIVATED, nullptr);
}

void HomeButtonController::OnAssistantFeatureAllowedChanged(
    chromeos::assistant::AssistantAllowedState state) {
  button_->OnAssistantAvailabilityChanged();
}

void HomeButtonController::OnAssistantSettingsEnabled(bool enabled) {
  button_->OnAssistantAvailabilityChanged();
}

void HomeButtonController::OnUiVisibilityChanged(
    AssistantVisibility new_visibility,
    AssistantVisibility old_visibility,
    base::Optional<AssistantEntryPoint> entry_point,
    base::Optional<AssistantExitPoint> exit_point) {
  button_->OnAssistantAvailabilityChanged();
}

void HomeButtonController::StartAssistantAnimation() {
  assistant_overlay_->StartAnimation(false);
}

void HomeButtonController::OnAppListShown() {
  // Do not show a highlight in tablet mode, since the home screen view is
  // always open in the background.
  if (!Shell::Get()->IsInTabletMode())
    button_->AnimateInkDrop(views::InkDropState::ACTIVATED, nullptr);
  is_showing_app_list_ = true;
  RootWindowController::ForWindow(button_->GetWidget()->GetNativeWindow())
      ->UpdateShelfVisibility();
}

void HomeButtonController::OnAppListDismissed() {
  // If ink drop is not hidden already, snap it to active state, so animation to
  // DEACTIVATED state starts immediately (the animation would otherwise wait
  // for the current animation to finish).
  views::InkDrop* const ink_drop = button_->GetInkDrop();
  if (ink_drop->GetTargetInkDropState() != views::InkDropState::HIDDEN)
    ink_drop->SnapToActivated();
  button_->AnimateInkDrop(views::InkDropState::DEACTIVATED, nullptr);

  is_showing_app_list_ = false;
  RootWindowController::ForWindow(button_->GetWidget()->GetNativeWindow())
      ->UpdateShelfVisibility();
}

void HomeButtonController::InitializeAssistantOverlay() {
  DCHECK_EQ(nullptr, assistant_overlay_);
  assistant_overlay_ = new AssistantOverlay(button_);
  button_->AddChildView(assistant_overlay_);
  assistant_overlay_->SetVisible(false);
  assistant_animation_delay_timer_ = std::make_unique<base::OneShotTimer>();
}

}  // namespace ash
