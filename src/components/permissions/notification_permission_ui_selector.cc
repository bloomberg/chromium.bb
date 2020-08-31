// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/notification_permission_ui_selector.h"

namespace permissions {

NotificationPermissionUiSelector::Decision::Decision(
    base::Optional<QuietUiReason> quiet_ui_reason,
    base::Optional<WarningReason> warning_reason)
    : quiet_ui_reason(quiet_ui_reason), warning_reason(warning_reason) {}
NotificationPermissionUiSelector::Decision::~Decision() = default;

NotificationPermissionUiSelector::Decision::Decision(const Decision&) = default;
NotificationPermissionUiSelector::Decision&
NotificationPermissionUiSelector::Decision::operator=(const Decision&) =
    default;

// static
NotificationPermissionUiSelector::Decision
NotificationPermissionUiSelector::Decision::UseNormalUiAndShowNoWarning() {
  return Decision(UseNormalUi(), ShowNoWarning());
}

}  // namespace permissions
