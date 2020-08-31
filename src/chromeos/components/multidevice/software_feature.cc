// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/multidevice/software_feature.h"

#include "base/notreached.h"

namespace chromeos {

namespace multidevice {

SoftwareFeature FromCryptAuthFeature(
    cryptauth::SoftwareFeature cryptauth_feature) {
  switch (cryptauth_feature) {
    case cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST:
      return SoftwareFeature::kBetterTogetherHost;
    case cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT:
      return SoftwareFeature::kBetterTogetherClient;
    case cryptauth::SoftwareFeature::EASY_UNLOCK_HOST:
      return SoftwareFeature::kSmartLockHost;
    case cryptauth::SoftwareFeature::EASY_UNLOCK_CLIENT:
      return SoftwareFeature::kSmartLockClient;
    case cryptauth::SoftwareFeature::MAGIC_TETHER_HOST:
      return SoftwareFeature::kInstantTetheringHost;
    case cryptauth::SoftwareFeature::MAGIC_TETHER_CLIENT:
      return SoftwareFeature::kInstantTetheringClient;
    case cryptauth::SoftwareFeature::SMS_CONNECT_HOST:
      return SoftwareFeature::kMessagesForWebHost;
    case cryptauth::SoftwareFeature::SMS_CONNECT_CLIENT:
      return SoftwareFeature::kMessagesForWebClient;
    case cryptauth::SoftwareFeature::UNKNOWN_FEATURE:
      NOTREACHED();
  }

  NOTREACHED();
  return SoftwareFeature::kBetterTogetherHost;
}

cryptauth::SoftwareFeature ToCryptAuthFeature(
    SoftwareFeature multidevice_feature) {
  // Note: No default case needed since SoftwareFeature is an enum class.
  switch (multidevice_feature) {
    case SoftwareFeature::kBetterTogetherHost:
      return cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST;
    case SoftwareFeature::kBetterTogetherClient:
      return cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT;
    case SoftwareFeature::kSmartLockHost:
      return cryptauth::SoftwareFeature::EASY_UNLOCK_HOST;
    case SoftwareFeature::kSmartLockClient:
      return cryptauth::SoftwareFeature::EASY_UNLOCK_CLIENT;
    case SoftwareFeature::kInstantTetheringHost:
      return cryptauth::SoftwareFeature::MAGIC_TETHER_HOST;
    case SoftwareFeature::kInstantTetheringClient:
      return cryptauth::SoftwareFeature::MAGIC_TETHER_CLIENT;
    case SoftwareFeature::kMessagesForWebHost:
      return cryptauth::SoftwareFeature::SMS_CONNECT_HOST;
    case SoftwareFeature::kMessagesForWebClient:
      return cryptauth::SoftwareFeature::SMS_CONNECT_CLIENT;
  }

  NOTREACHED();
  return cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST;
}

std::ostream& operator<<(std::ostream& stream, const SoftwareFeature& feature) {
  switch (feature) {
    case SoftwareFeature::kBetterTogetherHost:
      stream << "[Better Together host]";
      break;
    case SoftwareFeature::kBetterTogetherClient:
      stream << "[Better Together client]";
      break;
    case SoftwareFeature::kSmartLockHost:
      stream << "[Smart Lock host]";
      break;
    case SoftwareFeature::kSmartLockClient:
      stream << "[Smart Lock client]";
      break;
    case SoftwareFeature::kInstantTetheringHost:
      stream << "[Instant Tethering host]";
      break;
    case SoftwareFeature::kInstantTetheringClient:
      stream << "[Instant Tethering client]";
      break;
    case SoftwareFeature::kMessagesForWebHost:
      stream << "[Messages for Web host]";
      break;
    case SoftwareFeature::kMessagesForWebClient:
      stream << "[Messages for Web client]";
      break;
  }
  return stream;
}

}  // namespace multidevice

}  // namespace chromeos
