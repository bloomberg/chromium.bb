// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/phonehub/fake_notification_access_manager.h"

namespace ash {
namespace phonehub {

FakeNotificationAccessManager::FakeNotificationAccessManager(
    AccessStatus access_status,
    AccessProhibitedReason reason)
    : access_status_(access_status), access_prohibited_reason_(reason) {}

FakeNotificationAccessManager::~FakeNotificationAccessManager() = default;

void FakeNotificationAccessManager::SetAccessStatusInternal(
    AccessStatus access_status,
    AccessProhibitedReason reason) {
  if (access_status_ == access_status)
    return;

  access_status_ = access_status;
  access_prohibited_reason_ = reason;
  NotifyNotificationAccessChanged();
}

NotificationAccessManager::AccessStatus
FakeNotificationAccessManager::GetAccessStatus() const {
  return access_status_;
}

NotificationAccessManager::AccessProhibitedReason
FakeNotificationAccessManager::GetAccessProhibitedReason() const {
  return access_prohibited_reason_;
}

bool FakeNotificationAccessManager::HasNotificationSetupUiBeenDismissed()
    const {
  return has_notification_setup_ui_been_dismissed_;
}

void FakeNotificationAccessManager::DismissSetupRequiredUi() {
  has_notification_setup_ui_been_dismissed_ = true;
}

void FakeNotificationAccessManager::ResetHasNotificationSetupUiBeenDismissed() {
  has_notification_setup_ui_been_dismissed_ = false;
}

void FakeNotificationAccessManager::SetNotificationSetupOperationStatus(
    NotificationAccessSetupOperation::Status new_status) {
  switch (new_status) {
    case NotificationAccessSetupOperation::Status::kCompletedSuccessfully:
      SetAccessStatusInternal(AccessStatus::kAccessGranted);
      break;
    case NotificationAccessSetupOperation::Status::
        kProhibitedFromProvidingAccess:
      SetAccessStatusInternal(AccessStatus::kProhibited);
      break;
    default:
      // Do not update access status based on other operation status values.
      break;
  }

  NotificationAccessManager::SetNotificationSetupOperationStatus(new_status);
}

}  // namespace phonehub
}  // namespace ash
