// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/cloud_policy_client.h"

#include <utility>

#include "build/build_config.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/guid.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/core/common/cloud/dmserver_job_configurations.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "components/policy/core/common/cloud/signing_service.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace em = enterprise_management;

// The type for variables containing an error from DM Server response.
using CertProvisioningResponseErrorType =
    enterprise_management::ClientCertificateProvisioningResponse::Error;
// The namespace that contains convenient aliases for error values, e.g.
// UNDEFINED, TIMED_OUT, IDENTITY_VERIFICATION_ERROR, CA_ERROR.
using CertProvisioningResponseError =
    enterprise_management::ClientCertificateProvisioningResponse;

namespace policy {

namespace {

// Translates the DeviceRegisterResponse::DeviceMode |mode| to the enum used
// internally to represent different device modes.
DeviceMode TranslateProtobufDeviceMode(
    em::DeviceRegisterResponse::DeviceMode mode) {
  switch (mode) {
    case em::DeviceRegisterResponse::ENTERPRISE:
      return DEVICE_MODE_ENTERPRISE;
    case em::DeviceRegisterResponse::RETAIL_DEPRECATED:
      return DEVICE_MODE_LEGACY_RETAIL_MODE;
    case em::DeviceRegisterResponse::CHROME_AD:
      return DEVICE_MODE_ENTERPRISE_AD;
    case em::DeviceRegisterResponse::DEMO:
      return DEVICE_MODE_DEMO;
  }
  LOG(ERROR) << "Unknown enrollment mode in registration response: " << mode;
  return DEVICE_MODE_NOT_SET;
}

bool IsChromePolicy(const std::string& type) {
  return type == dm_protocol::kChromeDevicePolicyType ||
         type == dm_protocol::kChromeUserPolicyType ||
         type == dm_protocol::kChromeMachineLevelUserCloudPolicyType;
}

em::PolicyValidationReportRequest::ValidationResultType
TranslatePolicyValidationResult(CloudPolicyValidatorBase::Status status) {
  using report = em::PolicyValidationReportRequest;
  using policyValidationStatus = CloudPolicyValidatorBase::Status;
  switch (status) {
    case policyValidationStatus::VALIDATION_OK:
      return report::VALIDATION_RESULT_TYPE_SUCCESS;
    case policyValidationStatus::VALIDATION_BAD_INITIAL_SIGNATURE:
      return report::VALIDATION_RESULT_TYPE_BAD_INITIAL_SIGNATURE;
    case policyValidationStatus::VALIDATION_BAD_SIGNATURE:
      return report::VALIDATION_RESULT_TYPE_BAD_SIGNATURE;
    case policyValidationStatus::VALIDATION_ERROR_CODE_PRESENT:
      return report::VALIDATION_RESULT_TYPE_ERROR_CODE_PRESENT;
    case policyValidationStatus::VALIDATION_PAYLOAD_PARSE_ERROR:
      return report::VALIDATION_RESULT_TYPE_PAYLOAD_PARSE_ERROR;
    case policyValidationStatus::VALIDATION_WRONG_POLICY_TYPE:
      return report::VALIDATION_RESULT_TYPE_WRONG_POLICY_TYPE;
    case policyValidationStatus::VALIDATION_WRONG_SETTINGS_ENTITY_ID:
      return report::VALIDATION_RESULT_TYPE_WRONG_SETTINGS_ENTITY_ID;
    case policyValidationStatus::VALIDATION_BAD_TIMESTAMP:
      return report::VALIDATION_RESULT_TYPE_BAD_TIMESTAMP;
    case policyValidationStatus::VALIDATION_BAD_DM_TOKEN:
      return report::VALIDATION_RESULT_TYPE_BAD_DM_TOKEN;
    case policyValidationStatus::VALIDATION_BAD_DEVICE_ID:
      return report::VALIDATION_RESULT_TYPE_BAD_DEVICE_ID;
    case policyValidationStatus::VALIDATION_BAD_USER:
      return report::VALIDATION_RESULT_TYPE_BAD_USER;
    case policyValidationStatus::VALIDATION_POLICY_PARSE_ERROR:
      return report::VALIDATION_RESULT_TYPE_POLICY_PARSE_ERROR;
    case policyValidationStatus::VALIDATION_BAD_KEY_VERIFICATION_SIGNATURE:
      return report::VALIDATION_RESULT_TYPE_BAD_KEY_VERIFICATION_SIGNATURE;
    case policyValidationStatus::VALIDATION_VALUE_WARNING:
      return report::VALIDATION_RESULT_TYPE_VALUE_WARNING;
    case policyValidationStatus::VALIDATION_VALUE_ERROR:
      return report::VALIDATION_RESULT_TYPE_VALUE_ERROR;
    case policyValidationStatus::VALIDATION_STATUS_SIZE:
      return report::VALIDATION_RESULT_TYPE_ERROR_UNSPECIFIED;
  }
  return report::VALIDATION_RESULT_TYPE_ERROR_UNSPECIFIED;
}

em::PolicyValueValidationIssue::ValueValidationIssueSeverity
TranslatePolicyValidationResultSeverity(
    ValueValidationIssue::Severity severity) {
  using issue = em::PolicyValueValidationIssue;
  switch (severity) {
    case ValueValidationIssue::Severity::kWarning:
      return issue::VALUE_VALIDATION_ISSUE_SEVERITY_WARNING;
    case ValueValidationIssue::Severity::kError:
      return issue::VALUE_VALIDATION_ISSUE_SEVERITY_ERROR;
  }
  NOTREACHED();
  return issue::VALUE_VALIDATION_ISSUE_SEVERITY_UNSPECIFIED;
}

}  // namespace

CloudPolicyClient::RegistrationParameters::RegistrationParameters(
    em::DeviceRegisterRequest::Type registration_type,
    em::DeviceRegisterRequest::Flavor flavor)
    : registration_type(registration_type), flavor(flavor) {}

CloudPolicyClient::RegistrationParameters::~RegistrationParameters() = default;

CloudPolicyClient::Observer::~Observer() {}

CloudPolicyClient::CloudPolicyClient(
    const std::string& machine_id,
    const std::string& machine_model,
    const std::string& brand_code,
    const std::string& attested_device_id,
    const std::string& ethernet_mac_address,
    const std::string& dock_mac_address,
    const std::string& manufacture_date,
    SigningService* signing_service,
    DeviceManagementService* service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    DeviceDMTokenCallback device_dm_token_callback)
    : machine_id_(machine_id),
      machine_model_(machine_model),
      brand_code_(brand_code),
      attested_device_id_(attested_device_id),
      ethernet_mac_address_(ethernet_mac_address),
      dock_mac_address_(dock_mac_address),
      manufacture_date_(manufacture_date),
      signing_service_(signing_service),
      service_(service),  // Can be null for unit tests.
      device_dm_token_callback_(device_dm_token_callback),
      url_loader_factory_(url_loader_factory) {}

CloudPolicyClient::CloudPolicyClient(
    DeviceManagementService* service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    DeviceDMTokenCallback device_dm_token_callback)
    : service_(service),  // Can be null for unit tests.
      device_dm_token_callback_(device_dm_token_callback),
      url_loader_factory_(url_loader_factory) {}

CloudPolicyClient::~CloudPolicyClient() = default;

void CloudPolicyClient::SetupRegistration(
    const std::string& dm_token,
    const std::string& client_id,
    const std::vector<std::string>& user_affiliation_ids) {
  DCHECK(!dm_token.empty());
  DCHECK(!client_id.empty());
  DCHECK(!is_registered());

  dm_token_ = dm_token;
  client_id_ = client_id;
  request_jobs_.clear();
  app_install_report_request_job_ = nullptr;
  policy_fetch_request_job_.reset();
  responses_.clear();
  if (device_dm_token_callback_) {
    device_dm_token_ = device_dm_token_callback_.Run(user_affiliation_ids);
  }

  NotifyRegistrationStateChanged();
}

// Sets the client ID or generate a new one. A new one is intentionally
// generated on each new registration request in order to preserve privacy.
// Reusing IDs would mean the server could track clients by their registration
// attempts.
void CloudPolicyClient::SetClientId(const std::string& client_id) {
  client_id_ = client_id.empty() ? base::GenerateGUID() : client_id;
}

void CloudPolicyClient::Register(const RegistrationParameters& parameters,
                                 const std::string& client_id,
                                 const std::string& oauth_token) {
  DCHECK(service_);
  DCHECK(!oauth_token.empty());
  DCHECK(!is_registered());

  SetClientId(client_id);

  std::unique_ptr<RegistrationJobConfiguration> config =
      std::make_unique<RegistrationJobConfiguration>(
          DeviceManagementService::JobConfiguration::TYPE_REGISTRATION, this,
          DMAuth::NoAuth(), oauth_token,
          base::BindOnce(&CloudPolicyClient::OnRegisterCompleted,
                         weak_ptr_factory_.GetWeakPtr()));

  em::DeviceRegisterRequest* request =
      config->request()->mutable_register_request();
  CreateDeviceRegisterRequest(parameters, client_id, request);

  if (requires_reregistration())
    request->set_reregistration_dm_token(reregistration_dm_token_);

  policy_fetch_request_job_ = service_->CreateJob(std::move(config));
}

void CloudPolicyClient::RegisterWithCertificate(
    const RegistrationParameters& parameters,
    const std::string& client_id,
    std::unique_ptr<DMAuth> auth,
    const std::string& pem_certificate_chain,
    const std::string& sub_organization) {
  DCHECK(signing_service_);
  DCHECK(service_);
  DCHECK(!is_registered());

  SetClientId(client_id);

  em::CertificateBasedDeviceRegistrationData data;
  data.set_certificate_type(em::CertificateBasedDeviceRegistrationData::
                                ENTERPRISE_ENROLLMENT_CERTIFICATE);
  data.set_device_certificate(pem_certificate_chain);

  em::DeviceRegisterRequest* request = data.mutable_device_register_request();
  CreateDeviceRegisterRequest(parameters, client_id, request);
  if (!sub_organization.empty()) {
    em::DeviceRegisterConfiguration* configuration =
        data.mutable_device_register_configuration();
    configuration->set_device_owner(sub_organization);
  }

  signing_service_->SignData(
      data.SerializeAsString(),
      base::BindOnce(&CloudPolicyClient::OnRegisterWithCertificateRequestSigned,
                     weak_ptr_factory_.GetWeakPtr(), std::move(auth)));
}

void CloudPolicyClient::RegisterWithToken(const std::string& token,
                                          const std::string& client_id) {
  DCHECK(service_);
  DCHECK(!token.empty());
  DCHECK(!client_id.empty());
  DCHECK(!is_registered());

  SetClientId(client_id);

  std::unique_ptr<RegistrationJobConfiguration> config =
      std::make_unique<RegistrationJobConfiguration>(
          DeviceManagementService::JobConfiguration::TYPE_TOKEN_ENROLLMENT,
          this, DMAuth::FromEnrollmentToken(token),
          /*oauth_token=*/base::nullopt,
          base::BindOnce(&CloudPolicyClient::OnRegisterCompleted,
                         weak_ptr_factory_.GetWeakPtr()));

  enterprise_management::RegisterBrowserRequest* request =
      config->request()->mutable_register_browser_request();
  request->set_machine_name(GetMachineName());
  request->set_os_platform(GetOSPlatform());
  request->set_os_version(GetOSVersion());

  policy_fetch_request_job_ = service_->CreateJob(std::move(config));
}

void CloudPolicyClient::OnRegisterWithCertificateRequestSigned(
    std::unique_ptr<DMAuth> auth,
    bool success,
    em::SignedData signed_data) {
  if (!success) {
    const em::DeviceManagementResponse response;
    OnRegisterCompleted(nullptr, DM_STATUS_CANNOT_SIGN_REQUEST, 0, response);
    return;
  }

  std::unique_ptr<RegistrationJobConfiguration> config = std::make_unique<
      RegistrationJobConfiguration>(
      DeviceManagementService::JobConfiguration::TYPE_CERT_BASED_REGISTRATION,
      this, std::move(auth),
      /*oauth_token=*/base::nullopt,
      base::BindOnce(&CloudPolicyClient::OnRegisterCompleted,
                     weak_ptr_factory_.GetWeakPtr()));

  em::SignedData* signed_request =
      config->request()
          ->mutable_certificate_based_register_request()
          ->mutable_signed_request();
  signed_request->set_data(signed_data.data());
  signed_request->set_signature(signed_data.signature());
  signed_request->set_extra_data_bytes(signed_data.extra_data_bytes());

  policy_fetch_request_job_ = service_->CreateJob(std::move(config));
}

void CloudPolicyClient::SetInvalidationInfo(int64_t version,
                                            const std::string& payload) {
  invalidation_version_ = version;
  invalidation_payload_ = payload;
}

void CloudPolicyClient::SetOAuthTokenAsAdditionalAuth(
    const std::string& oauth_token) {
  oauth_token_ = oauth_token;
}

void CloudPolicyClient::FetchPolicy() {
  CHECK(is_registered());
  CHECK(!types_to_fetch_.empty());

  std::unique_ptr<DMServerJobConfiguration> config =
      std::make_unique<DMServerJobConfiguration>(
          DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH, this,
          /*critical=*/true, DMAuth::FromDMToken(dm_token_),
          /*oauth_token=*/oauth_token_,
          base::BindOnce(&CloudPolicyClient::OnPolicyFetchCompleted,
                         weak_ptr_factory_.GetWeakPtr()));

  em::DeviceManagementRequest* request = config->request();

  // Build policy fetch requests.
  em::DevicePolicyRequest* policy_request = request->mutable_policy_request();
  for (const auto& type_to_fetch : types_to_fetch_) {
    em::PolicyFetchRequest* fetch_request = policy_request->add_requests();
    fetch_request->set_policy_type(type_to_fetch.first);
    if (!type_to_fetch.second.empty())
      fetch_request->set_settings_entity_id(type_to_fetch.second);

    // Request signed policy blobs to help prevent tampering on the client.
    fetch_request->set_signature_type(em::PolicyFetchRequest::SHA1_RSA);
    if (public_key_version_valid_)
      fetch_request->set_public_key_version(public_key_version_);

    fetch_request->set_verification_key_hash(kPolicyVerificationKeyHash);

    // These fields are included only in requests for chrome policy.
    if (IsChromePolicy(type_to_fetch.first)) {
      if (!device_dm_token_.empty())
        fetch_request->set_device_dm_token(device_dm_token_);
      if (!last_policy_timestamp_.is_null())
        fetch_request->set_timestamp(last_policy_timestamp_.ToJavaTime());
      if (!invalidation_payload_.empty()) {
        fetch_request->set_invalidation_version(invalidation_version_);
        fetch_request->set_invalidation_payload(invalidation_payload_);
      }
    }
  }

  // Add device state keys.
  if (!state_keys_to_upload_.empty()) {
    em::DeviceStateKeyUpdateRequest* key_update_request =
        request->mutable_device_state_key_update_request();
    for (std::vector<std::string>::const_iterator key(
             state_keys_to_upload_.begin());
         key != state_keys_to_upload_.end(); ++key) {
      key_update_request->add_server_backed_state_keys(*key);
    }
  }

  // Set the fetched invalidation version to the latest invalidation version
  // since it is now the invalidation version used for the latest fetch.
  fetched_invalidation_version_ = invalidation_version_;

  policy_fetch_request_job_ = service_->CreateJob(std::move(config));
}

void CloudPolicyClient::UploadPolicyValidationReport(
    CloudPolicyValidatorBase::Status status,
    const std::vector<ValueValidationIssue>& value_validation_issues,
    const std::string& policy_type,
    const std::string& policy_token) {
  CHECK(is_registered());

  StatusCallback callback = base::DoNothing();
  std::unique_ptr<DMServerJobConfiguration> config =
      std::make_unique<DMServerJobConfiguration>(
          DeviceManagementService::JobConfiguration::
              TYPE_UPLOAD_POLICY_VALIDATION_REPORT,
          this,
          /*critical=*/false, DMAuth::FromDMToken(dm_token_),
          /*oauth_token=*/base::nullopt,
          base::BindOnce(&CloudPolicyClient::OnReportUploadCompleted,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  em::DeviceManagementRequest* request = config->request();
  em::PolicyValidationReportRequest* policy_validation_report_request =
      request->mutable_policy_validation_report_request();

  policy_validation_report_request->set_policy_type(policy_type);
  policy_validation_report_request->set_policy_token(policy_token);
  policy_validation_report_request->set_validation_result_type(
      TranslatePolicyValidationResult(status));

  for (const ValueValidationIssue& issue : value_validation_issues) {
    em::PolicyValueValidationIssue* proto_result =
        policy_validation_report_request->add_policy_value_validation_issues();
    proto_result->set_policy_name(issue.policy_name);
    proto_result->set_severity(
        TranslatePolicyValidationResultSeverity(issue.severity));
    proto_result->set_debug_message(issue.message);
  }

  request_jobs_.push_back(service_->CreateJob(std::move(config)));
}

void CloudPolicyClient::FetchRobotAuthCodes(
    std::unique_ptr<DMAuth> auth,
    enterprise_management::DeviceServiceApiAccessRequest::DeviceType
        device_type,
    const std::string& oauth_scopes,
    RobotAuthCodeCallback callback) {
  CHECK(is_registered());
  DCHECK(auth->has_dm_token());

  std::unique_ptr<DMServerJobConfiguration> config =
      std::make_unique<DMServerJobConfiguration>(
          DeviceManagementService::JobConfiguration::TYPE_API_AUTH_CODE_FETCH,
          this,
          /*critical=*/false, std::move(auth),
          /*oauth_token=*/base::nullopt,
          base::AdaptCallbackForRepeating(base::BindOnce(
              &CloudPolicyClient::OnFetchRobotAuthCodesCompleted,
              weak_ptr_factory_.GetWeakPtr(), std::move(callback))));

  em::DeviceServiceApiAccessRequest* request =
      config->request()->mutable_service_api_access_request();
  request->set_oauth2_client_id(
      GaiaUrls::GetInstance()->oauth2_chrome_client_id());
  request->add_auth_scopes(oauth_scopes);
  request->set_device_type(device_type);

  policy_fetch_request_job_ = service_->CreateJob(std::move(config));
}

void CloudPolicyClient::Unregister() {
  DCHECK(service_);
  std::unique_ptr<DMServerJobConfiguration> config =
      std::make_unique<DMServerJobConfiguration>(
          DeviceManagementService::JobConfiguration::TYPE_UNREGISTRATION, this,
          /*critical=*/false, DMAuth::FromDMToken(dm_token_),
          /*oauth_token=*/base::nullopt,
          base::BindOnce(&CloudPolicyClient::OnUnregisterCompleted,
                         weak_ptr_factory_.GetWeakPtr()));

  config->request()->mutable_unregister_request();

  policy_fetch_request_job_ = service_->CreateJob(std::move(config));
}

void CloudPolicyClient::UploadEnterpriseMachineCertificate(
    const std::string& certificate_data,
    CloudPolicyClient::StatusCallback callback) {
  UploadCertificate(certificate_data,
                    em::DeviceCertUploadRequest::ENTERPRISE_MACHINE_CERTIFICATE,
                    std::move(callback));
}

void CloudPolicyClient::UploadEnterpriseEnrollmentCertificate(
    const std::string& certificate_data,
    CloudPolicyClient::StatusCallback callback) {
  UploadCertificate(
      certificate_data,
      em::DeviceCertUploadRequest::ENTERPRISE_ENROLLMENT_CERTIFICATE,
      std::move(callback));
}

void CloudPolicyClient::UploadEnterpriseEnrollmentId(
    const std::string& enrollment_id,
    CloudPolicyClient::StatusCallback callback) {
  std::unique_ptr<DMServerJobConfiguration> config =
      CreateCertUploadJobConfiguration(std::move(callback));
  em::DeviceManagementRequest* request = config->request();
  em::DeviceCertUploadRequest* upload_request =
      request->mutable_cert_upload_request();
  upload_request->set_enrollment_id(enrollment_id);
  ExecuteCertUploadJob(std::move(config));
}

void CloudPolicyClient::UploadDeviceStatus(
    const em::DeviceStatusReportRequest* device_status,
    const em::SessionStatusReportRequest* session_status,
    const em::ChildStatusReportRequest* child_status,
    CloudPolicyClient::StatusCallback callback) {
  CHECK(is_registered());
  // Should pass in at least one type of status.
  DCHECK(device_status || session_status || child_status);

  std::unique_ptr<DMServerJobConfiguration> config =
      std::make_unique<DMServerJobConfiguration>(
          DeviceManagementService::JobConfiguration::TYPE_UPLOAD_STATUS, this,
          /*critical=*/false, DMAuth::FromDMToken(dm_token_),
          /*oauth_token=*/oauth_token_,
          base::BindOnce(&CloudPolicyClient::OnReportUploadCompleted,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  em::DeviceManagementRequest* request = config->request();
  if (device_status)
    *request->mutable_device_status_report_request() = *device_status;
  if (session_status)
    *request->mutable_session_status_report_request() = *session_status;
  if (child_status)
    *request->mutable_child_status_report_request() = *child_status;

  request_jobs_.push_back(service_->CreateJob(std::move(config)));
}

void CloudPolicyClient::UploadChromeDesktopReport(
    std::unique_ptr<em::ChromeDesktopReportRequest> chrome_desktop_report,
    CloudPolicyClient::StatusCallback callback) {
  CHECK(is_registered());
  DCHECK(chrome_desktop_report);
  std::unique_ptr<DMServerJobConfiguration> config =
      std::make_unique<DMServerJobConfiguration>(
          DeviceManagementService::JobConfiguration::TYPE_CHROME_DESKTOP_REPORT,
          this,
          /*critical=*/false, DMAuth::FromDMToken(dm_token_),
          /*oauth_token=*/base::nullopt,
          base::BindOnce(&CloudPolicyClient::OnReportUploadCompleted,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  em::DeviceManagementRequest* request = config->request();
  request->set_allocated_chrome_desktop_report_request(
      chrome_desktop_report.release());

  request_jobs_.push_back(service_->CreateJob(std::move(config)));
}

void CloudPolicyClient::UploadChromeOsUserReport(
    std::unique_ptr<enterprise_management::ChromeOsUserReportRequest>
        chrome_os_user_report,
    CloudPolicyClient::StatusCallback callback) {
  CHECK(is_registered());
  DCHECK(chrome_os_user_report);
  std::unique_ptr<DMServerJobConfiguration> config =
      std::make_unique<DMServerJobConfiguration>(
          DeviceManagementService::JobConfiguration::TYPE_CHROME_OS_USER_REPORT,
          this,
          /*critical=*/false, DMAuth::FromDMToken(dm_token_),
          /*oauth_token=*/base::nullopt,
          base::BindOnce(&CloudPolicyClient::OnReportUploadCompleted,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  em::DeviceManagementRequest* request = config->request();
  request->set_allocated_chrome_os_user_report_request(
      chrome_os_user_report.release());

  request_jobs_.push_back(service_->CreateJob(std::move(config)));
}

void CloudPolicyClient::UploadRealtimeReport(base::Value report,
                                             StatusCallback callback) {
  CHECK(is_registered());
  std::unique_ptr<RealtimeReportingJobConfiguration> config =
      std::make_unique<RealtimeReportingJobConfiguration>(
          this, DMAuth::FromDMToken(dm_token_),
          base::BindOnce(&CloudPolicyClient::OnRealtimeReportUploadCompleted,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  config->AddReport(std::move(report));

  request_jobs_.push_back(service_->CreateJob(std::move(config)));
}

void CloudPolicyClient::CancelAppInstallReportUpload() {
  if (app_install_report_request_job_) {
    RemoveJob(app_install_report_request_job_);
  }
}

void CloudPolicyClient::FetchRemoteCommands(
    std::unique_ptr<RemoteCommandJob::UniqueIDType> last_command_id,
    const std::vector<em::RemoteCommandResult>& command_results,
    RemoteCommandCallback callback) {
  CHECK(is_registered());
  std::unique_ptr<DMServerJobConfiguration> config =
      std::make_unique<DMServerJobConfiguration>(
          DeviceManagementService::JobConfiguration::TYPE_REMOTE_COMMANDS, this,
          /*critical=*/false, DMAuth::FromDMToken(dm_token_),
          /*oauth_token=*/base::nullopt,
          base::AdaptCallbackForRepeating(base::BindOnce(
              &CloudPolicyClient::OnRemoteCommandsFetched,
              weak_ptr_factory_.GetWeakPtr(), std::move(callback))));

  em::DeviceRemoteCommandRequest* const request =
      config->request()->mutable_remote_command_request();

  if (last_command_id)
    request->set_last_command_unique_id(*last_command_id);

  for (const auto& command_result : command_results)
    *request->add_command_results() = command_result;

  request->set_send_secure_commands(true);

  request_jobs_.push_back(service_->CreateJob(std::move(config)));
}

void CloudPolicyClient::GetDeviceAttributeUpdatePermission(
    std::unique_ptr<DMAuth> auth,
    CloudPolicyClient::StatusCallback callback) {
  CHECK(is_registered());
  // This condition is wrong in case of Attestation enrollment
  // (https://crbug.com/942013).
  // DCHECK(auth->has_oauth_token() || auth->has_enrollment_token());

  bool has_oauth_token = auth->has_oauth_token();
  std::unique_ptr<DMServerJobConfiguration> config =
      std::make_unique<DMServerJobConfiguration>(
          DeviceManagementService::JobConfiguration::
              TYPE_ATTRIBUTE_UPDATE_PERMISSION,
          this,
          /*critical=*/false,
          !has_oauth_token ? std::move(auth) : DMAuth::NoAuth(),
          has_oauth_token ? auth->oauth_token() : std::string(),
          base::BindOnce(
              &CloudPolicyClient::OnDeviceAttributeUpdatePermissionCompleted,
              weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  config->request()->mutable_device_attribute_update_permission_request();

  request_jobs_.push_back(service_->CreateJob(std::move(config)));
}

void CloudPolicyClient::UpdateDeviceAttributes(
    std::unique_ptr<DMAuth> auth,
    const std::string& asset_id,
    const std::string& location,
    CloudPolicyClient::StatusCallback callback) {
  CHECK(is_registered());
  DCHECK(auth->has_oauth_token() || auth->has_enrollment_token());

  bool has_oauth_token = auth->has_oauth_token();
  std::unique_ptr<DMServerJobConfiguration> config =
      std::make_unique<DMServerJobConfiguration>(
          DeviceManagementService::JobConfiguration::TYPE_ATTRIBUTE_UPDATE,
          this,
          /*critical=*/false,
          !has_oauth_token ? std::move(auth) : DMAuth::NoAuth(),
          has_oauth_token ? auth->oauth_token() : std::string(),
          base::BindOnce(&CloudPolicyClient::OnDeviceAttributeUpdated,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  em::DeviceAttributeUpdateRequest* request =
      config->request()->mutable_device_attribute_update_request();

  request->set_asset_id(asset_id);
  request->set_location(location);

  request_jobs_.push_back(service_->CreateJob(std::move(config)));
}

void CloudPolicyClient::UpdateGcmId(
    const std::string& gcm_id,
    CloudPolicyClient::StatusCallback callback) {
  CHECK(is_registered());

  std::unique_ptr<DMServerJobConfiguration> config =
      std::make_unique<DMServerJobConfiguration>(
          DeviceManagementService::JobConfiguration::TYPE_GCM_ID_UPDATE, this,
          /*critical=*/false, DMAuth::FromDMToken(dm_token_),
          /*oauth_token=*/base::nullopt,
          base::BindOnce(&CloudPolicyClient::OnGcmIdUpdated,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  em::GcmIdUpdateRequest* const request =
      config->request()->mutable_gcm_id_update_request();

  request->set_gcm_id(gcm_id);

  request_jobs_.push_back(service_->CreateJob(std::move(config)));
}

void CloudPolicyClient::ClientCertProvisioningStartCsr(
    const std::string& cert_scope,
    const std::string& cert_profile_id,
    const std::string& cert_profile_version,
    const std::string& public_key,
    ClientCertProvisioningStartCsrCallback callback) {
  CHECK(is_registered());

  std::unique_ptr<DMServerJobConfiguration> config = std::make_unique<
      DMServerJobConfiguration>(
      DeviceManagementService::JobConfiguration::TYPE_CERT_PROVISIONING_REQUEST,
      this,
      /*critical=*/false, DMAuth::FromDMToken(dm_token_),
      /*oauth_token=*/base::nullopt,
      base::BindOnce(
          &CloudPolicyClient::OnClientCertProvisioningStartCsrResponse,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  em::ClientCertificateProvisioningRequest* request =
      config->request()->mutable_client_certificate_provisioning_request();

  request->set_certificate_scope(cert_scope);
  request->set_cert_profile_id(cert_profile_id);
  request->set_policy_version(cert_profile_version);
  request->set_public_key(public_key);
  // Sets the request type, no actual data is required.
  request->mutable_start_csr_request();

  request_jobs_.push_back(service_->CreateJob(std::move(config)));
}

void CloudPolicyClient::ClientCertProvisioningFinishCsr(
    const std::string& cert_scope,
    const std::string& cert_profile_id,
    const std::string& cert_profile_version,
    const std::string& public_key,
    const std::string& va_challenge_response,
    const std::string& signature,
    ClientCertProvisioningFinishCsrCallback callback) {
  CHECK(is_registered());

  std::unique_ptr<DMServerJobConfiguration> config = std::make_unique<
      DMServerJobConfiguration>(
      DeviceManagementService::JobConfiguration::TYPE_CERT_PROVISIONING_REQUEST,
      this,
      /*critical=*/false, DMAuth::FromDMToken(dm_token_),
      /*oauth_token=*/base::nullopt,
      base::BindOnce(
          &CloudPolicyClient::OnClientCertProvisioningFinishCsrResponse,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  em::ClientCertificateProvisioningRequest* const request =
      config->request()->mutable_client_certificate_provisioning_request();

  request->set_certificate_scope(cert_scope);
  request->set_cert_profile_id(cert_profile_id);
  request->set_policy_version(cert_profile_version);
  request->set_public_key(public_key);

  em::FinishCsrRequest* finish_csr_request =
      request->mutable_finish_csr_request();
  if (!va_challenge_response.empty()) {
    finish_csr_request->set_va_challenge_response(va_challenge_response);
  }
  finish_csr_request->set_signature(signature);

  request_jobs_.push_back(service_->CreateJob(std::move(config)));
}

void CloudPolicyClient::ClientCertProvisioningDownloadCert(
    const std::string& cert_scope,
    const std::string& cert_profile_id,
    const std::string& cert_profile_version,
    const std::string& public_key,
    ClientCertProvisioningDownloadCertCallback callback) {
  CHECK(is_registered());

  std::unique_ptr<DMServerJobConfiguration> config = std::make_unique<
      DMServerJobConfiguration>(
      DeviceManagementService::JobConfiguration::TYPE_CERT_PROVISIONING_REQUEST,
      this,
      /*critical=*/false, DMAuth::FromDMToken(dm_token_),
      /*oauth_token=*/base::nullopt,
      base::BindOnce(
          &CloudPolicyClient::OnClientCertProvisioningDownloadCertResponse,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  em::ClientCertificateProvisioningRequest* const request =
      config->request()->mutable_client_certificate_provisioning_request();

  request->set_certificate_scope(cert_scope);
  request->set_cert_profile_id(cert_profile_id);
  request->set_policy_version(cert_profile_version);
  request->set_public_key(public_key);
  // Sets the request type, no actual data is required.
  request->mutable_download_cert_request();

  request_jobs_.push_back(service_->CreateJob(std::move(config)));
}

void CloudPolicyClient::UpdateServiceAccount(const std::string& account_email) {
  NotifyServiceAccountSet(account_email);
}

void CloudPolicyClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void CloudPolicyClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void CloudPolicyClient::AddPolicyTypeToFetch(
    const std::string& policy_type,
    const std::string& settings_entity_id) {
  types_to_fetch_.insert(std::make_pair(policy_type, settings_entity_id));
}

void CloudPolicyClient::RemovePolicyTypeToFetch(
    const std::string& policy_type,
    const std::string& settings_entity_id) {
  types_to_fetch_.erase(std::make_pair(policy_type, settings_entity_id));
}

void CloudPolicyClient::SetStateKeysToUpload(
    const std::vector<std::string>& keys) {
  state_keys_to_upload_ = keys;
}

const em::PolicyFetchResponse* CloudPolicyClient::GetPolicyFor(
    const std::string& policy_type,
    const std::string& settings_entity_id) const {
  auto it = responses_.find(std::make_pair(policy_type, settings_entity_id));
  return it == responses_.end() ? nullptr : it->second.get();
}

scoped_refptr<network::SharedURLLoaderFactory>
CloudPolicyClient::GetURLLoaderFactory() {
  return url_loader_factory_;
}

int CloudPolicyClient::GetActiveRequestCountForTest() const {
  return request_jobs_.size();
}

void CloudPolicyClient::SetURLLoaderFactoryForTesting(
    scoped_refptr<network::SharedURLLoaderFactory> factory) {
  url_loader_factory_ = factory;
}

void CloudPolicyClient::UploadCertificate(
    const std::string& certificate_data,
    em::DeviceCertUploadRequest::CertificateType certificate_type,
    CloudPolicyClient::StatusCallback callback) {
  std::unique_ptr<DMServerJobConfiguration> config =
      CreateCertUploadJobConfiguration(std::move(callback));
  PrepareCertUploadRequest(config.get(), certificate_data, certificate_type);
  ExecuteCertUploadJob(std::move(config));
}

void CloudPolicyClient::PrepareCertUploadRequest(
    DMServerJobConfiguration* config,
    const std::string& certificate_data,
    enterprise_management::DeviceCertUploadRequest::CertificateType
        certificate_type) {
  em::DeviceManagementRequest* request = config->request();
  em::DeviceCertUploadRequest* upload_request =
      request->mutable_cert_upload_request();
  upload_request->set_device_certificate(certificate_data);
  upload_request->set_certificate_type(certificate_type);
}

std::unique_ptr<DMServerJobConfiguration>
CloudPolicyClient::CreateCertUploadJobConfiguration(
    CloudPolicyClient::StatusCallback callback) {
  CHECK(is_registered());
  return std::make_unique<DMServerJobConfiguration>(
      service_,
      DeviceManagementService::JobConfiguration::TYPE_UPLOAD_CERTIFICATE,
      client_id(),
      /*critical=*/false, DMAuth::FromDMToken(dm_token_),
      /*oauth_token=*/base::nullopt, GetURLLoaderFactory(),
      base::BindOnce(&CloudPolicyClient::OnCertificateUploadCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CloudPolicyClient::ExecuteCertUploadJob(
    std::unique_ptr<DMServerJobConfiguration> config) {
  request_jobs_.push_back(service_->CreateJob(std::move(config)));
}

void CloudPolicyClient::OnRegisterCompleted(
    DeviceManagementService::Job* job,
    DeviceManagementStatus status,
    int net_error,
    const em::DeviceManagementResponse& response) {
  if (status == DM_STATUS_SUCCESS) {
    if (!response.has_register_response() ||
        !response.register_response().has_device_management_token()) {
      LOG(WARNING) << "Invalid registration response.";
      status = DM_STATUS_RESPONSE_DECODING_ERROR;
    } else if (!reregistration_dm_token_.empty() &&
               reregistration_dm_token_ !=
                   response.register_response().device_management_token()) {
      LOG(WARNING) << "Reregistration DMToken mismatch.";
      status = DM_STATUS_SERVICE_MANAGEMENT_TOKEN_INVALID;
    }
  }

  status_ = status;
  if (status == DM_STATUS_SUCCESS) {
    dm_token_ = response.register_response().device_management_token();
    reregistration_dm_token_.clear();
    if (response.register_response().has_configuration_seed()) {
      configuration_seed_ =
          base::DictionaryValue::From(base::JSONReader::ReadDeprecated(
              response.register_response().configuration_seed(),
              base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS));
      if (!configuration_seed_)
        LOG(ERROR) << "Failed to parse configuration seed";
    }
    DVLOG(1) << "Client registration complete - DMToken = " << dm_token_;

    // Device mode is only relevant for device policy really, it's the
    // responsibility of the consumer of the field to check validity.
    device_mode_ = DEVICE_MODE_NOT_SET;
    if (response.register_response().has_enrollment_type()) {
      device_mode_ = TranslateProtobufDeviceMode(
          response.register_response().enrollment_type());
    }

    if (device_dm_token_callback_) {
      std::vector<std::string> user_affiliation_ids(
          response.register_response().user_affiliation_ids().begin(),
          response.register_response().user_affiliation_ids().end());
      device_dm_token_ = device_dm_token_callback_.Run(user_affiliation_ids);
    }
    NotifyRegistrationStateChanged();
  } else {
    NotifyClientError();
  }
}

void CloudPolicyClient::OnFetchRobotAuthCodesCompleted(
    RobotAuthCodeCallback callback,
    DeviceManagementService::Job* job,
    DeviceManagementStatus status,
    int net_error,
    const em::DeviceManagementResponse& response) {
  if (status == DM_STATUS_SUCCESS &&
      (!response.has_service_api_access_response())) {
    LOG(WARNING) << "Invalid service api access response.";
    status = DM_STATUS_RESPONSE_DECODING_ERROR;
  }
  status_ = status;
  if (status == DM_STATUS_SUCCESS) {
    DVLOG(1) << "Device robot account auth code fetch complete - code = "
             << response.service_api_access_response().auth_code();
    std::move(callback).Run(status,
                            response.service_api_access_response().auth_code());
  } else {
    std::move(callback).Run(status, std::string());
  }
}

void CloudPolicyClient::OnPolicyFetchCompleted(
    DeviceManagementService::Job* job,
    DeviceManagementStatus status,
    int net_error,
    const em::DeviceManagementResponse& response) {
  if (status == DM_STATUS_SUCCESS) {
    if (!response.has_policy_response() ||
        response.policy_response().responses_size() == 0) {
      LOG(WARNING) << "Empty policy response.";
      status = DM_STATUS_RESPONSE_DECODING_ERROR;
    }
  }

  status_ = status;
  if (status == DM_STATUS_SUCCESS) {
    const em::DevicePolicyResponse& policy_response =
        response.policy_response();
    responses_.clear();
    for (int i = 0; i < policy_response.responses_size(); ++i) {
      const em::PolicyFetchResponse& response = policy_response.responses(i);
      em::PolicyData policy_data;
      if (!policy_data.ParseFromString(response.policy_data()) ||
          !policy_data.IsInitialized() || !policy_data.has_policy_type()) {
        LOG(WARNING) << "Invalid PolicyData received, ignoring";
        continue;
      }
      const std::string& type = policy_data.policy_type();
      std::string entity_id;
      if (policy_data.has_settings_entity_id())
        entity_id = policy_data.settings_entity_id();
      std::pair<std::string, std::string> key(type, entity_id);
      if (base::Contains(responses_, key)) {
        LOG(WARNING) << "Duplicate PolicyFetchResponse for type: " << type
                     << ", entity: " << entity_id << ", ignoring";
        continue;
      }
      responses_[key] = std::make_unique<em::PolicyFetchResponse>(response);
    }
    state_keys_to_upload_.clear();
    NotifyPolicyFetched();
  } else {
    NotifyClientError();

    if (status == DM_STATUS_SERVICE_DEVICE_NOT_FOUND) {
      // Mark as unregistered and initialize re-registration flow.
      reregistration_dm_token_ = dm_token_;
      dm_token_.clear();
      NotifyRegistrationStateChanged();
    }
  }
}

void CloudPolicyClient::OnUnregisterCompleted(
    DeviceManagementService::Job* job,
    DeviceManagementStatus status,
    int net_error,
    const em::DeviceManagementResponse& response) {
  if (status == DM_STATUS_SUCCESS && !response.has_unregister_response()) {
    // Assume unregistration has succeeded either way.
    LOG(WARNING) << "Empty unregistration response.";
  }

  status_ = status;
  if (status == DM_STATUS_SUCCESS) {
    dm_token_.clear();
    // Cancel all outstanding jobs.
    request_jobs_.clear();
    app_install_report_request_job_ = nullptr;
    device_dm_token_.clear();
    NotifyRegistrationStateChanged();
  } else {
    NotifyClientError();
  }
}

void CloudPolicyClient::OnCertificateUploadCompleted(
    CloudPolicyClient::StatusCallback callback,
    DeviceManagementService::Job* job,
    DeviceManagementStatus status,
    int net_error,
    const em::DeviceManagementResponse& response) {
  bool success = true;
  status_ = status;
  if (status != DM_STATUS_SUCCESS) {
    success = false;
    NotifyClientError();
  } else if (!response.has_cert_upload_response()) {
    LOG(WARNING) << "Empty upload certificate response.";
    success = false;
  }
  std::move(callback).Run(success);
  RemoveJob(job);
}

void CloudPolicyClient::OnDeviceAttributeUpdatePermissionCompleted(
    CloudPolicyClient::StatusCallback callback,
    DeviceManagementService::Job* job,
    DeviceManagementStatus status,
    int net_error,
    const em::DeviceManagementResponse& response) {
  bool success = false;

  if (status == DM_STATUS_SUCCESS &&
      !response.has_device_attribute_update_permission_response()) {
    LOG(WARNING) << "Invalid device attribute update permission response.";
    status = DM_STATUS_RESPONSE_DECODING_ERROR;
  }

  status_ = status;
  if (status == DM_STATUS_SUCCESS &&
      response.device_attribute_update_permission_response().has_result() &&
      response.device_attribute_update_permission_response().result() ==
          em::DeviceAttributeUpdatePermissionResponse::
              ATTRIBUTE_UPDATE_ALLOWED) {
    success = true;
  }

  std::move(callback).Run(success);
  RemoveJob(job);
}

void CloudPolicyClient::OnDeviceAttributeUpdated(
    CloudPolicyClient::StatusCallback callback,
    DeviceManagementService::Job* job,
    DeviceManagementStatus status,
    int net_error,
    const em::DeviceManagementResponse& response) {
  bool success = false;

  if (status == DM_STATUS_SUCCESS &&
      !response.has_device_attribute_update_response()) {
    LOG(WARNING) << "Invalid device attribute update response.";
    status = DM_STATUS_RESPONSE_DECODING_ERROR;
  }

  status_ = status;
  if (status == DM_STATUS_SUCCESS &&
      response.device_attribute_update_response().has_result() &&
      response.device_attribute_update_response().result() ==
          em::DeviceAttributeUpdateResponse::ATTRIBUTE_UPDATE_SUCCESS) {
    success = true;
  }

  std::move(callback).Run(success);
  RemoveJob(job);
}

void CloudPolicyClient::RemoveJob(DeviceManagementService::Job* job) {
  if (app_install_report_request_job_ == job) {
    app_install_report_request_job_ = nullptr;
  }
  for (auto it = request_jobs_.begin(); it != request_jobs_.end(); ++it) {
    if (it->get() == job) {
      request_jobs_.erase(it);
      return;
    }
  }
  // This job was already deleted from our list, somehow. This shouldn't
  // happen since deleting the job should cancel the callback.
  NOTREACHED();
}

void CloudPolicyClient::OnReportUploadCompleted(
    StatusCallback callback,
    DeviceManagementService::Job* job,
    DeviceManagementStatus status,
    int net_error,
    const em::DeviceManagementResponse& response) {
  status_ = status;
  if (status != DM_STATUS_SUCCESS)
    NotifyClientError();

  std::move(callback).Run(status == DM_STATUS_SUCCESS);
  RemoveJob(job);
}

void CloudPolicyClient::OnRealtimeReportUploadCompleted(
    StatusCallback callback,
    DeviceManagementService::Job* job,
    DeviceManagementStatus status,
    int net_error,
    const base::Value& response) {
  status_ = status;
  if (status != DM_STATUS_SUCCESS)
    NotifyClientError();

  std::move(callback).Run(status == DM_STATUS_SUCCESS);
  RemoveJob(job);
}

void CloudPolicyClient::OnRemoteCommandsFetched(
    RemoteCommandCallback callback,
    DeviceManagementService::Job* job,
    DeviceManagementStatus status,
    int net_error,
    const em::DeviceManagementResponse& response) {
  std::vector<em::RemoteCommand> commands;
  std::vector<em::SignedData> signed_commands;
  if (status == DM_STATUS_SUCCESS) {
    if (response.has_remote_command_response()) {
      for (const auto& command : response.remote_command_response().commands())
        commands.push_back(command);

      for (const auto& secure_command :
           response.remote_command_response().secure_commands()) {
        signed_commands.push_back(secure_command);
      }
    } else {
      status = DM_STATUS_RESPONSE_DECODING_ERROR;
    }
  }
  std::move(callback).Run(status, commands, signed_commands);
  RemoveJob(job);
}

void CloudPolicyClient::OnGcmIdUpdated(
    StatusCallback callback,
    DeviceManagementService::Job* job,
    DeviceManagementStatus status,
    int net_error,
    const em::DeviceManagementResponse& response) {
  status_ = status;
  if (status != DM_STATUS_SUCCESS)
    NotifyClientError();

  std::move(callback).Run(status == DM_STATUS_SUCCESS);
  RemoveJob(job);
}

namespace {
// Checks all error-like fields of a client cert provisioning response. Uses
// |status| as an input and output parameter. Extracts error and try_again_later
// fields from the |response| into |response_error| and |try_later|. Returns
// true if all error-like fields are empty or "ok" and the parsing of the
// |response| can be continued.
bool CheckCommonClientCertProvisioningResponse(
    const em::DeviceManagementResponse& response,
    policy::DeviceManagementStatus* status,
    base::Optional<CertProvisioningResponseErrorType>* response_error,
    base::Optional<int64_t>* try_later) {
  if (*status != DM_STATUS_SUCCESS) {
    return false;
  }

  if (!response.has_client_certificate_provisioning_response()) {
    *status = DM_STATUS_RESPONSE_DECODING_ERROR;
    return false;
  }

  const em::ClientCertificateProvisioningResponse& cert_provisioning_response =
      response.client_certificate_provisioning_response();

  if (cert_provisioning_response.has_error()) {
    *response_error = cert_provisioning_response.error();
    return false;
  }

  if (cert_provisioning_response.has_try_again_later()) {
    *try_later = cert_provisioning_response.try_again_later();
    return false;
  }

  return true;
}

}  // namespace

void CloudPolicyClient::OnClientCertProvisioningStartCsrResponse(
    ClientCertProvisioningStartCsrCallback callback,
    policy::DeviceManagementService::Job* job,
    policy::DeviceManagementStatus status,
    int net_error,
    const em::DeviceManagementResponse& response) {
  base::ScopedClosureRunner job_cleaner(base::BindOnce(
      &CloudPolicyClient::RemoveJob, base::Unretained(this), job));

  status_ = status;
  base::Optional<CertProvisioningResponseErrorType> response_error;
  base::Optional<int64_t> try_later;

  // Single step loop for convenience.
  do {
    if (!CheckCommonClientCertProvisioningResponse(
            response, &status, &response_error, &try_later)) {
      break;
    }

    const em::ClientCertificateProvisioningResponse&
        cert_provisioning_response =
            response.client_certificate_provisioning_response();

    if (!cert_provisioning_response.has_start_csr_response()) {
      status = DM_STATUS_RESPONSE_DECODING_ERROR;
      break;
    }

    const em::StartCsrResponse& start_csr_response =
        cert_provisioning_response.start_csr_response();

    if (!start_csr_response.has_hashing_algorithm() ||
        !start_csr_response.has_signing_algorithm() ||
        !start_csr_response.has_data_to_sign()) {
      status = DM_STATUS_RESPONSE_DECODING_ERROR;
      break;
    }

    if (start_csr_response.signing_algorithm() !=
        em::SigningAlgorithm::RSA_PKCS1_V1_5) {
      status = DM_STATUS_RESPONSE_DECODING_ERROR;
      break;
    }

    const std::string empty_str;

    const std::string& invalidation_topic =
        start_csr_response.has_invalidation_topic()
            ? start_csr_response.invalidation_topic()
            : empty_str;

    const std::string& va_challenge = start_csr_response.has_va_challenge()
                                          ? start_csr_response.va_challenge()
                                          : empty_str;

    // Everything is ok, run |callback| with data.
    std::move(callback).Run(status, response_error, try_later,
                            invalidation_topic, va_challenge,
                            start_csr_response.hashing_algorithm(),
                            start_csr_response.data_to_sign());
    return;
  } while (false);

  // Something went wrong. Return error via |status|, |response_error|,
  // |try_later|.
  const std::string empty_str;
  em::HashingAlgorithm hash_algo = {};
  std::move(callback).Run(status, response_error, try_later, empty_str,
                          empty_str, hash_algo, empty_str);
}

void CloudPolicyClient::OnClientCertProvisioningFinishCsrResponse(
    ClientCertProvisioningFinishCsrCallback callback,
    policy::DeviceManagementService::Job* job,
    policy::DeviceManagementStatus status,
    int net_error,
    const em::DeviceManagementResponse& response) {
  base::ScopedClosureRunner job_cleaner(base::BindOnce(
      &CloudPolicyClient::RemoveJob, base::Unretained(this), job));

  status_ = status;
  base::Optional<CertProvisioningResponseErrorType> response_error;
  base::Optional<int64_t> try_later;

  // Single step loop for convenience.
  do {
    if (!CheckCommonClientCertProvisioningResponse(
            response, &status, &response_error, &try_later)) {
      break;
    }

    const em::ClientCertificateProvisioningResponse&
        cert_provisioning_response =
            response.client_certificate_provisioning_response();

    if (!cert_provisioning_response.has_finish_csr_response()) {
      status = DM_STATUS_RESPONSE_DECODING_ERROR;
      break;
    }
  } while (false);

  std::move(callback).Run(status, response_error, try_later);
}

void CloudPolicyClient::OnClientCertProvisioningDownloadCertResponse(
    ClientCertProvisioningDownloadCertCallback callback,
    policy::DeviceManagementService::Job* job,
    policy::DeviceManagementStatus status,
    int net_error,
    const em::DeviceManagementResponse& response) {
  base::ScopedClosureRunner job_cleaner(base::BindOnce(
      &CloudPolicyClient::RemoveJob, base::Unretained(this), job));

  status_ = status;
  base::Optional<CertProvisioningResponseErrorType> response_error;
  base::Optional<int64_t> try_later;

  // Single step loop for convenience.
  do {
    if (!CheckCommonClientCertProvisioningResponse(
            response, &status, &response_error, &try_later)) {
      break;
    }

    const em::ClientCertificateProvisioningResponse&
        cert_provisioning_response =
            response.client_certificate_provisioning_response();

    if (!cert_provisioning_response.has_download_cert_response()) {
      status = DM_STATUS_RESPONSE_DECODING_ERROR;
      break;
    }

    const em::DownloadCertResponse& download_cert_response =
        cert_provisioning_response.download_cert_response();

    if (!download_cert_response.has_pem_encoded_certificate()) {
      status = DM_STATUS_RESPONSE_DECODING_ERROR;
      break;
    }

    // Everything is ok, run |callback| with data.
    std::move(callback).Run(status, response_error, try_later,
                            download_cert_response.pem_encoded_certificate());
    return;
  } while (false);

  // Something went wrong. Return error via |status|, |response_error|,
  // |try_later|.
  std::move(callback).Run(status, response_error, try_later, std::string());
}

void CloudPolicyClient::NotifyPolicyFetched() {
  for (auto& observer : observers_)
    observer.OnPolicyFetched(this);
}

void CloudPolicyClient::NotifyRegistrationStateChanged() {
  for (auto& observer : observers_)
    observer.OnRegistrationStateChanged(this);
}

void CloudPolicyClient::NotifyClientError() {
  for (auto& observer : observers_)
    observer.OnClientError(this);
}

void CloudPolicyClient::NotifyServiceAccountSet(
    const std::string& account_email) {
  for (auto& observer : observers_)
    observer.OnServiceAccountSet(this, account_email);
}

void CloudPolicyClient::CreateDeviceRegisterRequest(
    const RegistrationParameters& params,
    const std::string& client_id,
    em::DeviceRegisterRequest* request) {
  if (!client_id.empty())
    request->set_reregister(true);
  request->set_type(params.registration_type);
  request->set_flavor(params.flavor);
  request->set_lifetime(params.lifetime);
  if (!machine_id_.empty())
    request->set_machine_id(machine_id_);
  if (!machine_model_.empty())
    request->set_machine_model(machine_model_);
  if (!brand_code_.empty())
    request->set_brand_code(brand_code_);
  if (!attested_device_id_.empty())
    request->mutable_device_register_identification()->set_attested_device_id(
        attested_device_id_);
  if (!ethernet_mac_address_.empty())
    request->set_ethernet_mac_address(ethernet_mac_address_);
  if (!dock_mac_address_.empty())
    request->set_dock_mac_address(dock_mac_address_);
  if (!manufacture_date_.empty())
    request->set_manufacture_date(manufacture_date_);
  if (!params.requisition.empty())
    request->set_requisition(params.requisition);
  if (!params.current_state_key.empty())
    request->set_server_backed_state_key(params.current_state_key);
}

}  // namespace policy
