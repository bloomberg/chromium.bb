// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/parent_access_controller.h"

#include "ash/login/login_screen_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string16.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

// Number of digits displayed in parent access code input.
constexpr int kParentAccessCodePinLength = 6;

base::string16 GetTitle(ParentAccessRequestReason reason) {
  int title_id;
  switch (reason) {
    case ParentAccessRequestReason::kUnlockTimeLimits:
      title_id = IDS_ASH_LOGIN_PARENT_ACCESS_TITLE;
      break;
    case ParentAccessRequestReason::kChangeTime:
      title_id = IDS_ASH_LOGIN_PARENT_ACCESS_TITLE_CHANGE_TIME;
      break;
    case ParentAccessRequestReason::kChangeTimezone:
      title_id = IDS_ASH_LOGIN_PARENT_ACCESS_TITLE_CHANGE_TIMEZONE;
      break;
  }
  return l10n_util::GetStringUTF16(title_id);
}

base::string16 GetDescription(ParentAccessRequestReason reason) {
  int description_id;
  switch (reason) {
    case ParentAccessRequestReason::kUnlockTimeLimits:
      description_id = IDS_ASH_LOGIN_PARENT_ACCESS_DESCRIPTION;
      break;
    case ParentAccessRequestReason::kChangeTime:
    case ParentAccessRequestReason::kChangeTimezone:
      description_id = IDS_ASH_LOGIN_PARENT_ACCESS_GENERIC_DESCRIPTION;
      break;
  }
  return l10n_util::GetStringUTF16(description_id);
}

base::string16 GetAccessibleTitle() {
  return l10n_util::GetStringUTF16(IDS_ASH_LOGIN_PARENT_ACCESS_DIALOG_NAME);
}

}  // namespace

ParentAccessController::ParentAccessController() {}

ParentAccessController::~ParentAccessController() = default;

// static
constexpr char ParentAccessController::kUMAParentAccessCodeAction[];

// static
constexpr char ParentAccessController::kUMAParentAccessCodeUsage[];

void RecordParentAccessAction(ParentAccessController::UMAAction action) {
  UMA_HISTOGRAM_ENUMERATION(ParentAccessController::kUMAParentAccessCodeAction,
                            action);
}

void RecordParentAccessUsage(ParentAccessRequestReason reason) {
  switch (reason) {
    case ParentAccessRequestReason::kUnlockTimeLimits: {
      UMA_HISTOGRAM_ENUMERATION(
          ParentAccessController::kUMAParentAccessCodeUsage,
          ParentAccessController::UMAUsage::kTimeLimits);
      return;
    }
    case ParentAccessRequestReason::kChangeTime: {
      bool is_login = Shell::Get()->session_controller()->GetSessionState() ==
                      session_manager::SessionState::LOGIN_PRIMARY;
      UMA_HISTOGRAM_ENUMERATION(
          ParentAccessController::kUMAParentAccessCodeUsage,
          is_login ? ParentAccessController::UMAUsage::kTimeChangeLoginScreen
                   : ParentAccessController::UMAUsage::kTimeChangeInSession);
      return;
    }
    case ParentAccessRequestReason::kChangeTimezone: {
      UMA_HISTOGRAM_ENUMERATION(
          ParentAccessController::kUMAParentAccessCodeUsage,
          ParentAccessController::UMAUsage::kTimezoneChange);
      return;
    }
  }
  NOTREACHED() << "Unknown ParentAccessRequestReason";
}

PinRequestView::SubmissionResult ParentAccessController::OnPinSubmitted(
    const std::string& pin) {
  bool pin_is_valid =
      Shell::Get()->login_screen_controller()->ValidateParentAccessCode(
          account_id_, validation_time_, pin);

  if (pin_is_valid) {
    VLOG(1) << "Parent access code successfully validated";
    RecordParentAccessAction(
        ParentAccessController::UMAAction::kValidationSuccess);
    return PinRequestView::SubmissionResult::kPinAccepted;
  }

  VLOG(1) << "Invalid parent access code entered";
  RecordParentAccessAction(ParentAccessController::UMAAction::kValidationError);
  PinRequestWidget::Get()->UpdateState(
      PinRequestViewState::kError,
      l10n_util::GetStringUTF16(IDS_ASH_LOGIN_PARENT_ACCESS_TITLE_ERROR),
      GetDescription(reason_));
  return PinRequestView::SubmissionResult::kPinError;
}

void ParentAccessController::OnBack() {
  RecordParentAccessAction(ParentAccessController::UMAAction::kCanceledByUser);
}

void ParentAccessController::OnHelp(gfx::NativeWindow parent_window) {
  RecordParentAccessAction(ParentAccessController::UMAAction::kGetHelp);
  // TODO(https://crbug.com/999387): Remove this when handling touch
  // cancellation is fixed for system modal windows.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](gfx::NativeWindow parent_window) {
            Shell::Get()->login_screen_controller()->ShowParentAccessHelpApp(
                parent_window);
          },
          parent_window));
}

bool ParentAccessController::ShowWidget(
    const AccountId& child_account_id,
    PinRequest::OnPinRequestDone on_exit_callback,
    ParentAccessRequestReason reason,
    bool extra_dimmer,
    base::Time validation_time) {
  if (PinRequestWidget::Get())
    return false;

  account_id_ = child_account_id;
  reason_ = reason;
  validation_time_ = validation_time;
  PinRequest request;
  request.on_pin_request_done = std::move(on_exit_callback);
  request.help_button_enabled = true;
  request.extra_dimmer = extra_dimmer;
  request.pin_length = kParentAccessCodePinLength;
  request.obscure_pin = false;
  request.title = GetTitle(reason);
  request.description = GetDescription(reason);
  request.accessible_title = GetAccessibleTitle();
  PinRequestWidget::Show(std::move(request), this);
  RecordParentAccessUsage(reason);
  return true;
}

}  // namespace ash
