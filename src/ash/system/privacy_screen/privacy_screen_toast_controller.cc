// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_screen/privacy_screen_toast_controller.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_view.h"

namespace ash {

PrivacyScreenToastController::PrivacyScreenToastController(
    UnifiedSystemTray* tray)
    : tray_(tray) {
  Shell::Get()->privacy_screen_controller()->AddObserver(this);
}

PrivacyScreenToastController::~PrivacyScreenToastController() {
  close_timer_.Stop();
  if (bubble_widget_ && !bubble_widget_->IsClosed())
    bubble_widget_->CloseNow();
  Shell::Get()->privacy_screen_controller()->RemoveObserver(this);
}

void PrivacyScreenToastController::ShowToast() {
  // If the bubble already exists, update the content of the bubble and extend
  // the autoclose timer.
  if (bubble_widget_) {
    UpdateToastView();
    if (!mouse_hovered_)
      StartAutoCloseTimer();
    return;
  }

  tray_->CloseSecondaryBubbles();

  TrayBubbleView::InitParams init_params;
  init_params.shelf_alignment = tray_->shelf()->alignment();
  init_params.preferred_width = kPrivacyScreenToastMinWidth;
  init_params.delegate = this;
  init_params.parent_window = tray_->GetBubbleWindowContainer();
  init_params.anchor_view = nullptr;
  init_params.anchor_mode = TrayBubbleView::AnchorMode::kRect;
  init_params.anchor_rect = tray_->shelf()->GetSystemTrayAnchorRect();
  // Decrease bottom and right insets to compensate for the adjustment of
  // the respective edges in Shelf::GetSystemTrayAnchorRect().
  init_params.insets = GetTrayBubbleInsets();
  init_params.corner_radius = kUnifiedTrayCornerRadius;
  init_params.has_shadow = false;
  init_params.translucent = true;

  bubble_view_ = new TrayBubbleView(init_params);
  toast_view_ = new PrivacyScreenToastView(this);
  bubble_view_->AddChildView(toast_view_);

  bubble_widget_ = views::BubbleDialogDelegateView::CreateBubble(bubble_view_);

  TrayBackgroundView::InitializeBubbleAnimations(bubble_widget_);
  bubble_view_->InitializeAndShowBubble();

  StartAutoCloseTimer();
  UpdateToastView();

  tray_->SetTrayBubbleHeight(
      bubble_widget_->GetWindowBoundsInScreen().height());

  // Activate the bubble so ChromeVox can announce the toast.
  if (Shell::Get()->accessibility_controller()->spoken_feedback_enabled()) {
    bubble_widget_->widget_delegate()->SetCanActivate(true);
    bubble_widget_->Activate();
  }
}

void PrivacyScreenToastController::HideToast() {
  close_timer_.Stop();
  if (!bubble_widget_ || bubble_widget_->IsClosed())
    return;
  bubble_widget_->Close();
  tray_->SetTrayBubbleHeight(0);
}

void PrivacyScreenToastController::BubbleViewDestroyed() {
  close_timer_.Stop();
  bubble_view_ = nullptr;
  bubble_widget_ = nullptr;
}

void PrivacyScreenToastController::OnMouseEnteredView() {
  close_timer_.Stop();
  mouse_hovered_ = true;
}

void PrivacyScreenToastController::OnMouseExitedView() {
  StartAutoCloseTimer();
  mouse_hovered_ = false;
}

base::string16 PrivacyScreenToastController::GetAccessibleNameForBubble() {
  if (!toast_view_)
    return base::string16();
  return toast_view_->GetAccessibleName();
}

void PrivacyScreenToastController::OnPrivacyScreenSettingChanged(bool enabled) {
  if (tray_->IsBubbleShown())
    return;

  ShowToast();
}

void PrivacyScreenToastController::StartAutoCloseTimer() {
  close_timer_.Stop();

  // Don't start the timer if the toast is focused.
  if (toast_view_ && toast_view_->IsButtonFocused())
    return;

  int autoclose_delay = kTrayPopupAutoCloseDelayInSeconds;
  if (Shell::Get()->accessibility_controller()->spoken_feedback_enabled())
    autoclose_delay = kTrayPopupAutoCloseDelayInSecondsWithSpokenFeedback;

  close_timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(autoclose_delay),
                     this, &PrivacyScreenToastController::HideToast);
}

void PrivacyScreenToastController::UpdateToastView() {
  if (toast_view_) {
    auto* privacy_screen_controller = Shell::Get()->privacy_screen_controller();
    toast_view_->SetPrivacyScreenEnabled(
        /*enabled=*/privacy_screen_controller->GetEnabled(),
        /*managed=*/privacy_screen_controller->IsManaged());
    int width = base::ClampToRange(toast_view_->GetPreferredSize().width(),
                                   kPrivacyScreenToastMinWidth,
                                   kPrivacyScreenToastMaxWidth);
    bubble_view_->SetPreferredWidth(width);
  }
}

void PrivacyScreenToastController::ButtonPressed(views::Button* sender,
                                                 const ui::Event& event) {
  auto* privacy_screen_controller = Shell::Get()->privacy_screen_controller();
  privacy_screen_controller->SetEnabled(
      !privacy_screen_controller->GetEnabled());
}

void PrivacyScreenToastController::StopAutocloseTimer() {
  close_timer_.Stop();
}

}  // namespace ash
