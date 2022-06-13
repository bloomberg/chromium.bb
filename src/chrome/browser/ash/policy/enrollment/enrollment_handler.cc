// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/enrollment_handler.h"

#include <utility>

#include "ash/components/attestation/attestation_flow.h"
#include "ash/constants/ash_switches.h"
#include "base/base64.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ash/login/enrollment/auto_enrollment_controller.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/policy/active_directory/active_directory_join_delegate.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_store_ash.h"
#include "chrome/browser/ash/policy/core/dm_token_storage.h"
#include "chrome/browser/ash/policy/server_backed_state/server_backed_state_keys_broker.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/enrollment_status.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/authpolicy/authpolicy_client.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/upstart/upstart_client.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/core/common/cloud/signing_service.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace em = enterprise_management;

// An enum for PSM execution result values.
using PsmExecutionResult = em::DeviceRegisterRequest::PsmExecutionResult;

namespace policy {

namespace {

// Retry for InstallAttrs initialization every 500ms.
const int kLockRetryIntervalMs = 500;
// Maximum time to retry InstallAttrs initialization before we give up.
const int kLockRetryTimeoutMs = 10 * 60 * 1000;  // 10 minutes.

em::DeviceRegisterRequest::Flavor EnrollmentModeToRegistrationFlavor(
    EnrollmentConfig::Mode mode) {
  switch (mode) {
    case EnrollmentConfig::MODE_NONE:
    case EnrollmentConfig::MODE_OFFLINE_DEMO:
    case EnrollmentConfig::OBSOLETE_MODE_ENROLLED_ROLLBACK:
      break;
    case EnrollmentConfig::MODE_MANUAL:
      return em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_MANUAL;
    case EnrollmentConfig::MODE_MANUAL_REENROLLMENT:
      return em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_MANUAL_RENEW;
    case EnrollmentConfig::MODE_LOCAL_FORCED:
      return em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_LOCAL_FORCED;
    case EnrollmentConfig::MODE_LOCAL_ADVERTISED:
      return em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_LOCAL_ADVERTISED;
    case EnrollmentConfig::MODE_SERVER_FORCED:
      return em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_SERVER_FORCED;
    case EnrollmentConfig::MODE_SERVER_ADVERTISED:
      return em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_SERVER_ADVERTISED;
    case EnrollmentConfig::MODE_RECOVERY:
      return em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_RECOVERY;
    case EnrollmentConfig::MODE_ATTESTATION:
      return em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_ATTESTATION;
    case EnrollmentConfig::MODE_ATTESTATION_LOCAL_FORCED:
      return em::DeviceRegisterRequest::
          FLAVOR_ENROLLMENT_ATTESTATION_LOCAL_FORCED;
    case EnrollmentConfig::MODE_ATTESTATION_SERVER_FORCED:
      return em::DeviceRegisterRequest::
          FLAVOR_ENROLLMENT_ATTESTATION_SERVER_FORCED;
    case EnrollmentConfig::MODE_ATTESTATION_MANUAL_FALLBACK:
      return em::DeviceRegisterRequest::
          FLAVOR_ENROLLMENT_ATTESTATION_MANUAL_FALLBACK;
    case EnrollmentConfig::MODE_INITIAL_SERVER_FORCED:
      return em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_INITIAL_SERVER_FORCED;
    case EnrollmentConfig::MODE_ATTESTATION_INITIAL_SERVER_FORCED:
      return em::DeviceRegisterRequest::
          FLAVOR_ENROLLMENT_ATTESTATION_INITIAL_SERVER_FORCED;
    case EnrollmentConfig::MODE_ATTESTATION_INITIAL_MANUAL_FALLBACK:
      return em::DeviceRegisterRequest::
          FLAVOR_ENROLLMENT_ATTESTATION_INITIAL_MANUAL_FALLBACK;
  }

  NOTREACHED() << "Bad enrollment mode: " << mode;
  return em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_MANUAL;
}

// Returns whether block_devmode is set.
bool GetBlockdevmodeFromPolicy(
    const enterprise_management::PolicyFetchResponse* policy) {
  DCHECK(policy);
  em::PolicyData policy_data;
  if (!policy_data.ParseFromString(policy->policy_data())) {
    LOG(ERROR) << "Failed to parse policy data";
    return false;
  }

  em::ChromeDeviceSettingsProto payload;
  if (!payload.ParseFromString(policy_data.policy_value())) {
    LOG(ERROR) << "Failed to parse policy value";
    return false;
  }

  bool block_devmode = false;
  if (payload.has_system_settings()) {
    const em::SystemSettingsProto& container = payload.system_settings();
    if (container.has_block_devmode()) {
      block_devmode = container.block_devmode();
    }
  }

  // TODO(crbug.com/1271134): Logging as "WARNING" to make sure it's preserved
  // in the logs.
  LOG(WARNING) << (block_devmode ? "Blocking" : "Allowing")
               << " dev mode by device policy";

  return block_devmode;
}

// A utility funciton of base::ReadFileToString which returns an optional
// string.
// TODO(mukai): move this to base/files.
absl::optional<std::string> ReadFileToOptionalString(
    const base::FilePath& file_path) {
  std::string content;
  absl::optional<std::string> result;
  if (base::ReadFileToString(file_path, &content))
    result = std::move(content);
  return result;
}

// Returns the PSM protocol execution result if prefs::kEnrollmentPsmResult is
// set, and its value is within the
// em::DeviceRegisterRequest::PsmExecutionResult enum range. Otherwise,
// absl::nullopt.
absl::optional<PsmExecutionResult> GetPsmExecutionResult(
    const PrefService& local_state) {
  const PrefService::Preference* has_psm_execution_result_pref =
      local_state.FindPreference(prefs::kEnrollmentPsmResult);

  if (!has_psm_execution_result_pref ||
      has_psm_execution_result_pref->IsDefaultValue() ||
      !has_psm_execution_result_pref->GetValue()->is_int()) {
    return absl::nullopt;
  }

  int psm_execution_result =
      has_psm_execution_result_pref->GetValue()->GetInt();

  // Check if the psm_execution_result is a valid value of
  // em::DeviceRegisterRequest::PsmExecutionResult enum.
  if (!em::DeviceRegisterRequest::PsmExecutionResult_IsValid(
          psm_execution_result))
    return absl::nullopt;

  // Cast the psm_execution_result integer value to its corresponding enum
  // entry.
  return static_cast<PsmExecutionResult>(psm_execution_result);
}

// Returns the PSM determination timestamp in ms if
// prefs::kEnrollmentPsmDeterminationTime is set. Otherwise, absl::nullopt.
absl::optional<int64_t> GetPsmDeterminationTimestamp(
    const PrefService& local_state) {
  const PrefService::Preference* has_psm_determination_timestamp_pref =
      local_state.FindPreference(prefs::kEnrollmentPsmDeterminationTime);

  if (!has_psm_determination_timestamp_pref ||
      has_psm_determination_timestamp_pref->IsDefaultValue()) {
    return absl::nullopt;
  }

  const base::Time psm_determination_timestamp =
      local_state.GetTime(prefs::kEnrollmentPsmDeterminationTime);

  // The PSM determination timestamp should exist at this stage. Because
  // we already checked the existence of the pref with non-default value.
  DCHECK(!psm_determination_timestamp.is_null());

  return psm_determination_timestamp.ToJavaTime();
}

// Returns binary config which is encrypted by a password that the joining user
// has to enter.
std::string GetActiveDirectoryDomainJoinConfig(
    const base::DictionaryValue* config) {
  if (!config)
    return std::string();
  const base::Value* base64_value = config->FindKeyOfType(
      "active_directory_domain_join_config", base::Value::Type::STRING);
  if (!base64_value)
    return std::string();
  std::string result;
  if (!base::Base64Decode(base64_value->GetString(), &result)) {
    LOG(ERROR) << "Active Directory config is not base64";
    return std::string();
  }
  return result;
}

}  // namespace

EnrollmentHandler::EnrollmentHandler(
    DeviceCloudPolicyStoreAsh* store,
    chromeos::InstallAttributes* install_attributes,
    ServerBackedStateKeysBroker* state_keys_broker,
    ash::attestation::AttestationFlow* attestation_flow,
    std::unique_ptr<SigningService> signing_service,
    std::unique_ptr<CloudPolicyClient> client,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    ActiveDirectoryJoinDelegate* ad_join_delegate,
    const EnrollmentConfig& enrollment_config,
    DMAuth dm_auth,
    const std::string& client_id,
    const std::string& requisition,
    const std::string& sub_organization,
    EnrollmentCallback completion_callback)
    : store_(store),
      install_attributes_(install_attributes),
      state_keys_broker_(state_keys_broker),
      attestation_flow_(attestation_flow),
      signing_service_(std::move(signing_service)),
      client_(std::move(client)),
      background_task_runner_(background_task_runner),
      ad_join_delegate_(ad_join_delegate),
      enrollment_config_(enrollment_config),
      client_id_(client_id),
      sub_organization_(sub_organization),
      completion_callback_(std::move(completion_callback)),
      enrollment_step_(STEP_PENDING) {
  dm_auth_ = std::move(dm_auth);
  CHECK(!client_->is_registered());
  CHECK_EQ(DM_STATUS_SUCCESS, client_->status());
  if (enrollment_config_.is_mode_attestation() ||
      enrollment_config.mode == EnrollmentConfig::MODE_OFFLINE_DEMO) {
    CHECK(dm_auth_.empty());
  } else {
    CHECK(!dm_auth_.empty());
  }
  CHECK_NE(enrollment_config.mode == EnrollmentConfig::MODE_OFFLINE_DEMO,
           enrollment_config.offline_policy_path.empty());
  CHECK(enrollment_config_.auth_mechanism !=
            EnrollmentConfig::AUTH_MECHANISM_ATTESTATION ||
        attestation_flow_);
  if (enrollment_config.mode != EnrollmentConfig::MODE_OFFLINE_DEMO) {
    register_params_ =
        std::make_unique<CloudPolicyClient::RegistrationParameters>(
            em::DeviceRegisterRequest::DEVICE,
            EnrollmentModeToRegistrationFlavor(enrollment_config.mode));
    register_params_->SetPsmExecutionResult(
        GetPsmExecutionResult(*g_browser_process->local_state()));
    register_params_->SetPsmDeterminationTimestamp(
        GetPsmDeterminationTimestamp(*g_browser_process->local_state()));

    register_params_->requisition = requisition;
  }

  store_->AddObserver(this);
  client_->AddObserver(this);
  client_->AddPolicyTypeToFetch(dm_protocol::kChromeDevicePolicyType,
                                std::string());
}

EnrollmentHandler::~EnrollmentHandler() {
  Stop();
  store_->RemoveObserver(this);
}

void EnrollmentHandler::StartEnrollment() {
  CHECK_EQ(STEP_PENDING, enrollment_step_);

  if (enrollment_config_.skip_state_keys_request()) {
    // TODO(crbug.com/1271134): Logging as "WARNING" to make sure it's preserved
    // in the logs.
    LOG(WARNING) << "Skipping state keys request.";
    SetStep(STEP_LOADING_STORE);
    StartRegistration();
    return;
  }

  SetStep(STEP_STATE_KEYS);

  if (client_->machine_id().empty()) {
    LOG(ERROR) << "Machine id empty.";
    ReportResult(EnrollmentStatus::ForStatus(
        EnrollmentStatus::NO_MACHINE_IDENTIFICATION));
    return;
  }
  if (client_->machine_model().empty()) {
    LOG(ERROR) << "Machine model empty.";
    ReportResult(EnrollmentStatus::ForStatus(
        EnrollmentStatus::NO_MACHINE_IDENTIFICATION));
    return;
  }

  // Currently reven devices don't support sever-backed state keys, but they
  // also don't support FRE/AutoRE so don't block enrollment on the
  // availablility of state keys.
  // TODO(b/208705225): Remove this special case when reven supports state keys.
  if (ash::switches::IsRevenBranding()) {
    LOG(WARNING) << "Skipping state keys.";
    HandleStateKeysResult({});
    return;
  }
  LOG(WARNING) << "Requesting state keys.";
  state_keys_broker_->RequestStateKeys(
      base::BindOnce(&EnrollmentHandler::HandleStateKeysResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

std::unique_ptr<CloudPolicyClient> EnrollmentHandler::ReleaseClient() {
  Stop();
  return std::move(client_);
}

void EnrollmentHandler::OnPolicyFetched(CloudPolicyClient* client) {
  DCHECK_EQ(client_.get(), client);
  CHECK_EQ(STEP_POLICY_FETCH, enrollment_step_);
  SetStep(STEP_VALIDATION);

  // Validate the policy.
  const em::PolicyFetchResponse* policy = client_->GetPolicyFor(
      dm_protocol::kChromeDevicePolicyType, std::string());
  if (!policy) {
    ReportResult(
        EnrollmentStatus::ForFetchError(DM_STATUS_RESPONSE_DECODING_ERROR));
    return;
  }

  // If this is re-enrollment, make sure that the new policy matches the
  // previously-enrolled domain.  (Currently only implemented for cloud
  // management.)
  std::string domain;
  if (install_attributes_->IsCloudManaged())
    domain = install_attributes_->GetDomain();

  auto validator = CreateValidator(
      std::make_unique<em::PolicyFetchResponse>(*policy), domain);

  if (install_attributes_->IsCloudManaged())
    validator->ValidateDomain(domain);
  validator->ValidateDMToken(client->dm_token(),
                             CloudPolicyValidatorBase::DM_TOKEN_REQUIRED);
  DeviceCloudPolicyValidator::StartValidation(
      std::move(validator),
      base::BindOnce(&EnrollmentHandler::HandlePolicyValidationResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EnrollmentHandler::OnRegistrationStateChanged(CloudPolicyClient* client) {
  DCHECK_EQ(client_.get(), client);

  if (enrollment_step_ != STEP_REGISTRATION || !client_->is_registered()) {
    LOG(FATAL) << "Registration state changed to " << client_->is_registered()
               << " in step " << enrollment_step_ << ".";
    return;
  }

  device_mode_ = client_->device_mode();
  switch (device_mode_) {
    case DEVICE_MODE_ENTERPRISE:
    case DEVICE_MODE_DEMO:
      // Do nothing.
      break;
    case DEVICE_MODE_ENTERPRISE_AD:
      chromeos::UpstartClient::Get()->StartAuthPolicyService();
      break;
    default:
      LOG(ERROR) << "Supplied device mode is not supported:" << device_mode_;
      ReportResult(
          EnrollmentStatus::ForStatus(EnrollmentStatus::REGISTRATION_BAD_MODE));
      return;
  }
  // Only use DMToken from now on.
  dm_auth_ = DMAuth::FromDMToken(client_->dm_token());
  SetStep(STEP_POLICY_FETCH);
  client_->FetchPolicy();
}

void EnrollmentHandler::OnClientError(CloudPolicyClient* client) {
  DCHECK_EQ(client_.get(), client);

  if (enrollment_step_ == STEP_ROBOT_AUTH_FETCH ||
      enrollment_step_ == STEP_STORE_ROBOT_AUTH) {
    // Handled in OnDeviceAccountTokenError().
    return;
  }

  if (enrollment_step_ < STEP_POLICY_FETCH) {
    ReportResult(EnrollmentStatus::ForRegistrationError(client_->status()));
  } else {
    ReportResult(EnrollmentStatus::ForFetchError(client_->status()));
  }
}

void EnrollmentHandler::OnStoreLoaded(CloudPolicyStore* store) {
  DCHECK_EQ(store_, store);

  if (enrollment_step_ == STEP_LOADING_STORE) {
    // If the |store_| wasn't initialized when StartEnrollment() was called,
    // then StartRegistration() bails silently. This gets registration rolling
    // again after the store finishes loading.
    StartRegistration();
  } else if (enrollment_step_ == STEP_STORE_POLICY) {
    ReportResult(EnrollmentStatus::ForStatus(EnrollmentStatus::SUCCESS));
  }
}

void EnrollmentHandler::OnStoreError(CloudPolicyStore* store) {
  DCHECK_EQ(store_, store);

  if (enrollment_step_ < STEP_STORE_POLICY) {
    // At those steps it is not expected to have any error notifications from
    // |store_| since they are not initiated by enrollment handler and stored
    // policies are not in a consistent state (e.g. a late response from
    // |store_| loaded at boot). So the notification is ignored.
    // Notifications are only expected starting STEP_STORE_POLICY
    // when OnDeviceAccountTokenStored() is called.
    LOG(WARNING) << "Unexpected store error with status: " << store->status()
                 << " at step: " << enrollment_step_;
    return;
  }

  LOG(ERROR) << "Error in device policy store.";
  ReportResult(EnrollmentStatus::ForStoreError(store_->status(),
                                               store_->validation_status()));
}

void EnrollmentHandler::HandleStateKeysResult(
    const std::vector<std::string>& state_keys) {
  DCHECK_EQ(STEP_STATE_KEYS, enrollment_step_);

  // Make sure state keys are available if forced re-enrollment is on.
  if (ash::AutoEnrollmentController::IsFREEnabled()) {
    client_->SetStateKeysToUpload(state_keys);
    register_params_->current_state_key =
        state_keys_broker_->current_state_key();
    if (state_keys.empty() || register_params_->current_state_key.empty()) {
      LOG(ERROR) << "State keys empty.";
      ReportResult(
          EnrollmentStatus::ForStatus(EnrollmentStatus::NO_STATE_KEYS));
      return;
    }
  }

  // TODO(crbug.com/1271134): Logging as "WARNING" to make sure it's preserved
  // in the logs.
  LOG(WARNING) << "State keys generated, success=" << !state_keys.empty();
  SetStep(STEP_LOADING_STORE);
  StartRegistration();
}

void EnrollmentHandler::StartRegistration() {
  DCHECK_EQ(STEP_LOADING_STORE, enrollment_step_);
  if (!store_->is_initialized()) {
    // Do nothing. StartRegistration() will be called again from OnStoreLoaded()
    // after the CloudPolicyStore has initialized.
    return;
  }

  // TODO(crbug.com/1271134): Logging as "WARNING" to make sure it's preserved
  // in the logs.
  LOG(WARNING) << "Start registration, config mode = "
               << enrollment_config_.mode;

  SetStep(STEP_REGISTRATION);
  if (enrollment_config_.is_mode_attestation()) {
    StartAttestationBasedEnrollmentFlow();
  } else if (enrollment_config_.mode == EnrollmentConfig::MODE_OFFLINE_DEMO) {
    StartOfflineDemoEnrollmentFlow();
  } else {
    client_->Register(*register_params_, client_id_, dm_auth_.oauth_token());
  }
}

void EnrollmentHandler::StartAttestationBasedEnrollmentFlow() {
  ash::attestation::AttestationFlow::CertificateCallback callback =
      base::BindOnce(&EnrollmentHandler::HandleRegistrationCertificateResult,
                     weak_ptr_factory_.GetWeakPtr());
  attestation_flow_->GetCertificate(
      chromeos::attestation::PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE,
      EmptyAccountId(), std::string() /* request_origin */,
      false /* force_new_key */, std::string(), /* key_name */
      std::move(callback));
}

void EnrollmentHandler::HandleRegistrationCertificateResult(
    chromeos::attestation::AttestationStatus status,
    const std::string& pem_certificate_chain) {
  if (status != chromeos::attestation::ATTESTATION_SUCCESS) {
    ReportResult(EnrollmentStatus::ForStatus(
        EnrollmentStatus::REGISTRATION_CERT_FETCH_FAILED));
    return;
  }
  client_->RegisterWithCertificate(*register_params_, client_id_,
                                   dm_auth_.Clone(), pem_certificate_chain,
                                   sub_organization_, signing_service_.get());
}

void EnrollmentHandler::StartOfflineDemoEnrollmentFlow() {
  DCHECK(!enrollment_config_.offline_policy_path.empty());

  device_mode_ = policy::DeviceMode::DEVICE_MODE_DEMO;
  domain_ = enrollment_config_.management_domain;
  skip_robot_auth_ = true;
  SetStep(STEP_POLICY_FETCH);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&ReadFileToOptionalString,
                     enrollment_config_.offline_policy_path),
      base::BindOnce(&EnrollmentHandler::OnOfflinePolicyBlobLoaded,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EnrollmentHandler::OnOfflinePolicyBlobLoaded(
    absl::optional<std::string> blob) {
  DCHECK_EQ(EnrollmentConfig::MODE_OFFLINE_DEMO, enrollment_config_.mode);
  DCHECK_EQ(STEP_POLICY_FETCH, enrollment_step_);

  if (!blob.has_value()) {
    ReportResult(EnrollmentStatus::ForStatus(
        EnrollmentStatus::OFFLINE_POLICY_LOAD_FAILED));
    return;
  }

  SetStep(STEP_VALIDATION);

  // Validate the policy.
  auto policy = std::make_unique<em::PolicyFetchResponse>();
  if (!policy->ParseFromString(blob.value())) {
    ReportResult(EnrollmentStatus::ForStatus(
        EnrollmentStatus::OFFLINE_POLICY_DECODING_FAILED));
    return;
  }

  // Validate the device policy for the offline demo mode.
  auto validator = CreateValidator(std::move(policy), domain_);
  validator->ValidateDomain(domain_);
  DeviceCloudPolicyValidator::StartValidation(
      std::move(validator),
      base::BindOnce(&EnrollmentHandler::OnOfflinePolicyValidated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EnrollmentHandler::OnOfflinePolicyValidated(
    DeviceCloudPolicyValidator* validator) {
  DCHECK_EQ(enrollment_config_.mode, EnrollmentConfig::MODE_OFFLINE_DEMO);
  DCHECK_EQ(STEP_VALIDATION, enrollment_step_);

  if (!validator->success()) {
    ReportResult(EnrollmentStatus::ForValidationError(validator->status()));
    return;
  }

  // Don't use the device ID within the validated policy -- it's common among
  // all of the offline-enrolled devices.
  device_id_ = base::GenerateGUID();
  policy_ = std::move(validator->policy());

  // The steps for OAuth2 token fetching is skipped for the OFFLINE_DEMO_MODE.
  SetStep(STEP_SET_FWMP_DATA);
  SetFirmwareManagementParametersData();
}

std::unique_ptr<DeviceCloudPolicyValidator> EnrollmentHandler::CreateValidator(
    std::unique_ptr<enterprise_management::PolicyFetchResponse> policy,
    const std::string& domain) {
  auto validator = std::make_unique<DeviceCloudPolicyValidator>(
      std::move(policy), background_task_runner_);

  validator->ValidateTimestamp(base::Time(),
                               CloudPolicyValidatorBase::TIMESTAMP_VALIDATED);
  validator->ValidatePolicyType(dm_protocol::kChromeDevicePolicyType);
  validator->ValidatePayload();
  // If |domain| is empty here, the policy validation code will just use the
  // domain from the username field in the policy itself to do key validation.
  // TODO(mnissler): Plumb the enrolling user's username into this object so we
  // can validate the username on the resulting policy, and use the domain from
  // that username to validate the key below (http://crbug.com/343074).
  validator->ValidateInitialKey(domain);
  return validator;
}

void EnrollmentHandler::HandlePolicyValidationResult(
    DeviceCloudPolicyValidator* validator) {
  DCHECK_EQ(STEP_VALIDATION, enrollment_step_);
  if (!validator->success()) {
    ReportResult(EnrollmentStatus::ForValidationError(validator->status()));
    return;
  }
  std::string username = validator->policy_data()->username();
  device_id_ = validator->policy_data()->device_id();
  policy_ = std::move(validator->policy());
  if (device_mode_ == DEVICE_MODE_ENTERPRISE_AD) {
    // Don't use robot account for the Active Directory managed devices.
    skip_robot_auth_ = true;
    SetStep(STEP_AD_DOMAIN_JOIN);
    StartJoinAdDomain();
  } else {
    domain_ = gaia::ExtractDomainName(gaia::CanonicalizeEmail(username));
    SetStep(STEP_ROBOT_AUTH_FETCH);
    device_account_initializer_ =
        std::make_unique<DeviceAccountInitializer>(client_.get(), this);
    device_account_initializer_->FetchToken();
  }
}

void EnrollmentHandler::OnDeviceAccountTokenFetched(bool empty_token) {
  CHECK_EQ(STEP_ROBOT_AUTH_FETCH, enrollment_step_);
  skip_robot_auth_ = empty_token;
  SetStep(STEP_AD_DOMAIN_JOIN);
  StartJoinAdDomain();
}

void EnrollmentHandler::OnDeviceAccountTokenError(EnrollmentStatus status) {
  CHECK(enrollment_step_ == STEP_ROBOT_AUTH_FETCH ||
        enrollment_step_ == STEP_STORE_ROBOT_AUTH);
  ReportResult(status);
}

void EnrollmentHandler::OnDeviceAccountClientError(
    DeviceManagementStatus status) {
  // Do nothing, it would be handled in OnClientError.
}

enterprise_management::DeviceServiceApiAccessRequest::DeviceType
EnrollmentHandler::GetRobotAuthCodeDeviceType() {
  return em::DeviceServiceApiAccessRequest::CHROME_OS;
}

std::set<std::string> EnrollmentHandler::GetRobotOAuthScopes() {
  return {GaiaConstants::kAnyApiOAuth2Scope};
}

scoped_refptr<network::SharedURLLoaderFactory>
EnrollmentHandler::GetURLLoaderFactory() {
  return g_browser_process->shared_url_loader_factory();
}

void EnrollmentHandler::SetFirmwareManagementParametersData() {
  DCHECK_EQ(STEP_SET_FWMP_DATA, enrollment_step_);

  // In case of reenrollment, the device has the TPM locked and nothing has to
  // change in install attributes. No need to update firmware parameters in this
  // case.
  if (install_attributes_->IsDeviceLocked()) {
    SetStep(STEP_LOCK_DEVICE);
    StartLockDevice();
    return;
  }

  install_attributes_->SetBlockDevmodeInTpm(
      GetBlockdevmodeFromPolicy(policy_.get()),
      base::BindOnce(&EnrollmentHandler::OnFirmwareManagementParametersDataSet,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EnrollmentHandler::OnFirmwareManagementParametersDataSet(
    absl::optional<user_data_auth::SetFirmwareManagementParametersReply>
        reply) {
  DCHECK_EQ(STEP_SET_FWMP_DATA, enrollment_step_);
  if (!reply.has_value()) {
    LOG(ERROR) << "Failed to update firmware management parameters in TPM due "
                  "to DBus error.";
  } else if (reply->error() !=
             user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET) {
    LOG(ERROR) << "Failed to update firmware management parameters in TPM, "
                  "error code: "
               << static_cast<int>(reply->error());
  }

  SetStep(STEP_LOCK_DEVICE);
  StartLockDevice();
}

void EnrollmentHandler::StartJoinAdDomain() {
  DCHECK_EQ(STEP_AD_DOMAIN_JOIN, enrollment_step_);
  if (device_mode_ != DEVICE_MODE_ENTERPRISE_AD) {
    SetStep(STEP_SET_FWMP_DATA);
    SetFirmwareManagementParametersData();
    return;
  }
  DCHECK(ad_join_delegate_);
  ad_join_delegate_->JoinDomain(
      client_->dm_token(),
      GetActiveDirectoryDomainJoinConfig(client_->configuration_seed()),
      base::BindOnce(&EnrollmentHandler::OnAdDomainJoined,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EnrollmentHandler::OnAdDomainJoined(const std::string& realm) {
  DCHECK_EQ(STEP_AD_DOMAIN_JOIN, enrollment_step_);
  CHECK(!realm.empty());
  realm_ = realm;
  SetStep(STEP_SET_FWMP_DATA);
  SetFirmwareManagementParametersData();
}

void EnrollmentHandler::StartLockDevice() {
  DCHECK_EQ(STEP_LOCK_DEVICE, enrollment_step_);
  // Since this method is also called directly.
  weak_ptr_factory_.InvalidateWeakPtrs();

  install_attributes_->LockDevice(
      device_mode_, domain_, realm_, device_id_,
      base::BindOnce(&EnrollmentHandler::HandleLockDeviceResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EnrollmentHandler::HandleDMTokenStoreResult(bool success) {
  CHECK_EQ(STEP_STORE_TOKEN, enrollment_step_);
  if (!success) {
    ReportResult(
        EnrollmentStatus::ForStatus(EnrollmentStatus::DM_TOKEN_STORE_FAILED));
    return;
  }

  StartStoreRobotAuth();
}

void EnrollmentHandler::HandleLockDeviceResult(
    chromeos::InstallAttributes::LockResult lock_result) {
  DCHECK_EQ(STEP_LOCK_DEVICE, enrollment_step_);
  switch (lock_result) {
    case chromeos::InstallAttributes::LOCK_SUCCESS:
      if (device_mode_ == DEVICE_MODE_ENTERPRISE_AD) {
        StartStoreDMToken();
      } else {
        StartStoreRobotAuth();
      }
      break;
    case chromeos::InstallAttributes::LOCK_NOT_READY:
      // We wait up to |kLockRetryTimeoutMs| milliseconds and if it hasn't
      // succeeded by then show an error to the user and stop the enrollment.
      if (lockbox_init_duration_ < kLockRetryTimeoutMs) {
        // InstallAttributes not ready yet, retry later.
        LOG(WARNING) << "Install Attributes not ready yet will retry in "
                     << kLockRetryIntervalMs << "ms.";
        base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(&EnrollmentHandler::StartLockDevice,
                           weak_ptr_factory_.GetWeakPtr()),
            base::Milliseconds(kLockRetryIntervalMs));
        lockbox_init_duration_ += kLockRetryIntervalMs;
      } else {
        HandleLockDeviceResult(chromeos::InstallAttributes::LOCK_TIMEOUT);
      }
      break;
    case chromeos::InstallAttributes::LOCK_TIMEOUT:
    case chromeos::InstallAttributes::LOCK_BACKEND_INVALID:
    case chromeos::InstallAttributes::LOCK_ALREADY_LOCKED:
    case chromeos::InstallAttributes::LOCK_SET_ERROR:
    case chromeos::InstallAttributes::LOCK_FINALIZE_ERROR:
    case chromeos::InstallAttributes::LOCK_READBACK_ERROR:
    case chromeos::InstallAttributes::LOCK_WRONG_DOMAIN:
    case chromeos::InstallAttributes::LOCK_WRONG_MODE:
      ReportResult(EnrollmentStatus::ForLockError(lock_result));
      break;
  }
}

void EnrollmentHandler::StartStoreDMToken() {
  DCHECK(device_mode_ == DEVICE_MODE_ENTERPRISE_AD);
  SetStep(STEP_STORE_TOKEN);
  dm_token_storage_ = std::make_unique<policy::DMTokenStorage>(
      g_browser_process->local_state());
  dm_token_storage_->StoreDMToken(
      client_->dm_token(),
      base::BindOnce(&EnrollmentHandler::HandleDMTokenStoreResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EnrollmentHandler::StartStoreRobotAuth() {
  SetStep(STEP_STORE_ROBOT_AUTH);

  // Don't store the token if robot auth was skipped.
  if (skip_robot_auth_) {
    OnDeviceAccountTokenStored();
    return;
  }
  device_account_initializer_->StoreToken();
}

void EnrollmentHandler::OnDeviceAccountTokenStored() {
  DCHECK_EQ(STEP_STORE_ROBOT_AUTH, enrollment_step_);
  SetStep(STEP_STORE_POLICY);
  if (device_mode_ == policy::DEVICE_MODE_ENTERPRISE_AD) {
    CHECK(install_attributes_->IsActiveDirectoryManaged());
    // Update device settings so that in case of Active Directory unsigned
    // policy is accepted.
    ash::DeviceSettingsService::Get()->SetDeviceMode(
        install_attributes_->GetMode());
    chromeos::AuthPolicyClient::Get()->RefreshDevicePolicy(
        base::BindOnce(&EnrollmentHandler::HandleActiveDirectoryPolicyRefreshed,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    store_->InstallInitialPolicy(*policy_);
  }
}

void EnrollmentHandler::HandleActiveDirectoryPolicyRefreshed(
    authpolicy::ErrorType error) {
  DCHECK_EQ(STEP_STORE_POLICY, enrollment_step_);

  if (error != authpolicy::ERROR_NONE) {
    LOG(ERROR) << "Failed to load Active Directory policy.";
    ReportResult(EnrollmentStatus::ForStatus(
        EnrollmentStatus::ACTIVE_DIRECTORY_POLICY_FETCH_FAILED));
    return;
  }

  // After that, the enrollment flow continues in one of the OnStore* observers.
  store_->Load();
}

void EnrollmentHandler::Stop() {
  if (client_.get())
    client_->RemoveObserver(this);
  if (device_account_initializer_.get()) {
    device_account_initializer_->Stop();
    device_account_initializer_.reset();
  }
  SetStep(STEP_FINISHED);
  weak_ptr_factory_.InvalidateWeakPtrs();
  completion_callback_.Reset();
}

void EnrollmentHandler::ReportResult(EnrollmentStatus status) {
  EnrollmentCallback callback = std::move(completion_callback_);
  Stop();

  if (status.status() != EnrollmentStatus::SUCCESS) {
    LOG(WARNING) << "Enrollment failed: " << status.status()
                 << ", client: " << status.client_status()
                 << ", validation: " << status.validation_status()
                 << ", store: " << status.store_status()
                 << ", lock: " << status.lock_status();
  }

  if (!callback.is_null())
    std::move(callback).Run(status);
}

void EnrollmentHandler::SetStep(EnrollmentStep step) {
  DCHECK_LE(enrollment_step_, step);

  // TODO(crbug.com/1271134): Logging as "WARNING" to make sure it's preserved
  // in the logs.
  LOG(WARNING) << "Step: " << step;

  enrollment_step_ = step;
}

}  // namespace policy
