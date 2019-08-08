// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_FEATURE_TYPE_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_FEATURE_TYPE_H_

#include <ostream>
#include <string>

#include "base/optional.h"

namespace chromeos {

namespace device_sync {

// The feature types used by CryptAuth v2 DeviceSync's BatchGetFeatureStatuses,
// BatchSetFeatureStatuses, and BatchNotifyGroupDevices. Each enum value is
// uniquely mapped to a string used in the protos and understood by CryptAuth.
//
// Example: The following FeatureStatus messages received in a
// BatchGetFeatureStatuses response would indicate that BetterTogetherHost is
// supported but not enabled:
//   [
//     message FeatureStatus {
//       string feature_type = "BetterTogetherHostSupported";
//       bool enabled = true;
//     },
//     message FeatureStatus {
//       string feature_type = "BetterTogetherHostEnabled";
//       bool enabled = false;
//     }
//   ]
enum class CryptAuthFeatureType {
  // Support for multi-device features in general.
  kBetterTogetherHostSupported,
  kBetterTogetherHostEnabled,
  kBetterTogetherClientSupported,
  kBetterTogetherClientEnabled,

  // Smart Lock, which gives the user the ability to unlock and/or sign into a
  // Chromebook using an Android phone.
  kEasyUnlockHostSupported,
  kEasyUnlockHostEnabled,
  kEasyUnlockClientSupported,
  kEasyUnlockClientEnabled,

  // Instant Tethering, which gives the user the ability to use an Android
  // phone's Internet connection on a Chromebook.
  kMagicTetherHostSupported,
  kMagicTetherHostEnabled,
  kMagicTetherClientSupported,
  kMagicTetherClientEnabled,

  // Messages for Web, which gives the user the ability to sync messages (e.g.,
  // SMS) between an Android phone and a Chromebook.
  kSmsConnectHostSupported,
  kSmsConnectHostEnabled,
  kSmsConnectClientSupported,
  kSmsConnectClientEnabled,
};

// Uniquely maps each enum value to a string used in the protos and understood
// by CryptAuth.
std::string CryptAuthFeatureTypeToString(
    const CryptAuthFeatureType& feature_type);

// Returns null if |feature_type_string| does not map to a known
// CryptAuthFeatureType.
base::Optional<CryptAuthFeatureType> CryptAuthFeatureTypeFromString(
    const std::string& feature_type_string);

std::ostream& operator<<(std::ostream& stream,
                         const CryptAuthFeatureType& feature_type);

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_FEATURE_TYPE_H_
