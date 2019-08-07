// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_PERSISTENT_ENROLLMENT_SCHEDULER_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_PERSISTENT_ENROLLMENT_SCHEDULER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "base/timer/timer.h"
#include "chromeos/services/device_sync/cryptauth_enrollment_scheduler.h"
#include "chromeos/services/device_sync/proto/cryptauth_directive.pb.h"

class PrefRegistrySimple;
class PrefService;

namespace chromeos {

namespace device_sync {

// CryptAuthEnrollmentScheduler implementation which stores scheduling metadata
// persistently so that the enrollment schedule is saved across device reboots.
// If this class is instantiated before at least one enrollment has been
// completed successfully, it requests enrollment immediately. Once enrollment
// has been completed successfully, this class schedules the next enrollment
// attempt at a time provided by the server via a ClientDirective proto.
class PersistentEnrollmentScheduler : public CryptAuthEnrollmentScheduler {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<CryptAuthEnrollmentScheduler> BuildInstance(
        Delegate* delegate,
        PrefService* pref_service,
        base::Clock* clock = base::DefaultClock::GetInstance(),
        std::unique_ptr<base::OneShotTimer> timer =
            std::make_unique<base::OneShotTimer>(),
        scoped_refptr<base::TaskRunner> task_runner =
            base::ThreadTaskRunnerHandle::Get());

   private:
    static Factory* test_factory_;
  };

  // Registers the prefs used by this class to the given |registry|.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  ~PersistentEnrollmentScheduler() override;

 private:
  PersistentEnrollmentScheduler(Delegate* delegate,
                                PrefService* pref_service,
                                base::Clock* clock,
                                std::unique_ptr<base::OneShotTimer> timer,
                                scoped_refptr<base::TaskRunner> task_runner);

  // CryptAuthEnrollmentScheduler:
  void RequestEnrollmentNow() override;
  void HandleEnrollmentResult(
      const CryptAuthEnrollmentResult& enrollment_result) override;
  base::Optional<base::Time> GetLastSuccessfulEnrollmentTime() const override;
  base::TimeDelta GetRefreshPeriod() const override;
  base::TimeDelta GetTimeToNextEnrollmentRequest() const override;
  bool IsWaitingForEnrollmentResult() const override;
  size_t GetNumConsecutiveFailures() const override;

  // Calculates the time period between the previous enrollment attempt and the
  // next enrollment attempt, taking failures into consideration.
  base::TimeDelta CalculateTimeBetweenEnrollmentRequests() const;

  // Starts a new timer that will fire when an enrollment is ready to be
  // attempted.
  void ScheduleNextEnrollment();

  // Get the ClientDirective's PolicyReference. If one has not been set, returns
  // base::nullopt.
  base::Optional<cryptauthv2::PolicyReference> GetPolicyReference() const;

  PrefService* pref_service_;
  base::Clock* clock_;
  std::unique_ptr<base::OneShotTimer> timer_;
  cryptauthv2::ClientDirective client_directive_;
  base::WeakPtrFactory<PersistentEnrollmentScheduler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PersistentEnrollmentScheduler);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_PERSISTENT_ENROLLMENT_SCHEDULER_H_
