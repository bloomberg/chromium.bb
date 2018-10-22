// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cryptauth/proto/enum_util.h"

namespace cryptauth {

std::ostream& operator<<(std::ostream& stream,
                         const SoftwareFeature& software_feature) {
  switch (software_feature) {
    case SoftwareFeature::BETTER_TOGETHER_HOST:
      stream << "[Better Together host]";
      break;
    case SoftwareFeature::BETTER_TOGETHER_CLIENT:
      stream << "[Better Together client]";
      break;
    case SoftwareFeature::EASY_UNLOCK_HOST:
      stream << "[EasyUnlock host]";
      break;
    case SoftwareFeature::EASY_UNLOCK_CLIENT:
      stream << "[EasyUnlock client]";
      break;
    case SoftwareFeature::MAGIC_TETHER_HOST:
      stream << "[Instant Tethering host]";
      break;
    case SoftwareFeature::MAGIC_TETHER_CLIENT:
      stream << "[Instant Tethering client]";
      break;
    case SoftwareFeature::SMS_CONNECT_HOST:
      stream << "[SMS Connect host]";
      break;
    case SoftwareFeature::SMS_CONNECT_CLIENT:
      stream << "[SMS Connect client]";
      break;
    default:
      stream << "[unknown software feature]";
      break;
  }
  return stream;
}

cryptauth::SoftwareFeature SoftwareFeatureStringToEnum(
    const std::string& software_feature_as_string) {
  if (software_feature_as_string == "betterTogetherHost")
    return cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST;
  if (software_feature_as_string == "betterTogetherClient")
    return cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT;
  if (software_feature_as_string == "easyUnlockHost")
    return cryptauth::SoftwareFeature::EASY_UNLOCK_HOST;
  if (software_feature_as_string == "easyUnlockClient")
    return cryptauth::SoftwareFeature::EASY_UNLOCK_CLIENT;
  if (software_feature_as_string == "magicTetherHost")
    return cryptauth::SoftwareFeature::MAGIC_TETHER_HOST;
  if (software_feature_as_string == "magicTetherClient")
    return cryptauth::SoftwareFeature::MAGIC_TETHER_CLIENT;
  if (software_feature_as_string == "smsConnectHost")
    return cryptauth::SoftwareFeature::SMS_CONNECT_HOST;
  if (software_feature_as_string == "smsConnectClient")
    return cryptauth::SoftwareFeature::SMS_CONNECT_CLIENT;

  return cryptauth::SoftwareFeature::UNKNOWN_FEATURE;
}

std::string SoftwareFeatureEnumToString(
    cryptauth::SoftwareFeature software_feature) {
  switch (software_feature) {
    case SoftwareFeature::BETTER_TOGETHER_HOST:
      return "betterTogetherHost";
    case SoftwareFeature::BETTER_TOGETHER_CLIENT:
      return "betterTogetherClient";
    case SoftwareFeature::EASY_UNLOCK_HOST:
      return "easyUnlockHost";
    case SoftwareFeature::EASY_UNLOCK_CLIENT:
      return "easyUnlockClient";
    case SoftwareFeature::MAGIC_TETHER_HOST:
      return "magicTetherHost";
    case SoftwareFeature::MAGIC_TETHER_CLIENT:
      return "magicTetherClient";
    case SoftwareFeature::SMS_CONNECT_HOST:
      return "smsConnectHost";
    case SoftwareFeature::SMS_CONNECT_CLIENT:
      return "smsConnectClient";
    default:
      return "unknownFeature";
  }
}

}  // namespace cryptauth
