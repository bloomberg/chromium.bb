// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/quick_unlock/fingerprint_storage.h"

#include "ash/constants/ash_pref_names.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/components/feature_usage/feature_usage_metrics.h"
#include "chromeos/dbus/biod/biod_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/device_service.h"
#include "services/device/public/mojom/fingerprint.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace quick_unlock {
namespace {

constexpr char kFingerprintUMAFeatureName[] = "Fingerprint";

}

// static
void FingerprintStorage::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kQuickUnlockFingerprintRecord, 0);
}

FingerprintStorage::FingerprintStorage(Profile* profile) : profile_(profile) {
  if (!chromeos::BiodClient::Get()) {
    // Could be nullptr in tests.
    return;
  }

  content::GetDeviceService().BindFingerprint(
      fp_service_.BindNewPipeAndPassReceiver());

  fp_service_->AddFingerprintObserver(
      fingerprint_observer_receiver_.BindNewPipeAndPassRemote());

  GetRecordsForUser();

  feature_usage_metrics_service_ =
      std::make_unique<feature_usage::FeatureUsageMetrics>(
          kFingerprintUMAFeatureName, this);
}

FingerprintStorage::~FingerprintStorage() = default;

void FingerprintStorage::GetRecordsForUser() {
  const std::string user_id =
      ProfileHelper::Get()->GetUserIdHashFromProfile(profile_);
  fp_service_->GetRecordsForUser(
      user_id, base::BindOnce(&FingerprintStorage::OnGetRecords,
                              weak_factory_.GetWeakPtr()));
}

bool FingerprintStorage::IsEligible() const {
  return IsFingerprintSupported();
}

absl::optional<bool> FingerprintStorage::IsAccessible() const {
  return IsFingerprintEnabled(profile_);
}

bool FingerprintStorage::IsEnabled() const {
  return IsFingerprintEnabled(profile_) && HasRecord();
}

void FingerprintStorage::RecordFingerprintUnlockResult(
    FingerprintUnlockResult result) {
  base::UmaHistogramEnumeration("Fingerprint.Unlock.Result", result);

  const bool success = (result == FingerprintUnlockResult::kSuccess);
  base::UmaHistogramBoolean("Fingerprint.Unlock.AuthSuccessful", success);
  if (success) {
    base::UmaHistogramCounts100("Fingerprint.Unlock.AttemptsCountBeforeSuccess",
                                unlock_attempt_count());
  }
  feature_usage_metrics_service_->RecordUsage(success);
}

bool FingerprintStorage::IsFingerprintAvailable() const {
  return !ExceededUnlockAttempts() && IsFingerprintEnabled(profile_) &&
         HasRecord();
}

bool FingerprintStorage::HasRecord() const {
  return profile_->GetPrefs()->GetInteger(
             prefs::kQuickUnlockFingerprintRecord) != 0;
}

void FingerprintStorage::AddUnlockAttempt() {
  ++unlock_attempt_count_;
}

void FingerprintStorage::ResetUnlockAttemptCount() {
  unlock_attempt_count_ = 0;
}

bool FingerprintStorage::ExceededUnlockAttempts() const {
  return unlock_attempt_count() >= kMaximumUnlockAttempts;
}

void FingerprintStorage::OnRestarted() {
  GetRecordsForUser();
}

void FingerprintStorage::OnEnrollScanDone(device::mojom::ScanResult scan_result,
                                          bool is_complete,
                                          int32_t percent_complete) {
  base::UmaHistogramEnumeration("Fingerprint.Enroll.ScanResult", scan_result);
}

void FingerprintStorage::OnAuthScanDone(
    const device::mojom::FingerprintMessagePtr msg,
    const base::flat_map<std::string, std::vector<std::string>>& matches) {
  switch (msg->which()) {
    case device::mojom::FingerprintMessage::Tag::kScanResult:
      base::UmaHistogramEnumeration("Fingerprint.Auth.ScanResult",
                                    msg->get_scan_result());
      return;
    case device::mojom::FingerprintMessage::Tag::kFingerprintError:
      base::UmaHistogramEnumeration("Fingerprint.Auth.Error",
                                    msg->get_fingerprint_error());
      return;
  }
  NOTREACHED();
}

void FingerprintStorage::OnSessionFailed() {}

void FingerprintStorage::OnGetRecords(
    const base::flat_map<std::string, std::string>& fingerprints_list_mapping) {
  if (!IsFingerprintDisabledByPolicy(profile_->GetPrefs())) {
    profile_->GetPrefs()->SetInteger(prefs::kQuickUnlockFingerprintRecord,
                                     fingerprints_list_mapping.size());
    return;
  }

  for (const auto& it : fingerprints_list_mapping) {
    fp_service_->RemoveRecord(it.first, base::BindOnce([](bool success) {
                                if (success)
                                  return;
                                LOG(ERROR)
                                    << "Failed to remove fingerprint record";
                              }));
  }

  profile_->GetPrefs()->SetInteger(prefs::kQuickUnlockFingerprintRecord, 0);
}

}  // namespace quick_unlock
}  // namespace ash
