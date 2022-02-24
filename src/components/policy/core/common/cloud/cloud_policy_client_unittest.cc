// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/cloud_policy_client.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/compiler_specific.h"
#include "base/cxx17_backports.h"
#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/policy/core/common/cloud/client_data_delegate.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "components/policy/core/common/cloud/mock_signing_service.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "components/policy/core/common/cloud/reporting_job_configuration_base.h"
#include "components/policy/core/common/features.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/version_info/version_info.h"
#include "google_apis/gaia/gaia_urls.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/system/fake_statistics_provider.h"
#endif

using testing::_;
using testing::Contains;
using testing::DoAll;
using testing::ElementsAre;
using testing::Invoke;
using testing::Mock;
using testing::Not;
using testing::Pair;
using testing::Return;
using testing::SaveArg;
using testing::StrictMock;
using testing::WithArg;

// Matcher for absl::optional. Can be combined with Not().
MATCHER(HasValue, "Has value") {
  return arg.has_value();
}

// The type for variables containing an error from DM Server response.
using CertProvisioningResponseErrorType =
    enterprise_management::ClientCertificateProvisioningResponse::Error;
// The namespace that contains convenient aliases for error values, e.g.
// UNDEFINED, TIMED_OUT, IDENTITY_VERIFICATION_ERROR, CA_ERROR.
using CertProvisioningResponseError =
    enterprise_management::ClientCertificateProvisioningResponse;

namespace em = enterprise_management;

// An enum for PSM execution result values.
using PsmExecutionResult = em::DeviceRegisterRequest::PsmExecutionResult;

namespace policy {

namespace {

constexpr char kClientID[] = "fake-client-id";
constexpr char kMachineID[] = "fake-machine-id";
constexpr char kMachineModel[] = "fake-machine-model";
constexpr char kBrandCode[] = "fake-brand-code";
constexpr char kAttestedDeviceId[] = "fake-attested-device-id";
constexpr char kEthernetMacAddress[] = "fake-ethernet-mac-address";
constexpr char kDockMacAddress[] = "fake-dock-mac-address";
constexpr char kManufactureDate[] = "fake-manufacture-date";
constexpr char kOAuthToken[] = "fake-oauth-token";
constexpr char kDMToken[] = "fake-dm-token";
constexpr char kDeviceDMToken[] = "fake-device-dm-token";
constexpr char kMachineCertificate[] = "fake-machine-certificate";
constexpr char kEnrollmentCertificate[] = "fake-enrollment-certificate";
constexpr char kEnrollmentId[] = "fake-enrollment-id";
constexpr char kOsName[] = "fake-os-name";

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
constexpr char kEnrollmentToken[] = "enrollment_token";
#endif

constexpr char kRequisition[] = "fake-requisition";
constexpr char kStateKey[] = "fake-state-key";
constexpr char kPayload[] = "input_payload";
constexpr char kResultPayload[] = "output_payload";
constexpr char kAssetId[] = "fake-asset-id";
constexpr char kLocation[] = "fake-location";
constexpr char kGcmID[] = "fake-gcm-id";
constexpr char kPolicyToken[] = "fake-policy-token";
constexpr char kPolicyName[] = "fake-policy-name";
constexpr char kValueValidationMessage[] = "fake-value-validation-message";
constexpr char kRobotAuthCode[] = "fake-robot-auth-code";
constexpr char kApiAuthScope[] = "fake-api-auth-scope";

constexpr int64_t kAgeOfCommand = 123123123;
constexpr int64_t kLastCommandId = 123456789;
constexpr int64_t kTimestamp = 987654321;

MATCHER_P(MatchProto, expected, "matches protobuf") {
  return arg.SerializePartialAsString() == expected.SerializePartialAsString();
}

base::Value::Dict ConvertEncryptedRecordToValue(
    const ::reporting::EncryptedRecord& record) {
  base::Value::Dict record_request;
  if (record.has_encrypted_wrapped_record()) {
    base::Value::Dict encrypted_wrapped_record;
    std::string base64_encode;
    base::Base64Encode(record.encrypted_wrapped_record(), &base64_encode);
    record_request.Set("encryptedWrappedRecord", base64_encode);
  }
  if (record.has_encryption_info()) {
    base::Value::Dict encryption_info;
    if (record.encryption_info().has_encryption_key()) {
      std::string base64_encode;
      base::Base64Encode(record.encryption_info().encryption_key(),
                         &base64_encode);
      encryption_info.Set("encryptionKey", base64_encode);
    }
    if (record.encryption_info().has_public_key_id()) {
      encryption_info.Set(
          "publicKeyId",
          base::NumberToString(record.encryption_info().public_key_id()));
    }
    record_request.Set("encryptionInfo", std::move(encryption_info));
  }
  if (record.has_sequence_information()) {
    base::Value::Dict sequence_information;
    if (record.sequence_information().has_sequencing_id()) {
      sequence_information.Set(
          "sequencingId",
          base::NumberToString(record.sequence_information().sequencing_id()));
    }
    if (record.sequence_information().has_generation_id()) {
      sequence_information.Set(
          "generationId",
          base::NumberToString(record.sequence_information().generation_id()));
    }
    if (record.sequence_information().has_priority()) {
      sequence_information.Set("priority",
                               record.sequence_information().priority());
    }
    record_request.Set("sequencingInformation",
                       std::move(sequence_information));
  }
  return record_request;
}

// A mock class to allow us to set expectations on upload callbacks.
struct MockStatusCallbackObserver {
  MOCK_METHOD(void, OnCallbackComplete, (bool));
};

// A mock class to allow us to set expectations on remote command fetch
// callbacks.
struct MockRemoteCommandsObserver {
  MOCK_METHOD(void,
              OnRemoteCommandsFetched,
              (DeviceManagementStatus, const std::vector<em::SignedData>&));
};

struct MockDeviceDMTokenCallbackObserver {
  MOCK_METHOD(std::string,
              OnDeviceDMTokenRequested,
              (const std::vector<std::string>&));
};

struct MockRobotAuthCodeCallbackObserver {
  MOCK_METHOD(void,
              OnRobotAuthCodeFetched,
              (DeviceManagementStatus, const std::string&));
};

struct MockResponseCallbackObserver {
  MOCK_METHOD(void, OnResponseReceived, (absl::optional<base::Value>));
};

class FakeClientDataDelegate : public ClientDataDelegate {
 public:
  void FillRegisterBrowserRequest(
      enterprise_management::RegisterBrowserRequest* request,
      base::OnceClosure callback) const override {
    request->set_os_platform(GetOSPlatform());
    request->set_os_version(GetOSVersion());

    std::move(callback).Run();
  }
};

std::string CreatePolicyData(const std::string& policy_value) {
  em::PolicyData policy_data;
  policy_data.set_policy_type(dm_protocol::kChromeUserPolicyType);
  policy_data.set_policy_value(policy_value);
  return policy_data.SerializeAsString();
}

em::DeviceManagementRequest GetPolicyRequest() {
  em::DeviceManagementRequest policy_request;

  em::PolicyFetchRequest* policy_fetch_request =
      policy_request.mutable_policy_request()->add_requests();
  policy_fetch_request->set_policy_type(dm_protocol::kChromeUserPolicyType);
  policy_fetch_request->set_signature_type(em::PolicyFetchRequest::SHA1_RSA);
  policy_fetch_request->set_verification_key_hash(kPolicyVerificationKeyHash);
  policy_fetch_request->set_device_dm_token(kDeviceDMToken);

  return policy_request;
}

em::DeviceManagementResponse GetPolicyResponse() {
  em::DeviceManagementResponse policy_response;
  policy_response.mutable_policy_response()->add_responses()->set_policy_data(
      CreatePolicyData("fake-policy-data"));
  return policy_response;
}

em::DeviceManagementRequest GetRegistrationRequest() {
  em::DeviceManagementRequest request;

  em::DeviceRegisterRequest* register_request =
      request.mutable_register_request();
  register_request->set_type(em::DeviceRegisterRequest::USER);
  register_request->set_machine_id(kMachineID);
  register_request->set_machine_model(kMachineModel);
  register_request->set_brand_code(kBrandCode);
  register_request->mutable_device_register_identification()
      ->set_attested_device_id(kAttestedDeviceId);
  register_request->set_ethernet_mac_address(kEthernetMacAddress);
  register_request->set_dock_mac_address(kDockMacAddress);
  register_request->set_manufacture_date(kManufactureDate);
  register_request->set_lifetime(
      em::DeviceRegisterRequest::LIFETIME_INDEFINITE);
  register_request->set_flavor(
      em::DeviceRegisterRequest::FLAVOR_USER_REGISTRATION);

  return request;
}

em::DeviceManagementResponse GetRegistrationResponse() {
  em::DeviceManagementResponse registration_response;
  registration_response.mutable_register_response()
      ->set_device_management_token(kDMToken);
  return registration_response;
}

em::DeviceManagementRequest GetReregistrationRequest() {
  em::DeviceManagementRequest request;

  em::DeviceRegisterRequest* reregister_request =
      request.mutable_register_request();
  reregister_request->set_type(em::DeviceRegisterRequest::USER);
  reregister_request->set_machine_id(kMachineID);
  reregister_request->set_machine_model(kMachineModel);
  reregister_request->set_brand_code(kBrandCode);
  reregister_request->mutable_device_register_identification()
      ->set_attested_device_id(kAttestedDeviceId);
  reregister_request->set_ethernet_mac_address(kEthernetMacAddress);
  reregister_request->set_dock_mac_address(kDockMacAddress);
  reregister_request->set_manufacture_date(kManufactureDate);
  reregister_request->set_lifetime(
      em::DeviceRegisterRequest::LIFETIME_INDEFINITE);
  reregister_request->set_flavor(
      em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_RECOVERY);
  reregister_request->set_reregister(true);
  reregister_request->set_reregistration_dm_token(kDMToken);

  return request;
}

// Constructs the DeviceManagementRequest with
// CertificateBasedDeviceRegistrationData.
// Also, if |psm_execution_result| or |psm_determination_timestamp| has a value,
// then populate its corresponding PSM field in DeviceRegisterRequest.
em::DeviceManagementRequest GetCertBasedRegistrationRequest(
    FakeSigningService* fake_signing_service,
    absl::optional<PsmExecutionResult> psm_execution_result,
    absl::optional<int64_t> psm_determination_timestamp) {
  em::CertificateBasedDeviceRegistrationData data;
  data.set_certificate_type(em::CertificateBasedDeviceRegistrationData::
                                ENTERPRISE_ENROLLMENT_CERTIFICATE);
  data.set_device_certificate(kEnrollmentCertificate);

  em::DeviceRegisterRequest* register_request =
      data.mutable_device_register_request();
  register_request->set_type(em::DeviceRegisterRequest::DEVICE);
  register_request->set_machine_id(kMachineID);
  register_request->set_machine_model(kMachineModel);
  register_request->set_brand_code(kBrandCode);
  register_request->mutable_device_register_identification()
      ->set_attested_device_id(kAttestedDeviceId);
  register_request->set_ethernet_mac_address(kEthernetMacAddress);
  register_request->set_dock_mac_address(kDockMacAddress);
  register_request->set_manufacture_date(kManufactureDate);
  register_request->set_lifetime(
      em::DeviceRegisterRequest::LIFETIME_INDEFINITE);
  register_request->set_flavor(
      em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_ATTESTATION);
  if (psm_determination_timestamp.has_value()) {
    register_request->set_psm_determination_timestamp_ms(
        psm_determination_timestamp.value());
  }
  if (psm_execution_result.has_value())
    register_request->set_psm_execution_result(psm_execution_result.value());

  em::DeviceManagementRequest request;

  em::CertificateBasedDeviceRegisterRequest* cert_based_register_request =
      request.mutable_certificate_based_register_request();
  fake_signing_service->SignDataSynchronously(
      data.SerializeAsString(),
      cert_based_register_request->mutable_signed_request());

  return request;
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
em::DeviceManagementRequest GetEnrollmentRequest() {
  em::DeviceManagementRequest request;

  em::RegisterBrowserRequest* enrollment_request =
      request.mutable_register_browser_request();
  enrollment_request->set_os_platform(GetOSPlatform());
  enrollment_request->set_os_version(GetOSVersion());
  return request;
}
#endif

em::DeviceManagementRequest GetUnregistrationRequest() {
  em::DeviceManagementRequest unregistration_request;
  // Accessing the field sets the type of the request.
  unregistration_request.mutable_unregister_request();
  return unregistration_request;
}

em::DeviceManagementResponse GetUnregistrationResponse() {
  em::DeviceManagementResponse unregistration_response;
  // Accessing the field sets the type of the response.
  unregistration_response.mutable_unregister_response();
  return unregistration_response;
}

em::DeviceManagementRequest GetUploadMachineCertificateRequest() {
  em::DeviceManagementRequest upload_machine_certificate_request;
  upload_machine_certificate_request.mutable_cert_upload_request()
      ->set_device_certificate(kMachineCertificate);
  upload_machine_certificate_request.mutable_cert_upload_request()
      ->set_certificate_type(
          em::DeviceCertUploadRequest::ENTERPRISE_MACHINE_CERTIFICATE);
  return upload_machine_certificate_request;
}

em::DeviceManagementRequest GetUploadEnrollmentCertificateRequest() {
  em::DeviceManagementRequest upload_enrollment_certificate_request;
  upload_enrollment_certificate_request.mutable_cert_upload_request()
      ->set_device_certificate(kEnrollmentCertificate);
  upload_enrollment_certificate_request.mutable_cert_upload_request()
      ->set_certificate_type(
          em::DeviceCertUploadRequest::ENTERPRISE_ENROLLMENT_CERTIFICATE);
  return upload_enrollment_certificate_request;
}

em::DeviceManagementResponse GetUploadCertificateResponse() {
  em::DeviceManagementResponse upload_certificate_response;
  upload_certificate_response.mutable_cert_upload_response();
  return upload_certificate_response;
}

em::DeviceManagementRequest GetUploadStatusRequest() {
  em::DeviceManagementRequest upload_status_request;
  upload_status_request.mutable_device_status_report_request();
  upload_status_request.mutable_session_status_report_request();
  upload_status_request.mutable_child_status_report_request();
  return upload_status_request;
}

em::DeviceManagementRequest GetRemoteCommandRequest() {
  em::DeviceManagementRequest remote_command_request;

  remote_command_request.mutable_remote_command_request()
      ->set_last_command_unique_id(kLastCommandId);
  em::RemoteCommandResult* command_result =
      remote_command_request.mutable_remote_command_request()
          ->add_command_results();
  command_result->set_command_id(kLastCommandId);
  command_result->set_result(em::RemoteCommandResult_ResultType_RESULT_SUCCESS);
  command_result->set_payload(kResultPayload);
  command_result->set_timestamp(kTimestamp);
  remote_command_request.mutable_remote_command_request()
      ->set_send_secure_commands(true);

  return remote_command_request;
}

em::DeviceManagementRequest GetRobotAuthCodeFetchRequest() {
  em::DeviceManagementRequest robot_auth_code_fetch_request;

  em::DeviceServiceApiAccessRequest* api_request =
      robot_auth_code_fetch_request.mutable_service_api_access_request();
  api_request->set_oauth2_client_id(
      GaiaUrls::GetInstance()->oauth2_chrome_client_id());
  api_request->add_auth_scopes(kApiAuthScope);
  api_request->set_device_type(em::DeviceServiceApiAccessRequest::CHROME_OS);

  return robot_auth_code_fetch_request;
}

em::DeviceManagementResponse GetRobotAuthCodeFetchResponse() {
  em::DeviceManagementResponse robot_auth_code_fetch_response;

  em::DeviceServiceApiAccessResponse* api_response =
      robot_auth_code_fetch_response.mutable_service_api_access_response();
  api_response->set_auth_code(kRobotAuthCode);

  return robot_auth_code_fetch_response;
}

em::DeviceManagementResponse GetEmptyResponse() {
  return em::DeviceManagementResponse();
}

}  // namespace

class CloudPolicyClientTest : public testing::Test {
 protected:
  CloudPolicyClientTest()
      : job_type_(DeviceManagementService::JobConfiguration::TYPE_INVALID),
        client_id_(kClientID),
        policy_type_(dm_protocol::kChromeUserPolicyType) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    fake_statistics_provider_.SetMachineStatistic(
        chromeos::system::kSerialNumberKeyForTest, "fake_serial_number");
#endif
  }

  void SetUp() override { CreateClient(); }

  void TearDown() override { client_->RemoveObserver(&observer_); }

  void RegisterClient(const std::string& device_dm_token) {
    EXPECT_CALL(observer_, OnRegistrationStateChanged);
    EXPECT_CALL(device_dmtoken_callback_observer_,
                OnDeviceDMTokenRequested(
                    /*user_affiliation_ids=*/std::vector<std::string>()))
        .WillOnce(Return(device_dm_token));
    client_->SetupRegistration(kDMToken, client_id_,
                               std::vector<std::string>());
  }

  void RegisterClient() { RegisterClient(kDeviceDMToken); }

  void CreateClient() {
    service_.ScheduleInitialization(0);
    base::RunLoop().RunUntilIdle();

    if (client_)
      client_->RemoveObserver(&observer_);

    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &url_loader_factory_);
    client_ = std::make_unique<CloudPolicyClient>(
        kMachineID, kMachineModel, kBrandCode, kAttestedDeviceId,
        kEthernetMacAddress, kDockMacAddress, kManufactureDate, &service_,
        shared_url_loader_factory_,
        base::BindRepeating(
            &MockDeviceDMTokenCallbackObserver::OnDeviceDMTokenRequested,
            base::Unretained(&device_dmtoken_callback_observer_)));
    client_->AddPolicyTypeToFetch(policy_type_, std::string());
    client_->AddObserver(&observer_);
  }

  base::Value::Dict MakeDefaultRealtimeReport() {
    base::Value::Dict context;
    context.SetByDottedPath("profile.gaiaEmail", "name@gmail.com");
    context.SetByDottedPath("browser.userAgent", "User-Agent");
    context.SetByDottedPath("profile.profileName", "Profile 1");
    context.SetByDottedPath("profile.profilePath", "C:\\User Data\\Profile 1");

    base::Value::Dict event;
    event.Set("time", "2019-05-22T13:01:45Z");
    event.SetByDottedPath("foo.prop1", "value1");
    event.SetByDottedPath("foo.prop2", "value2");
    event.SetByDottedPath("foo.prop3", "value3");

    base::Value::List event_list;
    event_list.Append(std::move(event));
    return policy::RealtimeReportingJobConfiguration::BuildReport(
        std::move(event_list), std::move(context));
  }

  void ExpectAndCaptureJob(const em::DeviceManagementResponse& response) {
    EXPECT_CALL(job_creation_handler_, OnJobCreation)
        .WillOnce(DoAll(service_.CaptureJobType(&job_type_),
                        service_.CaptureAuthData(&auth_data_),
                        service_.CaptureQueryParams(&query_params_),
                        service_.CaptureRequest(&job_request_),
                        service_.SendJobOKAsync(response)));
  }

  void ExpectAndCaptureJSONJob(const std::string& response) {
    EXPECT_CALL(job_creation_handler_, OnJobCreation)
        .WillOnce(DoAll(service_.CaptureJobType(&job_type_),
                        service_.CaptureAuthData(&auth_data_),
                        service_.CaptureQueryParams(&query_params_),
                        service_.CapturePayload(&job_payload_),
                        service_.SendJobOKAsync(response)));
  }

  void ExpectAndCaptureJobReplyFailure(int net_error, int response_code) {
    EXPECT_CALL(job_creation_handler_, OnJobCreation)
        .WillOnce(
            DoAll(service_.CaptureJobType(&job_type_),
                  service_.CaptureAuthData(&auth_data_),
                  service_.CaptureQueryParams(&query_params_),
                  service_.CaptureRequest(&job_request_),
                  service_.SendJobResponseAsync(net_error, response_code)));
  }

  void CheckPolicyResponse(
      const em::DeviceManagementResponse& policy_response) {
    ASSERT_TRUE(client_->GetPolicyFor(policy_type_, std::string()));
    EXPECT_THAT(*client_->GetPolicyFor(policy_type_, std::string()),
                MatchProto(policy_response.policy_response().responses(0)));
  }

  void AttemptUploadEncryptedWaitUntilIdle(
      const ::reporting::EncryptedRecord& record,
      absl::optional<base::Value::Dict> context = absl::nullopt) {
    CloudPolicyClient::ResponseCallback response_callback =
        base::BindOnce(&MockResponseCallbackObserver::OnResponseReceived,
                       base::Unretained(&response_callback_observer_));
    client_->UploadEncryptedReport(ConvertEncryptedRecordToValue(record),
                                   std::move(context),
                                   std::move(response_callback));
    base::RunLoop().RunUntilIdle();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  DeviceManagementService::JobConfiguration::JobType job_type_;
  DeviceManagementService::JobConfiguration::ParameterMap query_params_;
  DMAuth auth_data_;
  em::DeviceManagementRequest job_request_;
  std::string job_payload_;
  std::string client_id_;
  std::string policy_type_;
  StrictMock<MockJobCreationHandler> job_creation_handler_;
  FakeDeviceManagementService service_{&job_creation_handler_};
  StrictMock<MockCloudPolicyClientObserver> observer_;
  StrictMock<MockStatusCallbackObserver> callback_observer_;
  StrictMock<MockDeviceDMTokenCallbackObserver>
      device_dmtoken_callback_observer_;
  StrictMock<MockRobotAuthCodeCallbackObserver>
      robot_auth_code_callback_observer_;
  StrictMock<MockResponseCallbackObserver> response_callback_observer_;
  FakeSigningService fake_signing_service_;
  std::unique_ptr<CloudPolicyClient> client_;
  network::TestURLLoaderFactory url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  chromeos::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
#endif
};

TEST_F(CloudPolicyClientTest, Init) {
  EXPECT_FALSE(client_->is_registered());
  EXPECT_FALSE(client_->GetPolicyFor(policy_type_, std::string()));
  EXPECT_EQ(0, client_->fetched_invalidation_version());
}

TEST_F(CloudPolicyClientTest, SetupRegistrationAndPolicyFetch) {
  const em::DeviceManagementResponse policy_response = GetPolicyResponse();

  EXPECT_CALL(observer_, OnRegistrationStateChanged);
  EXPECT_CALL(device_dmtoken_callback_observer_,
              OnDeviceDMTokenRequested(
                  /*user_affiliation_ids=*/std::vector<std::string>()))
      .WillOnce(Return(kDeviceDMToken));
  client_->SetupRegistration(kDMToken, client_id_, std::vector<std::string>());
  EXPECT_TRUE(client_->is_registered());
  EXPECT_FALSE(client_->GetPolicyFor(policy_type_, std::string()));

  ExpectAndCaptureJob(policy_response);
  EXPECT_CALL(observer_, OnPolicyFetched);
  client_->FetchPolicy();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetPolicyRequest().SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
  CheckPolicyResponse(policy_response);
}

TEST_F(CloudPolicyClientTest, SetupRegistrationAndPolicyFetchWithOAuthToken) {
  const em::DeviceManagementResponse policy_response = GetPolicyResponse();

  EXPECT_CALL(observer_, OnRegistrationStateChanged);
  EXPECT_CALL(device_dmtoken_callback_observer_,
              OnDeviceDMTokenRequested(
                  /*user_affiliation_ids=*/std::vector<std::string>()))
      .WillOnce(Return(kDeviceDMToken));
  client_->SetupRegistration(kDMToken, client_id_, std::vector<std::string>());
  client_->SetOAuthTokenAsAdditionalAuth(kOAuthToken);
  EXPECT_TRUE(client_->is_registered());
  EXPECT_FALSE(client_->GetPolicyFor(policy_type_, std::string()));

  ExpectAndCaptureJob(policy_response);
  EXPECT_CALL(observer_, OnPolicyFetched);
  client_->FetchPolicy();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_THAT(query_params_,
              Contains(Pair(dm_protocol::kParamOAuthToken, kOAuthToken)));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetPolicyRequest().SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
  CheckPolicyResponse(policy_response);
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
TEST_F(CloudPolicyClientTest, RegistrationWithTokenAndPolicyFetch) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kUploadBrowserDeviceIdentifier);

  const em::DeviceManagementResponse policy_response = GetPolicyResponse();

  ExpectAndCaptureJob(GetRegistrationResponse());
  EXPECT_CALL(observer_, OnRegistrationStateChanged);
  EXPECT_CALL(device_dmtoken_callback_observer_,
              OnDeviceDMTokenRequested(
                  /*user_affiliation_ids=*/std::vector<std::string>()))
      .WillOnce(Return(kDeviceDMToken));
  FakeClientDataDelegate client_data_delegate;
  client_->RegisterWithToken(kEnrollmentToken, "device_id",
                             client_data_delegate);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_TOKEN_ENROLLMENT,
            job_type_);
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetEnrollmentRequest().SerializePartialAsString());
  EXPECT_TRUE(client_->is_registered());
  EXPECT_FALSE(client_->GetPolicyFor(policy_type_, std::string()));
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());

  ExpectAndCaptureJob(policy_response);
  EXPECT_CALL(observer_, OnPolicyFetched);
  client_->FetchPolicy();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetPolicyRequest().SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
  CheckPolicyResponse(policy_response);
}
#endif

TEST_F(CloudPolicyClientTest, RegistrationAndPolicyFetch) {
  const em::DeviceManagementResponse policy_response = GetPolicyResponse();

  ExpectAndCaptureJob(GetRegistrationResponse());
  EXPECT_CALL(observer_, OnRegistrationStateChanged);
  EXPECT_CALL(device_dmtoken_callback_observer_,
              OnDeviceDMTokenRequested(
                  /*user_affiliation_ids=*/std::vector<std::string>()))
      .WillOnce(Return(kDeviceDMToken));
  CloudPolicyClient::RegistrationParameters register_user(
      em::DeviceRegisterRequest::USER,
      em::DeviceRegisterRequest::FLAVOR_USER_REGISTRATION);
  client_->Register(register_user, std::string() /* no client_id*/,
                    kOAuthToken);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_REGISTRATION,
            job_type_);
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetRegistrationRequest().SerializePartialAsString());
  EXPECT_EQ(auth_data_, DMAuth::NoAuth());
  EXPECT_THAT(query_params_,
              Contains(Pair(dm_protocol::kParamOAuthToken, kOAuthToken)));
  EXPECT_TRUE(client_->is_registered());
  EXPECT_FALSE(client_->GetPolicyFor(policy_type_, std::string()));
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());

  ExpectAndCaptureJob(policy_response);
  EXPECT_CALL(observer_, OnPolicyFetched);
  client_->FetchPolicy();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetPolicyRequest().SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
  CheckPolicyResponse(policy_response);
}

TEST_F(CloudPolicyClientTest, RegistrationAndPolicyFetchWithOAuthToken) {
  const em::DeviceManagementResponse policy_response = GetPolicyResponse();

  ExpectAndCaptureJob(GetRegistrationResponse());
  EXPECT_CALL(observer_, OnRegistrationStateChanged);
  EXPECT_CALL(device_dmtoken_callback_observer_,
              OnDeviceDMTokenRequested(
                  /*user_affiliation_ids=*/std::vector<std::string>()))
      .WillOnce(Return(kDeviceDMToken));
  CloudPolicyClient::RegistrationParameters register_user(
      em::DeviceRegisterRequest::USER,
      em::DeviceRegisterRequest::FLAVOR_USER_REGISTRATION);
  client_->Register(register_user, std::string() /* no client_id*/,
                    kOAuthToken);
  client_->SetOAuthTokenAsAdditionalAuth(kOAuthToken);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_REGISTRATION,
            job_type_);
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetRegistrationRequest().SerializePartialAsString());
  EXPECT_EQ(auth_data_, DMAuth::NoAuth());
  EXPECT_THAT(query_params_,
              Contains(Pair(dm_protocol::kParamOAuthToken, kOAuthToken)));
  EXPECT_TRUE(client_->is_registered());
  EXPECT_FALSE(client_->GetPolicyFor(policy_type_, std::string()));
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());

  ExpectAndCaptureJob(policy_response);
  EXPECT_CALL(observer_, OnPolicyFetched);
  client_->FetchPolicy();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_THAT(query_params_,
              Contains(Pair(dm_protocol::kParamOAuthToken, kOAuthToken)));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetPolicyRequest().SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
  CheckPolicyResponse(policy_response);
}

TEST_F(CloudPolicyClientTest, RegistrationWithCertificateAndPolicyFetch) {
  const em::DeviceManagementResponse policy_response = GetPolicyResponse();

  EXPECT_CALL(device_dmtoken_callback_observer_,
              OnDeviceDMTokenRequested(
                  /*user_affiliation_ids=*/std::vector<std::string>()))
      .WillOnce(Return(kDeviceDMToken));
  ExpectAndCaptureJob(GetRegistrationResponse());
  fake_signing_service_.set_success(true);
  EXPECT_CALL(observer_, OnRegistrationStateChanged);
  CloudPolicyClient::RegistrationParameters device_attestation(
      em::DeviceRegisterRequest::DEVICE,
      em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_ATTESTATION);
  client_->RegisterWithCertificate(
      device_attestation, std::string() /* client_id */, kEnrollmentCertificate,
      std::string() /* sub_organization */, &fake_signing_service_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      DeviceManagementService::JobConfiguration::TYPE_CERT_BASED_REGISTRATION,
      job_type_);
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetCertBasedRegistrationRequest(
                &fake_signing_service_,
                /*psm_execution_result=*/absl::nullopt,
                /*psm_determination_timestamp=*/absl::nullopt)
                .SerializePartialAsString());
  EXPECT_TRUE(client_->is_registered());
  EXPECT_FALSE(client_->GetPolicyFor(policy_type_, std::string()));
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());

  ExpectAndCaptureJob(policy_response);
  EXPECT_CALL(observer_, OnPolicyFetched);
  client_->FetchPolicy();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetPolicyRequest().SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
  CheckPolicyResponse(policy_response);
}

TEST_F(CloudPolicyClientTest, RegistrationWithCertificateFailToSignRequest) {
  fake_signing_service_.set_success(false);
  EXPECT_CALL(observer_, OnClientError);
  CloudPolicyClient::RegistrationParameters device_attestation(
      em::DeviceRegisterRequest::DEVICE,
      em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_ATTESTATION);
  client_->RegisterWithCertificate(
      device_attestation, std::string() /* client_id */, kEnrollmentCertificate,
      std::string() /* sub_organization */, &fake_signing_service_);
  EXPECT_FALSE(client_->is_registered());
  EXPECT_EQ(DM_STATUS_CANNOT_SIGN_REQUEST, client_->status());
}

TEST_F(CloudPolicyClientTest, RegistrationParametersPassedThrough) {
  em::DeviceManagementRequest registration_request = GetRegistrationRequest();
  registration_request.mutable_register_request()->set_reregister(true);
  registration_request.mutable_register_request()->set_requisition(
      kRequisition);
  registration_request.mutable_register_request()->set_server_backed_state_key(
      kStateKey);
  registration_request.mutable_register_request()->set_flavor(
      em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_MANUAL);
  ExpectAndCaptureJob(GetRegistrationResponse());
  EXPECT_CALL(observer_, OnRegistrationStateChanged);
  EXPECT_CALL(device_dmtoken_callback_observer_,
              OnDeviceDMTokenRequested(
                  /*user_affiliation_ids=*/std::vector<std::string>()))
      .WillOnce(Return(kDeviceDMToken));

  CloudPolicyClient::RegistrationParameters register_parameters(
      em::DeviceRegisterRequest::USER,
      em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_MANUAL);
  register_parameters.requisition = kRequisition;
  register_parameters.current_state_key = kStateKey;

  client_->Register(register_parameters, kClientID, kOAuthToken);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_REGISTRATION,
            job_type_);
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            registration_request.SerializePartialAsString());
  EXPECT_EQ(auth_data_, DMAuth::NoAuth());
  EXPECT_THAT(query_params_,
              Contains(Pair(dm_protocol::kParamOAuthToken, kOAuthToken)));
  EXPECT_EQ(kClientID, client_id_);
}

TEST_F(CloudPolicyClientTest, RegistrationNoToken) {
  em::DeviceManagementResponse registration_response =
      GetRegistrationResponse();
  registration_response.mutable_register_response()
      ->clear_device_management_token();
  ExpectAndCaptureJob(registration_response);
  EXPECT_CALL(observer_, OnClientError);
  CloudPolicyClient::RegistrationParameters register_user(
      em::DeviceRegisterRequest::USER,
      em::DeviceRegisterRequest::FLAVOR_USER_REGISTRATION);
  client_->Register(register_user, std::string() /* no client_id*/,
                    kOAuthToken);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_REGISTRATION,
            job_type_);
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetRegistrationRequest().SerializePartialAsString());
  EXPECT_EQ(auth_data_, DMAuth::NoAuth());
  EXPECT_THAT(query_params_,
              Contains(Pair(dm_protocol::kParamOAuthToken, kOAuthToken)));
  EXPECT_FALSE(client_->is_registered());
  EXPECT_FALSE(client_->GetPolicyFor(policy_type_, std::string()));
  EXPECT_EQ(DM_STATUS_RESPONSE_DECODING_ERROR, client_->status());
}

TEST_F(CloudPolicyClientTest, RegistrationFailure) {
  DeviceManagementService::JobConfiguration::JobType job_type;
  EXPECT_CALL(job_creation_handler_, OnJobCreation)
      .WillOnce(DoAll(
          service_.CaptureJobType(&job_type),
          service_.SendJobResponseAsync(
              net::ERR_FAILED, DeviceManagementService::kInvalidArgument)));
  EXPECT_CALL(observer_, OnClientError);
  CloudPolicyClient::RegistrationParameters register_user(
      em::DeviceRegisterRequest::USER,
      em::DeviceRegisterRequest::FLAVOR_USER_REGISTRATION);
  client_->Register(register_user, std::string() /* no client_id*/,
                    kOAuthToken);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_REGISTRATION,
            job_type);
  EXPECT_FALSE(client_->is_registered());
  EXPECT_FALSE(client_->GetPolicyFor(policy_type_, std::string()));
  EXPECT_EQ(DM_STATUS_REQUEST_FAILED, client_->status());
}

TEST_F(CloudPolicyClientTest, RetryRegistration) {
  // Force the register to fail with an error that causes a retry.
  enterprise_management::DeviceManagementRequest request;
  DeviceManagementService::JobConfiguration::JobType job_type;
  DeviceManagementService::JobForTesting job;
  EXPECT_CALL(job_creation_handler_, OnJobCreation)
      .WillOnce(DoAll(service_.CaptureJobType(&job_type),
                      service_.CaptureRequest(&request), SaveArg<0>(&job)));
  CloudPolicyClient::RegistrationParameters register_user(
      em::DeviceRegisterRequest::USER,
      em::DeviceRegisterRequest::FLAVOR_USER_REGISTRATION);
  client_->Register(register_user, std::string() /* no client_id*/,
                    kOAuthToken);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_REGISTRATION,
            job_type);
  EXPECT_EQ(GetRegistrationRequest().SerializePartialAsString(),
            request.SerializePartialAsString());
  EXPECT_FALSE(request.register_request().reregister());
  EXPECT_FALSE(client_->is_registered());
  Mock::VerifyAndClearExpectations(&service_);

  // Retry with network errors |DeviceManagementService::kMaxRetries| times.
  for (int i = 0; i < DeviceManagementService::kMaxRetries; ++i) {
    service_.SendJobResponseNow(&job, net::ERR_NETWORK_CHANGED, 0);
    ASSERT_TRUE(job.IsActive());
    request.ParseFromString(job.GetConfigurationForTesting()->GetPayload());
    EXPECT_TRUE(request.register_request().reregister());
  }

  // Expect failure with yet another retry.
  EXPECT_CALL(observer_, OnClientError);
  service_.SendJobResponseNow(&job, net::ERR_NETWORK_CHANGED, 0);
  EXPECT_FALSE(job.IsActive());
  EXPECT_FALSE(client_->is_registered());
  base::RunLoop().RunUntilIdle();
}

TEST_F(CloudPolicyClientTest, PolicyUpdate) {
  RegisterClient();

  const em::DeviceManagementRequest policy_request = GetPolicyRequest();

  {
    const em::DeviceManagementResponse policy_response = GetPolicyResponse();

    ExpectAndCaptureJob(policy_response);
    EXPECT_CALL(observer_, OnPolicyFetched);
    client_->FetchPolicy();
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
              job_type_);
    EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
    EXPECT_EQ(job_request_.SerializePartialAsString(),
              policy_request.SerializePartialAsString());
    CheckPolicyResponse(policy_response);
  }

  {
    em::DeviceManagementResponse policy_response;
    policy_response.mutable_policy_response()->add_responses()->set_policy_data(
        CreatePolicyData("updated-fake-policy-data"));

    ExpectAndCaptureJob(policy_response);
    EXPECT_CALL(observer_, OnPolicyFetched);
    client_->FetchPolicy();
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
              job_type_);
    EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
    EXPECT_EQ(job_request_.SerializePartialAsString(),
              policy_request.SerializePartialAsString());
    EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
    CheckPolicyResponse(policy_response);
  }
}

TEST_F(CloudPolicyClientTest, PolicyFetchWithMetaData) {
  RegisterClient();

  const int kPublicKeyVersion = 42;
  const base::Time kOldTimestamp(base::Time::UnixEpoch() + base::Days(20));

  em::DeviceManagementRequest policy_request = GetPolicyRequest();
  em::PolicyFetchRequest* policy_fetch_request =
      policy_request.mutable_policy_request()->mutable_requests(0);
  policy_fetch_request->set_timestamp(kOldTimestamp.ToJavaTime());
  policy_fetch_request->set_public_key_version(kPublicKeyVersion);

  em::DeviceManagementResponse policy_response = GetPolicyResponse();

  client_->set_last_policy_timestamp(kOldTimestamp);
  client_->set_public_key_version(kPublicKeyVersion);

  ExpectAndCaptureJob(policy_response);
  EXPECT_CALL(observer_, OnPolicyFetched);
  client_->FetchPolicy();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            policy_request.SerializePartialAsString());
  CheckPolicyResponse(policy_response);
}

TEST_F(CloudPolicyClientTest, PolicyFetchWithInvalidation) {
  RegisterClient();

  const int64_t kInvalidationVersion = 12345;
  const std::string kInvalidationPayload("12345");

  em::DeviceManagementRequest policy_request = GetPolicyRequest();
  em::PolicyFetchRequest* policy_fetch_request =
      policy_request.mutable_policy_request()->mutable_requests(0);
  policy_fetch_request->set_invalidation_version(kInvalidationVersion);
  policy_fetch_request->set_invalidation_payload(kInvalidationPayload);

  em::DeviceManagementResponse policy_response = GetPolicyResponse();

  int64_t previous_version = client_->fetched_invalidation_version();
  client_->SetInvalidationInfo(kInvalidationVersion, kInvalidationPayload);
  EXPECT_EQ(previous_version, client_->fetched_invalidation_version());

  ExpectAndCaptureJob(policy_response);
  EXPECT_CALL(observer_, OnPolicyFetched);
  client_->FetchPolicy();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            policy_request.SerializePartialAsString());
  CheckPolicyResponse(policy_response);
  EXPECT_EQ(12345, client_->fetched_invalidation_version());
}

TEST_F(CloudPolicyClientTest, PolicyFetchWithInvalidationNoPayload) {
  RegisterClient();

  const em::DeviceManagementResponse policy_response = GetPolicyResponse();

  int64_t previous_version = client_->fetched_invalidation_version();
  client_->SetInvalidationInfo(-12345, std::string());
  EXPECT_EQ(previous_version, client_->fetched_invalidation_version());

  ExpectAndCaptureJob(policy_response);
  EXPECT_CALL(observer_, OnPolicyFetched);
  client_->FetchPolicy();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetPolicyRequest().SerializePartialAsString());
  CheckPolicyResponse(policy_response);
  EXPECT_EQ(-12345, client_->fetched_invalidation_version());
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
TEST_F(CloudPolicyClientTest, PolicyFetchWithBrowserDeviceIdentifier) {
  RegisterClient();

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kUploadBrowserDeviceIdentifier);

  // Add the policy type that contains browser device identifier.
  client_->AddPolicyTypeToFetch(
      dm_protocol::kChromeMachineLevelUserCloudPolicyType, std::string());

  // Make a policy fetch.
  ExpectAndCaptureJob(GetPolicyResponse());
  EXPECT_CALL(observer_, OnPolicyFetched);
  client_->FetchPolicy();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type_);

  // Collect the policy requests.
  ASSERT_TRUE(job_request_.has_policy_request());
  const em::DevicePolicyRequest& policy_request = job_request_.policy_request();
  std::map<std::pair<std::string, std::string>, em::PolicyFetchRequest>
      request_map;
  for (int i = 0; i < policy_request.requests_size(); ++i) {
    const em::PolicyFetchRequest& fetch_request = policy_request.requests(i);
    ASSERT_TRUE(fetch_request.has_policy_type());
    std::pair<std::string, std::string> key(fetch_request.policy_type(),
                                            fetch_request.settings_entity_id());
    EXPECT_EQ(0UL, request_map.count(key));
    request_map[key].CopyFrom(fetch_request);
  }

  std::map<std::pair<std::string, std::string>, em::PolicyFetchRequest>
      expected_requests;
  // Expected user policy fetch request.
  std::pair<std::string, std::string> user_policy_key(
      dm_protocol::kChromeUserPolicyType, std::string());
  expected_requests[user_policy_key] =
      GetPolicyRequest().policy_request().requests(0);
  // Expected user cloud policy fetch request.
  std::pair<std::string, std::string> user_cloud_policy_key(
      dm_protocol::kChromeMachineLevelUserCloudPolicyType, std::string());
  em::PolicyFetchRequest policy_fetch_request =
      GetPolicyRequest().policy_request().requests(0);
  policy_fetch_request.set_policy_type(
      dm_protocol::kChromeMachineLevelUserCloudPolicyType);
  policy_fetch_request.set_allocated_browser_device_identifier(
      GetBrowserDeviceIdentifier().release());
  expected_requests[user_cloud_policy_key] = policy_fetch_request;

  EXPECT_EQ(request_map.size(), expected_requests.size());
  for (auto it = expected_requests.begin(); it != expected_requests.end();
       ++it) {
    EXPECT_EQ(1UL, request_map.count(it->first));
    EXPECT_EQ(request_map[it->first].SerializePartialAsString(),
              it->second.SerializePartialAsString());
  }
}
#endif

// Tests that previous OAuth token is no longer sent in policy fetch after its
// value was cleared.
TEST_F(CloudPolicyClientTest, PolicyFetchClearOAuthToken) {
  RegisterClient();

  em::DeviceManagementRequest policy_request = GetPolicyRequest();
  const em::DeviceManagementResponse policy_response = GetPolicyResponse();

  ExpectAndCaptureJob(policy_response);
  EXPECT_CALL(observer_, OnPolicyFetched);
  client_->SetOAuthTokenAsAdditionalAuth(kOAuthToken);
  client_->FetchPolicy();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_THAT(query_params_,
              Contains(Pair(dm_protocol::kParamOAuthToken, kOAuthToken)));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            policy_request.SerializePartialAsString());
  CheckPolicyResponse(policy_response);

  ExpectAndCaptureJob(policy_response);
  EXPECT_CALL(observer_, OnPolicyFetched);
  client_->SetOAuthTokenAsAdditionalAuth("");
  client_->FetchPolicy();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            policy_request.SerializePartialAsString());
  CheckPolicyResponse(policy_response);
}

TEST_F(CloudPolicyClientTest, BadPolicyResponse) {
  RegisterClient();

  const em::DeviceManagementRequest policy_request = GetPolicyRequest();
  em::DeviceManagementResponse policy_response;

  ExpectAndCaptureJob(policy_response);
  EXPECT_CALL(observer_, OnClientError);
  client_->FetchPolicy();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            policy_request.SerializePartialAsString());
  EXPECT_FALSE(client_->GetPolicyFor(policy_type_, std::string()));
  EXPECT_EQ(DM_STATUS_RESPONSE_DECODING_ERROR, client_->status());

  policy_response.mutable_policy_response()->add_responses()->set_policy_data(
      CreatePolicyData("fake-policy-data"));
  policy_response.mutable_policy_response()->add_responses()->set_policy_data(
      CreatePolicyData("excess-fake-policy-data"));
  ExpectAndCaptureJob(policy_response);
  EXPECT_CALL(observer_, OnPolicyFetched);
  client_->FetchPolicy();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            policy_request.SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
  CheckPolicyResponse(policy_response);
}

TEST_F(CloudPolicyClientTest, PolicyRequestFailure) {
  RegisterClient();

  DeviceManagementService::JobConfiguration::JobType job_type;
  EXPECT_CALL(job_creation_handler_, OnJobCreation)
      .WillOnce(DoAll(
          service_.CaptureJobType(&job_type),
          service_.SendJobResponseAsync(
              net::ERR_FAILED, DeviceManagementService::kInvalidArgument)));
  EXPECT_CALL(observer_, OnClientError);
  client_->FetchPolicy();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type);
  EXPECT_EQ(DM_STATUS_REQUEST_FAILED, client_->status());
  EXPECT_FALSE(client_->GetPolicyFor(policy_type_, std::string()));
}

TEST_F(CloudPolicyClientTest, Unregister) {
  RegisterClient();

  ExpectAndCaptureJob(GetUnregistrationResponse());
  EXPECT_CALL(observer_, OnRegistrationStateChanged);
  client_->Unregister();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_UNREGISTRATION,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetUnregistrationRequest().SerializePartialAsString());
  EXPECT_FALSE(client_->is_registered());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
}

TEST_F(CloudPolicyClientTest, UnregisterEmpty) {
  RegisterClient();

  DeviceManagementService::JobConfiguration::JobType job_type;
  em::DeviceManagementResponse unregistration_response =
      GetUnregistrationResponse();
  unregistration_response.clear_unregister_response();
  EXPECT_CALL(job_creation_handler_, OnJobCreation)
      .WillOnce(DoAll(service_.CaptureJobType(&job_type),
                      service_.SendJobOKAsync(unregistration_response)));
  EXPECT_CALL(observer_, OnRegistrationStateChanged);
  client_->Unregister();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_UNREGISTRATION,
            job_type);
  EXPECT_FALSE(client_->is_registered());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
}

TEST_F(CloudPolicyClientTest, UnregisterFailure) {
  RegisterClient();

  DeviceManagementService::JobConfiguration::JobType job_type;
  EXPECT_CALL(job_creation_handler_, OnJobCreation)
      .WillOnce(DoAll(
          service_.CaptureJobType(&job_type),
          service_.SendJobResponseAsync(
              net::ERR_FAILED, DeviceManagementService::kInvalidArgument)));
  EXPECT_CALL(observer_, OnClientError);
  client_->Unregister();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_UNREGISTRATION,
            job_type);
  EXPECT_TRUE(client_->is_registered());
  EXPECT_EQ(DM_STATUS_REQUEST_FAILED, client_->status());
}

TEST_F(CloudPolicyClientTest, PolicyFetchWithExtensionPolicy) {
  RegisterClient();

  em::DeviceManagementResponse policy_response = GetPolicyResponse();

  // Set up the |expected_responses| and |policy_response|.
  static const char* kExtensions[] = {
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
      "cccccccccccccccccccccccccccccccc",
  };
  typedef std::map<std::pair<std::string, std::string>, em::PolicyFetchResponse>
      ResponseMap;
  ResponseMap expected_responses;
  std::set<std::pair<std::string, std::string>> expected_namespaces;
  std::pair<std::string, std::string> key(dm_protocol::kChromeUserPolicyType,
                                          std::string());
  // Copy the user policy fetch request.
  expected_responses[key].CopyFrom(
      policy_response.policy_response().responses(0));
  expected_namespaces.insert(key);
  key.first = dm_protocol::kChromeExtensionPolicyType;
  expected_namespaces.insert(key);
  for (size_t i = 0; i < base::size(kExtensions); ++i) {
    key.second = kExtensions[i];
    em::PolicyData policy_data;
    policy_data.set_policy_type(key.first);
    policy_data.set_settings_entity_id(key.second);
    expected_responses[key].set_policy_data(policy_data.SerializeAsString());
    policy_response.mutable_policy_response()->add_responses()->CopyFrom(
        expected_responses[key]);
  }

  // Make a policy fetch.
  em::DeviceManagementRequest request;
  DeviceManagementService::JobConfiguration::JobType job_type;
  EXPECT_CALL(job_creation_handler_, OnJobCreation)
      .WillOnce(DoAll(service_.CaptureJobType(&job_type),
                      service_.CaptureRequest(&request),
                      service_.SendJobOKAsync(policy_response)));
  EXPECT_CALL(observer_, OnPolicyFetched);
  client_->AddPolicyTypeToFetch(dm_protocol::kChromeExtensionPolicyType,
                                std::string());
  client_->FetchPolicy();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type);

  // Verify that the request includes the expected namespaces.
  ASSERT_TRUE(request.has_policy_request());
  const em::DevicePolicyRequest& policy_request = request.policy_request();
  ASSERT_EQ(2, policy_request.requests_size());
  for (int i = 0; i < policy_request.requests_size(); ++i) {
    const em::PolicyFetchRequest& fetch_request = policy_request.requests(i);
    ASSERT_TRUE(fetch_request.has_policy_type());
    EXPECT_FALSE(fetch_request.has_settings_entity_id());
    key = {fetch_request.policy_type(), std::string()};
    EXPECT_EQ(1u, expected_namespaces.erase(key));
  }
  EXPECT_TRUE(expected_namespaces.empty());

  // Verify that the client got all the responses mapped to their namespaces.
  for (auto it = expected_responses.begin(); it != expected_responses.end();
       ++it) {
    const em::PolicyFetchResponse* response =
        client_->GetPolicyFor(it->first.first, it->first.second);
    ASSERT_TRUE(response);
    EXPECT_EQ(it->second.SerializeAsString(), response->SerializeAsString());
  }
}

TEST_F(CloudPolicyClientTest, UploadEnterpriseMachineCertificate) {
  RegisterClient();

  ExpectAndCaptureJob(GetUploadCertificateResponse());
  EXPECT_CALL(callback_observer_, OnCallbackComplete(true)).Times(1);
  CloudPolicyClient::StatusCallback callback =
      base::BindOnce(&MockStatusCallbackObserver::OnCallbackComplete,
                     base::Unretained(&callback_observer_));
  client_->UploadEnterpriseMachineCertificate(kMachineCertificate,
                                              std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_UPLOAD_CERTIFICATE,
            job_type_);
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetUploadMachineCertificateRequest().SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
}

TEST_F(CloudPolicyClientTest, UploadEnterpriseEnrollmentCertificate) {
  RegisterClient();

  ExpectAndCaptureJob(GetUploadCertificateResponse());
  EXPECT_CALL(callback_observer_, OnCallbackComplete(true)).Times(1);
  CloudPolicyClient::StatusCallback callback =
      base::BindOnce(&MockStatusCallbackObserver::OnCallbackComplete,
                     base::Unretained(&callback_observer_));
  client_->UploadEnterpriseEnrollmentCertificate(kEnrollmentCertificate,
                                                 std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_UPLOAD_CERTIFICATE,
            job_type_);
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetUploadEnrollmentCertificateRequest().SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
}

TEST_F(CloudPolicyClientTest, UploadEnterpriseMachineCertificateEmpty) {
  RegisterClient();

  em::DeviceManagementResponse upload_certificate_response =
      GetUploadCertificateResponse();
  upload_certificate_response.clear_cert_upload_response();
  ExpectAndCaptureJob(upload_certificate_response);
  EXPECT_CALL(callback_observer_, OnCallbackComplete(false)).Times(1);
  CloudPolicyClient::StatusCallback callback =
      base::BindOnce(&MockStatusCallbackObserver::OnCallbackComplete,
                     base::Unretained(&callback_observer_));
  client_->UploadEnterpriseMachineCertificate(kMachineCertificate,
                                              std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_UPLOAD_CERTIFICATE,
            job_type_);
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetUploadMachineCertificateRequest().SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
}

TEST_F(CloudPolicyClientTest, UploadEnterpriseEnrollmentCertificateEmpty) {
  RegisterClient();

  em::DeviceManagementResponse upload_certificate_response =
      GetUploadCertificateResponse();
  upload_certificate_response.clear_cert_upload_response();
  ExpectAndCaptureJob(upload_certificate_response);
  EXPECT_CALL(callback_observer_, OnCallbackComplete(false)).Times(1);
  CloudPolicyClient::StatusCallback callback =
      base::BindOnce(&MockStatusCallbackObserver::OnCallbackComplete,
                     base::Unretained(&callback_observer_));
  client_->UploadEnterpriseEnrollmentCertificate(kEnrollmentCertificate,
                                                 std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_UPLOAD_CERTIFICATE,
            job_type_);
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetUploadEnrollmentCertificateRequest().SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
}

TEST_F(CloudPolicyClientTest, UploadCertificateFailure) {
  RegisterClient();

  DeviceManagementService::JobConfiguration::JobType job_type;
  EXPECT_CALL(callback_observer_, OnCallbackComplete(false)).Times(1);
  EXPECT_CALL(job_creation_handler_, OnJobCreation)
      .WillOnce(DoAll(
          service_.CaptureJobType(&job_type),
          service_.SendJobResponseAsync(
              net::ERR_FAILED, DeviceManagementService::kInvalidArgument)));
  EXPECT_CALL(observer_, OnClientError);
  CloudPolicyClient::StatusCallback callback =
      base::BindOnce(&MockStatusCallbackObserver::OnCallbackComplete,
                     base::Unretained(&callback_observer_));
  client_->UploadEnterpriseMachineCertificate(kMachineCertificate,
                                              std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_UPLOAD_CERTIFICATE,
            job_type);
  EXPECT_EQ(DM_STATUS_REQUEST_FAILED, client_->status());
}

TEST_F(CloudPolicyClientTest, UploadEnterpriseEnrollmentId) {
  RegisterClient();

  em::DeviceManagementRequest upload_enrollment_id_request;
  upload_enrollment_id_request.mutable_cert_upload_request()->set_enrollment_id(
      kEnrollmentId);

  ExpectAndCaptureJob(GetUploadCertificateResponse());
  EXPECT_CALL(callback_observer_, OnCallbackComplete(true)).Times(1);
  CloudPolicyClient::StatusCallback callback =
      base::BindOnce(&MockStatusCallbackObserver::OnCallbackComplete,
                     base::Unretained(&callback_observer_));
  client_->UploadEnterpriseEnrollmentId(kEnrollmentId, std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_UPLOAD_CERTIFICATE,
            job_type_);
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            upload_enrollment_id_request.SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
}

TEST_F(CloudPolicyClientTest, UploadStatus) {
  RegisterClient();

  ExpectAndCaptureJob(GetEmptyResponse());
  EXPECT_CALL(callback_observer_, OnCallbackComplete(true)).Times(1);
  CloudPolicyClient::StatusCallback callback =
      base::BindOnce(&MockStatusCallbackObserver::OnCallbackComplete,
                     base::Unretained(&callback_observer_));
  em::DeviceStatusReportRequest device_status;
  em::SessionStatusReportRequest session_status;
  em::ChildStatusReportRequest child_status;
  client_->UploadDeviceStatus(&device_status, &session_status, &child_status,
                              std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_UPLOAD_STATUS,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetUploadStatusRequest().SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
}

TEST_F(CloudPolicyClientTest, UploadStatusWithOAuthToken) {
  RegisterClient();

  // Test that OAuth token is sent in status upload.
  client_->SetOAuthTokenAsAdditionalAuth(kOAuthToken);

  ExpectAndCaptureJob(GetEmptyResponse());
  EXPECT_CALL(callback_observer_, OnCallbackComplete(true)).Times(1);
  CloudPolicyClient::StatusCallback callback =
      base::BindOnce(&MockStatusCallbackObserver::OnCallbackComplete,
                     base::Unretained(&callback_observer_));
  em::DeviceStatusReportRequest device_status;
  em::SessionStatusReportRequest session_status;
  em::ChildStatusReportRequest child_status;
  client_->UploadDeviceStatus(&device_status, &session_status, &child_status,
                              std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_UPLOAD_STATUS,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_THAT(query_params_,
              Contains(Pair(dm_protocol::kParamOAuthToken, kOAuthToken)));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetUploadStatusRequest().SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());

  // Tests that previous OAuth token is no longer sent in status upload after
  // its value was cleared.
  client_->SetOAuthTokenAsAdditionalAuth("");

  callback = base::BindOnce(&MockStatusCallbackObserver::OnCallbackComplete,
                            base::Unretained(&callback_observer_));
  ExpectAndCaptureJob(GetEmptyResponse());
  EXPECT_CALL(callback_observer_, OnCallbackComplete(true)).Times(1);
  client_->UploadDeviceStatus(&device_status, &session_status, &child_status,
                              std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_UPLOAD_STATUS,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_THAT(query_params_,
              Not(Contains(Pair(dm_protocol::kParamOAuthToken, kOAuthToken))));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetUploadStatusRequest().SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
}

TEST_F(CloudPolicyClientTest, UploadStatusWhilePolicyFetchActive) {
  RegisterClient();

  DeviceManagementService::JobConfiguration::JobType job_type;
  EXPECT_CALL(job_creation_handler_, OnJobCreation)
      .WillOnce(DoAll(service_.CaptureJobType(&job_type),
                      service_.SendJobOKAsync(GetEmptyResponse())));
  EXPECT_CALL(callback_observer_, OnCallbackComplete(true)).Times(1);
  CloudPolicyClient::StatusCallback callback =
      base::BindOnce(&MockStatusCallbackObserver::OnCallbackComplete,
                     base::Unretained(&callback_observer_));
  em::DeviceStatusReportRequest device_status;
  em::SessionStatusReportRequest session_status;
  em::ChildStatusReportRequest child_status;
  client_->UploadDeviceStatus(&device_status, &session_status, &child_status,
                              std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_UPLOAD_STATUS,
            job_type);

  // Now initiate a policy fetch - this should not cancel the upload job.
  const em::DeviceManagementResponse policy_response = GetPolicyResponse();

  ExpectAndCaptureJob(policy_response);
  EXPECT_CALL(observer_, OnPolicyFetched);
  client_->FetchPolicy();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetPolicyRequest().SerializePartialAsString());
  CheckPolicyResponse(policy_response);

  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
}

TEST_F(CloudPolicyClientTest, UploadPolicyValidationReport) {
  RegisterClient();

  em::DeviceManagementRequest upload_policy_validation_report_request;
  {
    em::PolicyValidationReportRequest* policy_validation_report_request =
        upload_policy_validation_report_request
            .mutable_policy_validation_report_request();
    policy_validation_report_request->set_policy_type(policy_type_);
    policy_validation_report_request->set_policy_token(kPolicyToken);
    policy_validation_report_request->set_validation_result_type(
        em::PolicyValidationReportRequest::
            VALIDATION_RESULT_TYPE_VALUE_WARNING);
    em::PolicyValueValidationIssue* policy_value_validation_issue =
        policy_validation_report_request->add_policy_value_validation_issues();
    policy_value_validation_issue->set_policy_name(kPolicyName);
    policy_value_validation_issue->set_severity(
        em::PolicyValueValidationIssue::
            VALUE_VALIDATION_ISSUE_SEVERITY_WARNING);
    policy_value_validation_issue->set_debug_message(kValueValidationMessage);
  }

  ExpectAndCaptureJob(GetEmptyResponse());
  std::vector<ValueValidationIssue> issues;
  issues.push_back(
      {kPolicyName, ValueValidationIssue::kWarning, kValueValidationMessage});
  client_->UploadPolicyValidationReport(
      CloudPolicyValidatorBase::VALIDATION_VALUE_WARNING, issues, policy_type_,
      kPolicyToken);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::
                TYPE_UPLOAD_POLICY_VALIDATION_REPORT,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            upload_policy_validation_report_request.SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
}

TEST_F(CloudPolicyClientTest, UploadChromeDesktopReport) {
  RegisterClient();

  em::DeviceManagementRequest chrome_desktop_report_request;
  chrome_desktop_report_request.mutable_chrome_desktop_report_request();

  ExpectAndCaptureJob(GetEmptyResponse());
  EXPECT_CALL(callback_observer_, OnCallbackComplete(true)).Times(1);
  CloudPolicyClient::StatusCallback callback =
      base::BindOnce(&MockStatusCallbackObserver::OnCallbackComplete,
                     base::Unretained(&callback_observer_));
  std::unique_ptr<em::ChromeDesktopReportRequest> chrome_desktop_report =
      std::make_unique<em::ChromeDesktopReportRequest>();
  client_->UploadChromeDesktopReport(std::move(chrome_desktop_report),
                                     std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      DeviceManagementService::JobConfiguration::TYPE_CHROME_DESKTOP_REPORT,
      job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            chrome_desktop_report_request.SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
}

TEST_F(CloudPolicyClientTest, UploadChromeOsUserReport) {
  RegisterClient();

  em::DeviceManagementRequest chrome_os_user_report_request;
  chrome_os_user_report_request.mutable_chrome_os_user_report_request();

  ExpectAndCaptureJob(GetEmptyResponse());
  EXPECT_CALL(callback_observer_, OnCallbackComplete(true)).Times(1);
  CloudPolicyClient::StatusCallback callback =
      base::BindOnce(&MockStatusCallbackObserver::OnCallbackComplete,
                     base::Unretained(&callback_observer_));
  std::unique_ptr<em::ChromeOsUserReportRequest> chrome_os_user_report =
      std::make_unique<em::ChromeOsUserReportRequest>();
  client_->UploadChromeOsUserReport(std::move(chrome_os_user_report),
                                    std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      DeviceManagementService::JobConfiguration::TYPE_CHROME_OS_USER_REPORT,
      job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            chrome_os_user_report_request.SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
}

TEST_F(CloudPolicyClientTest, UploadChromeProfileReport) {
  RegisterClient();

  em::DeviceManagementRequest device_managment_request;
  device_managment_request.mutable_chrome_profile_report_request()
      ->mutable_os_report()
      ->set_name(kOsName);

  ExpectAndCaptureJob(GetEmptyResponse());
  EXPECT_CALL(callback_observer_, OnCallbackComplete(true)).Times(1);
  CloudPolicyClient::StatusCallback callback =
      base::BindOnce(&MockStatusCallbackObserver::OnCallbackComplete,
                     base::Unretained(&callback_observer_));
  auto chrome_profile_report =
      std::make_unique<em::ChromeProfileReportRequest>();
  chrome_profile_report->mutable_os_report()->set_name(kOsName);
  client_->UploadChromeProfileReport(std::move(chrome_profile_report),
                                     std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      DeviceManagementService::JobConfiguration::TYPE_CHROME_PROFILE_REPORT,
      job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            device_managment_request.SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
}

// A helper class to test all em::DeviceRegisterRequest::PsmExecutionResult enum
// values.
class CloudPolicyClientRegisterWithPsmParamsTest
    : public CloudPolicyClientTest,
      public testing::WithParamInterface<PsmExecutionResult> {
 public:
  PsmExecutionResult GetPsmExecutionResult() const { return GetParam(); }
};

TEST_P(CloudPolicyClientRegisterWithPsmParamsTest,
       RegistrationWithCertificateAndPsmResult) {
  const int64_t kExpectedPsmDeterminationTimestamp = 2;

  const em::DeviceManagementResponse policy_response = GetPolicyResponse();
  const PsmExecutionResult psm_execution_result = GetPsmExecutionResult();

  EXPECT_CALL(device_dmtoken_callback_observer_,
              OnDeviceDMTokenRequested(
                  /*user_affiliation_ids=*/std::vector<std::string>()))
      .WillOnce(Return(kDeviceDMToken));
  ExpectAndCaptureJob(GetRegistrationResponse());
  fake_signing_service_.set_success(true);
  EXPECT_CALL(observer_, OnRegistrationStateChanged);
  CloudPolicyClient::RegistrationParameters device_attestation(
      em::DeviceRegisterRequest::DEVICE,
      em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_ATTESTATION);
  device_attestation.SetPsmDeterminationTimestamp(
      kExpectedPsmDeterminationTimestamp);
  device_attestation.SetPsmExecutionResult(psm_execution_result);
  client_->RegisterWithCertificate(
      device_attestation, std::string() /* client_id */, kEnrollmentCertificate,
      std::string() /* sub_organization */, &fake_signing_service_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      DeviceManagementService::JobConfiguration::TYPE_CERT_BASED_REGISTRATION,
      job_type_);
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetCertBasedRegistrationRequest(&fake_signing_service_,
                                            psm_execution_result,
                                            kExpectedPsmDeterminationTimestamp)
                .SerializePartialAsString());
  EXPECT_TRUE(client_->is_registered());
  EXPECT_FALSE(client_->GetPolicyFor(policy_type_, std::string()));
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
}

INSTANTIATE_TEST_SUITE_P(
    CloudPolicyClientRegisterWithPsmParams,
    CloudPolicyClientRegisterWithPsmParamsTest,
    ::testing::Values(
        em::DeviceRegisterRequest::PSM_RESULT_UNKNOWN,
        em::DeviceRegisterRequest::PSM_RESULT_SUCCESSFUL_WITH_STATE,
        em::DeviceRegisterRequest::PSM_RESULT_SUCCESSFUL_WITHOUT_STATE,
        em::DeviceRegisterRequest::PSM_RESULT_ERROR));

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)

class CloudPolicyClientUploadSecurityEventTest
    : public CloudPolicyClientTest,
      public testing::WithParamInterface<bool> {
 public:
  bool include_device_info() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(,
                         CloudPolicyClientUploadSecurityEventTest,
                         testing::Bool());

TEST_P(CloudPolicyClientUploadSecurityEventTest, Test) {
  RegisterClient();

  ExpectAndCaptureJSONJob(/*response=*/"{}");
  EXPECT_CALL(callback_observer_, OnCallbackComplete(true)).Times(1);
  CloudPolicyClient::StatusCallback callback =
      base::BindOnce(&MockStatusCallbackObserver::OnCallbackComplete,
                     base::Unretained(&callback_observer_));

  client_->UploadSecurityEventReport(nullptr, include_device_info(),
                                     MakeDefaultRealtimeReport(),
                                     std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      DeviceManagementService::JobConfiguration::TYPE_UPLOAD_REAL_TIME_REPORT,
      job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());

  absl::optional<base::Value> payload = base::JSONReader::Read(job_payload_);
  ASSERT_TRUE(payload);

  ASSERT_FALSE(policy::GetDeviceName().empty());
  EXPECT_EQ(version_info::GetVersionNumber(),
            *payload->FindStringPath(
                ReportingJobConfigurationBase::BrowserDictionaryBuilder::
                    GetChromeVersionPath()));

  if (include_device_info()) {
    EXPECT_EQ(kDMToken, *payload->FindStringPath(
                            ReportingJobConfigurationBase::
                                DeviceDictionaryBuilder::GetDMTokenPath()));
    EXPECT_EQ(client_id_, *payload->FindStringPath(
                              ReportingJobConfigurationBase::
                                  DeviceDictionaryBuilder::GetClientIdPath()));
    EXPECT_EQ(policy::GetOSUsername(),
              *payload->FindStringPath(
                  ReportingJobConfigurationBase::BrowserDictionaryBuilder::
                      GetMachineUserPath()));
    EXPECT_EQ(GetOSPlatform(),
              *payload->FindStringPath(
                  ReportingJobConfigurationBase::DeviceDictionaryBuilder::
                      GetOSPlatformPath()));
    EXPECT_EQ(GetOSVersion(),
              *payload->FindStringPath(
                  ReportingJobConfigurationBase::DeviceDictionaryBuilder::
                      GetOSVersionPath()));
    EXPECT_EQ(
        policy::GetDeviceName(),
        *payload->FindStringPath(ReportingJobConfigurationBase::
                                     DeviceDictionaryBuilder::GetNamePath()));
  } else {
    EXPECT_FALSE(
        payload->FindStringPath(ReportingJobConfigurationBase::
                                    DeviceDictionaryBuilder::GetDMTokenPath()));
    EXPECT_FALSE(payload->FindStringPath(
        ReportingJobConfigurationBase::DeviceDictionaryBuilder::
            GetClientIdPath()));
    EXPECT_FALSE(payload->FindStringPath(
        ReportingJobConfigurationBase::BrowserDictionaryBuilder::
            GetMachineUserPath()));
    EXPECT_FALSE(payload->FindStringPath(
        ReportingJobConfigurationBase::DeviceDictionaryBuilder::
            GetOSPlatformPath()));
    EXPECT_FALSE(payload->FindStringPath(
        ReportingJobConfigurationBase::DeviceDictionaryBuilder::
            GetOSVersionPath()));
    EXPECT_FALSE(payload->FindStringPath(
        ReportingJobConfigurationBase::DeviceDictionaryBuilder::GetNamePath()));
  }

  base::Value* events =
      payload->FindPath(RealtimeReportingJobConfiguration::kEventListKey);
  EXPECT_EQ(base::Value::Type::LIST, events->type());
  EXPECT_EQ(1u, events->GetListDeprecated().size());
}

TEST_F(CloudPolicyClientTest, RealtimeReportMerge) {
  auto config = std::make_unique<RealtimeReportingJobConfiguration>(
      client_.get(), service_.configuration()->GetRealtimeReportingServerUrl(),
      /*include_device_info*/ true, /*add_connector_url_params=*/false,
      RealtimeReportingJobConfiguration::UploadCompleteCallback());

  // Add one report to the config.
  {
    base::Value::Dict context;
    context.SetByDottedPath("profile.gaiaEmail", "name@gmail.com");
    context.SetByDottedPath("browser.userAgent", "User-Agent");
    context.SetByDottedPath("profile.profileName", "Profile 1");
    context.SetByDottedPath("profile.profilePath", "C:\\User Data\\Profile 1");

    base::Value::Dict event;
    event.Set("time", "2019-09-10T20:01:45Z");
    event.SetByDottedPath("foo.prop1", "value1");
    event.SetByDottedPath("foo.prop2", "value2");
    event.SetByDottedPath("foo.prop3", "value3");

    base::Value::List events;
    events.Append(std::move(event));

    base::Value::Dict report;
    report.Set(RealtimeReportingJobConfiguration::kEventListKey,
               std::move(events));
    report.Set(RealtimeReportingJobConfiguration::kContextKey,
               std::move(context));

    ASSERT_TRUE(config->AddReport(std::move(report)));
  }

  // Add a second report to the config with a different context.
  {
    base::Value::Dict context;
    context.SetByDottedPath("profile.gaiaEmail", "name2@gmail.com");
    context.SetByDottedPath("browser.userAgent", "User-Agent2");
    context.SetByDottedPath("browser.version", "1.0.0.0");

    base::Value::Dict event;
    event.Set("time", "2019-09-10T20:02:45Z");
    event.SetByDottedPath("foo.prop1", "value1");
    event.SetByDottedPath("foo.prop2", "value2");
    event.SetByDottedPath("foo.prop3", "value3");

    base::Value::List events;
    events.Append(std::move(event));

    base::Value::Dict report;
    report.Set(RealtimeReportingJobConfiguration::kEventListKey,
               std::move(events));
    report.Set(RealtimeReportingJobConfiguration::kContextKey,
               std::move(context));

    ASSERT_TRUE(config->AddReport(std::move(report)));
  }

  // The second config should trump the first.
  DeviceManagementService::JobConfiguration* job_config = config.get();
  absl::optional<base::Value> payload =
      base::JSONReader::Read(job_config->GetPayload());
  ASSERT_TRUE(payload);

  ASSERT_EQ("name2@gmail.com", *payload->FindStringPath("profile.gaiaEmail"));
  ASSERT_EQ("User-Agent2", *payload->FindStringPath("browser.userAgent"));
  ASSERT_EQ("Profile 1", *payload->FindStringPath("profile.profileName"));
  ASSERT_EQ("C:\\User Data\\Profile 1",
            *payload->FindStringPath("profile.profilePath"));
  ASSERT_EQ("1.0.0.0", *payload->FindStringPath("browser.version"));
  ASSERT_EQ(
      2u,
      payload->FindListPath(RealtimeReportingJobConfiguration::kEventListKey)
          ->GetListDeprecated()
          .size());
}

TEST_F(CloudPolicyClientTest, UploadEncryptedReport) {
  // Create record
  ::reporting::EncryptedRecord record;
  record.set_encrypted_wrapped_record("Enterprise");
  auto* sequence_information = record.mutable_sequence_information();
  sequence_information->set_sequencing_id(1701);
  sequence_information->set_generation_id(12345678);
  sequence_information->set_priority(::reporting::IMMEDIATE);

  RegisterClient();

  ExpectAndCaptureJSONJob(/*response=*/"{}");
  EXPECT_CALL(response_callback_observer_, OnResponseReceived(HasValue()))
      .Times(1);
  AttemptUploadEncryptedWaitUntilIdle(record);

  EXPECT_EQ(
      job_type_,
      DeviceManagementService::JobConfiguration::TYPE_UPLOAD_ENCRYPTED_REPORT);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(client_->status(), DM_STATUS_SUCCESS);
}

TEST_F(CloudPolicyClientTest, UploadAppInstallReport) {
  RegisterClient();

  ExpectAndCaptureJSONJob(/*response=*/"{}");
  EXPECT_CALL(callback_observer_, OnCallbackComplete(true)).Times(1);
  CloudPolicyClient::StatusCallback callback =
      base::BindOnce(&MockStatusCallbackObserver::OnCallbackComplete,
                     base::Unretained(&callback_observer_));

  client_->UploadAppInstallReport(MakeDefaultRealtimeReport(),
                                  std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      DeviceManagementService::JobConfiguration::TYPE_UPLOAD_REAL_TIME_REPORT,
      job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
}

TEST_F(CloudPolicyClientTest, CancelUploadAppInstallReport) {
  RegisterClient();

  ExpectAndCaptureJSONJob(/*response=*/"{}");
  EXPECT_CALL(callback_observer_, OnCallbackComplete(true)).Times(0);

  CloudPolicyClient::StatusCallback callback =
      base::BindOnce(&MockStatusCallbackObserver::OnCallbackComplete,
                     base::Unretained(&callback_observer_));

  em::AppInstallReportRequest app_install_report;
  client_->UploadAppInstallReport(MakeDefaultRealtimeReport(),
                                  std::move(callback));
  EXPECT_EQ(1, client_->GetActiveRequestCountForTest());

  // The job expected by the call to ExpectRealTimeReport() completes
  // when base::RunLoop().RunUntilIdle() is called.  To simulate a cancel
  // before the response for the request is processed, make sure to cancel it
  // before running a loop.
  client_->CancelAppInstallReportUpload();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, client_->GetActiveRequestCountForTest());
  EXPECT_EQ(
      DeviceManagementService::JobConfiguration::TYPE_UPLOAD_REAL_TIME_REPORT,
      job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
}

TEST_F(CloudPolicyClientTest, UploadAppInstallReportSupersedesPending) {
  RegisterClient();

  ExpectAndCaptureJSONJob(/*response=*/"{}");
  EXPECT_CALL(callback_observer_, OnCallbackComplete(true)).Times(0);
  CloudPolicyClient::StatusCallback callback =
      base::BindOnce(&MockStatusCallbackObserver::OnCallbackComplete,
                     base::Unretained(&callback_observer_));

  client_->UploadAppInstallReport(MakeDefaultRealtimeReport(),
                                  std::move(callback));

  EXPECT_EQ(1, client_->GetActiveRequestCountForTest());
  Mock::VerifyAndClearExpectations(&service_);
  Mock::VerifyAndClearExpectations(&callback_observer_);

  // Starting another app push-install report upload should cancel the pending
  // one.
  ExpectAndCaptureJSONJob(/*response=*/"{}");
  EXPECT_CALL(callback_observer_, OnCallbackComplete(true)).Times(1);
  callback = base::BindOnce(&MockStatusCallbackObserver::OnCallbackComplete,
                            base::Unretained(&callback_observer_));
  client_->UploadAppInstallReport(MakeDefaultRealtimeReport(),
                                  std::move(callback));
  EXPECT_EQ(1, client_->GetActiveRequestCountForTest());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      DeviceManagementService::JobConfiguration::TYPE_UPLOAD_REAL_TIME_REPORT,
      job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
  EXPECT_EQ(0, client_->GetActiveRequestCountForTest());
}

TEST_F(CloudPolicyClientTest, UploadExtensionInstallReport) {
  RegisterClient();

  ExpectAndCaptureJSONJob(/*response=*/"{}");
  EXPECT_CALL(callback_observer_, OnCallbackComplete(true)).Times(1);
  CloudPolicyClient::StatusCallback callback =
      base::BindOnce(&MockStatusCallbackObserver::OnCallbackComplete,
                     base::Unretained(&callback_observer_));

  client_->UploadExtensionInstallReport(MakeDefaultRealtimeReport(),
                                        std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      DeviceManagementService::JobConfiguration::TYPE_UPLOAD_REAL_TIME_REPORT,
      job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
}

TEST_F(CloudPolicyClientTest, CancelUploadExtensionInstallReport) {
  RegisterClient();

  ExpectAndCaptureJSONJob(/*response=*/"{}");
  EXPECT_CALL(callback_observer_, OnCallbackComplete(true)).Times(0);

  CloudPolicyClient::StatusCallback callback =
      base::BindOnce(&MockStatusCallbackObserver::OnCallbackComplete,
                     base::Unretained(&callback_observer_));

  em::ExtensionInstallReportRequest app_install_report;
  client_->UploadExtensionInstallReport(MakeDefaultRealtimeReport(),
                                        std::move(callback));
  EXPECT_EQ(1, client_->GetActiveRequestCountForTest());

  // The job expected by the call to ExpectRealTimeReport() completes
  // when base::RunLoop().RunUntilIdle() is called.  To simulate a cancel
  // before the response for the request is processed, make sure to cancel it
  // before running a loop.
  client_->CancelExtensionInstallReportUpload();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, client_->GetActiveRequestCountForTest());
  EXPECT_EQ(
      DeviceManagementService::JobConfiguration::TYPE_UPLOAD_REAL_TIME_REPORT,
      job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
}

TEST_F(CloudPolicyClientTest, UploadExtensionInstallReportSupersedesPending) {
  RegisterClient();

  ExpectAndCaptureJSONJob(/*response=*/"{}");
  EXPECT_CALL(callback_observer_, OnCallbackComplete(true)).Times(0);
  CloudPolicyClient::StatusCallback callback =
      base::BindOnce(&MockStatusCallbackObserver::OnCallbackComplete,
                     base::Unretained(&callback_observer_));

  client_->UploadExtensionInstallReport(MakeDefaultRealtimeReport(),
                                        std::move(callback));

  EXPECT_EQ(1, client_->GetActiveRequestCountForTest());
  Mock::VerifyAndClearExpectations(&service_);
  Mock::VerifyAndClearExpectations(&callback_observer_);

  // Starting another extension install report upload should cancel the pending
  // one.
  ExpectAndCaptureJSONJob(/*response=*/"{}");
  EXPECT_CALL(callback_observer_, OnCallbackComplete(true)).Times(1);
  callback = base::BindOnce(&MockStatusCallbackObserver::OnCallbackComplete,
                            base::Unretained(&callback_observer_));
  client_->UploadExtensionInstallReport(MakeDefaultRealtimeReport(),
                                        std::move(callback));
  EXPECT_EQ(1, client_->GetActiveRequestCountForTest());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      DeviceManagementService::JobConfiguration::TYPE_UPLOAD_REAL_TIME_REPORT,
      job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
  EXPECT_EQ(0, client_->GetActiveRequestCountForTest());
}

#endif

TEST_F(CloudPolicyClientTest, MultipleActiveRequests) {
  RegisterClient();

  // Set up pending upload status job.
  DeviceManagementService::JobConfiguration::JobType upload_type;
  EXPECT_CALL(job_creation_handler_, OnJobCreation)
      .WillOnce(DoAll(service_.CaptureJobType(&upload_type),
                      service_.SendJobOKAsync(GetEmptyResponse())));
  CloudPolicyClient::StatusCallback callback =
      base::BindOnce(&MockStatusCallbackObserver::OnCallbackComplete,
                     base::Unretained(&callback_observer_));
  em::DeviceStatusReportRequest device_status;
  em::SessionStatusReportRequest session_status;
  em::ChildStatusReportRequest child_status;
  client_->UploadDeviceStatus(&device_status, &session_status, &child_status,
                              std::move(callback));

  // Set up pending upload certificate job.
  DeviceManagementService::JobConfiguration::JobType cert_type;
  EXPECT_CALL(job_creation_handler_, OnJobCreation)
      .WillOnce(DoAll(service_.CaptureJobType(&cert_type),
                      service_.SendJobOKAsync(GetUploadCertificateResponse())));

  // Expect two calls on our upload observer, one for the status upload and
  // one for the certificate upload.
  CloudPolicyClient::StatusCallback callback2 =
      base::BindOnce(&MockStatusCallbackObserver::OnCallbackComplete,
                     base::Unretained(&callback_observer_));
  client_->UploadEnterpriseMachineCertificate(kMachineCertificate,
                                              std::move(callback2));
  EXPECT_EQ(2, client_->GetActiveRequestCountForTest());

  // Now satisfy both active jobs.
  EXPECT_CALL(callback_observer_, OnCallbackComplete(true)).Times(2);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_UPLOAD_STATUS,
            upload_type);
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_UPLOAD_CERTIFICATE,
            cert_type);
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());

  EXPECT_EQ(0, client_->GetActiveRequestCountForTest());
}

TEST_F(CloudPolicyClientTest, UploadStatusFailure) {
  RegisterClient();

  DeviceManagementService::JobConfiguration::JobType job_type;
  EXPECT_CALL(callback_observer_, OnCallbackComplete(false)).Times(1);
  EXPECT_CALL(job_creation_handler_, OnJobCreation)
      .WillOnce(DoAll(
          service_.CaptureJobType(&job_type),
          service_.SendJobResponseAsync(
              net::ERR_FAILED, DeviceManagementService::kInvalidArgument)));
  EXPECT_CALL(observer_, OnClientError);
  CloudPolicyClient::StatusCallback callback =
      base::BindOnce(&MockStatusCallbackObserver::OnCallbackComplete,
                     base::Unretained(&callback_observer_));

  em::DeviceStatusReportRequest device_status;
  em::SessionStatusReportRequest session_status;
  em::ChildStatusReportRequest child_status;
  client_->UploadDeviceStatus(&device_status, &session_status, &child_status,
                              std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_UPLOAD_STATUS,
            job_type);
  EXPECT_EQ(DM_STATUS_REQUEST_FAILED, client_->status());
}

TEST_F(CloudPolicyClientTest, RequestCancelOnUnregister) {
  RegisterClient();

  // Set up pending upload status job.
  DeviceManagementService::JobConfiguration::JobType upload_type;
  EXPECT_CALL(job_creation_handler_, OnJobCreation)
      .WillOnce(DoAll(service_.CaptureJobType(&upload_type)));
  CloudPolicyClient::StatusCallback callback =
      base::BindOnce(&MockStatusCallbackObserver::OnCallbackComplete,
                     base::Unretained(&callback_observer_));
  em::DeviceStatusReportRequest device_status;
  em::SessionStatusReportRequest session_status;
  em::ChildStatusReportRequest child_status;
  client_->UploadDeviceStatus(&device_status, &session_status, &child_status,
                              std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, client_->GetActiveRequestCountForTest());
  EXPECT_CALL(observer_, OnRegistrationStateChanged);
  ExpectAndCaptureJob(GetUnregistrationResponse());
  client_->Unregister();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_UPLOAD_STATUS,
            upload_type);
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_UNREGISTRATION,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetUnregistrationRequest().SerializePartialAsString());
  EXPECT_EQ(0, client_->GetActiveRequestCountForTest());
}

TEST_F(CloudPolicyClientTest, ShouldRejectUnsignedCommands) {
  const DeviceManagementStatus expected_error =
      DM_STATUS_RESPONSE_DECODING_ERROR;

  RegisterClient();

  em::DeviceManagementResponse remote_command_response;
  em::RemoteCommand* command =
      remote_command_response.mutable_remote_command_response()->add_commands();
  command->set_age_of_command(kAgeOfCommand);
  command->set_payload(kPayload);
  command->set_command_id(kLastCommandId + 1);
  command->set_type(em::RemoteCommand_Type_COMMAND_ECHO_TEST);

  ExpectAndCaptureJob(remote_command_response);

  StrictMock<MockRemoteCommandsObserver> remote_commands_observer;
  EXPECT_CALL(remote_commands_observer,
              OnRemoteCommandsFetched(expected_error, _))
      .Times(1);
  CloudPolicyClient::RemoteCommandCallback callback =
      base::BindOnce(&MockRemoteCommandsObserver::OnRemoteCommandsFetched,
                     base::Unretained(&remote_commands_observer));

  client_->FetchRemoteCommands(
      std::make_unique<RemoteCommandJob::UniqueIDType>(kLastCommandId), {},
      std::move(callback));
  base::RunLoop().RunUntilIdle();
}

TEST_F(CloudPolicyClientTest,
       ShouldIgnoreSignedCommandsIfUnsignedCommandsArePresent) {
  const DeviceManagementStatus expected_error =
      DM_STATUS_RESPONSE_DECODING_ERROR;

  RegisterClient();

  em::DeviceManagementResponse remote_command_response;
  auto* response = remote_command_response.mutable_remote_command_response();
  response->add_commands();
  response->add_secure_commands();

  ExpectAndCaptureJob(remote_command_response);

  std::vector<em::SignedData> received_commands;
  StrictMock<MockRemoteCommandsObserver> remote_commands_observer;
  EXPECT_CALL(remote_commands_observer,
              OnRemoteCommandsFetched(expected_error, _))
      .WillOnce(SaveArg<1>(&received_commands));
  CloudPolicyClient::RemoteCommandCallback callback =
      base::BindOnce(&MockRemoteCommandsObserver::OnRemoteCommandsFetched,
                     base::Unretained(&remote_commands_observer));

  client_->FetchRemoteCommands(
      std::make_unique<RemoteCommandJob::UniqueIDType>(kLastCommandId), {},
      std::move(callback));
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(received_commands, ElementsAre());
}

TEST_F(CloudPolicyClientTest, ShouldNotFailIfRemoteCommandResponseIsEmpty) {
  const DeviceManagementStatus expected_result = DM_STATUS_SUCCESS;

  RegisterClient();

  em::DeviceManagementResponse empty_server_response;

  ExpectAndCaptureJob(empty_server_response);

  std::vector<em::SignedData> received_commands;
  StrictMock<MockRemoteCommandsObserver> remote_commands_observer;
  EXPECT_CALL(remote_commands_observer,
              OnRemoteCommandsFetched(expected_result, _))
      .Times(1);
  CloudPolicyClient::RemoteCommandCallback callback =
      base::BindOnce(&MockRemoteCommandsObserver::OnRemoteCommandsFetched,
                     base::Unretained(&remote_commands_observer));

  client_->FetchRemoteCommands(
      std::make_unique<RemoteCommandJob::UniqueIDType>(kLastCommandId), {},
      std::move(callback));
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(received_commands, ElementsAre());
}

TEST_F(CloudPolicyClientTest, FetchSecureRemoteCommands) {
  RegisterClient();

  em::DeviceManagementRequest remote_command_request =
      GetRemoteCommandRequest();

  em::DeviceManagementResponse remote_command_response;
  em::SignedData* signed_command =
      remote_command_response.mutable_remote_command_response()
          ->add_secure_commands();
  signed_command->set_data("signed-data");
  signed_command->set_signature("signed-signature");

  ExpectAndCaptureJob(remote_command_response);

  StrictMock<MockRemoteCommandsObserver> remote_commands_observer;
  EXPECT_CALL(
      remote_commands_observer,
      OnRemoteCommandsFetched(
          DM_STATUS_SUCCESS,
          ElementsAre(MatchProto(
              remote_command_response.remote_command_response().secure_commands(
                  0)))))
      .Times(1);

  base::RunLoop run_loop;
  CloudPolicyClient::RemoteCommandCallback callback =
      base::BindLambdaForTesting(
          [&](DeviceManagementStatus status,
              const std::vector<enterprise_management::SignedData>&
                  signed_commands) {
            remote_commands_observer.OnRemoteCommandsFetched(status,
                                                             signed_commands);
            run_loop.Quit();
          });
  const std::vector<em::RemoteCommandResult> command_results(
      1, remote_command_request.remote_command_request().command_results(0));
  client_->FetchRemoteCommands(
      std::make_unique<RemoteCommandJob::UniqueIDType>(kLastCommandId),
      command_results, std::move(callback));
  run_loop.Run();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_REMOTE_COMMANDS,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            remote_command_request.SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
}

TEST_F(CloudPolicyClientTest, RequestDeviceAttributeUpdatePermission) {
  RegisterClient();

  em::DeviceManagementRequest attribute_update_permission_request;
  attribute_update_permission_request
      .mutable_device_attribute_update_permission_request();

  em::DeviceManagementResponse attribute_update_permission_response;
  attribute_update_permission_response
      .mutable_device_attribute_update_permission_response()
      ->set_result(
          em::DeviceAttributeUpdatePermissionResponse_ResultType_ATTRIBUTE_UPDATE_ALLOWED);

  ExpectAndCaptureJob(attribute_update_permission_response);
  EXPECT_CALL(callback_observer_, OnCallbackComplete(true)).Times(1);

  CloudPolicyClient::StatusCallback callback =
      base::BindOnce(&MockStatusCallbackObserver::OnCallbackComplete,
                     base::Unretained(&callback_observer_));
  client_->GetDeviceAttributeUpdatePermission(
      DMAuth::FromOAuthToken(kOAuthToken), std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::
                TYPE_ATTRIBUTE_UPDATE_PERMISSION,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::NoAuth());
  EXPECT_THAT(query_params_,
              Contains(Pair(dm_protocol::kParamOAuthToken, kOAuthToken)));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            attribute_update_permission_request.SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
}

TEST_F(CloudPolicyClientTest, RequestDeviceAttributeUpdate) {
  RegisterClient();

  em::DeviceManagementRequest attribute_update_request;
  attribute_update_request.mutable_device_attribute_update_request()
      ->set_asset_id(kAssetId);
  attribute_update_request.mutable_device_attribute_update_request()
      ->set_location(kLocation);

  em::DeviceManagementResponse attribute_update_response;
  attribute_update_response.mutable_device_attribute_update_response()
      ->set_result(
          em::DeviceAttributeUpdateResponse_ResultType_ATTRIBUTE_UPDATE_SUCCESS);

  ExpectAndCaptureJob(attribute_update_response);
  EXPECT_CALL(callback_observer_, OnCallbackComplete(true)).Times(1);

  CloudPolicyClient::StatusCallback callback =
      base::BindOnce(&MockStatusCallbackObserver::OnCallbackComplete,
                     base::Unretained(&callback_observer_));
  client_->UpdateDeviceAttributes(DMAuth::FromOAuthToken(kOAuthToken), kAssetId,
                                  kLocation, std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_ATTRIBUTE_UPDATE,
            job_type_);
  EXPECT_THAT(query_params_,
              Contains(Pair(dm_protocol::kParamOAuthToken, kOAuthToken)));
  EXPECT_EQ(auth_data_, DMAuth::NoAuth());
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            attribute_update_request.SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
}

TEST_F(CloudPolicyClientTest, RequestGcmIdUpdate) {
  RegisterClient();

  em::DeviceManagementRequest gcm_id_update_request;
  gcm_id_update_request.mutable_gcm_id_update_request()->set_gcm_id(kGcmID);

  ExpectAndCaptureJob(GetEmptyResponse());
  EXPECT_CALL(callback_observer_, OnCallbackComplete(true)).Times(1);

  CloudPolicyClient::StatusCallback callback =
      base::BindOnce(&MockStatusCallbackObserver::OnCallbackComplete,
                     base::Unretained(&callback_observer_));
  client_->UpdateGcmId(kGcmID, std::move(callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_GCM_ID_UPDATE,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            gcm_id_update_request.SerializePartialAsString());
}

TEST_F(CloudPolicyClientTest, PolicyReregistration) {
  RegisterClient();

  // Handle 410 (unknown deviceID) on policy fetch.
  EXPECT_TRUE(client_->is_registered());
  EXPECT_FALSE(client_->requires_reregistration());
  DeviceManagementService::JobConfiguration::JobType upload_type;
  EXPECT_CALL(job_creation_handler_, OnJobCreation)
      .WillOnce(DoAll(service_.CaptureJobType(&upload_type),
                      service_.SendJobResponseAsync(
                          net::OK, DeviceManagementService::kDeviceNotFound)));
  EXPECT_CALL(observer_, OnRegistrationStateChanged);
  EXPECT_CALL(observer_, OnClientError);
  client_->FetchPolicy();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DM_STATUS_SERVICE_DEVICE_NOT_FOUND, client_->status());
  EXPECT_FALSE(client_->GetPolicyFor(policy_type_, std::string()));
  EXPECT_FALSE(client_->is_registered());
  EXPECT_TRUE(client_->requires_reregistration());

  // Re-register.
  ExpectAndCaptureJob(GetRegistrationResponse());
  EXPECT_CALL(observer_, OnRegistrationStateChanged);
  EXPECT_CALL(device_dmtoken_callback_observer_,
              OnDeviceDMTokenRequested(
                  /*user_affiliation_ids=*/std::vector<std::string>()))
      .WillOnce(Return(kDeviceDMToken));
  CloudPolicyClient::RegistrationParameters user_recovery(
      em::DeviceRegisterRequest::USER,
      em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_RECOVERY);
  client_->Register(user_recovery, client_id_, kOAuthToken);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            upload_type);
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_REGISTRATION,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::NoAuth());
  EXPECT_THAT(query_params_,
              Contains(Pair(dm_protocol::kParamOAuthToken, kOAuthToken)));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetReregistrationRequest().SerializePartialAsString());
  EXPECT_TRUE(client_->is_registered());
  EXPECT_FALSE(client_->requires_reregistration());
  EXPECT_FALSE(client_->GetPolicyFor(policy_type_, std::string()));
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
}

TEST_F(CloudPolicyClientTest, PolicyReregistrationFailsWithNonMatchingDMToken) {
  RegisterClient();

  // Handle 410 (unknown deviceID) on policy fetch.
  EXPECT_TRUE(client_->is_registered());
  EXPECT_FALSE(client_->requires_reregistration());
  DeviceManagementService::JobConfiguration::JobType upload_type;
  EXPECT_CALL(job_creation_handler_, OnJobCreation)
      .WillOnce(DoAll(service_.CaptureJobType(&upload_type),
                      service_.SendJobResponseAsync(
                          net::OK, DeviceManagementService::kDeviceNotFound)));
  EXPECT_CALL(observer_, OnRegistrationStateChanged);
  EXPECT_CALL(observer_, OnClientError);
  client_->FetchPolicy();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DM_STATUS_SERVICE_DEVICE_NOT_FOUND, client_->status());
  EXPECT_FALSE(client_->GetPolicyFor(policy_type_, std::string()));
  EXPECT_FALSE(client_->is_registered());
  EXPECT_TRUE(client_->requires_reregistration());

  // Re-register (server sends wrong DMToken).
  ExpectAndCaptureJobReplyFailure(
      net::OK, DeviceManagementService::kInvalidAuthCookieOrDMToken);
  EXPECT_CALL(observer_, OnClientError);
  CloudPolicyClient::RegistrationParameters user_recovery(
      em::DeviceRegisterRequest::USER,
      em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_RECOVERY);
  client_->Register(user_recovery, client_id_, kOAuthToken);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            upload_type);
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_REGISTRATION,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::NoAuth());
  EXPECT_THAT(query_params_,
              Contains(Pair(dm_protocol::kParamOAuthToken, kOAuthToken)));
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            GetReregistrationRequest().SerializePartialAsString());
  EXPECT_FALSE(client_->is_registered());
  EXPECT_TRUE(client_->requires_reregistration());
  EXPECT_FALSE(client_->GetPolicyFor(policy_type_, std::string()));
  EXPECT_EQ(DM_STATUS_SERVICE_MANAGEMENT_TOKEN_INVALID, client_->status());
}

TEST_F(CloudPolicyClientTest, RequestFetchRobotAuthCodes) {
  RegisterClient();

  em::DeviceManagementRequest robot_auth_code_fetch_request =
      GetRobotAuthCodeFetchRequest();
  em::DeviceManagementResponse robot_auth_code_fetch_response =
      GetRobotAuthCodeFetchResponse();

  ExpectAndCaptureJob(robot_auth_code_fetch_response);
  EXPECT_CALL(robot_auth_code_callback_observer_,
              OnRobotAuthCodeFetched(_, kRobotAuthCode));

  em::DeviceServiceApiAccessRequest::DeviceType device_type =
      em::DeviceServiceApiAccessRequest::CHROME_OS;
  std::set<std::string> oauth_scopes = {kApiAuthScope};
  client_->FetchRobotAuthCodes(
      DMAuth::FromDMToken(kDMToken), device_type, oauth_scopes,
      base::BindOnce(&MockRobotAuthCodeCallbackObserver::OnRobotAuthCodeFetched,
                     base::Unretained(&robot_auth_code_callback_observer_)));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_API_AUTH_CODE_FETCH,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
  EXPECT_EQ(robot_auth_code_fetch_request.SerializePartialAsString(),
            job_request_.SerializePartialAsString());
  EXPECT_EQ(DM_STATUS_SUCCESS, client_->status());
}

TEST_F(CloudPolicyClientTest,
       RequestFetchRobotAuthCodesNotInterruptedByPolicyFetch) {
  RegisterClient();

  em::DeviceManagementResponse robot_auth_code_fetch_response =
      GetRobotAuthCodeFetchResponse();

  // Expect a robot auth code fetch request that never runs its callback to
  // simulate something happening while we wait for the request to return.
  DeviceManagementService::JobForTesting robot_job;
  DeviceManagementService::JobConfiguration::JobType robot_job_type;
  EXPECT_CALL(job_creation_handler_, OnJobCreation)
      .WillOnce(DoAll(service_.CaptureJobType(&robot_job_type),
                      SaveArg<0>(&robot_job)));

  EXPECT_CALL(robot_auth_code_callback_observer_,
              OnRobotAuthCodeFetched(_, kRobotAuthCode));

  em::DeviceServiceApiAccessRequest::DeviceType device_type =
      em::DeviceServiceApiAccessRequest::CHROME_OS;
  std::set<std::string> oauth_scopes = {kApiAuthScope};
  client_->FetchRobotAuthCodes(
      DMAuth::FromDMToken(kDMToken), device_type, oauth_scopes,
      base::BindOnce(&MockRobotAuthCodeCallbackObserver::OnRobotAuthCodeFetched,
                     base::Unretained(&robot_auth_code_callback_observer_)));
  base::RunLoop().RunUntilIdle();

  ExpectAndCaptureJob(GetPolicyResponse());
  EXPECT_CALL(observer_, OnPolicyFetched);

  client_->FetchPolicy();
  base::RunLoop().RunUntilIdle();

  // Try to manually finish the robot auth code fetch job.
  service_.SendJobResponseNow(&robot_job, net::OK,
                              DeviceManagementService::kSuccess,
                              robot_auth_code_fetch_response);

  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_API_AUTH_CODE_FETCH,
            robot_job_type);
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
            job_type_);
  EXPECT_EQ(auth_data_, DMAuth::FromDMToken(kDMToken));
}

struct MockClientCertProvisioningStartCsrCallbackObserver {
  MOCK_METHOD(void,
              Callback,
              (DeviceManagementStatus,
               absl::optional<CertProvisioningResponseErrorType>,
               absl::optional<int64_t> try_later,
               const std::string& invalidation_topic,
               const std::string& va_challenge,
               em::HashingAlgorithm hash_algorithm,
               const std::string& data_to_sign),
              (const));
};

class CloudPolicyClientCertProvisioningStartCsrTest
    : public CloudPolicyClientTest,
      public ::testing::WithParamInterface<bool> {
 public:
  void RunTest(const em::DeviceManagementResponse& fake_response,
               const MockClientCertProvisioningStartCsrCallbackObserver&
                   callback_observer);

  // Wraps the test parameter - returns true if in this test run
  // CloudPolicyClient has knowledge of the device DMToken.
  bool HasDeviceDMToken() { return GetParam(); }
};

void CloudPolicyClientCertProvisioningStartCsrTest::RunTest(
    const em::DeviceManagementResponse& fake_response,
    const MockClientCertProvisioningStartCsrCallbackObserver&
        callback_observer) {
  const std::string cert_scope = "fake_cert_scope_1";
  const std::string cert_profile_id = "fake_cert_profile_id_1";
  const std::string cert_profile_version = "fake_cert_profile_version_1";
  const std::string public_key = "fake_public_key_1";

  em::DeviceManagementRequest expected_request;
  {
    em::ClientCertificateProvisioningRequest* inner_request =
        expected_request.mutable_client_certificate_provisioning_request();
    inner_request->set_certificate_scope(cert_scope);
    inner_request->set_cert_profile_id(cert_profile_id);
    inner_request->set_policy_version(cert_profile_version);
    inner_request->set_public_key(public_key);
    if (HasDeviceDMToken()) {
      inner_request->set_device_dm_token(kDeviceDMToken);
    }
    // Sets the request type, no actual data is required.
    inner_request->mutable_start_csr_request();
  }

  if (HasDeviceDMToken()) {
    RegisterClient(kDeviceDMToken);
  } else {
    RegisterClient(/*device_dm_token=*/std::string());
  }

  EXPECT_CALL(job_creation_handler_, OnJobCreation)
      .WillOnce(DoAll(service_.CaptureJobType(&job_type_),
                      service_.CaptureRequest(&job_request_),
                      service_.SendJobOKAsync(fake_response)));

  client_->ClientCertProvisioningStartCsr(
      cert_scope, cert_profile_id, cert_profile_version, public_key,
      base::BindOnce(
          &MockClientCertProvisioningStartCsrCallbackObserver::Callback,
          base::Unretained(&callback_observer)));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(
      DeviceManagementService::JobConfiguration::TYPE_CERT_PROVISIONING_REQUEST,
      job_type_);
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            expected_request.SerializePartialAsString());
}

// 1. Checks that |ClientCertProvisioningStartCsr| generates a correct request.
// 2. Checks that |OnClientCertProvisioningStartCsrResponse| correctly extracts
// data from a response that contains data.
TEST_P(CloudPolicyClientCertProvisioningStartCsrTest,
       RequestClientCertProvisioningStartCsrSuccess) {
  const std::string invalidation_topic = "fake_invalidation_topic_1";
  const std::string va_challenge = "fake_va_challenge_1";
  const std::string data_to_sign = "fake_data_to_sign_1";
  em::HashingAlgorithm hash_algorithm = em::HashingAlgorithm::SHA256;
  em::SigningAlgorithm sign_algorithm = em::SigningAlgorithm::RSA_PKCS1_V1_5;

  em::DeviceManagementResponse fake_response;
  {
    em::ClientCertificateProvisioningResponse* inner_response =
        fake_response.mutable_client_certificate_provisioning_response();
    em::StartCsrResponse* start_csr_response =
        inner_response->mutable_start_csr_response();
    start_csr_response->set_invalidation_topic(invalidation_topic);
    start_csr_response->set_va_challenge(va_challenge);
    start_csr_response->set_hashing_algorithm(hash_algorithm);
    start_csr_response->set_signing_algorithm(sign_algorithm);
    start_csr_response->set_data_to_sign(data_to_sign);
  }

  MockClientCertProvisioningStartCsrCallbackObserver callback_observer;
  EXPECT_CALL(
      callback_observer,
      Callback(DeviceManagementStatus::DM_STATUS_SUCCESS,
               testing::Eq(absl::nullopt), testing::Eq(absl::nullopt),
               invalidation_topic, va_challenge, hash_algorithm, data_to_sign))
      .Times(1);

  RunTest(fake_response, callback_observer);
}

// 1. Checks that |ClientCertProvisioningStartCsr| generates a correct request.
// 2. Checks that |OnClientCertProvisioningStartCsrResponse| correctly extracts
// data from a response that contains the try_later field.
TEST_P(CloudPolicyClientCertProvisioningStartCsrTest,
       RequestClientCertProvisioningStartCsrTryLater) {
  const int64_t try_later = 60000;
  em::DeviceManagementResponse fake_response;
  {
    em::ClientCertificateProvisioningResponse* inner_response =
        fake_response.mutable_client_certificate_provisioning_response();
    inner_response->set_try_again_later(try_later);
  }

  MockClientCertProvisioningStartCsrCallbackObserver callback_observer;
  EXPECT_CALL(callback_observer,
              Callback(DeviceManagementStatus::DM_STATUS_SUCCESS,
                       testing::Eq(absl::nullopt), testing::Eq(try_later),
                       std::string(), std::string(),
                       em::HashingAlgorithm::HASHING_ALGORITHM_UNSPECIFIED,
                       std::string()))
      .Times(1);

  RunTest(fake_response, callback_observer);
}

// 1. Checks that |ClientCertProvisioningStartCsr| generates a correct request.
// 2. Checks that |OnClientCertProvisioningStartCsrResponse| correctly extracts
// data from a response that contains the error field.
TEST_P(CloudPolicyClientCertProvisioningStartCsrTest,
       RequestClientCertProvisioningStartCsrError) {
  const CertProvisioningResponseErrorType error =
      CertProvisioningResponseError::CA_ERROR;
  em::DeviceManagementResponse fake_response;
  {
    em::ClientCertificateProvisioningResponse* inner_response =
        fake_response.mutable_client_certificate_provisioning_response();
    inner_response->set_error(error);
  }

  MockClientCertProvisioningStartCsrCallbackObserver callback_observer;
  EXPECT_CALL(
      callback_observer,
      Callback(DeviceManagementStatus::DM_STATUS_SUCCESS, testing::Eq(error),
               testing::Eq(absl::nullopt), std::string(), std::string(),
               em::HashingAlgorithm::HASHING_ALGORITHM_UNSPECIFIED,
               std::string()))
      .Times(1);

  RunTest(fake_response, callback_observer);
}

INSTANTIATE_TEST_SUITE_P(,
                         CloudPolicyClientCertProvisioningStartCsrTest,
                         ::testing::Values(false, true));

class MockClientCertProvisioningFinishCsrCallbackObserver {
 public:
  MockClientCertProvisioningFinishCsrCallbackObserver() = default;

  MOCK_METHOD(void,
              Callback,
              (DeviceManagementStatus,
               absl::optional<CertProvisioningResponseErrorType>,
               absl::optional<int64_t> try_later),
              (const));
};

class CloudPolicyClientCertProvisioningFinishCsrTest
    : public CloudPolicyClientTest,
      public ::testing::WithParamInterface<bool> {
 public:
  void RunTest(const em::DeviceManagementResponse& fake_response,
               const MockClientCertProvisioningFinishCsrCallbackObserver&
                   callback_observer);

  // Wraps the test parameter - returns true if in this test run
  // CloudPolicyClient has knowledge of the device DMToken.
  bool HasDeviceDMToken() { return GetParam(); }
};

void CloudPolicyClientCertProvisioningFinishCsrTest::RunTest(
    const em::DeviceManagementResponse& fake_response,
    const MockClientCertProvisioningFinishCsrCallbackObserver&
        callback_observer) {
  const std::string cert_scope = "fake_cert_scope_1";
  const std::string cert_profile_id = "fake_cert_profile_id_1";
  const std::string cert_profile_version = "fake_cert_profile_version_1";
  const std::string public_key = "fake_public_key_1";
  const std::string va_challenge_response = "fake_va_challenge_response_1";
  const std::string signature = "fake_signature_1";

  em::DeviceManagementRequest expected_request;
  {
    em::ClientCertificateProvisioningRequest* inner_request =
        expected_request.mutable_client_certificate_provisioning_request();
    inner_request->set_certificate_scope(cert_scope);
    inner_request->set_cert_profile_id(cert_profile_id);
    inner_request->set_policy_version(cert_profile_version);
    inner_request->set_public_key(public_key);
    if (HasDeviceDMToken()) {
      inner_request->set_device_dm_token(kDeviceDMToken);
    }

    em::FinishCsrRequest* finish_csr_request =
        inner_request->mutable_finish_csr_request();
    finish_csr_request->set_va_challenge_response(va_challenge_response);
    finish_csr_request->set_signature(signature);
  }

  if (HasDeviceDMToken()) {
    RegisterClient(kDeviceDMToken);
  } else {
    RegisterClient(/*device_dm_token=*/std::string());
  }

  EXPECT_CALL(job_creation_handler_, OnJobCreation)
      .WillOnce(DoAll(service_.CaptureJobType(&job_type_),
                      service_.CaptureRequest(&job_request_),
                      service_.SendJobOKAsync(fake_response)));

  client_->ClientCertProvisioningFinishCsr(
      cert_scope, cert_profile_id, cert_profile_version, public_key,
      va_challenge_response, signature,
      base::BindOnce(
          &MockClientCertProvisioningFinishCsrCallbackObserver::Callback,
          base::Unretained(&callback_observer)));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(
      DeviceManagementService::JobConfiguration::TYPE_CERT_PROVISIONING_REQUEST,
      job_type_);
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            expected_request.SerializePartialAsString());
}

// 1. Checks that |ClientCertProvisioningFinishCsr| generates a correct request.
// 2. Checks that |OnClientCertProvisioningFinishCsrResponse| correctly extracts
// data from a response that contains success status code.
TEST_P(CloudPolicyClientCertProvisioningFinishCsrTest,
       RequestClientCertProvisioningFinishCsrSuccess) {
  em::DeviceManagementResponse fake_response;
  {
    em::ClientCertificateProvisioningResponse* inner_response =
        fake_response.mutable_client_certificate_provisioning_response();
    // Sets the response id, no actual data is required.
    inner_response->mutable_finish_csr_response();
  }

  MockClientCertProvisioningFinishCsrCallbackObserver callback_observer;
  EXPECT_CALL(callback_observer,
              Callback(DeviceManagementStatus::DM_STATUS_SUCCESS,
                       testing::Eq(absl::nullopt), testing::Eq(absl::nullopt)))
      .Times(1);

  RunTest(fake_response, callback_observer);
}

// 1. Checks that |ClientCertProvisioningFinishCsr| generates a correct request.
// 2. Checks that |OnClientCertProvisioningFinishCsrResponse| correctly extracts
// data from a response that contains the error field.
TEST_P(CloudPolicyClientCertProvisioningFinishCsrTest,
       RequestClientCertProvisioningFinishCsrError) {
  const CertProvisioningResponseErrorType error =
      CertProvisioningResponseError::CA_ERROR;
  em::DeviceManagementResponse fake_response;
  {
    em::ClientCertificateProvisioningResponse* inner_response =
        fake_response.mutable_client_certificate_provisioning_response();
    inner_response->set_error(error);
  }

  MockClientCertProvisioningFinishCsrCallbackObserver callback_observer;
  EXPECT_CALL(callback_observer,
              Callback(DeviceManagementStatus::DM_STATUS_SUCCESS,
                       testing::Eq(error), testing::Eq(absl::nullopt)))
      .Times(1);

  RunTest(fake_response, callback_observer);
}

INSTANTIATE_TEST_SUITE_P(,
                         CloudPolicyClientCertProvisioningFinishCsrTest,
                         ::testing::Values(false, true));

class MockClientCertProvisioningDownloadCertCallbackObserver {
 public:
  MockClientCertProvisioningDownloadCertCallbackObserver() = default;

  MOCK_METHOD(void,
              Callback,
              (DeviceManagementStatus,
               absl::optional<CertProvisioningResponseErrorType>,
               absl::optional<int64_t> try_later,
               const std::string& pem_encoded_certificate),
              (const));
};

class CloudPolicyClientCertProvisioningDownloadCertTest
    : public CloudPolicyClientTest,
      public ::testing::WithParamInterface<bool> {
 public:
  void RunTest(const em::DeviceManagementResponse& fake_response,
               const MockClientCertProvisioningDownloadCertCallbackObserver&
                   callback_observer);

  // Wraps the test parameter - returns true if in this test run
  // CloudPolicyClient has knowledge of the device DMToken.
  bool HasDeviceDMToken() { return GetParam(); }
};

void CloudPolicyClientCertProvisioningDownloadCertTest::RunTest(
    const em::DeviceManagementResponse& fake_response,
    const MockClientCertProvisioningDownloadCertCallbackObserver&
        callback_observer) {
  const std::string cert_scope = "fake_cert_scope_1";
  const std::string cert_profile_id = "fake_cert_profile_id_1";
  const std::string cert_profile_version = "fake_cert_profile_version_1";
  const std::string public_key = "fake_public_key_1";

  em::DeviceManagementRequest expected_request;
  {
    em::ClientCertificateProvisioningRequest* inner_request =
        expected_request.mutable_client_certificate_provisioning_request();
    inner_request->set_certificate_scope(cert_scope);
    inner_request->set_cert_profile_id(cert_profile_id);
    inner_request->set_policy_version(cert_profile_version);
    inner_request->set_public_key(public_key);
    if (HasDeviceDMToken()) {
      inner_request->set_device_dm_token(kDeviceDMToken);
    }
    // Sets the request type, no actual data is required.
    inner_request->mutable_download_cert_request();
  }

  if (HasDeviceDMToken()) {
    RegisterClient(kDeviceDMToken);
  } else {
    RegisterClient(/*device_dm_token=*/std::string());
  }

  EXPECT_CALL(job_creation_handler_, OnJobCreation)
      .WillOnce(DoAll(service_.CaptureJobType(&job_type_),
                      service_.CaptureRequest(&job_request_),
                      service_.SendJobOKAsync(fake_response)));

  client_->ClientCertProvisioningDownloadCert(
      cert_scope, cert_profile_id, cert_profile_version, public_key,
      base::BindOnce(
          &MockClientCertProvisioningDownloadCertCallbackObserver::Callback,
          base::Unretained(&callback_observer)));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(
      DeviceManagementService::JobConfiguration::TYPE_CERT_PROVISIONING_REQUEST,
      job_type_);
  EXPECT_EQ(job_request_.SerializePartialAsString(),
            expected_request.SerializePartialAsString());
}

// 1. Checks that |ClientCertProvisioningDownloadCert| generates a correct
// request.
// 2. Checks that |OnClientCertProvisioningDownloadCertResponse| correctly
// extracts data from a response that contains success status code.
TEST_P(CloudPolicyClientCertProvisioningDownloadCertTest,
       RequestClientCertProvisioningDownloadCertSuccess) {
  const std::string pem_encoded_cert = "fake_pem_encoded_cert_1";
  em::DeviceManagementResponse fake_response;
  {
    em::ClientCertificateProvisioningResponse* inner_response =
        fake_response.mutable_client_certificate_provisioning_response();

    em::DownloadCertResponse* download_cert_response =
        inner_response->mutable_download_cert_response();
    download_cert_response->set_pem_encoded_certificate(pem_encoded_cert);
  }

  MockClientCertProvisioningDownloadCertCallbackObserver callback_observer;
  EXPECT_CALL(callback_observer,
              Callback(DeviceManagementStatus::DM_STATUS_SUCCESS,
                       testing::Eq(absl::nullopt), testing::Eq(absl::nullopt),
                       pem_encoded_cert))
      .Times(1);

  RunTest(fake_response, callback_observer);
}

// 1. Checks that |ClientCertProvisioningDownloadCert| generates a correct
// request.
// 2. Checks that |OnClientCertProvisioningDownloadCertResponse| correctly
// extracts data from a response that contains the error field.
TEST_P(CloudPolicyClientCertProvisioningDownloadCertTest,
       RequestClientCertProvisioningDownloadCertError) {
  const CertProvisioningResponseErrorType error =
      CertProvisioningResponseError::CA_ERROR;
  em::DeviceManagementResponse fake_response;
  {
    em::ClientCertificateProvisioningResponse* inner_response =
        fake_response.mutable_client_certificate_provisioning_response();
    inner_response->set_error(error);
  }

  MockClientCertProvisioningDownloadCertCallbackObserver callback_observer;
  EXPECT_CALL(
      callback_observer,
      Callback(DeviceManagementStatus::DM_STATUS_SUCCESS, testing::Eq(error),
               testing::Eq(absl::nullopt), std::string()))
      .Times(1);

  RunTest(fake_response, callback_observer);
}

INSTANTIATE_TEST_SUITE_P(,
                         CloudPolicyClientCertProvisioningDownloadCertTest,
                         ::testing::Values(false, true));

}  // namespace policy
