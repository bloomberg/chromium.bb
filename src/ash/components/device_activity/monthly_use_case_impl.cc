// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/device_activity/monthly_use_case_impl.h"

#include "ash/components/device_activity/fresnel_pref_names.h"
#include "ash/components/device_activity/fresnel_service.pb.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"

namespace ash {
namespace device_activity {

namespace psm_rlwe = private_membership::rlwe;

MonthlyUseCaseImpl::MonthlyUseCaseImpl(
    PrefService* local_state,
    const std::string& psm_device_active_secret)
    : DeviceActiveUseCase(
          local_state,
          psm_device_active_secret,
          prefs::kDeviceActiveLastKnownMonthlyPingTimestamp,
          private_membership::rlwe::RlweUseCase::CROS_FRESNEL_MONTHLY) {}

MonthlyUseCaseImpl::~MonthlyUseCaseImpl() = default;

std::string MonthlyUseCaseImpl::GenerateUTCWindowIdentifier(
    base::Time ts) const {
  base::Time::Exploded exploded;
  ts.UTCExplode(&exploded);
  return base::StringPrintf("%04d%02d", exploded.year, exploded.month);
}

ImportDataRequest MonthlyUseCaseImpl::GenerateImportRequestBody() {
  std::string psm_id_str = GetPsmIdentifier().value().sensitive_id();
  std::string window_id_str = GetWindowIdentifier().value();

  // Generate Fresnel PSM import request body.
  device_activity::ImportDataRequest import_request;
  import_request.set_window_identifier(window_id_str);
  import_request.set_plaintext_identifier(psm_id_str);
  import_request.set_use_case(GetPsmUseCase());

  // Create fresh |DeviceMetadata| object.
  // Note every dimension added to this proto must be approved by privacy.
  DeviceMetadata* device_metadata = import_request.mutable_device_metadata();
  device_metadata->set_hardware_id(GetFullHardwareClass());
  device_metadata->set_chromeos_version(GetChromeOSVersion());

  return import_request;
}

}  // namespace device_activity
}  // namespace ash
