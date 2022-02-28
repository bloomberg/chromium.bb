// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/phonehub/feature_status.h"

namespace ash {
namespace phonehub {

std::ostream& operator<<(std::ostream& stream, FeatureStatus status) {
  switch (status) {
    case FeatureStatus::kNotEligibleForFeature:
      stream << "[Not eligible for feature]";
      break;
    case FeatureStatus::kEligiblePhoneButNotSetUp:
      stream << "[Eligible phone but not set up]";
      break;
    case FeatureStatus::kPhoneSelectedAndPendingSetup:
      stream << "[Phone selected and pending setup]";
      break;
    case FeatureStatus::kDisabled:
      stream << "[Disabled]";
      break;
    case FeatureStatus::kUnavailableBluetoothOff:
      stream << "[Unavailable; Bluetooth off]";
      break;
    case FeatureStatus::kEnabledButDisconnected:
      stream << "[Enabled; disconnected]";
      break;
    case FeatureStatus::kEnabledAndConnecting:
      stream << "[Enabled; connecting]";
      break;
    case FeatureStatus::kEnabledAndConnected:
      stream << "[Enabled; connected]";
      break;
    case FeatureStatus::kLockOrSuspended:
      stream << "[Unavailable; lock or suspended]";
      break;
  }

  return stream;
}

}  // namespace phonehub
}  // namespace ash
