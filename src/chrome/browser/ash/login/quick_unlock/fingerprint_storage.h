// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_FINGERPRINT_STORAGE_H_
#define CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_FINGERPRINT_STORAGE_H_

#include "chromeos/components/feature_usage/feature_usage_metrics.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/fingerprint.mojom.h"

class PrefRegistrySimple;
class Profile;

namespace ash {
namespace quick_unlock {

// The result of fingerprint auth attempt on the lock screen. These values are
// persisted to logs. Entries should not be renumbered and numeric values
// should never be reused.
enum class FingerprintUnlockResult {
  kSuccess = 0,
  kFingerprintUnavailable = 1,
  kAuthTemporarilyDisabled = 2,
  kMatchFailed = 3,
  kMatchNotForPrimaryUser = 4,
  kMaxValue = kMatchNotForPrimaryUser,
};

// `FingerprintStorage` manages fingerprint user preferences. Keeps them in sync
// with the actual fingerprint records state. The class also reports fingerprint
// metrics.
class FingerprintStorage final
    : public feature_usage::FeatureUsageMetrics::Delegate,
      public device::mojom::FingerprintObserver {
 public:
  static const int kMaximumUnlockAttempts = 5;

  // Registers profile prefs.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  explicit FingerprintStorage(Profile* profile);

  FingerprintStorage(const FingerprintStorage&) = delete;
  FingerprintStorage& operator=(const FingerprintStorage&) = delete;

  ~FingerprintStorage() override;

  // Get actual records to update cached prefs::kQuickUnlockFingerprintRecord.
  void GetRecordsForUser();

  // feature_usage::FeatureUsageMetrics::Delegate:
  bool IsEligible() const override;
  absl::optional<bool> IsAccessible() const override;
  bool IsEnabled() const override;

  // Called after a fingerprint unlock attempt to record the result.
  // `num_attempts`:  Only valid when auth success to record number of attempts.
  void RecordFingerprintUnlockResult(FingerprintUnlockResult result);

  // Returns true if fingerprint unlock is currently available.
  // This does not check if strong auth is available.
  bool IsFingerprintAvailable() const;

  // Returns true if the user has fingerprint record registered.
  bool HasRecord() const;

  // Add a fingerprint unlock attempt count.
  void AddUnlockAttempt();

  // Reset the number of unlock attempts to 0.
  void ResetUnlockAttemptCount();

  // Returns true if the user has exceeded fingerprint unlock attempts.
  bool ExceededUnlockAttempts() const;

  int unlock_attempt_count() const { return unlock_attempt_count_; }

  // device::mojom::FingerprintObserver:
  void OnRestarted() override;
  void OnEnrollScanDone(device::mojom::ScanResult scan_result,
                        bool is_complete,
                        int32_t percent_complete) override;
  void OnAuthScanDone(
      const device::mojom::FingerprintMessagePtr msg,
      const base::flat_map<std::string, std::vector<std::string>>& matches)
      override;
  void OnSessionFailed() override;

 private:
  void OnGetRecords(const base::flat_map<std::string, std::string>&
                        fingerprints_list_mapping);

  friend class FingerprintStorageTestApi;
  friend class QuickUnlockStorage;

  Profile* const profile_;
  // Number of fingerprint unlock attempt.
  int unlock_attempt_count_ = 0;

  mojo::Remote<device::mojom::Fingerprint> fp_service_;

  mojo::Receiver<device::mojom::FingerprintObserver>
      fingerprint_observer_receiver_{this};

  std::unique_ptr<feature_usage::FeatureUsageMetrics>
      feature_usage_metrics_service_;

  base::WeakPtrFactory<FingerprintStorage> weak_factory_{this};
};

}  // namespace quick_unlock
}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
namespace quick_unlock {
using ::ash::quick_unlock::FingerprintUnlockResult;
}
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_FINGERPRINT_STORAGE_H_
