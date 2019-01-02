// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_SETUP_TEST_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_SETUP_TEST_UTILS_H_

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_helper.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_helper_mock.h"
#include "chrome/browser/chromeos/policy/enrollment_config.h"
#include "chrome/browser/chromeos/policy/enrollment_status_chromeos.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace test {

// Result of Demo Mode setup.
enum class DemoModeSetupResult { SUCCESS, ERROR };

// Helper method that mocks EnterpriseEnrollmentHelper for online Demo Mode
// setup. It simulates specified Demo Mode enrollment |result|.
template <DemoModeSetupResult result>
EnterpriseEnrollmentHelper* MockDemoModeOnlineEnrollmentHelperCreator(
    EnterpriseEnrollmentHelper::EnrollmentStatusConsumer* status_consumer,
    const policy::EnrollmentConfig& enrollment_config,
    const std::string& enrolling_user_domain) {
  EnterpriseEnrollmentHelperMock* mock =
      new EnterpriseEnrollmentHelperMock(status_consumer);

  EXPECT_EQ(enrollment_config.mode, policy::EnrollmentConfig::MODE_ATTESTATION);
  EXPECT_CALL(*mock, EnrollUsingAttestation())
      .WillRepeatedly(testing::Invoke([mock]() {
        if (result == DemoModeSetupResult::SUCCESS) {
          mock->status_consumer()->OnDeviceEnrolled("");
        } else {
          // TODO(agawronska): Test different error types.
          mock->status_consumer()->OnEnrollmentError(
              policy::EnrollmentStatus::ForStatus(
                  policy::EnrollmentStatus::REGISTRATION_FAILED));
        }
      }));
  return mock;
}

// Helper method that mocks EnterpriseEnrollmentHelper for offline Demo Mode
// setup. It simulates specified Demo Mode enrollment |result|.
template <DemoModeSetupResult result>
EnterpriseEnrollmentHelper* MockDemoModeOfflineEnrollmentHelperCreator(
    EnterpriseEnrollmentHelper::EnrollmentStatusConsumer* status_consumer,
    const policy::EnrollmentConfig& enrollment_config,
    const std::string& enrolling_user_domain) {
  EnterpriseEnrollmentHelperMock* mock =
      new EnterpriseEnrollmentHelperMock(status_consumer);

  EXPECT_EQ(enrollment_config.mode,
            policy::EnrollmentConfig::MODE_OFFLINE_DEMO);
  EXPECT_CALL(*mock, EnrollForOfflineDemo())
      .WillRepeatedly(testing::Invoke([mock]() {
        if (result == DemoModeSetupResult::SUCCESS) {
          mock->status_consumer()->OnDeviceEnrolled("");
        } else {
          // TODO(agawronska): Test different error types.
          mock->status_consumer()->OnEnrollmentError(
              policy::EnrollmentStatus::ForStatus(
                  policy::EnrollmentStatus::Status::LOCK_ERROR));
        }
      }));
  return mock;
}

// Creates fake offline policy directory to be used in tests.
bool SetupDummyOfflinePolicyDir(const std::string& account_id,
                                base::ScopedTempDir* temp_dir) {
  if (!temp_dir->CreateUniqueTempDir()) {
    LOG(ERROR) << "Failed to create unique tempdir";
    return false;
  }

  if (base::WriteFile(temp_dir->GetPath().AppendASCII("device_policy"), "",
                      0) != 0) {
    LOG(ERROR) << "Failed to create device_policy file";
    return false;
  }

  // We use MockCloudPolicyStore for the device local account policy in the
  // tests, thus actual policy content can be empty. account_id is specified
  // since it is used by DemoSetupController to look up the store.
  std::string policy_blob;
  if (!account_id.empty()) {
    enterprise_management::PolicyData policy_data;
    policy_data.set_username(account_id);
    enterprise_management::PolicyFetchResponse policy;
    policy.set_policy_data(policy_data.SerializeAsString());
    policy_blob = policy.SerializeAsString();
  }
  if (base::WriteFile(temp_dir->GetPath().AppendASCII("local_account_policy"),
                      policy_blob.data(), policy_blob.size()) !=
      static_cast<int>(policy_blob.size())) {
    LOG(ERROR) << "Failed to create local_account_policy file";
    return false;
  }
  return true;
}

}  // namespace test

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_SETUP_TEST_UTILS_H_
