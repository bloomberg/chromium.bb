// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/auto_enrollment_client_impl.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/current_thread.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/ash/login/enrollment/auto_enrollment_controller.h"
#include "chrome/browser/ash/policy/enrollment/private_membership/testing_private_membership_rlwe_client.h"
#include "chrome/browser/ash/policy/server_backed_state/server_backed_device_state.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "crypto/sha2.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/private_membership/src/internal/testing/regression_test_data/regression_test_data.pb.h"
#include "third_party/shell-encryption/src/testing/status_testing.h"

namespace em = enterprise_management;
namespace psm_rlwe = private_membership::rlwe;

// An enum for PSM execution result values.
using PsmExecutionResult = em::DeviceRegisterRequest::PsmExecutionResult;

namespace policy {

namespace {

const char kStateKey[] = "state_key";
const char kStateKeyHash[] =
    "\xde\x74\xcd\xf0\x03\x36\x8c\x21\x79\xba\xb1\x5a\xc4\x32\xee\xd6"
    "\xb3\x4a\x5e\xff\x73\x7e\x92\xd9\xf8\x6e\x72\x44\xd0\x97\xc3\xe6";
const char kDisabledMessage[] = "This device has been disabled.";

const char kSerialNumber[] = "SN123456";
const char kBrandCode[] = "AABC";

const bool kNotWithLicense = false;
const bool kWithLicense = true;

const char kNoLicenseType[] = "";

// Start and limit powers for the hash dance clients.
const int kPowerStart = 4;
const int kPowerLimit = 8;

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::SaveArg;

// Number of test cases exist in cros_test_data.binarypb file, which is part of
// private_membership third_party library.
const int kNumberOfPsmTestCases = 10;

// Invalid test case index which acts as a dummy value when the PSM (private set
// membership) is disabled.
const int kInvalidPsmTestCaseIndex = -1;

// PrivateSetMembership regression tests maximum file size which is 4MB.
const size_t kMaxFileSizeInBytes = 4 * 1024 * 1024;

bool ParseProtoFromFile(const base::FilePath& file_path,
                        google::protobuf::MessageLite* out_proto) {
  if (!out_proto) {
    return false;
  }

  std::string file_content;
  if (!base::ReadFileToStringWithMaxSize(file_path, &file_content,
                                         kMaxFileSizeInBytes)) {
    return false;
  }

  return out_proto->ParseFromString(file_content);
}

enum class AutoEnrollmentProtocol { kFRE = 0, kInitialEnrollment = 1 };

enum class PsmState { kEnabled = 0, kDisabled = 1 };

// Holds the state of the AutoEnrollmentClientImplTest and its subclass i.e.
// PsmHelperTest. It will be used to run their tests with different values.
struct AutoEnrollmentClientImplTestState final {
  AutoEnrollmentClientImplTestState(
      AutoEnrollmentProtocol auto_enrollment_protocol,
      PsmState psm_state)
      : auto_enrollment_protocol(auto_enrollment_protocol),
        psm_state(psm_state) {}

  AutoEnrollmentProtocol auto_enrollment_protocol;
  PsmState psm_state;
};

// The integer parameter represents the index of PSM test case.
class AutoEnrollmentClientImplTest
    : public testing::Test,
      public ::testing::WithParamInterface<
          std::tuple<AutoEnrollmentClientImplTestState, int>> {
 public:
  AutoEnrollmentClientImplTest(const AutoEnrollmentClientImplTest&) = delete;
  AutoEnrollmentClientImplTest& operator=(const AutoEnrollmentClientImplTest&) =
      delete;

 protected:
  AutoEnrollmentClientImplTest()
      : scoped_testing_local_state_(TestingBrowserProcess::GetGlobal()),
        local_state_(scoped_testing_local_state_.Get()),
        state_(AUTO_ENROLLMENT_STATE_PENDING) {}

  void SetUp() override {
    CreateClient(kPowerStart, kPowerLimit);
    ASSERT_FALSE(local_state_->GetUserPref(prefs::kShouldAutoEnroll));
    ASSERT_FALSE(local_state_->GetUserPref(prefs::kAutoEnrollmentPowerLimit));
  }

  void TearDown() override {
    // Flush any deletion tasks.
    base::RunLoop().RunUntilIdle();
  }

  AutoEnrollmentProtocol GetAutoEnrollmentProtocol() const {
    return std::get<0>(GetParam()).auto_enrollment_protocol;
  }

  PsmState GetPsmState() const { return std::get<0>(GetParam()).psm_state; }

  int GetPsmTestCaseIndex() const { return std::get<1>(GetParam()); }

  std::string GetAutoEnrollmentProtocolUmaSuffix() const {
    return GetAutoEnrollmentProtocol() ==
                   AutoEnrollmentProtocol::kInitialEnrollment
               ? kUMASuffixInitialEnrollment
               : kUMASuffixFRE;
  }

  void CreateClient(int power_initial, int power_limit) {
    state_ = AUTO_ENROLLMENT_STATE_PENDING;
    service_ =
        std::make_unique<FakeDeviceManagementService>(&job_creation_handler_);
    service_->ScheduleInitialization(0);
    base::RunLoop().RunUntilIdle();

    auto progress_callback =
        base::BindRepeating(&AutoEnrollmentClientImplTest::ProgressCallback,
                            base::Unretained(this));
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &url_loader_factory_);
    if (GetAutoEnrollmentProtocol() == AutoEnrollmentProtocol::kFRE) {
      client_ = AutoEnrollmentClientImpl::FactoryImpl().CreateForFRE(
          progress_callback, service_.get(), local_state_,
          shared_url_loader_factory_, kStateKey, power_initial, power_limit);
    } else {
      // PSM has to be enabled whenever creating a client for initial
      // enrollment.
      DCHECK_EQ(GetPsmState(), PsmState::kEnabled);
      DCHECK(psm_rlwe_test_client_factory_);

      client_ =
          AutoEnrollmentClientImpl::FactoryImpl().CreateForInitialEnrollment(
              progress_callback, service_.get(), local_state_,
              shared_url_loader_factory_, kSerialNumber, kBrandCode,
              power_initial, power_limit, psm_rlwe_test_client_factory_.get());
    }
  }

  void ProgressCallback(AutoEnrollmentState state) { state_ = state; }

  void ServerWillFail(int net_error, int response_code) {
    EXPECT_CALL(job_creation_handler_, OnJobCreation)
        .WillOnce(
            DoAll(service_->CaptureJobType(&failed_job_type_),
                  service_->CaptureRequest(&last_request_),
                  service_->SendJobResponseAsync(net_error, response_code)))
        .RetiresOnSaturation();
  }

  void ServerWillReply(int64_t modulus, bool with_hashes, bool with_id_hash) {
    // This method should be called only when the client has been created for
    // FRE use case.
    ASSERT_EQ(GetAutoEnrollmentProtocol(), AutoEnrollmentProtocol::kFRE);

    em::DeviceManagementResponse response =
        GetAutoEnrollmentResponse(modulus, with_hashes, with_id_hash);

    EXPECT_CALL(job_creation_handler_, OnJobCreation)
        .WillOnce(DoAll(service_->CaptureJobType(&auto_enrollment_job_type_),
                        service_->CaptureRequest(&last_request_),
                        service_->SendJobOKAsync(response)))
        .RetiresOnSaturation();
  }

  em::DeviceInitialEnrollmentStateResponse::InitialEnrollmentMode
  MapRestoreModeToInitialEnrollmentMode(
      em::DeviceStateRetrievalResponse::RestoreMode restore_mode) {
    using DeviceStateRetrieval = em::DeviceStateRetrievalResponse;
    using DeviceInitialEnrollmentState =
        em::DeviceInitialEnrollmentStateResponse;

    switch (restore_mode) {
      case DeviceStateRetrieval::RESTORE_MODE_NONE:
        return DeviceInitialEnrollmentState::INITIAL_ENROLLMENT_MODE_NONE;
      case DeviceStateRetrieval::RESTORE_MODE_REENROLLMENT_REQUESTED:
        return DeviceInitialEnrollmentState::INITIAL_ENROLLMENT_MODE_NONE;
      case DeviceStateRetrieval::RESTORE_MODE_REENROLLMENT_ENFORCED:
        return DeviceInitialEnrollmentState::
            INITIAL_ENROLLMENT_MODE_ENROLLMENT_ENFORCED;
      case DeviceStateRetrieval::RESTORE_MODE_DISABLED:
        return DeviceInitialEnrollmentState::INITIAL_ENROLLMENT_MODE_DISABLED;
      case DeviceStateRetrieval::RESTORE_MODE_REENROLLMENT_ZERO_TOUCH:
        return DeviceInitialEnrollmentState::
            INITIAL_ENROLLMENT_MODE_ZERO_TOUCH_ENFORCED;
    }
  }

  std::string MapDeviceRestoreStateToDeviceInitialState(
      const std::string& restore_state) const {
    if (restore_state == kDeviceStateRestoreModeReEnrollmentEnforced)
      return kDeviceStateInitialModeEnrollmentEnforced;
    if (restore_state == kDeviceStateRestoreModeReEnrollmentZeroTouch)
      return kDeviceStateInitialModeEnrollmentZeroTouch;
    NOTREACHED();
    return "";
  }

  void ServerWillSendState(
      const std::string& management_domain,
      em::DeviceStateRetrievalResponse::RestoreMode restore_mode,
      const std::string& device_disabled_message,
      bool is_license_packaged_with_device,
      em::DeviceInitialEnrollmentStateResponse::LicensePackagingSKU
          license_sku) {
    if (GetAutoEnrollmentProtocol() == AutoEnrollmentProtocol::kFRE) {
      ServerWillSendStateForFRE(management_domain, restore_mode,
                                device_disabled_message, absl::nullopt);
    } else {
      ServerWillSendStateForInitialEnrollment(
          management_domain, is_license_packaged_with_device, license_sku,
          MapRestoreModeToInitialEnrollmentMode(restore_mode));
    }
  }

  DeviceManagementService::JobConfiguration::JobType
  GetStateRetrievalJobType() {
    return GetAutoEnrollmentProtocol() == AutoEnrollmentProtocol::kFRE
               ? DeviceManagementService::JobConfiguration::
                     TYPE_DEVICE_STATE_RETRIEVAL
               : DeviceManagementService::JobConfiguration::
                     TYPE_INITIAL_ENROLLMENT_STATE_RETRIEVAL;
  }

  void ServerWillSendStateForFRE(
      const std::string& management_domain,
      em::DeviceStateRetrievalResponse::RestoreMode restore_mode,
      const std::string& device_disabled_message,
      absl::optional<em::DeviceInitialEnrollmentStateResponse>
          initial_state_response) {
    em::DeviceManagementResponse response;
    em::DeviceStateRetrievalResponse* state_response =
        response.mutable_device_state_retrieval_response();
    state_response->set_restore_mode(restore_mode);
    if (!management_domain.empty())
      state_response->set_management_domain(management_domain);
    state_response->mutable_disabled_state()->set_message(
        device_disabled_message);

    ASSERT_TRUE(!initial_state_response ||
                restore_mode ==
                    em::DeviceStateRetrievalResponse::RESTORE_MODE_NONE);
    if (initial_state_response) {
      state_response->mutable_initial_state_response()->MergeFrom(
          *initial_state_response);
    }

    EXPECT_CALL(job_creation_handler_, OnJobCreation)
        .WillOnce(DoAll(service_->CaptureJobType(&state_retrieval_job_type_),
                        service_->CaptureRequest(&last_request_),
                        service_->SendJobOKAsync(response)))
        .RetiresOnSaturation();
  }

  void ServerWillSendStateForInitialEnrollment(
      const std::string& management_domain,
      bool is_license_packaged_with_device,
      em::DeviceInitialEnrollmentStateResponse::LicensePackagingSKU license_sku,
      em::DeviceInitialEnrollmentStateResponse::InitialEnrollmentMode
          initial_enrollment_mode) {
    em::DeviceManagementResponse response;
    em::DeviceInitialEnrollmentStateResponse* state_response =
        response.mutable_device_initial_enrollment_state_response();
    state_response->set_initial_enrollment_mode(initial_enrollment_mode);
    if (!management_domain.empty())
      state_response->set_management_domain(management_domain);
    state_response->set_is_license_packaged_with_device(
        is_license_packaged_with_device);
    state_response->set_license_packaging_sku(license_sku);
    EXPECT_CALL(job_creation_handler_, OnJobCreation)
        .WillOnce(DoAll(service_->CaptureJobType(&state_retrieval_job_type_),
                        service_->CaptureRequest(&last_request_),
                        service_->SendJobOKAsync(response)))
        .RetiresOnSaturation();
  }

  DeviceManagementService::JobConfiguration::JobType
  GetExpectedStateRetrievalJobType() {
    return GetAutoEnrollmentProtocol() == AutoEnrollmentProtocol::kFRE
               ? DeviceManagementService::JobConfiguration::
                     TYPE_DEVICE_STATE_RETRIEVAL
               : DeviceManagementService::JobConfiguration::
                     TYPE_INITIAL_ENROLLMENT_STATE_RETRIEVAL;
  }

  void ServerWillReplyAsync(DeviceManagementService::JobForTesting* job) {
    EXPECT_CALL(job_creation_handler_, OnJobCreation)
        .WillOnce(DoAll(service_->CaptureJobType(&last_async_job_type_),
                        SaveArg<0>(job)));
  }

  void ServerReplyAsyncJobWithAutoEnrollmentResponse(
      int64_t modulus,
      bool with_hashes,
      bool with_id_hash,
      DeviceManagementService::JobForTesting* job) {
    em::DeviceManagementResponse response =
        GetAutoEnrollmentResponse(modulus, with_hashes, with_id_hash);
    service_->SendJobOKNow(job, response);
  }

  void ServerRepliesEmptyResponseForAsyncJob(
      DeviceManagementService::JobForTesting* job) {
    em::DeviceManagementResponse dummy_response;
    service_->SendJobOKNow(job, dummy_response);
  }

  bool HasCachedDecision() {
    // This method should be called only when the client has been created for
    // FRE use case.
    EXPECT_EQ(GetAutoEnrollmentProtocol(), AutoEnrollmentProtocol::kFRE);

    return local_state_->GetUserPref(prefs::kShouldAutoEnroll);
  }

  void VerifyCachedResult(bool should_enroll, int power_limit) {
    // This method should be called only when the client has been created for
    // FRE use case.
    EXPECT_EQ(GetAutoEnrollmentProtocol(), AutoEnrollmentProtocol::kFRE);

    base::Value value_should_enroll(should_enroll);
    base::Value value_power_limit(power_limit);
    EXPECT_EQ(value_should_enroll,
              *local_state_->GetUserPref(prefs::kShouldAutoEnroll));
    EXPECT_EQ(value_power_limit,
              *local_state_->GetUserPref(prefs::kAutoEnrollmentPowerLimit));
  }

  bool HasServerBackedState() {
    return local_state_->GetUserPref(prefs::kServerBackedDeviceState);
  }

  void VerifyServerBackedState(const std::string& expected_management_domain,
                               const std::string& expected_restore_mode,
                               const std::string& expected_disabled_message,
                               bool expected_is_license_packaged_with_device,
                               const std::string& expected_license_type) {
    if (GetAutoEnrollmentProtocol() == AutoEnrollmentProtocol::kFRE) {
      VerifyServerBackedStateForFRE(expected_management_domain,
                                    expected_restore_mode,
                                    expected_disabled_message);
    } else {
      VerifyServerBackedStateForInitialEnrollment(
          expected_management_domain, expected_restore_mode,
          expected_is_license_packaged_with_device, expected_license_type);
    }
  }

  void VerifyServerBackedStateForAll(
      const std::string& expected_management_domain,
      const std::string& expected_restore_mode,
      const base::DictionaryValue** local_state_dict) {
    const base::Value* state =
        local_state_->GetUserPref(prefs::kServerBackedDeviceState);
    ASSERT_TRUE(state);
    const base::DictionaryValue* state_dict = nullptr;
    ASSERT_TRUE(state->GetAsDictionary(&state_dict));
    *local_state_dict = state_dict;

    std::string actual_management_domain;
    if (expected_management_domain.empty()) {
      EXPECT_FALSE(state_dict->GetString(kDeviceStateManagementDomain,
                                         &actual_management_domain));
    } else {
      EXPECT_TRUE(state_dict->GetString(kDeviceStateManagementDomain,
                                        &actual_management_domain));
      EXPECT_EQ(expected_management_domain, actual_management_domain);
    }

    if (!expected_restore_mode.empty()) {
      std::string actual_restore_mode;
      EXPECT_TRUE(
          state_dict->GetString(kDeviceStateMode, &actual_restore_mode));
    } else {
      EXPECT_EQ(state_dict->FindKey(kDeviceStateMode), nullptr);
    }
  }

  void VerifyServerBackedStateForFRE(
      const std::string& expected_management_domain,
      const std::string& expected_restore_mode,
      const std::string& expected_disabled_message) {
    const base::DictionaryValue* state_dict;
    VerifyServerBackedStateForAll(expected_management_domain,
                                  expected_restore_mode, &state_dict);

    if (!expected_restore_mode.empty()) {
      std::string actual_restore_mode;
      EXPECT_TRUE(
          state_dict->GetString(kDeviceStateMode, &actual_restore_mode));
      EXPECT_EQ(GetAutoEnrollmentProtocol() == AutoEnrollmentProtocol::kFRE
                    ? expected_restore_mode
                    : MapDeviceRestoreStateToDeviceInitialState(
                          expected_restore_mode),
                actual_restore_mode);
    }

    std::string actual_disabled_message;
    EXPECT_TRUE(state_dict->GetString(kDeviceStateDisabledMessage,
                                      &actual_disabled_message));
    EXPECT_EQ(expected_disabled_message, actual_disabled_message);

    EXPECT_FALSE(state_dict->FindBoolPath(kDeviceStatePackagedLicense));

    std::string actual_license_type;
    EXPECT_FALSE(
        state_dict->GetString(kDeviceStateLicenseType, &actual_license_type));
  }

  void VerifyServerBackedStateForInitialEnrollment(
      const std::string& expected_management_domain,
      const std::string& expected_restore_mode,
      bool expected_is_license_packaged_with_device,
      const std::string& expected_license_type) {
    const base::DictionaryValue* state_dict;
    VerifyServerBackedStateForAll(expected_management_domain,
                                  expected_restore_mode, &state_dict);

    std::string actual_disabled_message;
    EXPECT_FALSE(state_dict->GetString(kDeviceStateDisabledMessage,
                                       &actual_disabled_message));

    absl::optional<bool> actual_is_license_packaged_with_device;
    actual_is_license_packaged_with_device =
        state_dict->FindBoolPath(kDeviceStatePackagedLicense);
    if (actual_is_license_packaged_with_device.has_value()) {
      EXPECT_EQ(expected_is_license_packaged_with_device,
                actual_is_license_packaged_with_device.value());
    } else {
      EXPECT_FALSE(expected_is_license_packaged_with_device);
    }

    std::string actual_license_type;
    EXPECT_TRUE(
        state_dict->GetString(kDeviceStateLicenseType, &actual_license_type));
    EXPECT_EQ(actual_license_type, expected_license_type);
  }

  // Expects one sample for |kUMAHashDanceNetworkErrorCode| which has value of
  // |network_error|.
  void ExpectHashDanceNetworkErrorHistogram(int network_error) const {
    // This method should be called only when the client has been created for
    // FRE use case.
    EXPECT_EQ(GetAutoEnrollmentProtocol(), AutoEnrollmentProtocol::kFRE);

    histogram_tester_.ExpectBucketCount(
        kUMAHashDanceNetworkErrorCode + GetAutoEnrollmentProtocolUmaSuffix(),
        network_error, /*expected_count=*/1);
  }

  // Expects a sample for |kUMAHashDanceRequestStatus| with count
  // |dm_status_count|.
  void ExpectHashDanceRequestStatusHistogram(DeviceManagementStatus dm_status,
                                             int dm_status_count) const {
    // This method should be called only when the client has been created for
    // FRE use case.
    EXPECT_EQ(GetAutoEnrollmentProtocol(), AutoEnrollmentProtocol::kFRE);

    histogram_tester_.ExpectBucketCount(
        kUMAHashDanceRequestStatus + GetAutoEnrollmentProtocolUmaSuffix(),
        dm_status, dm_status_count);
  }

  // Expects a sample for |kUMAHashDanceProtocolTime| to have value
  // |expected_time_recorded|.
  // if |success_time_recorded| is true it expects one sample for
  // |kUMAHashDanceSuccessTime| to have value |expected_time_recorded|.
  // Otherwise, expects no sample for |kUMAHashDanceSuccessTime|.
  void ExpectHashDanceExecutionTimeHistogram(
      base::TimeDelta expected_time_recorded,
      bool success_time_recorded) const {
    // This method should be called only when the client has been created for
    // FRE use case.
    EXPECT_EQ(GetAutoEnrollmentProtocol(), AutoEnrollmentProtocol::kFRE);

    histogram_tester_.ExpectUniqueTimeSample(
        kUMAHashDanceProtocolTime + GetAutoEnrollmentProtocolUmaSuffix(),
        expected_time_recorded, /*expected_count=*/1);
    histogram_tester_.ExpectUniqueTimeSample(
        kUMAHashDanceSuccessTime + GetAutoEnrollmentProtocolUmaSuffix(),
        expected_time_recorded, success_time_recorded ? 1 : 0);
  }

  const em::DeviceAutoEnrollmentRequest& auto_enrollment_request() {
    return last_request_.auto_enrollment_request();
  }

  // Returns |client_| as |AutoEnrollmentClientImpl*|. This is fine because this
  // test only creates |client_| using |AutoEnrollmentClientImpl::FactoryImpl|.
  AutoEnrollmentClientImpl* client() {
    return static_cast<AutoEnrollmentClientImpl*>(client_.get());
  }

  // Releases |client_| and returns the pointer as |AutoEnrollmentClientImpl*|.
  // This is fine because this test only creates |client_| using
  // |AutoEnrollmentClientImpl::FactoryImpl|.
  AutoEnrollmentClientImpl* release_client() {
    return static_cast<AutoEnrollmentClientImpl*>(client_.release());
  }

  // Sets which PSM RLWE client will be created, depending on the factory. It is
  // only used for PSM during creating the client for initial enrollment.
  std::unique_ptr<TestingPrivateMembershipRlweClient::FactoryImpl>
      psm_rlwe_test_client_factory_;

  base::HistogramTester histogram_tester_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ScopedTestingLocalState scoped_testing_local_state_;
  TestingPrefServiceSimple* local_state_;
  testing::StrictMock<MockJobCreationHandler> job_creation_handler_;
  std::unique_ptr<FakeDeviceManagementService> service_;
  em::DeviceManagementRequest last_request_;
  AutoEnrollmentState state_;
  DeviceManagementService::JobConfiguration::JobType failed_job_type_ =
      DeviceManagementService::JobConfiguration::TYPE_INVALID;
  DeviceManagementService::JobConfiguration::JobType last_async_job_type_ =
      DeviceManagementService::JobConfiguration::TYPE_INVALID;
  DeviceManagementService::JobConfiguration::JobType auto_enrollment_job_type_ =
      DeviceManagementService::JobConfiguration::TYPE_INVALID;
  DeviceManagementService::JobConfiguration::JobType state_retrieval_job_type_ =
      DeviceManagementService::JobConfiguration::TYPE_INVALID;

 private:
  em::DeviceManagementResponse GetAutoEnrollmentResponse(
      int64_t modulus,
      bool with_hashes,
      bool with_id_hash) const {
    // This method should be called only when the client has been created for
    // FRE use case.
    EXPECT_EQ(GetAutoEnrollmentProtocol(), AutoEnrollmentProtocol::kFRE);

    em::DeviceManagementResponse response;
    em::DeviceAutoEnrollmentResponse* enrollment_response =
        response.mutable_auto_enrollment_response();
    if (modulus >= 0)
      enrollment_response->set_expected_modulus(modulus);
    if (with_hashes) {
      for (int i = 0; i < 10; ++i) {
        std::string state_key = base::StringPrintf("state_key %d", i);
        std::string hash = crypto::SHA256HashString(state_key);
        enrollment_response->mutable_hashes()->Add()->assign(hash);
      }
    }
    if (with_id_hash) {
      enrollment_response->mutable_hashes()->Add()->assign(
          kStateKeyHash, crypto::kSHA256Length);
    }

    return response;
  }

  network::TestURLLoaderFactory url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  std::unique_ptr<AutoEnrollmentClient> client_;
};

TEST_P(AutoEnrollmentClientImplTest, NetworkFailure) {
  ServerWillFail(net::OK, DeviceManagementService::kServiceUnavailable);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  ExpectHashDanceRequestStatusHistogram(DM_STATUS_TEMPORARY_UNAVAILABLE,
                                        /*dm_status_count=*/1);
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT,
            failed_job_type_);
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_SERVER_ERROR);
  EXPECT_FALSE(HasCachedDecision());
  EXPECT_FALSE(HasServerBackedState());
}

TEST_P(AutoEnrollmentClientImplTest, EmptyReply) {
  ServerWillReply(/*modulus=*/-1, /*with_hashes=*/false,
                  /*with_id_hash=*/false);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  ExpectHashDanceRequestStatusHistogram(DM_STATUS_SUCCESS,
                                        /*dm_status_count=*/1);

  // Verify Hash dance protocol overall execution time and its success time
  // histograms were recorded correctly with the same value.
  // Note: The expected time is the difference between starting off the client,
  // and finishing executing the protocol successfully. In this test, the
  // protocol requests are synchronized. Then the recorded time will be zero.
  ExpectHashDanceExecutionTimeHistogram(
      /*expected_time_recorded=*/base::TimeDelta(),
      /*success_time_recorded=*/true);

  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
  VerifyCachedResult(/*should_enroll=*/false, kPowerLimit);
  EXPECT_FALSE(HasServerBackedState());
}

TEST_P(AutoEnrollmentClientImplTest, ClientUploadsRightBits) {
  ServerWillReply(/*modulus=*/-1, /*with_hashes=*/false,
                  /*with_id_hash=*/false);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  ExpectHashDanceRequestStatusHistogram(DM_STATUS_SUCCESS,
                                        /*dm_status_count=*/1);

  // Verify Hash dance protocol overall execution time and its success time
  // histograms were recorded correctly with the same value.
  // Note: The expected time is the difference between starting off the client,
  // and finishing executing the protocol successfully. In this test, the
  // protocol requests are synchronized. Then the recorded time will be zero.
  ExpectHashDanceExecutionTimeHistogram(
      /*expected_time_recorded=*/base::TimeDelta(),
      /*success_time_recorded=*/true);

  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);

  EXPECT_TRUE(auto_enrollment_request().has_remainder());
  EXPECT_TRUE(auto_enrollment_request().has_modulus());
  EXPECT_EQ(16, auto_enrollment_request().modulus());
  EXPECT_EQ(kStateKeyHash[31] & 0xf, auto_enrollment_request().remainder());
  VerifyCachedResult(/*should_enroll=*/false, kPowerLimit);
  EXPECT_FALSE(HasServerBackedState());
}

TEST_P(AutoEnrollmentClientImplTest, AskForMoreThenFail) {
  InSequence sequence;
  ServerWillReply(/*modulus=*/32, /*with_hashes=*/false,
                  /*with_id_hash=*/false);
  ServerWillFail(net::OK, DeviceManagementService::kServiceUnavailable);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  ExpectHashDanceRequestStatusHistogram(DM_STATUS_SUCCESS,
                                        /*dm_status_count=*/1);
  ExpectHashDanceRequestStatusHistogram(DM_STATUS_TEMPORARY_UNAVAILABLE,
                                        /*dm_status_count=*/1);
  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(failed_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_SERVER_ERROR);
  EXPECT_FALSE(HasCachedDecision());
  EXPECT_FALSE(HasServerBackedState());
}

TEST_P(AutoEnrollmentClientImplTest, AskForMoreThenEvenMore) {
  InSequence sequence;
  ServerWillReply(/*modulus=*/32, /*with_hashes=*/false,
                  /*with_id_hash=*/false);
  ServerWillReply(/*modulus=*/64, /*with_hashes=*/false,
                  /*with_id_hash=*/false);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  ExpectHashDanceRequestStatusHistogram(DM_STATUS_SUCCESS,
                                        /*dm_status_count=*/2);

  // Verify Hash dance protocol overall execution time histogram has been
  // recorded correctly. And its success time histogram has not been recorded.
  // Note: The expected time is the difference between starting off the client,
  // and finishing executing the protocol successfully. In this test, the
  // protocol requests are synchronized. Then the recorded time will be zero.
  ExpectHashDanceExecutionTimeHistogram(
      /*expected_time_recorded=*/base::TimeDelta(),
      /*success_time_recorded=*/false);

  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_SERVER_ERROR);
  EXPECT_FALSE(HasCachedDecision());
  EXPECT_FALSE(HasServerBackedState());
}

TEST_P(AutoEnrollmentClientImplTest, AskForLess) {
  InSequence sequence;
  ServerWillReply(/*modulus=*/8, /*with_hashes=*/false, /*with_id_hash=*/false);
  ServerWillReply(/*modulus=*/-1, /*with_hashes=*/true, /*with_id_hash=*/true);
  ServerWillSendState(
      "example.com",
      em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_ENFORCED,
      kDisabledMessage, kWithLicense,
      em::DeviceInitialEnrollmentStateResponse::CHROME_EDUCATION);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  ExpectHashDanceRequestStatusHistogram(DM_STATUS_SUCCESS,
                                        /*dm_status_count=*/3);

  // Verify Hash dance protocol overall execution time and its success time
  // histograms were recorded correctly with the same value.
  // Note: The expected time is the difference between starting off the client,
  // and finishing executing the protocol successfully. In this test, the
  // protocol requests are synchronized. Then the recorded time will be zero.
  ExpectHashDanceExecutionTimeHistogram(
      /*expected_time_recorded=*/base::TimeDelta(),
      /*success_time_recorded=*/true);

  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_retrieval_job_type_, GetExpectedStateRetrievalJobType());
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT);
  VerifyCachedResult(/*should_enroll=*/true, kPowerLimit);
  VerifyServerBackedState(
      "example.com", kDeviceStateRestoreModeReEnrollmentEnforced,
      kDisabledMessage, kWithLicense, kDeviceStateLicenseTypeEducation);
}

TEST_P(AutoEnrollmentClientImplTest, AskForSame) {
  InSequence sequence;
  ServerWillReply(/*modulus=*/16, /*with_hashes=*/false,
                  /*with_id_hash=*/false);
  ServerWillReply(/*modulus=*/-1, /*with_hashes=*/true, /*with_id_hash=*/true);
  ServerWillSendState(
      "example.com",
      em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_ENFORCED,
      kDisabledMessage, kNotWithLicense,
      em::DeviceInitialEnrollmentStateResponse::NOT_EXIST);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  ExpectHashDanceRequestStatusHistogram(DM_STATUS_SUCCESS,
                                        /*dm_status_count=*/3);

  // Verify Hash dance protocol overall execution time and its success time
  // histograms were recorded correctly with the same value.
  // Note: The expected time is the difference between starting off the client,
  // and finishing executing the protocol successfully. In this test, the
  // protocol requests are synchronized. Then the recorded time will be zero.
  ExpectHashDanceExecutionTimeHistogram(
      /*expected_time_recorded=*/base::TimeDelta(),
      /*success_time_recorded=*/true);

  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_retrieval_job_type_, GetExpectedStateRetrievalJobType());
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT);
  VerifyCachedResult(/*should_enroll=*/true, kPowerLimit);
  VerifyServerBackedState("example.com",
                          kDeviceStateRestoreModeReEnrollmentEnforced,
                          kDisabledMessage, kNotWithLicense, kNoLicenseType);
}

TEST_P(AutoEnrollmentClientImplTest, AskForSameTwice) {
  InSequence sequence;
  ServerWillReply(/*modulus=*/16, /*with_hashes=*/false,
                  /*with_id_hash=*/false);
  ServerWillReply(/*modulus=*/16, /*with_hashes=*/false,
                  /*with_id_hash=*/false);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  ExpectHashDanceRequestStatusHistogram(DM_STATUS_SUCCESS,
                                        /*dm_status_count=*/2);

  // Verify Hash dance protocol overall execution time histogram has been
  // recorded correctly. And its success time histogram has not been recorded.
  // Note: The expected time is the difference between starting off the client,
  // and finishing executing the protocol successfully. In this test, the
  // protocol requests are synchronized. Then the recorded time will be zero.
  ExpectHashDanceExecutionTimeHistogram(
      /*expected_time_recorded=*/base::TimeDelta(),
      /*success_time_recorded=*/false);

  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_SERVER_ERROR);
  EXPECT_FALSE(HasCachedDecision());
  EXPECT_FALSE(HasServerBackedState());
}

TEST_P(AutoEnrollmentClientImplTest, AskForTooMuch) {
  ServerWillReply(/*modulus=*/512, /*with_hashes=*/false,
                  /*with_id_hash=*/false);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  ExpectHashDanceRequestStatusHistogram(DM_STATUS_SUCCESS,
                                        /*dm_status_count=*/1);

  // Verify Hash dance protocol overall execution time histogram has been
  // recorded correctly. And its success time histogram has not been recorded.
  // Note: The expected time is the difference between starting off the client,
  // and finishing executing the protocol successfully. In this test, the
  // protocol requests are synchronized. Then the recorded time will be zero.
  ExpectHashDanceExecutionTimeHistogram(
      /*expected_time_recorded=*/base::TimeDelta(),
      /*success_time_recorded=*/false);

  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_SERVER_ERROR);
  EXPECT_FALSE(HasCachedDecision());
  EXPECT_FALSE(HasServerBackedState());
}

TEST_P(AutoEnrollmentClientImplTest, AskNonPowerOf2) {
  InSequence sequence;
  ServerWillReply(/*modulus=*/100, /*with_hashes=*/false,
                  /*with_id_hash=*/false);
  ServerWillReply(/*modulus=*/-1, /*with_hashes=*/false,
                  /*with_id_hash=*/false);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  ExpectHashDanceRequestStatusHistogram(DM_STATUS_SUCCESS,
                                        /*dm_status_count=*/2);

  // Verify Hash dance protocol overall execution time and its success time
  // histograms were recorded correctly with the same value.
  // Note: The expected time is the difference between starting off the client,
  // and finishing executing the protocol successfully. In this test, the
  // protocol requests are synchronized. Then the recorded time will be zero.
  ExpectHashDanceExecutionTimeHistogram(
      /*expected_time_recorded=*/base::TimeDelta(),
      /*success_time_recorded=*/true);

  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
  EXPECT_TRUE(auto_enrollment_request().has_remainder());
  EXPECT_TRUE(auto_enrollment_request().has_modulus());
  EXPECT_EQ(128, auto_enrollment_request().modulus());
  EXPECT_EQ(kStateKeyHash[31] & 0x7f, auto_enrollment_request().remainder());
  VerifyCachedResult(/*should_enroll=*/false, kPowerLimit);
  EXPECT_FALSE(HasServerBackedState());
}

TEST_P(AutoEnrollmentClientImplTest, ConsumerDevice) {
  ServerWillReply(/*modulus=*/-1, /*with_hashes=*/true, /*with_id_hash=*/false);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  ExpectHashDanceRequestStatusHistogram(DM_STATUS_SUCCESS,
                                        /*dm_status_count=*/1);

  // Verify Hash dance protocol overall execution time and its success time
  // histograms were recorded correctly with the same value.
  // Note: The expected time is the difference between starting off the client,
  // and finishing executing the protocol successfully. In this test, the
  // protocol requests are synchronized. Then the recorded time will be zero.
  ExpectHashDanceExecutionTimeHistogram(
      /*expected_time_recorded=*/base::TimeDelta(),
      /*success_time_recorded=*/true);

  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
  VerifyCachedResult(/*should_enroll=*/false, kPowerLimit);
  EXPECT_FALSE(HasServerBackedState());

  // Network changes don't trigger retries after obtaining a response from
  // the server.
  client()->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
}

TEST_P(AutoEnrollmentClientImplTest, ForcedReEnrollment) {
  InSequence sequence;
  ServerWillReply(/*modulus=*/-1, /*with_hashes=*/true, /*with_id_hash=*/true);
  ServerWillSendState(
      "example.com",
      em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_ENFORCED,
      kDisabledMessage, kNotWithLicense,
      em::DeviceInitialEnrollmentStateResponse::NOT_EXIST);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  ExpectHashDanceRequestStatusHistogram(DM_STATUS_SUCCESS,
                                        /*dm_status_count=*/2);

  // Verify Hash dance protocol overall execution time and its success time
  // histograms were recorded correctly with the same value.
  // Note: The expected time is the difference between starting off the client,
  // and finishing executing the protocol successfully. In this test, the
  // protocol requests are synchronized. Then the recorded time will be zero.
  ExpectHashDanceExecutionTimeHistogram(
      /*expected_time_recorded=*/base::TimeDelta(),
      /*success_time_recorded=*/true);

  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_retrieval_job_type_, GetExpectedStateRetrievalJobType());
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT);
  VerifyCachedResult(/*should_enroll=*/true, kPowerLimit);
  VerifyServerBackedState("example.com",
                          kDeviceStateRestoreModeReEnrollmentEnforced,
                          kDisabledMessage, kNotWithLicense, kNoLicenseType);

  // Network changes don't trigger retries after obtaining a response from
  // the server.
  client()->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT);
}

TEST_P(AutoEnrollmentClientImplTest, ForcedReEnrollmentStateRetrivalfailure) {
  InSequence sequence;

  const base::TimeDelta kOneSecondTimeDelta = base::Seconds(1);

  DeviceManagementService::JobForTesting hash_dance_job;
  DeviceManagementService::JobForTesting device_state_job;

  // Expect two requests and capture them, in order, when available in
  // |hash_dance_job| and |device_state_job|.
  ServerWillReplyAsync(&hash_dance_job);
  ServerWillReplyAsync(&device_state_job);

  // Expect none of the jobs have been captured.
  EXPECT_FALSE(hash_dance_job.IsActive());
  EXPECT_FALSE(device_state_job.IsActive());

  client()->Start();
  base::RunLoop().RunUntilIdle();

  // Verify the only job that has been captured is the Hash dance request.
  EXPECT_EQ(last_async_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  ASSERT_TRUE(hash_dance_job.IsActive());
  EXPECT_FALSE(device_state_job.IsActive());

  // Advance the time forward one second.
  task_environment_.FastForwardBy(kOneSecondTimeDelta);

  // Succeed for Hash dance request i.e. DeviceAutoEnrollmentRequest.
  ServerReplyAsyncJobWithAutoEnrollmentResponse(
      /*modulus=*/-1, /*with_hashes=*/true, /*with_id_hash=*/true,
      &hash_dance_job);

  // Advance the time forward one second.
  task_environment_.FastForwardBy(kOneSecondTimeDelta);

  // Verify Hash dance success.
  VerifyCachedResult(/*should_enroll=*/true, kPowerLimit);
  ExpectHashDanceRequestStatusHistogram(DM_STATUS_SUCCESS,
                                        /*dm_status_count=*/1);

  // Verify Hash dance protocol overall execution time and its success time
  // histograms were recorded correctly with the same value.
  ExpectHashDanceExecutionTimeHistogram(
      /*expected_time_recorded=*/base::Seconds(1),
      /*success_time_recorded=*/true);

  // Verify device state job has been captured.
  ASSERT_TRUE(device_state_job.IsActive());
  EXPECT_EQ(last_async_job_type_, GetExpectedStateRetrievalJobType());

  // Fail for DeviceStateRetrievalRequest request by sending an empty response.
  ServerRepliesEmptyResponseForAsyncJob(&device_state_job);

  // Verify that no enrollment has been done, and no state has been retrieved.
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_SERVER_ERROR);
  EXPECT_FALSE(HasServerBackedState());

  // Verify all jobs have finished.
  EXPECT_FALSE(hash_dance_job.IsActive());
  EXPECT_FALSE(device_state_job.IsActive());
}

TEST_P(AutoEnrollmentClientImplTest, ForcedEnrollmentZeroTouch) {
  InSequence sequence;
  ServerWillReply(/*modulus=*/-1, /*with_hashes=*/true, /*with_id_hash=*/true);
  ServerWillSendState(
      "example.com",
      em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_ZERO_TOUCH,
      kDisabledMessage, kNotWithLicense,
      em::DeviceInitialEnrollmentStateResponse::NOT_EXIST);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  ExpectHashDanceRequestStatusHistogram(DM_STATUS_SUCCESS,
                                        /*dm_status_count=*/2);

  // Verify Hash dance protocol overall execution time and its success time
  // histograms were recorded correctly with the same value.
  // Note: The expected time is the difference between starting off the client,
  // and finishing executing the protocol successfully. In this test, the
  // protocol requests are synchronized. Then the recorded time will be zero.
  ExpectHashDanceExecutionTimeHistogram(
      /*expected_time_recorded=*/base::TimeDelta(),
      /*success_time_recorded=*/true);

  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_retrieval_job_type_, GetExpectedStateRetrievalJobType());
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_TRIGGER_ZERO_TOUCH);
  VerifyCachedResult(/*should_enroll=*/true, kPowerLimit);
  VerifyServerBackedState("example.com",
                          kDeviceStateRestoreModeReEnrollmentZeroTouch,
                          kDisabledMessage, kNotWithLicense, kNoLicenseType);

  // Network changes don't trigger retries after obtaining a response from
  // the server.
  client()->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_TRIGGER_ZERO_TOUCH);
}

TEST_P(AutoEnrollmentClientImplTest, RequestedReEnrollment) {
  InSequence sequence;
  ServerWillReply(/*modulus=*/-1, /*with_hashes=*/true, /*with_id_hash=*/true);
  ServerWillSendState(
      "example.com",
      em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_REQUESTED,
      kDisabledMessage, kNotWithLicense,
      em::DeviceInitialEnrollmentStateResponse::NOT_EXIST);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  ExpectHashDanceRequestStatusHistogram(DM_STATUS_SUCCESS,
                                        /*dm_status_count=*/2);

  // Verify Hash dance protocol overall execution time and its success time
  // histograms were recorded correctly with the same value.
  // Note: The expected time is the difference between starting off the client,
  // and finishing executing the protocol successfully. In this test, the
  // protocol requests are synchronized. Then the recorded time will be zero.
  ExpectHashDanceExecutionTimeHistogram(
      /*expected_time_recorded=*/base::TimeDelta(),
      /*success_time_recorded=*/true);

  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_retrieval_job_type_, GetExpectedStateRetrievalJobType());
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT);
  VerifyCachedResult(/*should_enroll=*/true, kPowerLimit);
  VerifyServerBackedState("example.com",
                          kDeviceStateRestoreModeReEnrollmentRequested,
                          kDisabledMessage, kNotWithLicense, kNoLicenseType);
}

TEST_P(AutoEnrollmentClientImplTest, DeviceDisabled) {
  InSequence sequence;
  ServerWillReply(/*modulus=*/-1, /*with_hashes=*/true, /*with_id_hash=*/true);
  ServerWillSendState("example.com",
                      em::DeviceStateRetrievalResponse::RESTORE_MODE_DISABLED,
                      kDisabledMessage, kNotWithLicense,
                      em::DeviceInitialEnrollmentStateResponse::NOT_EXIST);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  ExpectHashDanceRequestStatusHistogram(DM_STATUS_SUCCESS,
                                        /*dm_status_count=*/2);

  // Verify Hash dance protocol overall execution time and its success time
  // histograms were recorded correctly with the same value.
  // Note: The expected time is the difference between starting off the client,
  // and finishing executing the protocol successfully. In this test, the
  // protocol requests are synchronized. Then the recorded time will be zero.
  ExpectHashDanceExecutionTimeHistogram(
      /*expected_time_recorded=*/base::TimeDelta(),
      /*success_time_recorded=*/true);

  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_retrieval_job_type_, GetExpectedStateRetrievalJobType());
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_DISABLED);
  VerifyCachedResult(/*should_enroll=*/true, kPowerLimit);
  VerifyServerBackedState("example.com", kDeviceStateModeDisabled,
                          kDisabledMessage, kNotWithLicense, kNoLicenseType);
}

TEST_P(AutoEnrollmentClientImplTest, NoReEnrollment) {
  InSequence sequence;
  ServerWillReply(/*modulus=*/-1, /*with_hashes=*/true, /*with_id_hash=*/true);
  ServerWillSendState(std::string(),
                      em::DeviceStateRetrievalResponse::RESTORE_MODE_NONE,
                      std::string(), kNotWithLicense,
                      em::DeviceInitialEnrollmentStateResponse::NOT_EXIST);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  ExpectHashDanceRequestStatusHistogram(DM_STATUS_SUCCESS,
                                        /*dm_status_count=*/2);

  // Verify Hash dance protocol overall execution time and its success time
  // histograms were recorded correctly with the same value.
  // Note: The expected time is the difference between starting off the client,
  // and finishing executing the protocol successfully. In this test, the
  // protocol requests are synchronized. Then the recorded time will be zero.
  ExpectHashDanceExecutionTimeHistogram(
      /*expected_time_recorded=*/base::TimeDelta(),
      /*success_time_recorded=*/true);

  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_retrieval_job_type_, GetExpectedStateRetrievalJobType());
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
  VerifyCachedResult(/*should_enroll=*/true, kPowerLimit);
  VerifyServerBackedState(std::string(), std::string(), std::string(),
                          kNotWithLicense, kNoLicenseType);

  // Network changes don't trigger retries after obtaining a response from
  // the server.
  client()->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
}

TEST_P(AutoEnrollmentClientImplTest, NoBitsUploaded) {
  CreateClient(/*power_initial=*/0, /*power_limit=*/0);
  ServerWillReply(/*modulus=*/-1, /*with_hashes=*/false,
                  /*with_id_hash=*/false);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  ExpectHashDanceRequestStatusHistogram(DM_STATUS_SUCCESS,
                                        /*dm_status_count=*/1);

  // Verify Hash dance protocol overall execution time and its success time
  // histograms were recorded correctly with the same value.
  // Note: The expected time is the difference between starting off the client,
  // and finishing executing the protocol successfully. In this test, the
  // protocol requests are synchronized. Then the recorded time will be zero.
  ExpectHashDanceExecutionTimeHistogram(
      /*expected_time_recorded=*/base::TimeDelta(),
      /*success_time_recorded=*/true);

  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
  EXPECT_TRUE(auto_enrollment_request().has_remainder());
  EXPECT_TRUE(auto_enrollment_request().has_modulus());
  EXPECT_EQ(1, auto_enrollment_request().modulus());
  EXPECT_EQ(0, auto_enrollment_request().remainder());
  VerifyCachedResult(/*should_enroll=*/false, /*power_limit=*/0);
  EXPECT_FALSE(HasServerBackedState());
}

TEST_P(AutoEnrollmentClientImplTest, ManyBitsUploaded) {
  int64_t bottom62 = INT64_C(0x386e7244d097c3e6);
  for (int i = 0; i <= 62; ++i) {
    CreateClient(/*power_initial=*/i, /*power_limit=*/i);
    ServerWillReply(/*modulus=*/-1, /*with_hashes=*/false,
                    /*with_id_hash=*/false);
    client()->Start();
    base::RunLoop().RunUntilIdle();
    ExpectHashDanceRequestStatusHistogram(DM_STATUS_SUCCESS,
                                          /*dm_status_count=*/i + 1);
    EXPECT_EQ(auto_enrollment_job_type_,
              DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
    EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
    EXPECT_TRUE(auto_enrollment_request().has_remainder());
    EXPECT_TRUE(auto_enrollment_request().has_modulus());
    EXPECT_EQ(INT64_C(1) << i, auto_enrollment_request().modulus());
    EXPECT_EQ(bottom62 % (INT64_C(1) << i),
              auto_enrollment_request().remainder());
    VerifyCachedResult(/*should_enroll=*/false, /*power_limit=*/i);
    EXPECT_FALSE(HasServerBackedState());
  }
}

TEST_P(AutoEnrollmentClientImplTest, MoreThan32BitsUploaded) {
  CreateClient(/*power_initial=*/10, /*power_limit=*/37);
  InSequence sequence;
  ServerWillReply(/*modulus=*/INT64_C(1) << 37, /*with_hashes=*/false,
                  /*with_id_hash=*/false);
  ServerWillReply(/*modulus=*/-1, /*with_hashes=*/true, /*with_id_hash=*/true);
  ServerWillSendState(
      "example.com",
      em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_ENFORCED,
      kDisabledMessage, kNotWithLicense,
      em::DeviceInitialEnrollmentStateResponse::NOT_EXIST);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  ExpectHashDanceRequestStatusHistogram(DM_STATUS_SUCCESS,
                                        /*dm_status_count=*/3);

  // Verify Hash dance protocol overall execution time and its success time
  // histograms were recorded correctly with the same value.
  // Note: The expected time is the difference between starting off the client,
  // and finishing executing the protocol successfully. In this test, the
  // protocol requests are synchronized. Then the recorded time will be zero.
  ExpectHashDanceExecutionTimeHistogram(
      /*expected_time_recorded=*/base::TimeDelta(),
      /*success_time_recorded=*/true);

  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_retrieval_job_type_, GetExpectedStateRetrievalJobType());
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT);
  VerifyCachedResult(/*should_enroll=*/true, /*power_limit=*/37);
  VerifyServerBackedState("example.com",
                          kDeviceStateRestoreModeReEnrollmentEnforced,
                          kDisabledMessage, kNotWithLicense, kNoLicenseType);
}

TEST_P(AutoEnrollmentClientImplTest, ReuseCachedDecision) {
  // No bucket download requests should be issued.
  EXPECT_CALL(job_creation_handler_, OnJobCreation).Times(0);
  local_state_->SetUserPref(prefs::kShouldAutoEnroll,
                            std::make_unique<base::Value>(true));
  local_state_->SetUserPref(prefs::kAutoEnrollmentPowerLimit,
                            std::make_unique<base::Value>(8));

  // Note that device state will be retrieved every time, regardless of any
  // cached information. This is intentional, the idea is that device state on
  // the server may change.
  ServerWillSendState(
      "example.com",
      em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_ENFORCED,
      kDisabledMessage, kNotWithLicense,
      em::DeviceInitialEnrollmentStateResponse::NOT_EXIST);

  client()->Start();
  base::RunLoop().RunUntilIdle();
  ExpectHashDanceRequestStatusHistogram(DM_STATUS_SUCCESS,
                                        /*dm_status_count=*/1);
  EXPECT_EQ(state_retrieval_job_type_, GetExpectedStateRetrievalJobType());
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT);
  VerifyServerBackedState("example.com",
                          kDeviceStateRestoreModeReEnrollmentEnforced,
                          kDisabledMessage, kNotWithLicense, kNoLicenseType);
}

TEST_P(AutoEnrollmentClientImplTest, RetryIfPowerLargerThanCached) {
  local_state_->SetUserPref(prefs::kShouldAutoEnroll,
                            std::make_unique<base::Value>(false));
  local_state_->SetUserPref(prefs::kAutoEnrollmentPowerLimit,
                            std::make_unique<base::Value>(8));
  CreateClient(/*power_initial=*/5, /*power_limit=*/10);

  InSequence sequence;
  ServerWillReply(/*modulus=*/-1, /*with_hashes=*/true, /*with_id_hash=*/true);
  ServerWillSendState(
      "example.com",
      em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_ENFORCED,
      kDisabledMessage, kNotWithLicense,
      em::DeviceInitialEnrollmentStateResponse::NOT_EXIST);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  ExpectHashDanceRequestStatusHistogram(DM_STATUS_SUCCESS,
                                        /*dm_status_count=*/2);

  // Verify Hash dance protocol overall execution time and its success time
  // histograms were recorded correctly with the same value.
  // Note: The expected time is the difference between starting off the client,
  // and finishing executing the protocol successfully. In this test, the
  // protocol requests are synchronized. Then the recorded time will be zero.
  ExpectHashDanceExecutionTimeHistogram(
      /*expected_time_recorded=*/base::TimeDelta(),
      /*success_time_recorded=*/true);

  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_retrieval_job_type_, GetExpectedStateRetrievalJobType());
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT);
  VerifyServerBackedState("example.com",
                          kDeviceStateRestoreModeReEnrollmentEnforced,
                          kDisabledMessage, kNotWithLicense, kNoLicenseType);
}

TEST_P(AutoEnrollmentClientImplTest, NetworkChangeRetryAfterErrors) {
  ServerWillFail(net::OK, DeviceManagementService::kServiceUnavailable);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  ExpectHashDanceRequestStatusHistogram(DM_STATUS_TEMPORARY_UNAVAILABLE,
                                        /*dm_status_count=*/1);
  // Don't invoke the callback if there was a network failure.
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT,
            failed_job_type_);
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_SERVER_ERROR);
  EXPECT_FALSE(HasCachedDecision());
  EXPECT_FALSE(HasServerBackedState());

  // The client doesn't retry if no new connection became available.
  client()->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_NONE);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_SERVER_ERROR);
  EXPECT_FALSE(HasCachedDecision());
  EXPECT_FALSE(HasServerBackedState());

  // Retry once the network is back.
  InSequence sequence;
  ServerWillReply(/*modulus=*/-1, /*with_hashes=*/true, /*with_id_hash=*/true);
  ServerWillSendState(
      "example.com",
      em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_ENFORCED,
      kDisabledMessage, kNotWithLicense,
      em::DeviceInitialEnrollmentStateResponse::NOT_EXIST);
  client()->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  base::RunLoop().RunUntilIdle();
  ExpectHashDanceRequestStatusHistogram(DM_STATUS_SUCCESS,
                                        /*dm_status_count=*/2);
  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_retrieval_job_type_, GetExpectedStateRetrievalJobType());
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT);
  EXPECT_TRUE(HasCachedDecision());
  VerifyServerBackedState("example.com",
                          kDeviceStateRestoreModeReEnrollmentEnforced,
                          kDisabledMessage, kNotWithLicense, kNoLicenseType);

  // Subsequent network changes don't trigger retries.
  client()->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_NONE);
  base::RunLoop().RunUntilIdle();
  client()->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT);
  EXPECT_TRUE(HasCachedDecision());
  VerifyServerBackedState("example.com",
                          kDeviceStateRestoreModeReEnrollmentEnforced,
                          kDisabledMessage, kNotWithLicense, kNoLicenseType);
}

TEST_P(AutoEnrollmentClientImplTest, CancelAndDeleteSoonWithPendingRequest) {
  DeviceManagementService::JobForTesting job;
  ServerWillReplyAsync(&job);
  EXPECT_FALSE(job.IsActive());
  client()->Start();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(job.IsActive());
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_PENDING);

  // Cancel while a request is in flight.
  EXPECT_TRUE(base::CurrentThread::Get()->IsIdleForTesting());
  release_client()->CancelAndDeleteSoon();
  EXPECT_TRUE(base::CurrentThread::Get()->IsIdleForTesting());

  // The client cleans itself up once a reply is received.
  service_->SendJobResponseNow(&job, net::OK,
                               DeviceManagementService::kServiceUnavailable,
                               em::DeviceManagementResponse());
  EXPECT_FALSE(job.IsActive());
  // The DeleteSoon task has been posted:
  EXPECT_FALSE(base::CurrentThread::Get()->IsIdleForTesting());
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_PENDING);
}

TEST_P(AutoEnrollmentClientImplTest, NetworkChangedAfterCancelAndDeleteSoon) {
  DeviceManagementService::JobForTesting job;
  ServerWillReplyAsync(&job);
  EXPECT_FALSE(job.IsActive());
  client()->Start();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(job.IsActive());
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_PENDING);

  // Cancel while a request is in flight.
  EXPECT_TRUE(base::CurrentThread::Get()->IsIdleForTesting());
  AutoEnrollmentClientImpl* client = release_client();
  client->CancelAndDeleteSoon();
  EXPECT_TRUE(base::CurrentThread::Get()->IsIdleForTesting());

  // Network change events are ignored while a request is pending.
  client->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_PENDING);

  // The client cleans itself up once a reply is received.
  service_->SendJobResponseNow(&job, net::OK,
                               DeviceManagementService::kServiceUnavailable,
                               em::DeviceManagementResponse());
  EXPECT_FALSE(job.IsActive());
  // The DeleteSoon task has been posted:
  EXPECT_FALSE(base::CurrentThread::Get()->IsIdleForTesting());
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_PENDING);

  // Network changes that have been posted before are also ignored:
  client->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_PENDING);
}

TEST_P(AutoEnrollmentClientImplTest, CancelAndDeleteSoonAfterCompletion) {
  InSequence sequence;
  ServerWillReply(/*modulus=*/-1, /*with_hashes=*/true, /*with_id_hash=*/true);
  ServerWillSendState(
      "example.com",
      em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_ENFORCED,
      kDisabledMessage, kNotWithLicense,
      em::DeviceInitialEnrollmentStateResponse::NOT_EXIST);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_retrieval_job_type_, GetExpectedStateRetrievalJobType());
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT);
  VerifyServerBackedState("example.com",
                          kDeviceStateRestoreModeReEnrollmentEnforced,
                          kDisabledMessage, kNotWithLicense, kNoLicenseType);

  // The client will delete itself immediately if there are no pending
  // requests.
  EXPECT_TRUE(base::CurrentThread::Get()->IsIdleForTesting());
  release_client()->CancelAndDeleteSoon();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(base::CurrentThread::Get()->IsIdleForTesting());
}

TEST_P(AutoEnrollmentClientImplTest, CancelAndDeleteSoonAfterNetworkFailure) {
  ServerWillFail(net::OK, DeviceManagementService::kServiceUnavailable);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  ExpectHashDanceRequestStatusHistogram(DM_STATUS_TEMPORARY_UNAVAILABLE,
                                        /*dm_status_count=*/1);
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT,
            failed_job_type_);
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_SERVER_ERROR);

  // The client will delete itself immediately if there are no pending
  // requests.
  EXPECT_TRUE(base::CurrentThread::Get()->IsIdleForTesting());
  release_client()->CancelAndDeleteSoon();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(base::CurrentThread::Get()->IsIdleForTesting());
}

TEST_P(AutoEnrollmentClientImplTest, NetworkFailureThenRequireUpdatedModulus) {
  // This test verifies that if the first request fails due to a network
  // problem then the second request will correctly handle an updated
  // modulus request from the server.

  ServerWillFail(net::ERR_FAILED, DeviceManagementService::kSuccess);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  ExpectHashDanceNetworkErrorHistogram(-net::ERR_FAILED);
  // Callback should signal the connection error.
  EXPECT_EQ(DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT,
            failed_job_type_);
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_CONNECTION_ERROR);
  EXPECT_FALSE(HasCachedDecision());
  EXPECT_FALSE(HasServerBackedState());
  Mock::VerifyAndClearExpectations(service_.get());

  InSequence sequence;
  // The default client uploads 4 bits. Make the server ask for 5.
  ServerWillReply(/*modulus=*/1 << 5, /*with_hashes=*/false,
                  /*with_id_hash=*/false);
  // Then reply with a valid response and include the hash.
  ServerWillReply(/*modulus=*/-1, /*with_hashes=*/true, /*with_id_hash=*/true);
  // State download triggers.
  ServerWillSendState(
      "example.com",
      em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_ENFORCED,
      kDisabledMessage, kNotWithLicense,
      em::DeviceInitialEnrollmentStateResponse::NOT_EXIST);

  // Trigger a network change event.
  client()->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  base::RunLoop().RunUntilIdle();
  ExpectHashDanceRequestStatusHistogram(DM_STATUS_REQUEST_FAILED,
                                        /*dm_status_count=*/1);
  ExpectHashDanceRequestStatusHistogram(DM_STATUS_SUCCESS,
                                        /*dm_status_count=*/3);

  // Verify Hash dance protocol overall execution time and its success time
  // histograms were recorded correctly with the same value.
  // Note: The expected time is the difference between starting off the client,
  // and finishing executing the protocol successfully. In this test, the
  // protocol requests are synchronized. Then the recorded time will be zero.
  ExpectHashDanceExecutionTimeHistogram(
      /*expected_time_recorded=*/base::TimeDelta(),
      /*success_time_recorded=*/true);

  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT);
  EXPECT_TRUE(HasCachedDecision());
  VerifyServerBackedState("example.com",
                          kDeviceStateRestoreModeReEnrollmentEnforced,
                          kDisabledMessage, kNotWithLicense, kNoLicenseType);
  Mock::VerifyAndClearExpectations(service_.get());
  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_retrieval_job_type_, GetExpectedStateRetrievalJobType());
}

// PSM is disabed to test only Hash dance for FRE case extensively instead. That
// is because PSM is running only for initial enrollment, and Hash dance for FRE
// use case.
INSTANTIATE_TEST_SUITE_P(
    FRE,
    AutoEnrollmentClientImplTest,
    testing::Combine(testing::Values(AutoEnrollmentClientImplTestState(
                         AutoEnrollmentProtocol::kFRE,
                         PsmState::kDisabled)),
                     testing::Values(kInvalidPsmTestCaseIndex)));

using AutoEnrollmentClientImplFREToInitialEnrollmentTest =
    AutoEnrollmentClientImplTest;

TEST_P(AutoEnrollmentClientImplFREToInitialEnrollmentTest,
       NoReEnrollmentInitialEnrollmentLicensePackaging) {
  InSequence sequence;
  ServerWillReply(/*modulus=*/-1, /*with_hashes=*/true, /*with_id_hash=*/true);
  em::DeviceInitialEnrollmentStateResponse initial_state_response;
  initial_state_response.set_is_license_packaged_with_device(kWithLicense);
  initial_state_response.set_license_packaging_sku(
      em::DeviceInitialEnrollmentStateResponse::CHROME_ENTERPRISE);
  ServerWillSendStateForFRE(
      /*management_domain=*/std::string(),
      em::DeviceStateRetrievalResponse::RESTORE_MODE_NONE,
      /*device_disabled_message=*/std::string(), initial_state_response);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  ExpectHashDanceRequestStatusHistogram(DM_STATUS_SUCCESS,
                                        /*dm_status_count=*/2);
  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_retrieval_job_type_, GetExpectedStateRetrievalJobType());
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
  VerifyCachedResult(/*should_enroll=*/true, kPowerLimit);
  VerifyServerBackedStateForInitialEnrollment(
      std::string(), std::string(), kWithLicense,
      kDeviceStateLicenseTypeEnterprise);

  // Network changes don't trigger retries after obtaining a response from
  // the server.
  client()->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
}

TEST_P(AutoEnrollmentClientImplFREToInitialEnrollmentTest,
       NoReEnrollmentInitialEnrollmentZeroTouch) {
  InSequence sequence;
  ServerWillReply(/*modulus=*/-1, /*with_hashes=*/true, /*with_id_hash=*/true);
  em::DeviceInitialEnrollmentStateResponse initial_state_response;
  initial_state_response.set_initial_enrollment_mode(
      em::DeviceInitialEnrollmentStateResponse::
          INITIAL_ENROLLMENT_MODE_ZERO_TOUCH_ENFORCED);
  initial_state_response.set_management_domain("example.com");
  initial_state_response.set_is_license_packaged_with_device(kWithLicense);
  initial_state_response.set_license_packaging_sku(
      em::DeviceInitialEnrollmentStateResponse::CHROME_ENTERPRISE);
  ServerWillSendStateForFRE(
      /*management_domain=*/std::string(),
      em::DeviceStateRetrievalResponse::RESTORE_MODE_NONE,
      /*device_disabled_message=*/std::string(), initial_state_response);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  ExpectHashDanceRequestStatusHistogram(DM_STATUS_SUCCESS,
                                        /*dm_status_count=*/2);
  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_retrieval_job_type_, GetExpectedStateRetrievalJobType());
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_TRIGGER_ZERO_TOUCH);
  VerifyCachedResult(/*should_enroll=*/true, kPowerLimit);
  VerifyServerBackedStateForInitialEnrollment(
      "example.com", kDeviceStateInitialModeEnrollmentZeroTouch, kWithLicense,
      kDeviceStateLicenseTypeEnterprise);

  // Network changes don't trigger retries after obtaining a response from
  // the server.
  client()->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_TRIGGER_ZERO_TOUCH);
}

TEST_P(AutoEnrollmentClientImplFREToInitialEnrollmentTest,
       NoReEnrollmentInitialEnrollmentGuaranteed) {
  InSequence sequence;
  ServerWillReply(/*modulus=*/-1, /*with_hashes=*/true, /*with_id_hash=*/true);
  em::DeviceInitialEnrollmentStateResponse initial_state_response;
  initial_state_response.set_initial_enrollment_mode(
      em::DeviceInitialEnrollmentStateResponse::
          INITIAL_ENROLLMENT_MODE_ENROLLMENT_ENFORCED);
  initial_state_response.set_management_domain("example.com");
  initial_state_response.set_is_license_packaged_with_device(kWithLicense);
  initial_state_response.set_license_packaging_sku(
      em::DeviceInitialEnrollmentStateResponse::CHROME_ENTERPRISE);
  ServerWillSendStateForFRE(
      /*management_domain=*/std::string(),
      em::DeviceStateRetrievalResponse::RESTORE_MODE_NONE,
      /*device_disabled_message=*/std::string(), initial_state_response);
  client()->Start();
  base::RunLoop().RunUntilIdle();
  ExpectHashDanceRequestStatusHistogram(DM_STATUS_SUCCESS,
                                        /*dm_status_count=*/2);
  EXPECT_EQ(auto_enrollment_job_type_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT);
  EXPECT_EQ(state_retrieval_job_type_, GetExpectedStateRetrievalJobType());
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT);
  VerifyCachedResult(/*should_enroll=*/true, kPowerLimit);
  VerifyServerBackedStateForInitialEnrollment(
      "example.com", kDeviceStateInitialModeEnrollmentEnforced, kWithLicense,
      kDeviceStateLicenseTypeEnterprise);

  // Network changes don't trigger retries after obtaining a response from
  // the server.
  client()->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT);
}

// PSM is disabed to test only Hash dance for FRE case extensively instead. That
// is because PSM is running only for initial enrollment, and Hash dance for FRE
// use case.
INSTANTIATE_TEST_SUITE_P(
    FREToInitialEnrollment,
    AutoEnrollmentClientImplFREToInitialEnrollmentTest,
    testing::Combine(testing::Values(AutoEnrollmentClientImplTestState(
                         AutoEnrollmentProtocol::kFRE,
                         PsmState::kDisabled)),
                     testing::Values(kInvalidPsmTestCaseIndex)));

// This class is used to test any PSM related test cases only. Therefore, the
// PsmState param has to be kEnabled.
class PsmHelperTest : public AutoEnrollmentClientImplTest {
 protected:
  // Indicates the state of the PSM protocol.
  enum class StateDiscoveryResult {
    // Failed.
    kFailure = 0,
    // Succeeded, the result was that the server does not have any state for the
    // device.
    kSuccessNoServerSideState = 1,
    // Succeeded, the result was that the server does have a state for the
    // device.
    kSuccessHasServerSideState = 2,
  };

  PsmHelperTest() {}
  ~PsmHelperTest() {
    // Flush any deletion tasks.
    base::RunLoop().RunUntilIdle();
  }

  void SetUp() override {
    // Verify that PSM is enabled (i.e. PsmState has value kEnable).
    ASSERT_EQ(GetPsmState(), PsmState::kEnabled);

    // Verify that all PSM prefs have not been set before.
    ASSERT_EQ(local_state_->GetUserPref(prefs::kShouldRetrieveDeviceState),
              nullptr);
    ASSERT_EQ(local_state_->GetUserPref(prefs::kEnrollmentPsmDeterminationTime),
              nullptr);
    ASSERT_EQ(local_state_->GetUserPref(prefs::kEnrollmentPsmResult), nullptr);

    // Create PSM test case, before setting up the base class, to construct the
    // PSM RLWE testing client factory.
    CreatePsmTestCase();

    // Set up the base class AutoEnrollmentClientImplTest after creating the PSM
    // RLWE client factory for testing in |psm_rlwe_test_client_factory_|.
    AutoEnrollmentClientImplTest::SetUp();

    // Override the stored PSM ID in the client.
    SetPsmRlweIdClient();
  }

  void CreatePsmTestCase() {
    // Verify PSM test case index is valid.
    ASSERT_GE(GetPsmTestCaseIndex(), 0);

    // Retrieve the PSM test case.
    base::FilePath src_root_dir;
    EXPECT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &src_root_dir));
    const base::FilePath kPsmTestDataPath =
        src_root_dir.AppendASCII("third_party")
            .AppendASCII("private_membership")
            .AppendASCII("src")
            .AppendASCII("internal")
            .AppendASCII("testing")
            .AppendASCII("regression_test_data")
            .AppendASCII("cros_test_data.binarypb");
    EXPECT_TRUE(base::PathExists(kPsmTestDataPath));
    psm_rlwe::PrivateMembershipRlweClientRegressionTestData test_data;
    EXPECT_TRUE(ParseProtoFromFile(kPsmTestDataPath, &test_data));
    EXPECT_EQ(test_data.test_cases_size(), kNumberOfPsmTestCases);
    psm_test_case_ = test_data.test_cases(GetPsmTestCaseIndex());

    std::vector<private_membership::rlwe::RlwePlaintextId> plaintext_ids{
        psm_test_case_.plaintext_id()};

    // Sets the PSM RLWE client factory to testing client.
    psm_rlwe_test_client_factory_ =
        std::make_unique<TestingPrivateMembershipRlweClient::FactoryImpl>(
            psm_test_case_.ec_cipher_key(), psm_test_case_.seed(),
            plaintext_ids);
  }

  void SetPsmRlweIdClient() {
    client()->SetPsmRlweIdForTesting(psm_test_case_.plaintext_id());
  }

  void ServerWillReplyWithPsmOprfResponse() {
    em::DeviceManagementResponse response = GetPsmOprfResponse();

    ServerWillReplyForPsm(net::OK, DeviceManagementService::kSuccess, response);
  }

  void ServerWillReplyWithPsmQueryResponse() {
    em::DeviceManagementResponse response = GetPsmQueryResponse();

    ServerWillReplyForPsm(net::OK, DeviceManagementService::kSuccess, response);
  }

  void ServerWillReplyWithEmptyPsmResponse() {
    em::DeviceManagementResponse dummy_response;
    ServerWillReplyForPsm(net::OK, DeviceManagementService::kSuccess,
                          dummy_response);
  }

  void ServerWillFailForPsm(int net_error, int response_code) {
    em::DeviceManagementResponse dummy_response;
    ServerWillReplyForPsm(net_error, response_code, dummy_response);
  }

  // Mocks the server reply and captures the job type in |psm_last_job_type_|,
  // and the request in |psm_last_request_|.
  void ServerWillReplyForPsm(int net_error,
                             int response_code,
                             const em::DeviceManagementResponse& response) {
    EXPECT_CALL(job_creation_handler_, OnJobCreation)
        .WillOnce(DoAll(
            service_->CaptureJobType(&psm_last_job_type_),
            service_->CaptureRequest(&psm_last_request_),
            service_->SendJobResponseAsync(net_error, response_code, response)))
        .RetiresOnSaturation();
  }

  // Holds the full control of the given job in |job| and captures the job type
  // in |psm_last_job_type_|, and its request in |psm_last_request_|.
  void ServerWillReplyAsyncForPsm(DeviceManagementService::JobForTesting* job) {
    EXPECT_CALL(job_creation_handler_, OnJobCreation)
        .WillOnce(DoAll(service_->CaptureJobType(&psm_last_job_type_),
                        service_->CaptureRequest(&psm_last_request_),
                        SaveArg<0>(job)));
  }

  void ServerReplyForPsmAsyncJobWithOprfResponse(
      DeviceManagementService::JobForTesting* job) {
    em::DeviceManagementResponse response = GetPsmOprfResponse();
    service_->SendJobOKNow(job, response);
  }

  void ServerReplyForPsmAsyncJobWithQueryResponse(
      DeviceManagementService::JobForTesting* job) {
    em::DeviceManagementResponse response = GetPsmQueryResponse();
    service_->SendJobOKNow(job, response);
  }

  void ServerFailsForAsyncJob(DeviceManagementService::JobForTesting* job) {
    service_->SendJobResponseNow(job, net::OK,
                                 DeviceManagementService::kServiceUnavailable);
  }

  const em::PrivateSetMembershipRequest& psm_request() const {
    return psm_last_request_.private_set_membership_request();
  }

  // Returns the PSM execution result that has been stored in
  // prefs::kEnrollmentPsmResult. If prefs::kEnrollmentPsmResult is not set, or
  // its value is invalid compared to PsmExecutionResult enum values, then it
  // will return PSM_RESULT_UNKNOWN. Otherwise, it will return the coressponding
  // result.
  PsmExecutionResult GetPsmExecutionResult() const {
    const base::Value* psm_execution_result =
        local_state_->GetUserPref(prefs::kEnrollmentPsmResult);
    if (!psm_execution_result ||
        !em::DeviceRegisterRequest::PsmExecutionResult_IsValid(
            psm_execution_result->GetInt()))
      return em::DeviceRegisterRequest::PSM_RESULT_UNKNOWN;
    return static_cast<PsmExecutionResult>(psm_execution_result->GetInt());
  }

  // Returns the PSM determination timestamp that has been stored in
  // prefs::kEnrollmentPsmDeterminationTime.
  // Note: The function will return a NULL object of base::Time if
  // prefs::kEnrollmentPsmDeterminationTime is not set.
  base::Time GetPsmDeterminationTimestamp() const {
    return local_state_->GetTime(prefs::kEnrollmentPsmDeterminationTime);
  }

  StateDiscoveryResult GetStateDiscoveryResult() const {
    const base::Value* device_state_value =
        local_state_->GetUserPref(prefs::kShouldRetrieveDeviceState);
    if (!device_state_value)
      return StateDiscoveryResult::kFailure;
    return device_state_value->GetBool()
               ? StateDiscoveryResult::kSuccessHasServerSideState
               : StateDiscoveryResult::kSuccessNoServerSideState;
  }

  // Returns the expected membership result for the current private set
  // membership test case.
  bool GetExpectedMembershipResult() const {
    return psm_test_case_.is_positive_membership_expected();
  }

  // Expects a sample for kUMAPsmResult to be recorded once with value
  // |protocol_result|.
  // If |success_time_recorded| is true it expects one sample
  // for kUMAPsmSuccessTime. Otherwise, expects no sample to be recorded for
  // kUMAPsmSuccessTime.
  void ExpectPsmHistograms(PsmResult protocol_result,
                           bool success_time_recorded) const {
    histogram_tester_.ExpectBucketCount(
        kUMAPsmResult + GetAutoEnrollmentProtocolUmaSuffix(), protocol_result,
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(kUMAPsmSuccessTime,
                                       success_time_recorded ? 1 : 0);
  }

  // Expects a sample |dm_status| for kUMAPsmDmServerRequestStatus with count
  // |dm_status_count|.
  void ExpectPsmRequestStatusHistogram(DeviceManagementStatus dm_status,
                                       int dm_status_count) const {
    histogram_tester_.ExpectBucketCount(
        kUMAPsmDmServerRequestStatus + GetAutoEnrollmentProtocolUmaSuffix(),
        dm_status, dm_status_count);
  }

  // Expects one sample for |kUMAPsmNetworkErrorCode| which has value of
  // |network_error|.
  void ExpectPsmNetworkErrorHistogram(int network_error) const {
    histogram_tester_.ExpectBucketCount(
        kUMAPsmNetworkErrorCode + GetAutoEnrollmentProtocolUmaSuffix(),
        network_error, /*expected_count=*/1);
  }

  void VerifyPsmLastRequestJobType() const {
    EXPECT_EQ(DeviceManagementService::JobConfiguration::
                  TYPE_PSM_HAS_DEVICE_STATE_REQUEST,
              psm_last_job_type_);
  }

  void VerifyPsmRlweOprfRequest() const {
    EXPECT_EQ(psm_test_case_.expected_oprf_request().SerializeAsString(),
              psm_request().rlwe_request().oprf_request().SerializeAsString());
  }

  void VerifyPsmRlweQueryRequest() const {
    EXPECT_EQ(psm_test_case_.expected_query_request().SerializeAsString(),
              psm_request().rlwe_request().query_request().SerializeAsString());
  }

  // Disallow copy constructor and assignment operator.
  PsmHelperTest(const PsmHelperTest&) = delete;
  PsmHelperTest& operator=(const PsmHelperTest&) = delete;

 private:
  em::DeviceManagementResponse GetPsmOprfResponse() const {
    em::DeviceManagementResponse response;
    em::PrivateSetMembershipResponse* psm_response =
        response.mutable_private_set_membership_response();

    *psm_response->mutable_rlwe_response()->mutable_oprf_response() =
        psm_test_case_.oprf_response();
    return response;
  }

  em::DeviceManagementResponse GetPsmQueryResponse() const {
    em::DeviceManagementResponse response;
    em::PrivateSetMembershipResponse* psm_response =
        response.mutable_private_set_membership_response();

    *psm_response->mutable_rlwe_response()->mutable_query_response() =
        psm_test_case_.query_response();
    return response;
  }

  psm_rlwe::PrivateMembershipRlweClientRegressionTestData::TestCase
      psm_test_case_;
  DeviceManagementService::JobConfiguration::JobType psm_last_job_type_ =
      DeviceManagementService::JobConfiguration::TYPE_INVALID;
  em::DeviceManagementRequest psm_last_request_;
};

TEST_P(PsmHelperTest, MembershipRetrievedSuccessfully) {
  InSequence sequence;

  const bool kExpectedMembershipResult = GetExpectedMembershipResult();
  const base::TimeDelta kOneSecondTimeDelta = base::Seconds(1);
  const base::Time kExpectedPsmDeterminationTimestamp =
      base::Time::NowFromSystemTime() + kOneSecondTimeDelta;

  // Advance the time forward one second.
  task_environment_.FastForwardBy(kOneSecondTimeDelta);

  ServerWillReplyWithPsmOprfResponse();
  ServerWillReplyWithPsmQueryResponse();

  // Fail for DeviceInitialEnrollmentStateRequest if the device has a
  // server-backed state.
  if (kExpectedMembershipResult)
    ServerWillFail(net::OK, DeviceManagementService::kServiceUnavailable);

  client()->Start();

  // TODO(crbug.com/1143634) Remove all usages of RunUntilIdle for all PSM
  // tests, after removing support of Hash dance from client side.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GetStateDiscoveryResult(),
            kExpectedMembershipResult
                ? StateDiscoveryResult::kSuccessHasServerSideState
                : StateDiscoveryResult::kSuccessNoServerSideState);
  EXPECT_EQ(
      GetPsmExecutionResult(),
      kExpectedMembershipResult
          ? em::DeviceRegisterRequest::PSM_RESULT_SUCCESSFUL_WITH_STATE
          : em::DeviceRegisterRequest::PSM_RESULT_SUCCESSFUL_WITHOUT_STATE);
  EXPECT_EQ(kExpectedPsmDeterminationTimestamp, GetPsmDeterminationTimestamp());
  ExpectPsmHistograms(PsmResult::kSuccessfulDetermination,
                      /*success_time_recorded=*/true);
  ExpectPsmRequestStatusHistogram(DM_STATUS_SUCCESS,
                                  /*dm_status_count=*/2);
  VerifyPsmRlweQueryRequest();
  VerifyPsmLastRequestJobType();

  // Verify initial enrollment state retrieval.
  if (kExpectedMembershipResult) {
    EXPECT_EQ(failed_job_type_, GetExpectedStateRetrievalJobType());
    EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_SERVER_ERROR);
  } else {
    EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
  }
}

TEST_P(PsmHelperTest, EmptyRlweQueryResponse) {
  InSequence sequence;
  ServerWillReplyWithPsmOprfResponse();
  ServerWillReplyWithEmptyPsmResponse();

  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetStateDiscoveryResult(), StateDiscoveryResult::kFailure);
  EXPECT_EQ(GetPsmExecutionResult(),
            em::DeviceRegisterRequest::PSM_RESULT_ERROR);
  EXPECT_TRUE(GetPsmDeterminationTimestamp().is_null());
  ExpectPsmHistograms(PsmResult::kEmptyQueryResponseError,
                      /*success_time_recorded=*/false);
  ExpectPsmRequestStatusHistogram(DM_STATUS_SUCCESS,
                                  /*dm_status_count=*/2);
  VerifyPsmRlweQueryRequest();
  VerifyPsmLastRequestJobType();

  // Verify initial enrollment state retrieval.
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
}

TEST_P(PsmHelperTest, EmptyRlweOprfResponse) {
  InSequence sequence;
  ServerWillReplyWithEmptyPsmResponse();

  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetStateDiscoveryResult(), StateDiscoveryResult::kFailure);
  EXPECT_EQ(GetPsmExecutionResult(),
            em::DeviceRegisterRequest::PSM_RESULT_ERROR);
  EXPECT_TRUE(GetPsmDeterminationTimestamp().is_null());
  ExpectPsmHistograms(PsmResult::kEmptyOprfResponseError,
                      /*success_time_recorded=*/false);
  ExpectPsmRequestStatusHistogram(DM_STATUS_SUCCESS,
                                  /*dm_status_count=*/1);
  VerifyPsmRlweOprfRequest();
  VerifyPsmLastRequestJobType();

  // Verify initial enrollment state retrieval.
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
}

TEST_P(PsmHelperTest, ConnectionErrorForRlweQueryResponse) {
  InSequence sequence;
  ServerWillReplyWithPsmOprfResponse();
  ServerWillFailForPsm(net::ERR_FAILED, DeviceManagementService::kSuccess);

  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetStateDiscoveryResult(), StateDiscoveryResult::kFailure);
  EXPECT_EQ(GetPsmExecutionResult(),
            em::DeviceRegisterRequest::PSM_RESULT_ERROR);
  EXPECT_TRUE(GetPsmDeterminationTimestamp().is_null());
  ExpectPsmHistograms(PsmResult::kConnectionError,
                      /*success_time_recorded=*/false);
  ExpectPsmRequestStatusHistogram(DM_STATUS_SUCCESS,
                                  /*dm_status_count=*/1);
  ExpectPsmRequestStatusHistogram(DM_STATUS_REQUEST_FAILED,
                                  /*dm_status_count=*/1);
  ExpectPsmNetworkErrorHistogram(-net::ERR_FAILED);
  VerifyPsmRlweQueryRequest();
  VerifyPsmLastRequestJobType();

  // Verify initial enrollment state retrieval.
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_CONNECTION_ERROR);
}

TEST_P(PsmHelperTest, ConnectionErrorForRlweOprfResponse) {
  InSequence sequence;
  ServerWillFailForPsm(net::ERR_FAILED, DeviceManagementService::kSuccess);

  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetStateDiscoveryResult(), StateDiscoveryResult::kFailure);
  EXPECT_EQ(GetPsmExecutionResult(),
            em::DeviceRegisterRequest::PSM_RESULT_ERROR);
  EXPECT_TRUE(GetPsmDeterminationTimestamp().is_null());
  ExpectPsmHistograms(PsmResult::kConnectionError,
                      /*success_time_recorded=*/false);
  ExpectPsmRequestStatusHistogram(DM_STATUS_REQUEST_FAILED,
                                  /*dm_status_count=*/1);
  ExpectPsmNetworkErrorHistogram(-net::ERR_FAILED);
  VerifyPsmRlweOprfRequest();
  VerifyPsmLastRequestJobType();

  // Verify initial enrollment state retrieval.
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_CONNECTION_ERROR);
}

TEST_P(PsmHelperTest, NetworkFailureForRlweOprfResponse) {
  InSequence sequence;
  ServerWillFailForPsm(net::OK, net::ERR_CONNECTION_CLOSED);

  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetStateDiscoveryResult(), StateDiscoveryResult::kFailure);
  EXPECT_EQ(GetPsmExecutionResult(),
            em::DeviceRegisterRequest::PSM_RESULT_ERROR);
  EXPECT_TRUE(GetPsmDeterminationTimestamp().is_null());
  ExpectPsmHistograms(PsmResult::kServerError,
                      /*success_time_recorded=*/false);
  ExpectPsmRequestStatusHistogram(DM_STATUS_HTTP_STATUS_ERROR,
                                  /*dm_status_count=*/1);
  VerifyPsmLastRequestJobType();

  // Verify initial enrollment state retrieval.
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_SERVER_ERROR);
}

TEST_P(PsmHelperTest, NetworkFailureForRlweQueryResponse) {
  InSequence sequence;
  ServerWillReplyWithPsmOprfResponse();
  ServerWillFailForPsm(net::OK, net::ERR_CONNECTION_CLOSED);

  client()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetStateDiscoveryResult(), StateDiscoveryResult::kFailure);
  EXPECT_EQ(GetPsmExecutionResult(),
            em::DeviceRegisterRequest::PSM_RESULT_ERROR);
  EXPECT_TRUE(GetPsmDeterminationTimestamp().is_null());
  ExpectPsmHistograms(PsmResult::kServerError,
                      /*success_time_recorded=*/false);
  ExpectPsmRequestStatusHistogram(DM_STATUS_SUCCESS,
                                  /*dm_status_count=*/1);
  ExpectPsmRequestStatusHistogram(DM_STATUS_HTTP_STATUS_ERROR,
                                  /*dm_status_count=*/1);
  VerifyPsmRlweQueryRequest();
  VerifyPsmLastRequestJobType();

  // Verify initial enrollment state retrieval.
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_SERVER_ERROR);
}

TEST_P(PsmHelperTest, RetryLogicAfterMembershipSuccessfullyRetrieved) {
  InSequence sequence;

  const bool kExpectedMembershipResult = GetExpectedMembershipResult();
  const base::TimeDelta kOneSecondTimeDelta = base::Seconds(1);
  const base::Time kExpectedPsmDeterminationTimestamp =
      base::Time::NowFromSystemTime() + kOneSecondTimeDelta;

  // Advance the time forward one second.
  task_environment_.FastForwardBy(kOneSecondTimeDelta);

  ServerWillReplyWithPsmOprfResponse();
  ServerWillReplyWithPsmQueryResponse();

  // Fail for DeviceInitialEnrollmentStateRequest if the device has a
  // server-backed state.
  if (kExpectedMembershipResult)
    ServerWillFail(net::OK, DeviceManagementService::kServiceUnavailable);

  client()->Start();
  base::RunLoop().RunUntilIdle();

  const StateDiscoveryResult expected_state_result =
      kExpectedMembershipResult
          ? StateDiscoveryResult::kSuccessHasServerSideState
          : StateDiscoveryResult::kSuccessNoServerSideState;
  EXPECT_EQ(GetStateDiscoveryResult(), expected_state_result);

  EXPECT_EQ(
      GetPsmExecutionResult(),
      kExpectedMembershipResult
          ? em::DeviceRegisterRequest::PSM_RESULT_SUCCESSFUL_WITH_STATE
          : em::DeviceRegisterRequest::PSM_RESULT_SUCCESSFUL_WITHOUT_STATE);
  EXPECT_EQ(kExpectedPsmDeterminationTimestamp, GetPsmDeterminationTimestamp());

  // Verify that none of the PSM requests have been sent again. And its cached
  // membership result hasn't changed.

  // Fail for DeviceInitialEnrollmentStateRequest with connection error, if the
  // device has a server-backed state.
  if (kExpectedMembershipResult)
    ServerWillFail(net::ERR_FAILED, DeviceManagementService::kSuccess);

  client()->Retry();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GetStateDiscoveryResult(), expected_state_result);
  ExpectPsmHistograms(PsmResult::kSuccessfulDetermination,
                      /*success_time_recorded=*/true);
  ExpectPsmRequestStatusHistogram(DM_STATUS_SUCCESS,
                                  /*dm_status_count=*/2);
  VerifyPsmRlweQueryRequest();
  VerifyPsmLastRequestJobType();

  // Verify initial enrollment state retrieval.
  if (kExpectedMembershipResult) {
    EXPECT_EQ(failed_job_type_, GetExpectedStateRetrievalJobType());
    EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_CONNECTION_ERROR);
  } else {
    EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
  }
}

TEST_P(PsmHelperTest, RetryLogicAfterNetworkFailureForRlweQueryResponse) {
  InSequence sequence;
  ServerWillReplyWithPsmOprfResponse();
  ServerWillFailForPsm(net::OK, net::ERR_CONNECTION_CLOSED);

  client()->Start();
  base::RunLoop().RunUntilIdle();

  const StateDiscoveryResult kExpectedStateResult =
      StateDiscoveryResult::kFailure;
  const PsmExecutionResult kExpectedPsmExecutionResult =
      em::DeviceRegisterRequest::PSM_RESULT_ERROR;
  EXPECT_EQ(GetStateDiscoveryResult(), kExpectedStateResult);
  EXPECT_EQ(GetPsmExecutionResult(), kExpectedPsmExecutionResult);
  EXPECT_TRUE(GetPsmDeterminationTimestamp().is_null());

  // Verify that none of the PSM requests have been sent again. And its cached
  // membership result hasn't changed.

  client()->Retry();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GetStateDiscoveryResult(), kExpectedStateResult);
  EXPECT_EQ(GetPsmExecutionResult(), kExpectedPsmExecutionResult);
  EXPECT_TRUE(GetPsmDeterminationTimestamp().is_null());
  ExpectPsmHistograms(PsmResult::kServerError,
                      /*success_time_recorded=*/false);
  ExpectPsmRequestStatusHistogram(DM_STATUS_SUCCESS,
                                  /*dm_status_count=*/1);
  ExpectPsmRequestStatusHistogram(DM_STATUS_HTTP_STATUS_ERROR,
                                  /*dm_status_count=*/1);
  VerifyPsmRlweQueryRequest();
  VerifyPsmLastRequestJobType();

  // Verify initial enrollment state retrieval.
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_SERVER_ERROR);
}

TEST_P(PsmHelperTest, CancelAndDeleteSoonWithPendingRequest) {
  DeviceManagementService::JobForTesting psm_rlwe_oprf_job;

  // Expect one request to be captured when available in |psm_rlwe_oprf_job|.
  ServerWillReplyAsyncForPsm(&psm_rlwe_oprf_job);

  // Verify that the PSM RLWE OPRF request has not been captured yet.
  EXPECT_FALSE(psm_rlwe_oprf_job.IsActive());

  client()->Start();
  base::RunLoop().RunUntilIdle();

  // Verify the PSM RLWE OPRF request has been captured.
  ASSERT_TRUE(psm_rlwe_oprf_job.IsActive());
  VerifyPsmRlweOprfRequest();
  VerifyPsmLastRequestJobType();
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_PENDING);

  // Cancel any running jobs and delete the client by `CancelAndDeleteSoon()`
  // while PSM RLWE OPRF request is in flight.
  EXPECT_TRUE(base::CurrentThread::Get()->IsIdleForTesting());
  release_client()->CancelAndDeleteSoon();

  // Verify the client has been deleted immediately and inexistence of any
  // pending jobs.
  EXPECT_TRUE(base::CurrentThread::Get()->IsIdleForTesting());
  EXPECT_FALSE(psm_rlwe_oprf_job.IsActive());
  EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_PENDING);
}

// PSM is enabled to test initial enrollment case extensively only.
// Note that: PSM is running only for initial enrollment, and Hash dance for FRE
// use case.
INSTANTIATE_TEST_SUITE_P(
    Psm,
    PsmHelperTest,
    testing::Combine(testing::Values(AutoEnrollmentClientImplTestState(
                         AutoEnrollmentProtocol::kInitialEnrollment,
                         PsmState::kEnabled)),
                     ::testing::Range(0, kNumberOfPsmTestCases)));

using PsmHelperInitialEnrollmentTest = PsmHelperTest;

TEST_P(PsmHelperInitialEnrollmentTest, PsmSucceedAndStateRetrievalSucceed) {
  InSequence sequence;

  const bool kExpectedMembershipResult = GetExpectedMembershipResult();
  const base::TimeDelta kOneSecondTimeDelta = base::Seconds(1);
  const base::Time kExpectedPsmDeterminationTimestamp =
      base::Time::NowFromSystemTime() + kOneSecondTimeDelta;

  // Advance the time forward one second.
  task_environment_.FastForwardBy(kOneSecondTimeDelta);

  // Succeed for both PSM RLWE requests.
  ServerWillReplyWithPsmOprfResponse();
  ServerWillReplyWithPsmQueryResponse();

  // Succeed for DeviceInitialEnrollmentStateRequest if the device has a
  // server-backed state.
  if (kExpectedMembershipResult) {
    ServerWillSendState(
        "example.com",
        em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_ENFORCED,
        kDisabledMessage, kWithLicense,
        em::DeviceInitialEnrollmentStateResponse::CHROME_ENTERPRISE);
  }

  client()->Start();
  base::RunLoop().RunUntilIdle();

  // Verify PSM result.
  EXPECT_EQ(GetStateDiscoveryResult(),
            GetExpectedMembershipResult()
                ? StateDiscoveryResult::kSuccessHasServerSideState
                : StateDiscoveryResult::kSuccessNoServerSideState);
  EXPECT_EQ(
      GetPsmExecutionResult(),
      GetExpectedMembershipResult()
          ? em::DeviceRegisterRequest::PSM_RESULT_SUCCESSFUL_WITH_STATE
          : em::DeviceRegisterRequest::PSM_RESULT_SUCCESSFUL_WITHOUT_STATE);
  EXPECT_EQ(kExpectedPsmDeterminationTimestamp, GetPsmDeterminationTimestamp());
  ExpectPsmHistograms(PsmResult::kSuccessfulDetermination,
                      /*success_time_recorded=*/true);
  ExpectPsmRequestStatusHistogram(DM_STATUS_SUCCESS,
                                  /*dm_status_count=*/2);
  VerifyPsmRlweQueryRequest();
  VerifyPsmLastRequestJobType();

  // Verify initial enrollment state retrieval.
  if (kExpectedMembershipResult) {
    EXPECT_EQ(state_retrieval_job_type_, GetExpectedStateRetrievalJobType());
    EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT);
    VerifyServerBackedState(
        "example.com", kDeviceStateRestoreModeReEnrollmentEnforced,
        kDisabledMessage, kWithLicense, kDeviceStateLicenseTypeEnterprise);
  } else {
    EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
  }
}

TEST_P(PsmHelperInitialEnrollmentTest, PsmSucceedAndStateRetrievalFailed) {
  InSequence sequence;

  const bool kExpectedMembershipResult = GetExpectedMembershipResult();
  const base::TimeDelta kOneSecondTimeDelta = base::Seconds(1);
  const base::Time kExpectedPsmDeterminationTimestamp =
      base::Time::NowFromSystemTime() + kOneSecondTimeDelta;

  // Advance the time forward one second.
  task_environment_.FastForwardBy(kOneSecondTimeDelta);

  // Succeed for both PSM RLWE requests.
  ServerWillReplyWithPsmOprfResponse();
  ServerWillReplyWithPsmQueryResponse();

  // Fail for DeviceInitialEnrollmentStateRequest if the device has a
  // server-backed state.
  if (kExpectedMembershipResult)
    ServerWillFail(net::OK, DeviceManagementService::kServiceUnavailable);

  client()->Start();
  base::RunLoop().RunUntilIdle();

  // Verify PSM result.
  EXPECT_EQ(GetStateDiscoveryResult(),
            GetExpectedMembershipResult()
                ? StateDiscoveryResult::kSuccessHasServerSideState
                : StateDiscoveryResult::kSuccessNoServerSideState);
  EXPECT_EQ(
      GetPsmExecutionResult(),
      GetExpectedMembershipResult()
          ? em::DeviceRegisterRequest::PSM_RESULT_SUCCESSFUL_WITH_STATE
          : em::DeviceRegisterRequest::PSM_RESULT_SUCCESSFUL_WITHOUT_STATE);
  EXPECT_EQ(kExpectedPsmDeterminationTimestamp, GetPsmDeterminationTimestamp());
  ExpectPsmHistograms(PsmResult::kSuccessfulDetermination,
                      /*success_time_recorded=*/true);
  ExpectPsmRequestStatusHistogram(DM_STATUS_SUCCESS,
                                  /*dm_status_count=*/2);
  VerifyPsmRlweQueryRequest();
  VerifyPsmLastRequestJobType();

  // Verify initial enrollment state retrieval.
  if (kExpectedMembershipResult) {
    EXPECT_EQ(failed_job_type_, GetExpectedStateRetrievalJobType());
    EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_SERVER_ERROR);
  } else {
    EXPECT_EQ(state_, AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
  }
}

// PSM is enabled to test initial enrollment case extensively only.
// Note that: PSM is running only for initial enrollment, and Hash dance for FRE
// use case.
INSTANTIATE_TEST_SUITE_P(
    PsmForInitialEnrollment,
    PsmHelperInitialEnrollmentTest,
    testing::Combine(testing::Values(AutoEnrollmentClientImplTestState(
                         AutoEnrollmentProtocol::kInitialEnrollment,
                         PsmState::kEnabled)),
                     ::testing::Range(0, kNumberOfPsmTestCases)));

}  // namespace
}  // namespace policy
