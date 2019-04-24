// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_ENROLLMENT_SCHEDULER_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_ENROLLMENT_SCHEDULER_H_

#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chromeos/services/device_sync/cryptauth_enrollment_result.h"
#include "chromeos/services/device_sync/cryptauth_enrollment_scheduler.h"
#include "chromeos/services/device_sync/proto/cryptauth_common.pb.h"

namespace chromeos {

namespace device_sync {

// Fake CryptAuthEnrollmentScheduler implementation.
class FakeCryptAuthEnrollmentScheduler : public CryptAuthEnrollmentScheduler {
 public:
  static constexpr base::TimeDelta kDefaultRefreshPeriod =
      base::TimeDelta::FromDays(30);
  static constexpr base::TimeDelta kDefaultTimeToNextEnrollmentRequest =
      base::TimeDelta::FromHours(12);

  FakeCryptAuthEnrollmentScheduler(Delegate* delegate);
  ~FakeCryptAuthEnrollmentScheduler() override;

  const std::vector<CryptAuthEnrollmentResult>& handled_enrollment_results()
      const {
    return handled_enrollment_results_;
  }

  void set_client_directive_policy_reference(
      const base::Optional<cryptauthv2::PolicyReference>&
          client_directive_policy_reference) {
    client_directive_policy_reference_ = client_directive_policy_reference;
  }

  void set_last_successful_enrollment_time(
      base::Time last_successful_enrollment_time) {
    last_successful_enrollment_time_ = last_successful_enrollment_time;
  }

  void set_refresh_period(base::TimeDelta refresh_period) {
    refresh_period_ = refresh_period;
  }

  void set_time_to_next_enrollment_request(
      base::TimeDelta time_to_next_enrollment_request) {
    time_to_next_enrollment_request_ = time_to_next_enrollment_request;
  }

  void set_num_consecutive_failures(size_t num_consecutive_failures) {
    num_consecutive_failures_ = num_consecutive_failures;
  }

  // CryptAuthEnrollmentScheduler:
  void RequestEnrollmentNow() override;
  void HandleEnrollmentResult(
      const CryptAuthEnrollmentResult& enrollment_result) override;
  base::Optional<base::Time> GetLastSuccessfulEnrollmentTime() const override;
  base::TimeDelta GetRefreshPeriod() const override;
  base::TimeDelta GetTimeToNextEnrollmentRequest() const override;
  bool IsWaitingForEnrollmentResult() const override;
  size_t GetNumConsecutiveFailures() const override;

 private:
  std::vector<CryptAuthEnrollmentResult> handled_enrollment_results_;
  base::Optional<cryptauthv2::PolicyReference>
      client_directive_policy_reference_;
  base::Optional<base::Time> last_successful_enrollment_time_;
  base::TimeDelta refresh_period_ = kDefaultRefreshPeriod;
  base::TimeDelta time_to_next_enrollment_request_ =
      kDefaultTimeToNextEnrollmentRequest;
  size_t num_consecutive_failures_ = 0u;
  bool is_waiting_for_enrollment_result_ = false;

  DISALLOW_COPY_AND_ASSIGN(FakeCryptAuthEnrollmentScheduler);
};

// Fake CryptAuthEnrollmentScheduler::Delegate implementation.
class FakeCryptAuthEnrollmentSchedulerDelegate
    : public CryptAuthEnrollmentScheduler::Delegate {
 public:
  FakeCryptAuthEnrollmentSchedulerDelegate();
  ~FakeCryptAuthEnrollmentSchedulerDelegate() override;

  const std::vector<base::Optional<cryptauthv2::PolicyReference>>&
  policy_references_from_enrollment_requests() const {
    return policy_references_from_enrollment_requests_;
  }

 private:
  // CryptAuthEnrollmentScheduler::Delegate:
  void OnEnrollmentRequested(const base::Optional<cryptauthv2::PolicyReference>&
                                 client_directive_policy_reference) override;

  std::vector<base::Optional<cryptauthv2::PolicyReference>>
      policy_references_from_enrollment_requests_;

  DISALLOW_COPY_AND_ASSIGN(FakeCryptAuthEnrollmentSchedulerDelegate);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_ENROLLMENT_SCHEDULER_H_
