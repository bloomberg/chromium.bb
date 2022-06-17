// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/private_membership/psm_rlwe_dmserver_client_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/enrollment/private_membership/private_membership_rlwe_client.h"
#include "chrome/browser/ash/policy/enrollment/private_membership/psm_rlwe_id_provider.h"
#include "chrome/browser/ash/policy/enrollment/private_membership/testing_private_membership_rlwe_client.h"
#include "chrome/browser/ash/policy/enrollment/private_membership/testing_psm_rlwe_id_provider.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/dmserver_job_configurations.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/private_membership/src/internal/testing/regression_test_data/regression_test_data.pb.h"
#include "third_party/private_membership/src/private_membership_rlwe.pb.h"
#include "third_party/shell-encryption/src/testing/status_testing.h"

namespace psm_rlwe = private_membership::rlwe;
namespace em = enterprise_management;

// An enum for PSM execution result values.
using PsmExecutionResult = em::DeviceRegisterRequest::PsmExecutionResult;

namespace policy {

namespace {
// A struct reporesents the PSM execution result params.
using PsmResultHolder = PsmRlweDmserverClient::ResultHolder;

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::InvokeWithoutArgs;
using ::testing::Mock;
using ::testing::SaveArg;

// Number of test cases exist in cros_test_data.binarypb file, which is part of
// private_membership third_party library.
const int kNumberOfPsmTestCases = 10;

// PrivateSetMembership regression tests maximum file size which is 4MB.
const size_t kMaxFileSizeInBytes = 4 * 1024 * 1024;

bool ParseProtoFromFile(const base::FilePath& file_path,
                        google::protobuf::MessageLite* out_proto) {
  DCHECK(out_proto);

  if (!base::PathExists(file_path))
    return false;

  std::string file_content;
  if (!base::ReadFileToStringWithMaxSize(file_path, &file_content,
                                         kMaxFileSizeInBytes)) {
    return false;
  }

  return out_proto->ParseFromString(file_content);
}

}  // namespace

// The integer parameter represents the index of PSM test case.
class PsmRlweDmserverClientImplTest : public testing::TestWithParam<int> {
 protected:
  PsmRlweDmserverClientImplTest() {
    // Create PSM test case, before PSM client to construct the
    // PSM RLWE testing client factory and its RLWE ID.
    CreatePsmTestCase();

    // Create PSM RLWE DMServer client.
    CreateClient();
  }

  ~PsmRlweDmserverClientImplTest() = default;

  int GetPsmTestCaseIndex() const { return GetParam(); }

  void CreateClient() {
    service_ =
        std::make_unique<FakeDeviceManagementService>(&job_creation_handler_);
    service_->ScheduleInitialization(0);

    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &url_loader_factory_);

    psm_client_ = std::make_unique<PsmRlweDmserverClientImpl>(
        service_.get(), shared_url_loader_factory_,
        psm_rlwe_test_client_factory_.get(),
        testing_psm_rlwe_id_provider_.get());
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
            .AppendASCII("test_data.binarypb");
    psm_rlwe::PrivateMembershipRlweClientRegressionTestData test_data;
    ASSERT_TRUE(ParseProtoFromFile(kPsmTestDataPath, &test_data));
    EXPECT_EQ(test_data.test_cases_size(), kNumberOfPsmTestCases);
    psm_test_case_ = test_data.test_cases(GetPsmTestCaseIndex());

    std::vector<private_membership::rlwe::RlwePlaintextId> plaintext_ids{
        psm_test_case_.plaintext_id()};

    // Sets the PSM RLWE client factory to testing client.
    psm_rlwe_test_client_factory_ =
        std::make_unique<TestingPrivateMembershipRlweClient::FactoryImpl>(
            psm_test_case_.ec_cipher_key(), psm_test_case_.seed(),
            plaintext_ids);

    // Sets the PSM RLWE ID.
    testing_psm_rlwe_id_provider_ = std::make_unique<TestingPsmRlweIdProvider>(
        psm_test_case_.plaintext_id());
  }

  // Start the `PsmRlweDmserverClient` to retrieve the device state.
  void CheckMembershipWithRlweClient() {
    psm_client_->CheckMembership(future_result_holder_.GetCallback());
  }

  // Expects the `future_result_holder_` will be retrieved and compared
  // against `expected_result_holder`.
  void VerifyResultHolder(PsmResultHolder expected_result_holder) {
    PsmResultHolder psm_params = future_result_holder_.Take();
    EXPECT_EQ(expected_result_holder.psm_result, psm_params.psm_result);
    if (expected_result_holder.membership_result.has_value()) {
      EXPECT_EQ(expected_result_holder.membership_result.value(),
                psm_params.membership_result.value());
    }
    if (expected_result_holder.membership_determination_time.has_value()) {
      EXPECT_EQ(expected_result_holder.membership_determination_time.value(),
                psm_params.membership_determination_time.value());
    }
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

  const em::PrivateSetMembershipRequest& psm_request() const {
    return psm_last_request_.private_set_membership_request();
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
        kUMAPsmResult + kUMASuffixInitialEnrollmentStr, protocol_result,
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(kUMAPsmSuccessTime,
                                       success_time_recorded ? 1 : 0);
  }

  // Expects a sample |dm_status| for kUMAPsmDmServerRequestStatus with count
  // |dm_status_count|.
  void ExpectPsmRequestStatusHistogram(DeviceManagementStatus dm_status,
                                       int dm_status_count) const {
    histogram_tester_.ExpectBucketCount(
        kUMAPsmDmServerRequestStatus + kUMASuffixInitialEnrollmentStr,
        dm_status, dm_status_count);
  }

  // Expects one sample for |kUMAPsmNetworkErrorCode| which has value of
  // |network_error|.
  void ExpectPsmNetworkErrorHistogram(int network_error) const {
    histogram_tester_.ExpectBucketCount(
        kUMAPsmNetworkErrorCode + kUMASuffixInitialEnrollmentStr, network_error,
        /*expected_count=*/1);
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
  PsmRlweDmserverClientImplTest(const PsmRlweDmserverClientImplTest&) = delete;
  PsmRlweDmserverClientImplTest& operator=(
      const PsmRlweDmserverClientImplTest&) = delete;

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

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

  std::unique_ptr<PsmRlweDmserverClient> psm_client_;
  base::test::TestFuture<PsmResultHolder> future_result_holder_;
  psm_rlwe::PrivateMembershipRlweClientRegressionTestData::TestCase
      psm_test_case_;

  // Sets which PSM RLWE client will be created, depending on the factory. It
  // is only used for PSM during creating the client for initial enrollment.
  std::unique_ptr<TestingPrivateMembershipRlweClient::FactoryImpl>
      psm_rlwe_test_client_factory_;

  // Sets the PSM RLWE ID directly for testing.
  std::unique_ptr<TestingPsmRlweIdProvider> testing_psm_rlwe_id_provider_;

  base::HistogramTester histogram_tester_;
  std::unique_ptr<FakeDeviceManagementService> service_;
  network::TestURLLoaderFactory url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  testing::StrictMock<MockJobCreationHandler> job_creation_handler_;
  base::RunLoop run_loop_;

  DeviceManagementService::JobConfiguration::JobType psm_last_job_type_ =
      DeviceManagementService::JobConfiguration::TYPE_INVALID;
  em::DeviceManagementRequest psm_last_request_;
  const std::string kUMASuffixInitialEnrollmentStr =
      kUMASuffixInitialEnrollment;
};

TEST_P(PsmRlweDmserverClientImplTest, MembershipRetrievedSuccessfully) {
  InSequence sequence;

  const bool kExpectedMembershipResult = GetExpectedMembershipResult();
  const base::TimeDelta kOneSecondTimeDelta = base::Seconds(1);
  const base::Time kExpectedPsmDeterminationTimestamp =
      base::Time::NowFromSystemTime() + kOneSecondTimeDelta;

  // Advance the time forward one second.
  task_environment_.FastForwardBy(kOneSecondTimeDelta);

  ServerWillReplyWithPsmOprfResponse();
  ServerWillReplyWithPsmQueryResponse();

  ASSERT_NO_FATAL_FAILURE(CheckMembershipWithRlweClient());

  VerifyResultHolder(PsmResultHolder(PsmResult::kSuccessfulDetermination,
                                     kExpectedMembershipResult,
                                     kExpectedPsmDeterminationTimestamp));

  ExpectPsmHistograms(PsmResult::kSuccessfulDetermination,
                      /*success_time_recorded=*/true);
  ExpectPsmRequestStatusHistogram(DM_STATUS_SUCCESS,
                                  /*dm_status_count=*/2);
  VerifyPsmRlweQueryRequest();
  VerifyPsmLastRequestJobType();
}

TEST_P(PsmRlweDmserverClientImplTest, EmptyRlweQueryResponse) {
  InSequence sequence;
  ServerWillReplyWithPsmOprfResponse();
  ServerWillReplyWithEmptyPsmResponse();

  ASSERT_NO_FATAL_FAILURE(CheckMembershipWithRlweClient());

  VerifyResultHolder(PsmResultHolder(PsmResult::kEmptyQueryResponseError));

  ExpectPsmHistograms(PsmResult::kEmptyQueryResponseError,
                      /*success_time_recorded=*/false);
  ExpectPsmRequestStatusHistogram(DM_STATUS_SUCCESS,
                                  /*dm_status_count=*/2);
  VerifyPsmRlweQueryRequest();
  VerifyPsmLastRequestJobType();
}

TEST_P(PsmRlweDmserverClientImplTest, EmptyRlweOprfResponse) {
  InSequence sequence;
  ServerWillReplyWithEmptyPsmResponse();

  ASSERT_NO_FATAL_FAILURE(CheckMembershipWithRlweClient());

  VerifyResultHolder(PsmResultHolder(PsmResult::kEmptyOprfResponseError));

  ExpectPsmHistograms(PsmResult::kEmptyOprfResponseError,
                      /*success_time_recorded=*/false);
  ExpectPsmRequestStatusHistogram(DM_STATUS_SUCCESS,
                                  /*dm_status_count=*/1);
  VerifyPsmRlweOprfRequest();
  VerifyPsmLastRequestJobType();
}

TEST_P(PsmRlweDmserverClientImplTest, ConnectionErrorForRlweQueryResponse) {
  InSequence sequence;
  ServerWillReplyWithPsmOprfResponse();
  ServerWillFailForPsm(net::ERR_FAILED, DeviceManagementService::kSuccess);

  ASSERT_NO_FATAL_FAILURE(CheckMembershipWithRlweClient());

  VerifyResultHolder(PsmResultHolder(PsmResult::kConnectionError));

  ExpectPsmHistograms(PsmResult::kConnectionError,
                      /*success_time_recorded=*/false);
  ExpectPsmRequestStatusHistogram(DM_STATUS_SUCCESS,
                                  /*dm_status_count=*/1);
  ExpectPsmRequestStatusHistogram(DM_STATUS_REQUEST_FAILED,
                                  /*dm_status_count=*/1);
  ExpectPsmNetworkErrorHistogram(-net::ERR_FAILED);
  VerifyPsmRlweQueryRequest();
  VerifyPsmLastRequestJobType();
}

TEST_P(PsmRlweDmserverClientImplTest, ConnectionErrorForRlweOprfResponse) {
  InSequence sequence;
  ServerWillFailForPsm(net::ERR_FAILED, DeviceManagementService::kSuccess);

  ASSERT_NO_FATAL_FAILURE(CheckMembershipWithRlweClient());

  VerifyResultHolder(PsmResultHolder(PsmResult::kConnectionError));

  ExpectPsmHistograms(PsmResult::kConnectionError,
                      /*success_time_recorded=*/false);
  ExpectPsmRequestStatusHistogram(DM_STATUS_REQUEST_FAILED,
                                  /*dm_status_count=*/1);
  ExpectPsmNetworkErrorHistogram(-net::ERR_FAILED);
  VerifyPsmRlweOprfRequest();
  VerifyPsmLastRequestJobType();
}

TEST_P(PsmRlweDmserverClientImplTest, NetworkFailureForRlweOprfResponse) {
  InSequence sequence;
  ServerWillFailForPsm(net::OK, net::ERR_CONNECTION_CLOSED);

  ASSERT_NO_FATAL_FAILURE(CheckMembershipWithRlweClient());

  VerifyResultHolder(PsmResultHolder(PsmResult::kServerError));

  ExpectPsmHistograms(PsmResult::kServerError,
                      /*success_time_recorded=*/false);
  ExpectPsmRequestStatusHistogram(DM_STATUS_HTTP_STATUS_ERROR,
                                  /*dm_status_count=*/1);
  VerifyPsmLastRequestJobType();
}

TEST_P(PsmRlweDmserverClientImplTest, NetworkFailureForRlweQueryResponse) {
  InSequence sequence;
  ServerWillReplyWithPsmOprfResponse();
  ServerWillFailForPsm(net::OK, net::ERR_CONNECTION_CLOSED);

  ASSERT_NO_FATAL_FAILURE(CheckMembershipWithRlweClient());

  VerifyResultHolder(PsmResultHolder(PsmResult::kServerError));

  ExpectPsmHistograms(PsmResult::kServerError,
                      /*success_time_recorded=*/false);
  ExpectPsmRequestStatusHistogram(DM_STATUS_SUCCESS,
                                  /*dm_status_count=*/1);
  ExpectPsmRequestStatusHistogram(DM_STATUS_HTTP_STATUS_ERROR,
                                  /*dm_status_count=*/1);
  VerifyPsmRlweQueryRequest();
  VerifyPsmLastRequestJobType();
}

INSTANTIATE_TEST_SUITE_P(PsmRlweDmserverClientImplTest,
                         PsmRlweDmserverClientImplTest,
                         // Loop over all indices starting from 0, and smaller
                         // than `kNumberOfPsmTestCases`.
                         ::testing::Range(0, kNumberOfPsmTestCases));

}  // namespace policy
