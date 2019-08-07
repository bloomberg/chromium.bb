// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_ENROLLMENT_SCHEDULER_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_ENROLLMENT_SCHEDULER_H_

#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chromeos/services/device_sync/cryptauth_enrollment_result.h"
#include "chromeos/services/device_sync/proto/cryptauth_common.pb.h"

namespace chromeos {

namespace device_sync {

// Schedules periodic enrollments, alerting the delegate when an enrollment
// attempt is due via Delegate::OnEnrollmentRequested(). The periodic scheduling
// begins on construction. The client can bypass the periodic schedule and
// immediately trigger an enrollment request via RequestEnrollmentNow(). When an
// enrollment attempt has completed, successfully or not, the client should
// invoke HandleEnrollmentResult() so the scheduler can process the enrollment
// attempt outcome.
class CryptAuthEnrollmentScheduler {
 public:
  class Delegate {
   public:
    Delegate() = default;
    virtual ~Delegate() = default;

    // Called to alert the delegate that an enrollment attempt has been
    // requested by the scheduler.
    // |client_directive_policy_reference|: Identifies the CryptAuth policy
    // associated with the ClientDirective parameters used to schedule this
    // enrollment attempt. If no ClientDirective was used by the scheduler,
    // base::nullopt is passed.
    virtual void OnEnrollmentRequested(
        const base::Optional<cryptauthv2::PolicyReference>&
            client_directive_policy_reference) = 0;
  };

  virtual ~CryptAuthEnrollmentScheduler();

  // Cancels the currently scheduled enrollment, and requests an enrollment
  // immediately.
  virtual void RequestEnrollmentNow() = 0;

  // Processes the result of the previous enrollment attempt.
  virtual void HandleEnrollmentResult(
      const CryptAuthEnrollmentResult& enrollment_result) = 0;

  // Returns the time of the last known successful enrollment. If no successful
  // enrollment has occurred, base::nullopt is returned.
  virtual base::Optional<base::Time> GetLastSuccessfulEnrollmentTime()
      const = 0;

  // Returns the scheduler's time period between a successful enrollment and
  // its next enrollment request. Note that this period may not be strictly
  // adhered to due to RequestEnrollmentNow() calls or the system being offline,
  // for instance.
  virtual base::TimeDelta GetRefreshPeriod() const = 0;

  // Returns the time until the next scheduled enrollment request.
  virtual base::TimeDelta GetTimeToNextEnrollmentRequest() const = 0;

  // Return true if an enrollment has been requested but the enrollment result
  // has not been processed yet.
  virtual bool IsWaitingForEnrollmentResult() const = 0;

  // The number of consecutive failed enrollment attempts. Once an enrollment
  // attempt succeeds, this counter is reset.
  virtual size_t GetNumConsecutiveFailures() const = 0;

 protected:
  CryptAuthEnrollmentScheduler(Delegate* delegate);

  // Alerts the delegate that an enrollment has been requested.
  void NotifyEnrollmentRequested(
      const base::Optional<cryptauthv2::PolicyReference>&
          client_directive_policy_reference) const;

 private:
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(CryptAuthEnrollmentScheduler);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_ENROLLMENT_SCHEDULER_H_
