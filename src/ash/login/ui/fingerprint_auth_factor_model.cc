// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/fingerprint_auth_factor_model.h"

#include "ash/login/resources/grit/login_resources.h"
#include "ash/login/ui/auth_icon_view.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "base/time/time.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/vector_icon_types.h"

namespace ash {

namespace {

constexpr base::TimeDelta kFingerprintFailedAnimationDuration =
    base::Milliseconds(700);
constexpr int kFingerprintFailedAnimationNumFrames = 45;

}  // namespace

FingerprintAuthFactorModel::FingerprintAuthFactorModel() = default;

FingerprintAuthFactorModel::~FingerprintAuthFactorModel() = default;

void FingerprintAuthFactorModel::SetFingerprintState(FingerprintState state) {
  if (state_ == state)
    return;

  // Clear out the timeout if the state changes. This shouldn't happen
  // ordinarily -- permanent error states are permanent after all -- but this is
  // required for the debug overlay to work properly when cycling states.
  has_permanent_error_display_timed_out_ = false;

  state_ = state;
  RefreshUI();
}

void FingerprintAuthFactorModel::NotifyFingerprintAuthResult(bool result) {
  auth_result_ = result;
  RefreshUI();
}

AuthFactorModel::AuthFactorState
FingerprintAuthFactorModel::GetAuthFactorState() const {
  if (!available_)
    return AuthFactorState::kUnavailable;

  if (auth_result_.has_value()) {
    if (auth_result_.value()) {
      return AuthFactorState::kAuthenticated;
    } else if (state_ != FingerprintState::DISABLED_FROM_ATTEMPTS) {
      return AuthFactorState::kErrorTemporary;
    }
  }

  switch (state_) {
    case FingerprintState::UNAVAILABLE:
      return AuthFactorState::kUnavailable;
    case FingerprintState::AVAILABLE_DEFAULT:
      return AuthFactorState::kReady;
    case FingerprintState::AVAILABLE_WITH_TOUCH_SENSOR_WARNING:
      return AuthFactorState::kErrorTemporary;
    case FingerprintState::DISABLED_FROM_ATTEMPTS:
      FALLTHROUGH;
    case FingerprintState::DISABLED_FROM_TIMEOUT:
      return AuthFactorState::kErrorPermanent;
  }
}

AuthFactorType FingerprintAuthFactorModel::GetType() const {
  return AuthFactorType::kFingerprint;
}

int FingerprintAuthFactorModel::GetLabelId() const {
  if (auth_result_.has_value()) {
    if (auth_result_.value()) {
      return IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_AUTH_SUCCESS;
    } else if (state_ != FingerprintState::DISABLED_FROM_ATTEMPTS) {
      return IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_AUTH_FAILED;
    }
  }

  switch (state_) {
    case FingerprintState::UNAVAILABLE:
      FALLTHROUGH;
    case FingerprintState::AVAILABLE_DEFAULT:
      return IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_AVAILABLE;
    case FingerprintState::AVAILABLE_WITH_TOUCH_SENSOR_WARNING:
      return IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_TOUCH_SENSOR;
    case FingerprintState::DISABLED_FROM_ATTEMPTS:
      // TODO(crbug.com/1233614): Update this string: "Too many attempts" ->
      // "Too many fingerprint attempts".
      return IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_DISABLED_FROM_ATTEMPTS;
    case FingerprintState::DISABLED_FROM_TIMEOUT:
      return can_use_pin_ ? IDS_AUTH_FACTOR_LABEL_PASSWORD_OR_PIN_REQUIRED
                          : IDS_AUTH_FACTOR_LABEL_PASSWORD_REQUIRED;
  }
  NOTREACHED();
}

bool FingerprintAuthFactorModel::ShouldAnnounceLabel() const {
  return state_ == FingerprintState::DISABLED_FROM_ATTEMPTS ||
         state_ == FingerprintState::DISABLED_FROM_TIMEOUT ||
         (auth_result_.has_value() && !auth_result_.value());
}

int FingerprintAuthFactorModel::GetAccessibleNameId() const {
  if (state_ == FingerprintState::DISABLED_FROM_ATTEMPTS)
    return IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_ACCESSIBLE_AUTH_DISABLED_FROM_ATTEMPTS;

  return GetLabelId();
}

void FingerprintAuthFactorModel::UpdateIcon(AuthIconView* icon) {
  if (auth_result_.has_value() && !auth_result_.value()) {
    icon->SetAnimation(IDR_LOGIN_FINGERPRINT_UNLOCK_SPINNER,
                       kFingerprintFailedAnimationDuration,
                       kFingerprintFailedAnimationNumFrames);
    return;
  }

  switch (state_) {
    case FingerprintState::AVAILABLE_DEFAULT:
      FALLTHROUGH;
    case FingerprintState::AVAILABLE_WITH_TOUCH_SENSOR_WARNING:
      icon->SetIcon(kLockScreenFingerprintIcon);
      break;
    case FingerprintState::UNAVAILABLE:
      FALLTHROUGH;
    case FingerprintState::DISABLED_FROM_TIMEOUT:
      icon->SetIcon(kLockScreenFingerprintDisabledIcon,
                    AuthIconView::Color::kDisabled);
      break;
    case FingerprintState::DISABLED_FROM_ATTEMPTS:
      if (has_permanent_error_display_timed_out_) {
        icon->SetIcon(kLockScreenFingerprintDisabledIcon,
                      AuthIconView::Color::kDisabled);
      } else {
        icon->SetAnimation(IDR_LOGIN_FINGERPRINT_UNLOCK_SPINNER,
                           kFingerprintFailedAnimationDuration,
                           kFingerprintFailedAnimationNumFrames);
      }
      break;
  }
}

void FingerprintAuthFactorModel::DoHandleTapOrClick() {
  if (state_ == FingerprintState::AVAILABLE_DEFAULT) {
    state_ = FingerprintState::AVAILABLE_WITH_TOUCH_SENSOR_WARNING;
  }
}

void FingerprintAuthFactorModel::DoHandleErrorTimeout() {
  if (auth_result_.has_value() && !auth_result_.value()) {
    // Clear failed auth attempt to allow retry.
    auth_result_.reset();
  }
  if (GetAuthFactorState() == AuthFactorState::kErrorTemporary) {
    state_ = FingerprintState::AVAILABLE_DEFAULT;
  }
}

}  // namespace ash
