// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/attestation/attestation_flow_utils.h"

#include <string>

#include "base/notreached.h"
#include "chromeos/dbus/constants/attestation_constants.h"

namespace chromeos {
namespace attestation {

std::string GetKeyNameForProfile(
    AttestationCertificateProfile certificate_profile,
    const std::string& request_origin) {
  switch (certificate_profile) {
    case PROFILE_ENTERPRISE_MACHINE_CERTIFICATE:
      return kEnterpriseMachineKey;
    case PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE:
      return kEnterpriseEnrollmentKey;
    case PROFILE_ENTERPRISE_USER_CERTIFICATE:
      return kEnterpriseUserKey;
    case PROFILE_CONTENT_PROTECTION_CERTIFICATE:
      return std::string(kContentProtectionKeyPrefix) + request_origin;
  }
  NOTREACHED();
  return "";
}

}  // namespace attestation
}  // namespace chromeos
