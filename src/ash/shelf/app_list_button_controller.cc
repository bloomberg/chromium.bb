// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/app_list_button_controller.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/assistant/assistant_controller.h"
#include "ash/home_screen/home_screen_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/app_list_button.h"
#include "ash/shelf/assistant_overlay.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "ash/shell_state.h"
#include "ash/voice_interaction/voice_interaction_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/timer/timer.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/account_id/account_id.h"
#include "ui/display/screen.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

constexpr int kVoiceInteractionAnimationDelayMs = 200;
constexpr int kVoiceInteractionAnimationHideDelayMs = 500;

// Returns true if the button should appear activatable.
bool CanActivate() {
  return Shell::Get()->home_screen_controller()->IsHomeScreenAvailable() ||
         !Shell::Get()->app_list_controller()->IsVisible();
}

}  // namespace

AppListButtonController::AppListButtonController(AppListButton* button,
                                                 Shelf* shelf)
    : button_(button), shelf_(shelf) {
  DCHECK(button_);
  DCHECK(shelf_);
  Shell* shell = Shell::Get();
  // AppListController is only available in non-KioskNext sessions.
  if (shell->app_list_controller())
    shell->app_list_controller()->AddObserver(this);
  shell->session_controller()->AddObserver(this);
  shell->tablet_mode_controller()->AddObserver(this);
  shell->voice_interaction_controller()->AddLocalObserver(this);

  // Initialize voice interaction overlay and sync the flags if active user
  // session has already started. This could happen when an external monitor
  // is plugged in.
  if (shell->session_controller()->IsActiveUserSessionStarted() &&
      chromeos::switches::IsAssistantEnabled()) {
    InitializeVoiceInteractionOverlay();
  }
}

AppListButtonController::~AppListButtonController() {
  Shell* shell = Shell::Get();

  // AppListController and TabletModeController are destroyed early when Shell
  // is being destroyed, so they may not exist.
  if (shell->app_list_controller())
    shell->app_list_controller()->RemoveObserver(this);
  if (shell->tablet_mode_controller())
    shell->tablet_mode_controller()->RemoveObserver(this);
  shell->session_controller()->RemoveObserver(this);
  shell->voice_interaction_controller()->RemoveLocalObserver(this);
}

bool AppListButtonController::MaybeHandleGestureEvent(ui::GestureEvent* event,
                                                      ShelfView* shelf_view) {
  switch (event->type()) {
    case ui::ET_GESTURE_TAP:
    case ui::ET_GESTURE_TAP_CANCEL:
      if (IsVoiceInteractionAvailable()) {
        assistant_overlay_->EndAnimation();
        assistant_animation_delay_timer_->Stop();
      }

      if (CanActivate())
        button_->AnimateInkDrop(views::InkDropState::ACTION_TRIGGERED, event);

      // After animating the ripple, let the button handle the event.
      return false;
    case ui::ET_GESTURE_TAP_DOWN:
      // If |!ShouldEnterPushedState|, Button::OnGestureEvent will not set the
      // |ET_GESTURE_TAP_DOWN| event to be handled. This will cause the
      // |ET_GESTURE_TAP| or |ET_GESTURE_TAP_CANCEL| not to be sent to
      // |button_|, therefore leaving the assistant overlay ripple visible.
      if (!shelf_view->ShouldEventActivateButton(button_, *event))
        return true;

      if (IsVoiceInteractionAvailable()) {
        assistant_animation_delay_timer_->Start(
            FROM_HERE,
            base::TimeDelta::FromMilliseconds(
                kVoiceInteractionAnimationDelayMs),
            base::BindOnce(
                &AppListButtonController::StartVoiceInteractionAnimation,
                base::Unretained(this)));
      }

      if (CanActivate())
        button_->AnimateInkDrop(views::InkDropState::ACTION_PENDING, event);

      return false;
    case ui::ET_GESTURE_LONG_PRESS:
      // Only consume the long press event if voice interaction is available.
      if (!IsVoiceInteractionAvailable())
        return false;

      base::RecordAction(base::UserMetricsAction(
          "VoiceInteraction.Started.AppListButtonLongPress"));
      assistant_overlay_->BurstAnimation();
      event->SetHandled();
      Shell::Get()->shell_state()->SetRootWindowForNewWindows(
          button_->GetWidget()->GetNativeWindow()->GetRootWindow());
      Shell::Get()->assistant_controller()->ui_controller()->ShowUi(
          AssistantEntryPoint::kLongPressLauncher);
      return true;
    case ui::ET_GESTURE_LONG_TAP:
      // Only consume the long tap event if voice interaction is available.
      if (!IsVoiceInteractionAvailable())
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

bool AppListButtonController::IsVoiceInteractionAvailable() {
  VoiceInteractionController* controller =
      Shell::Get()->voice_interaction_controller();
  bool settings_enabled = controller->settings_enabled().value_or(false);
  bool consent_given = controller->consent_status() ==
                       mojom::ConsentStatus::kActivityControlAccepted;
  bool feature_allowed =
      controller->allowed_state() == mojom::AssistantAllowedState::ALLOWED;

  return assistant_overlay_ && feature_allowed &&
         (settings_enabled || !consent_given);
}

bool AppListButtonController::IsVoiceInteractionRunning() {
  return Shell::Get()
             ->voice_interaction_controller()
             ->voice_interaction_state()
             .value_or(mojom::VoiceInteractionState::STOPPED) ==
         mojom::VoiceInteractionState::RUNNING;
}

void AppListButtonController::OnAppListVisibilityChanged(bool shown,
                                                         int64_t display_id) {
  if (display::Screen::GetScreen()
          ->GetDisplayNearestWindow(button_->GetWidget()->GetNativeWindow())
          .id() != display_id) {
    return;
  }
  if (shown)
    OnAppListShown();
  else
    OnAppListDismissed();
}

void AppListButtonController::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  button_->OnVoiceInteractionAvailabilityChanged();
  // Initialize voice interaction overlay when primary user session becomes
  // active.
  if (Shell::Get()->session_controller()->IsUserPrimary() &&
      !assistant_overlay_ && chromeos::switches::IsAssistantEnabled()) {
    InitializeVoiceInteractionOverlay();
  }
}

void AppListButtonController::OnTabletModeStarted() {
  button_->AnimateInkDrop(views::InkDropState::DEACTIVATED, nullptr);
}

void AppListButtonController::OnVoiceInteractionStatusChanged(
    mojom::VoiceInteractionState state) {
  button_->OnVoiceInteractionAvailabilityChanged();

  if (!assistant_overlay_)
    return;

  switch (state) {
    case mojom::VoiceInteractionState::STOPPED:
      UMA_HISTOGRAM_TIMES(
          "VoiceInteraction.OpenDuration",
          base::TimeTicks::Now() - voice_interaction_start_timestamp_);
      break;
    case mojom::VoiceInteractionState::NOT_READY:
      // If we are showing the bursting or waiting animation, no need to do
      // anything. Otherwise show the waiting animation now.
      // NOTE: No waiting animation for native assistant.
      if (!chromeos::switches::IsAssistantEnabled() &&
          !assistant_overlay_->IsBursting() &&
          !assistant_overlay_->IsWaiting()) {
        assistant_overlay_->WaitingAnimation();
      }
      break;
    case mojom::VoiceInteractionState::RUNNING:
      // we start hiding the animation if it is running.
      if (assistant_overlay_->IsBursting() || assistant_overlay_->IsWaiting()) {
        assistant_animation_hide_delay_timer_->Start(
            FROM_HERE,
            base::TimeDelta::FromMilliseconds(
                kVoiceInteractionAnimationHideDelayMs),
            base::BindOnce(&AssistantOverlay::HideAnimation,
                           base::Unretained(assistant_overlay_)));
      }

      voice_interaction_start_timestamp_ = base::TimeTicks::Now();
      break;
  }
}

void AppListButtonController::OnVoiceInteractionSettingsEnabled(bool enabled) {
  button_->OnVoiceInteractionAvailabilityChanged();
}

void AppListButtonController::OnVoiceInteractionConsentStatusUpdated(
    mojom::ConsentStatus consent_status) {
  button_->OnVoiceInteractionAvailabilityChanged();
}

void AppListButtonController::StartVoiceInteractionAnimation() {
  assistant_overlay_->StartAnimation(false);
}

void AppListButtonController::OnAppListShown() {
  // Do not show a highlight if the home screen is available, since the home
  // screen view is always open in the background.
  if (!Shell::Get()->home_screen_controller()->IsHomeScreenAvailable())
    button_->AnimateInkDrop(views::InkDropState::ACTIVATED, nullptr);
  is_showing_app_list_ = true;
  shelf_->UpdateAutoHideState();
}

void AppListButtonController::OnAppListDismissed() {
  button_->AnimateInkDrop(views::InkDropState::DEACTIVATED, nullptr);
  is_showing_app_list_ = false;
  shelf_->UpdateAutoHideState();
}

void AppListButtonController::InitializeVoiceInteractionOverlay() {
  assistant_overlay_ = new AssistantOverlay(button_);
  button_->AddChildView(assistant_overlay_);
  assistant_overlay_->SetVisible(false);
  assistant_animation_delay_timer_ = std::make_unique<base::OneShotTimer>();
  assistant_animation_hide_delay_timer_ =
      std::make_unique<base::OneShotTimer>();
}

}  // namespace ash
