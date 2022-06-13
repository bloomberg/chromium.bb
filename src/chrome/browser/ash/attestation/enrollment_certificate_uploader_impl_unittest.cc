// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>

#include "ash/components/attestation/mock_attestation_flow.h"
#include "ash/components/settings/cros_settings_names.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/ash/attestation/attestation_key_payload.pb.h"
#include "chrome/browser/ash/attestation/enrollment_certificate_uploader_impl.h"
#include "chrome/browser/ash/attestation/fake_certificate.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using CertStatus = ash::attestation::EnrollmentCertificateUploader::Status;
using CertCallback = ash::attestation::AttestationFlow::CertificateCallback;
using testing::_;
using ::testing::InSequence;
using testing::Invoke;
using testing::StrictMock;
using testing::WithArgs;

namespace ash {
namespace attestation {

namespace {

constexpr int kRetryLimit = 3;

void CertCallbackSuccess(CertCallback callback,
                         const std::string& certificate) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), ATTESTATION_SUCCESS, certificate));
}

void CertCallbackUnspecifiedFailure(CertCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), ATTESTATION_UNSPECIFIED_FAILURE, ""));
}

void CertCallbackBadRequestFailure(CertCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                ATTESTATION_SERVER_BAD_REQUEST_FAILURE, ""));
}

void StatusCallbackFailure(policy::CloudPolicyClient::StatusCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), false));
}

void StatusCallbackSuccess(policy::CloudPolicyClient::StatusCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

}  // namespace

class EnrollmentCertificateUploaderTest : public ::testing::Test {
 public:
  EnrollmentCertificateUploaderTest() : uploader_(&policy_client_) {
    settings_helper_.ReplaceDeviceSettingsProviderWithStub();
    policy_client_.SetDMToken("fake_dm_token");

    uploader_.set_retry_limit_for_testing(kRetryLimit);
    uploader_.set_retry_delay_for_testing(base::TimeDelta());
    uploader_.set_attestation_flow_for_testing(&attestation_flow_);
  }

 protected:
  void Run(CertStatus expected_status) {
    uploader_.ObtainAndUploadCertificate(
        base::BindLambdaForTesting([expected_status](CertStatus status) {
          EXPECT_EQ(status, expected_status);
        }));

    base::RunLoop().RunUntilIdle();
  }

  content::BrowserTaskEnvironment task_environment_;
  ScopedCrosSettingsTestHelper settings_helper_;
  StrictMock<MockAttestationFlow> attestation_flow_;
  StrictMock<policy::MockCloudPolicyClient> policy_client_;

  EnrollmentCertificateUploaderImpl uploader_;
};

TEST_F(EnrollmentCertificateUploaderTest, UnregisteredPolicyClient) {
  InSequence s;

  EXPECT_CALL(attestation_flow_, GetCertificate(_, _, _, _, _, _)).Times(0);
  EXPECT_CALL(policy_client_, UploadEnterpriseEnrollmentCertificate(_, _))
      .Times(0);

  policy_client_.SetDMToken("");
  Run(/*expected_status=*/CertStatus::kFailedToFetch);
}

TEST_F(EnrollmentCertificateUploaderTest, GetCertificateUnspecifiedFailure) {
  InSequence s;

  // Shall try to fetch existing certificate through all attempts.
  constexpr int total_attempts = kRetryLimit + 1;
  EXPECT_CALL(attestation_flow_,
              GetCertificate(PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE, _, _,
                             /*force_new_key=*/false, _, _))
      .Times(total_attempts)
      .WillRepeatedly(WithArgs<5>(Invoke(CertCallbackUnspecifiedFailure)));

  Run(/*expected_status=*/CertStatus::kFailedToFetch);
}

TEST_F(EnrollmentCertificateUploaderTest, GetCertificateBadRequestFailure) {
  InSequence s;

  // Shall fail without retries.
  EXPECT_CALL(attestation_flow_,
              GetCertificate(PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE, _, _,
                             /*force_new_key=*/false, _, _))
      .Times(1)
      .WillOnce(WithArgs<5>(Invoke(CertCallbackBadRequestFailure)));

  Run(/*expected_status=*/CertStatus::kFailedToFetch);
}

TEST_F(EnrollmentCertificateUploaderTest,
       GetCertificateFailureWithUnregisteredClientOnRetry) {
  InSequence s;

  // Shall fail on |CloudPolicyClient::is_registered()| check and not retry.
  EXPECT_CALL(attestation_flow_,
              GetCertificate(PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE, _, _,
                             /*force_new_key=*/false, _, _))
      .Times(1)
      .WillOnce(WithArgs<5>(Invoke([this](CertCallback callback) {
        policy_client_.SetDMToken("");
        CertCallbackUnspecifiedFailure(std::move(callback));
      })));

  EXPECT_CALL(attestation_flow_, GetCertificate(_, _, _, _, _, _)).Times(0);

  Run(/*expected_status=*/CertStatus::kFailedToFetch);
}

TEST_F(EnrollmentCertificateUploaderTest, UploadCertificateFailure) {
  std::string valid_certificate;
  ASSERT_TRUE(GetFakeCertificatePEM(base::Days(1), &valid_certificate));
  InSequence s;

  constexpr int total_attempts = kRetryLimit + 1;
  for (int i = 0; i < total_attempts; ++i) {
    // Cannot use Times(kRetryLimit) because of expected sequence.
    EXPECT_CALL(attestation_flow_,
                GetCertificate(PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE, _, _,
                               /*force_new_key=*/false, _, _))
        .Times(1)
        .WillOnce(
            WithArgs<5>(Invoke([valid_certificate](CertCallback callback) {
              CertCallbackSuccess(std::move(callback), valid_certificate);
            })));
    EXPECT_CALL(policy_client_,
                UploadEnterpriseEnrollmentCertificate(valid_certificate, _))
        .Times(1)
        .WillOnce(WithArgs<1>(Invoke(StatusCallbackFailure)));
  }

  Run(/*expected_status=*/CertStatus::kFailedToUpload);
}

TEST_F(EnrollmentCertificateUploaderTest,
       UploadCertificateFailureWithUnregisteredClientOnRetry) {
  std::string valid_certificate;
  ASSERT_TRUE(GetFakeCertificatePEM(base::Days(1), &valid_certificate));
  InSequence s;

  // Shall fail on |CloudPolicyClient::is_registered()| check and not retry.
  EXPECT_CALL(attestation_flow_,
              GetCertificate(PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE, _, _,
                             /*force_new_key=*/false, _, _))
      .Times(1)
      .WillOnce(WithArgs<5>(Invoke([valid_certificate](CertCallback callback) {
        CertCallbackSuccess(std::move(callback), valid_certificate);
      })));
  EXPECT_CALL(policy_client_,
              UploadEnterpriseEnrollmentCertificate(valid_certificate, _))
      .Times(1)
      .WillOnce(WithArgs<1>(
          Invoke([this](policy::CloudPolicyClient::StatusCallback callback) {
            policy_client_.SetDMToken("");
            StatusCallbackFailure(std::move(callback));
          })));

  EXPECT_CALL(attestation_flow_,
              GetCertificate(PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE, _, _,
                             /*force_new_key=*/false, _, _))
      .Times(0);

  Run(/*expected_status=*/CertStatus::kFailedToFetch);
}

TEST_F(EnrollmentCertificateUploaderTest, UploadValidCertificate) {
  std::string valid_certificate;
  ASSERT_TRUE(GetFakeCertificatePEM(base::Days(1), &valid_certificate));
  InSequence s;

  EXPECT_CALL(attestation_flow_,
              GetCertificate(PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE, _, _,
                             /*force_new_key=*/false, _, _))
      .Times(1)
      .WillOnce(WithArgs<5>(Invoke([valid_certificate](CertCallback callback) {
        CertCallbackSuccess(std::move(callback), valid_certificate);
      })));
  EXPECT_CALL(policy_client_,
              UploadEnterpriseEnrollmentCertificate(valid_certificate, _))
      .Times(1)
      .WillOnce(WithArgs<1>(Invoke(StatusCallbackSuccess)));

  Run(/*expected_status=*/CertStatus::kSuccess);
}

TEST_F(EnrollmentCertificateUploaderTest,
       UploadValidCertificateWhenOldCertificateExpired) {
  std::string valid_certificate;
  ASSERT_TRUE(GetFakeCertificatePEM(base::Days(1), &valid_certificate));
  std::string expired_certificate;
  ASSERT_TRUE(GetFakeCertificatePEM(base::Days(-1), &expired_certificate));
  InSequence s;

  // Shall check that existing certificate has expired and fetch and upload
  // a new one.
  EXPECT_CALL(attestation_flow_,
              GetCertificate(PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE, _, _,
                             /*force_new_key=*/false, _, _))
      .Times(1)
      .WillOnce(
          WithArgs<5>(Invoke([expired_certificate](CertCallback callback) {
            CertCallbackSuccess(std::move(callback), expired_certificate);
          })));
  EXPECT_CALL(attestation_flow_,
              GetCertificate(PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE, _, _,
                             /*force_new_key=*/true, _, _))
      .Times(1)
      .WillOnce(WithArgs<5>(Invoke([valid_certificate](CertCallback callback) {
        CertCallbackSuccess(std::move(callback), valid_certificate);
      })));
  EXPECT_CALL(policy_client_,
              UploadEnterpriseEnrollmentCertificate(valid_certificate, _))
      .Times(1)
      .WillOnce(WithArgs<1>(Invoke(StatusCallbackSuccess)));

  Run(/*expected_status=*/CertStatus::kSuccess);
}

TEST_F(EnrollmentCertificateUploaderTest,
       UploadValidCertificateWhenOldCertificateExpiredButOnce) {
  std::string valid_certificate;
  ASSERT_TRUE(GetFakeCertificatePEM(base::Days(1), &valid_certificate));
  std::string expired_certificate;
  ASSERT_TRUE(GetFakeCertificatePEM(base::Days(-1), &expired_certificate));
  InSequence s;

  // Shall check that existing certificate has expired and fetch and upload
  // a new one.
  EXPECT_CALL(attestation_flow_,
              GetCertificate(PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE, _, _,
                             /*force_new_key=*/false, _, _))
      .Times(1)
      .WillOnce(
          WithArgs<5>(Invoke([expired_certificate](CertCallback callback) {
            CertCallbackSuccess(std::move(callback), expired_certificate);
          })));
  EXPECT_CALL(attestation_flow_,
              GetCertificate(PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE, _, _,
                             /*force_new_key=*/true, _, _))
      .Times(1)
      .WillOnce(WithArgs<5>(Invoke([valid_certificate](CertCallback callback) {
        CertCallbackSuccess(std::move(callback), valid_certificate);
      })));
  EXPECT_CALL(policy_client_,
              UploadEnterpriseEnrollmentCertificate(valid_certificate, _))
      .Times(1)
      .WillOnce(WithArgs<1>(Invoke(StatusCallbackFailure)));
  // After upload failure, shall fetch existing certificate.
  for (int i = 0; i < kRetryLimit; ++i) {
    // Cannot use Times(kRetryLimit) because of expected sequence.
    EXPECT_CALL(attestation_flow_,
                GetCertificate(PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE, _, _,
                               /*force_new_key=*/false, _, _))
        .Times(1)
        .WillOnce(
            WithArgs<5>(Invoke([valid_certificate](CertCallback callback) {
              CertCallbackSuccess(std::move(callback), valid_certificate);
            })));
    EXPECT_CALL(policy_client_,
                UploadEnterpriseEnrollmentCertificate(valid_certificate, _))
        .Times(1)
        .WillOnce(WithArgs<1>(Invoke(StatusCallbackFailure)));
  }

  Run(/*expected_status=*/CertStatus::kFailedToUpload);
}

TEST_F(EnrollmentCertificateUploaderTest,
       UploadInvalidCertificateWhenCannotCheckExpiry) {
  const std::string empty_certificate;
  InSequence s;

  constexpr int total_attempts = kRetryLimit + 1;
  for (int i = 0; i < total_attempts; ++i) {
    // Cannot use Times(kRetryLimit) because of expected sequence.
    EXPECT_CALL(attestation_flow_,
                GetCertificate(PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE, _, _,
                               /*force_new_key=*/false, _, _))
        .Times(1)
        .WillOnce(
            WithArgs<5>(Invoke([empty_certificate](CertCallback callback) {
              CertCallbackSuccess(std::move(callback), empty_certificate);
            })));
    EXPECT_CALL(policy_client_,
                UploadEnterpriseEnrollmentCertificate(empty_certificate, _))
        .Times(1)
        .WillOnce(WithArgs<1>(Invoke(StatusCallbackFailure)));
  }

  Run(/*expected_status=*/CertStatus::kFailedToUpload);
}

TEST_F(EnrollmentCertificateUploaderTest, UploadValidCertificateOnlyOnce) {
  std::string valid_certificate;
  ASSERT_TRUE(GetFakeCertificatePEM(base::Days(1), &valid_certificate));
  InSequence s;

  EXPECT_CALL(attestation_flow_,
              GetCertificate(PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE, _, _,
                             /*force_new_key=*/false, _, _))
      .Times(1)
      .WillOnce(WithArgs<5>(Invoke([valid_certificate](CertCallback callback) {
        CertCallbackSuccess(std::move(callback), valid_certificate);
      })));
  EXPECT_CALL(policy_client_,
              UploadEnterpriseEnrollmentCertificate(valid_certificate, _))
      .Times(1)
      .WillOnce(WithArgs<1>(Invoke(StatusCallbackSuccess)));

  Run(/*expected_status=*/CertStatus::kSuccess);

  // Mocks expect single upload request and will fail if requested more than
  // once.
  EXPECT_CALL(attestation_flow_,
              GetCertificate(PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE, _, _,
                             /*force_new_key=*/false, _, _))
      .Times(1)
      .WillOnce(WithArgs<5>(Invoke([valid_certificate](CertCallback callback) {
        CertCallbackSuccess(std::move(callback), valid_certificate);
      })));
  Run(/*expected_status=*/CertStatus::kSuccess);
}

}  // namespace attestation
}  // namespace ash
