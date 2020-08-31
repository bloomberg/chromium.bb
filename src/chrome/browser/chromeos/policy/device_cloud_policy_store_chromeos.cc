// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_cloud_policy_store_chromeos.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/policy/device_policy_decoder_chromeos.h"
#include "chrome/browser/chromeos/policy/value_validation/onc_device_policy_value_validator.h"
#include "chromeos/tpm/install_attributes.h"
#include "components/ownership/owner_key_util.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace policy {

namespace {

const char kDMTokenCheckHistogram[] = "Enterprise.EnrolledPolicyHasDMToken";

void RecordDeviceIdValidityMetric(
    const std::string& histogram_name,
    const em::PolicyData& policy_data,
    const chromeos::InstallAttributes& install_attributes) {
  PolicyDeviceIdValidity device_id_validity = PolicyDeviceIdValidity::kMaxValue;
  if (install_attributes.GetDeviceId().empty())
    device_id_validity = PolicyDeviceIdValidity::kActualIdUnknown;
  else if (policy_data.device_id().empty())
    device_id_validity = PolicyDeviceIdValidity::kMissing;
  else if (policy_data.device_id() != install_attributes.GetDeviceId())
    device_id_validity = PolicyDeviceIdValidity::kInvalid;
  else
    device_id_validity = PolicyDeviceIdValidity::kValid;
  base::UmaHistogramEnumeration(histogram_name, device_id_validity);
}

}  // namespace

DeviceCloudPolicyStoreChromeOS::DeviceCloudPolicyStoreChromeOS(
    chromeos::DeviceSettingsService* device_settings_service,
    chromeos::InstallAttributes* install_attributes,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : device_settings_service_(device_settings_service),
      install_attributes_(install_attributes),
      background_task_runner_(background_task_runner) {
  device_settings_service_->AddObserver(this);
  device_settings_service_->SetDeviceMode(install_attributes_->GetMode());
}

DeviceCloudPolicyStoreChromeOS::~DeviceCloudPolicyStoreChromeOS() {
  if (device_settings_service_)
    device_settings_service_->RemoveObserver(this);
}

void DeviceCloudPolicyStoreChromeOS::Store(
    const em::PolicyFetchResponse& policy) {
  // The policy and the public key must have already been loaded by the device
  // settings service.
  DCHECK(is_initialized());

  // Cancel all pending requests.
  weak_factory_.InvalidateWeakPtrs();

  scoped_refptr<ownership::PublicKey> public_key(
      device_settings_service_->GetPublicKey());
  if (!install_attributes_->IsCloudManaged() ||
      !device_settings_service_->policy_data() || !public_key.get() ||
      !public_key->is_loaded()) {
    LOG(ERROR) << "Policy store failed, is_cloud_managed: "
               << install_attributes_->IsCloudManaged() << ", policy_data: "
               << (device_settings_service_->policy_data() != nullptr)
               << ", public_key: " << (public_key.get() != nullptr)
               << ", public_key_is_loaded: "
               << (public_key.get() ? public_key->is_loaded() : false);
    status_ = STATUS_BAD_STATE;
    NotifyStoreError();
    return;
  }

  std::unique_ptr<DeviceCloudPolicyValidator> validator(
      CreateValidator(policy));
  validator->ValidateSignatureAllowingRotation(
      public_key->as_string(), install_attributes_->GetDomain());
  validator->ValidateAgainstCurrentPolicy(
      device_settings_service_->policy_data(),
      CloudPolicyValidatorBase::TIMESTAMP_VALIDATED,
      CloudPolicyValidatorBase::DM_TOKEN_REQUIRED,
      CloudPolicyValidatorBase::DEVICE_ID_REQUIRED);
  DeviceCloudPolicyValidator::StartValidation(
      std::move(validator),
      base::BindOnce(&DeviceCloudPolicyStoreChromeOS::OnPolicyToStoreValidated,
                     weak_factory_.GetWeakPtr(),
                     /*is_initial=*/false));
}

void DeviceCloudPolicyStoreChromeOS::Load() {
  // Cancel all pending requests.
  weak_factory_.InvalidateWeakPtrs();

  device_settings_service_->Load();
}

void DeviceCloudPolicyStoreChromeOS::InstallInitialPolicy(
    const em::PolicyFetchResponse& policy) {
  // Cancel all pending requests.
  weak_factory_.InvalidateWeakPtrs();

  if (!install_attributes_->IsCloudManaged()) {
    status_ = STATUS_BAD_STATE;
    NotifyStoreError();
    return;
  }

  std::unique_ptr<DeviceCloudPolicyValidator> validator(
      CreateValidator(policy));
  validator->ValidateInitialKey(install_attributes_->GetDomain());
  DeviceCloudPolicyValidator::StartValidation(
      std::move(validator),
      base::BindOnce(&DeviceCloudPolicyStoreChromeOS::OnPolicyToStoreValidated,
                     weak_factory_.GetWeakPtr(),
                     /*is_initial=*/true));
}

void DeviceCloudPolicyStoreChromeOS::DeviceSettingsUpdated() {
  if (!weak_factory_.HasWeakPtrs())
    UpdateFromService();
}

void DeviceCloudPolicyStoreChromeOS::OnDeviceSettingsServiceShutdown() {
  device_settings_service_->RemoveObserver(this);
  device_settings_service_ = nullptr;
}

std::unique_ptr<DeviceCloudPolicyValidator>
DeviceCloudPolicyStoreChromeOS::CreateValidator(
    const em::PolicyFetchResponse& policy) {
  auto validator = std::make_unique<DeviceCloudPolicyValidator>(
      std::make_unique<em::PolicyFetchResponse>(policy),
      background_task_runner_);
  validator->ValidateDomain(install_attributes_->GetDomain());
  validator->ValidatePolicyType(dm_protocol::kChromeDevicePolicyType);
  validator->ValidatePayload();
  validator->ValidateValues(std::make_unique<ONCDevicePolicyValueValidator>());
  return validator;
}

void DeviceCloudPolicyStoreChromeOS::OnPolicyToStoreValidated(
    bool is_initial,
    DeviceCloudPolicyValidator* validator) {
  validation_result_ = validator->GetValidationResult();
  if (!validator->success()) {
    status_ = STATUS_VALIDATION_ERROR;
    NotifyStoreError();
    return;
  }

  RecordDeviceIdValidityMetric(
      is_initial ? "Enterprise.DevicePolicyDeviceIdValidity.InitialStore"
                 : "Enterprise.DevicePolicyDeviceIdValidity.Update",
      *validator->policy_data(), *install_attributes_);

  device_settings_service_->Store(
      std::move(validator->policy()),
      base::Bind(&DeviceCloudPolicyStoreChromeOS::OnPolicyStored,
                 weak_factory_.GetWeakPtr()));
}

void DeviceCloudPolicyStoreChromeOS::OnPolicyStored() {
  UpdateFromService();
}

void DeviceCloudPolicyStoreChromeOS::UpdateFromService() {
  if (!install_attributes_->IsEnterpriseManaged()) {
    status_ = STATUS_BAD_STATE;
    NotifyStoreError();
    return;
  }

  CheckDMToken();
  UpdateStatusFromService();

  const chromeos::DeviceSettingsService::Status service_status =
      device_settings_service_->status();
  if (service_status == chromeos::DeviceSettingsService::STORE_SUCCESS) {
    policy_ = std::make_unique<em::PolicyData>();
    const em::PolicyData* policy_data = device_settings_service_->policy_data();
    if (policy_data)
      policy_->MergeFrom(*policy_data);

    PolicyMap new_policy_map;
    if (is_managed()) {
      DecodeDevicePolicy(*device_settings_service_->device_settings(),
                         external_data_manager(), &new_policy_map);
    }
    policy_map_.Swap(&new_policy_map);

    scoped_refptr<ownership::PublicKey> key =
        device_settings_service_->GetPublicKey();
    policy_signature_public_key_ = key ? key->as_string() : std::string();

    NotifyStoreLoaded();
    return;
  }
  NotifyStoreError();
}

void DeviceCloudPolicyStoreChromeOS::UpdateStatusFromService() {
  switch (device_settings_service_->status()) {
    case chromeos::DeviceSettingsService::STORE_SUCCESS:
      status_ = STATUS_OK;
      return;
    case chromeos::DeviceSettingsService::STORE_KEY_UNAVAILABLE:
      status_ = STATUS_BAD_STATE;
      return;
    case chromeos::DeviceSettingsService::STORE_OPERATION_FAILED:
      status_ = STATUS_STORE_ERROR;
      return;
    case chromeos::DeviceSettingsService::STORE_NO_POLICY:
    case chromeos::DeviceSettingsService::STORE_INVALID_POLICY:
    case chromeos::DeviceSettingsService::STORE_VALIDATION_ERROR:
      status_ = STATUS_LOAD_ERROR;
      return;
  }
  NOTREACHED();
}

void DeviceCloudPolicyStoreChromeOS::CheckDMToken() {
  const chromeos::DeviceSettingsService::Status service_status =
      device_settings_service_->status();
  switch (service_status) {
    case chromeos::DeviceSettingsService::STORE_SUCCESS:
    case chromeos::DeviceSettingsService::STORE_KEY_UNAVAILABLE:
    case chromeos::DeviceSettingsService::STORE_NO_POLICY:
    case chromeos::DeviceSettingsService::STORE_INVALID_POLICY:
    case chromeos::DeviceSettingsService::STORE_VALIDATION_ERROR:
      // Continue with the check below.
      break;
    case chromeos::DeviceSettingsService::STORE_OPERATION_FAILED:
      // Don't check for write errors or transient read errors.
      return;
  }

  if (dm_token_checked_) {
    return;
  }
  dm_token_checked_ = true;

  // PolicyData from Active Directory doesn't contain a DM token.
  if (install_attributes_->IsActiveDirectoryManaged()) {
    return;
  }

  const em::PolicyData* policy_data = device_settings_service_->policy_data();
  if (policy_data && policy_data->has_request_token()) {
    UMA_HISTOGRAM_BOOLEAN(kDMTokenCheckHistogram, true);
  } else {
    UMA_HISTOGRAM_BOOLEAN(kDMTokenCheckHistogram, false);
    LOG(ERROR) << "Device policy read on enrolled device yields "
               << "no DM token! Status: " << service_status << ".";

    // At the time LoginDisplayHostWebUI decides whether enrollment flow is to
    // be started, policy hasn't been read yet.  To work around this, once the
    // need for recovery is detected upon policy load, a flag is stored in prefs
    // which is accessed by LoginDisplayHostWebUI early during (next) boot.
    chromeos::StartupUtils::MarkEnrollmentRecoveryRequired();
  }
}

}  // namespace policy
