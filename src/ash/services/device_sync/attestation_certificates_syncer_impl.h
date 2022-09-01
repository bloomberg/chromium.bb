// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_DEVICE_SYNC_ATTESTATION_CERTIFICATES_SYNCER_IMPL_H_
#define ASH_SERVICES_DEVICE_SYNC_ATTESTATION_CERTIFICATES_SYNCER_IMPL_H_

#include "ash/services/device_sync/attestation_certificates_syncer.h"
#include "base/callback.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

class PrefRegistrySimple;
class PrefService;

namespace ash {
namespace device_sync {

class CryptAuthScheduler;

// Uploads the attestation certs to cryptauth.
//
// CryptAuthDeviceSyncer delegates to this class to obtain the attestation
// certificates (UpdateCerts) and then uploads them.
//
// Because the certs expire after 72 hours, ensure that the certificates have
// been regenerated and uploaded within the past 71 hours. To do so, we schedule
// a recurring timer to ensure that either something else has already triggered
// an upload, or manually trigger one.
class AttestationCertificatesSyncerImpl : public AttestationCertificatesSyncer {
 public:
  class Factory {
   public:
    static std::unique_ptr<AttestationCertificatesSyncer> Create(
        CryptAuthScheduler* cryptauth_scheduler,
        PrefService* pref_service,
        AttestationCertificatesSyncer::GetAttestationCertificatesFunction
            get_attestation_certificates_function);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<AttestationCertificatesSyncer> CreateInstance(
        CryptAuthScheduler* cryptauth_scheduler,
        PrefService* pref_service,
        AttestationCertificatesSyncer::GetAttestationCertificatesFunction
            get_attestation_certificates_function) = 0;

   private:
    static Factory* test_factory_;
  };

  static void RegisterPrefs(PrefRegistrySimple* registry);

  ~AttestationCertificatesSyncerImpl() override;

 private:
  AttestationCertificatesSyncerImpl(
      CryptAuthScheduler* cryptauth_scheduler,
      PrefService* pref_service,
      AttestationCertificatesSyncer::GetAttestationCertificatesFunction
          get_attestation_certificates_function);

  // AttestationCertificatesSyncer:
  bool IsUpdateRequired() override;
  void SetLastSyncTimestamp() override;
  void UpdateCerts(NotifyCallback callback,
                   const std::string& user_key) override;
  void ScheduleSyncForTest() override;

  void ScheduleSync();
  void StartTimer(base::TimeDelta timeout);
  base::TimeDelta CalculateTimeToRegeneration();

  CryptAuthScheduler* cryptauth_scheduler_ = nullptr;
  PrefService* pref_service_ = nullptr;
  AttestationCertificatesSyncer::GetAttestationCertificatesFunction
      get_attestation_certificates_function_;
  base::Time last_update_time_;
  base::OneShotTimer timer_;
};

}  // namespace device_sync
}  // namespace ash

#endif  // ASH_SERVICES_DEVICE_SYNC_ATTESTATION_CERTIFICATES_SYNCER_IMPL_H_
