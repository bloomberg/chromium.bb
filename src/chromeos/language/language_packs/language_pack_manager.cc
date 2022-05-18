// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/language/language_packs/language_pack_manager.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "chromeos/dbus/dlcservice/dlcservice.pb.h"
#include "chromeos/dbus/dlcservice/dlcservice_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos::language_packs {
namespace {

PackResult ConvertDlcStateToPackResult(const dlcservice::DlcState& dlc_state) {
  PackResult result;

  switch (dlc_state.state()) {
    case dlcservice::DlcState_State_INSTALLED:
      result.pack_state = PackResult::INSTALLED;
      result.path = dlc_state.root_path();
      break;
    case dlcservice::DlcState_State_INSTALLING:
      result.pack_state = PackResult::IN_PROGRESS;
      break;
    case dlcservice::DlcState_State_NOT_INSTALLED:
      result.pack_state = PackResult::NOT_INSTALLED;
      break;
    default:
      result.pack_state = PackResult::UNKNOWN;
      break;
  }

  return result;
}

const base::flat_map<PackSpecPair, std::string>& GetAllLanguagePackDlcIds() {
  // Map of all DLCs and corresponding IDs.
  // It's a map from PackSpecPair to DLC ID. The pair is <feature id, locale>.
  // Whenever a new DLC is created, it needs to be added here.
  // Clients of Language Packs don't need to know the IDs.
  // TODO(b/223250258): We currently only have 10 languages. Add all remaining
  // languages once the infra issue is fixed.
  static const base::NoDestructor<base::flat_map<PackSpecPair, std::string>>
      all_dlc_ids({
          // Handwriting Recognition.
          {{kHandwritingFeatureId, "da"}, "handwriting-da"},
          {{kHandwritingFeatureId, "de"}, "handwriting-de"},
          {{kHandwritingFeatureId, "es"}, "handwriting-es"},
          {{kHandwritingFeatureId, "fi"}, "handwriting-fi"},
          {{kHandwritingFeatureId, "fr"}, "handwriting-fr"},
          {{kHandwritingFeatureId, "it"}, "handwriting-it"},
          {{kHandwritingFeatureId, "ja"}, "handwriting-ja"},
          {{kHandwritingFeatureId, "nl"}, "handwriting-nl"},
          {{kHandwritingFeatureId, "pt"}, "handwriting-pt"},
          {{kHandwritingFeatureId, "sv"}, "handwriting-sv"},

          // Text-To-Speech.
          {{kTtsFeatureId, "es-us"}, "tts-es-us"},
      });

  return *all_dlc_ids;
}

const base::flat_map<std::string, std::string>& GetAllBasePayloadDlcIds() {
  // Map of all features and corresponding Base Payload DLC IDs.
  static const base::NoDestructor<base::flat_map<std::string, std::string>>
      all_dlc_ids({
          {kHandwritingFeatureId, "handwriting"},
      });

  return *all_dlc_ids;
}

// Finds the ID of the DLC corresponding to the given spec.
// Returns the DLC ID if the DLC exists or absl::nullopt otherwise.
absl::optional<std::string> GetDlcIdForLanguagePack(
    const std::string& feature_id,
    const std::string& locale) {
  // We search in the static list for the given Pack spec.
  const PackSpecPair spec(feature_id, locale);
  const auto it = GetAllLanguagePackDlcIds().find(spec);

  if (it == GetAllLanguagePackDlcIds().end()) {
    return absl::nullopt;
  }

  return it->second;
}

// Finds the ID of the DLC corresponding to the Base Payload for a feature.
// Returns the DLC ID if the feature has a Base Payload or absl::nullopt
// otherwise.
absl::optional<std::string> GetDlcIdForBasePayload(
    const std::string& feature_id) {
  // We search in the static list for the given |feature_id|.
  const auto it = GetAllBasePayloadDlcIds().find(feature_id);

  if (it == GetAllBasePayloadDlcIds().end()) {
    return absl::nullopt;
  }

  return it->second;
}

void OnInstallDlcComplete(OnInstallCompleteCallback callback,
                          const DlcserviceClient::InstallResult& dlc_result) {
  PackResult result;
  result.operation_error = dlc_result.error;

  const bool success = dlc_result.error == dlcservice::kErrorNone;
  if (success) {
    result.pack_state = PackResult::INSTALLED;
    result.path = dlc_result.root_path;
  } else {
    result.pack_state = PackResult::UNKNOWN;
  }

  base::UmaHistogramBoolean("ChromeOS.LanguagePacks.InstallComplete.Success",
                            success);

  std::move(callback).Run(result);
}

void OnUninstallDlcComplete(OnUninstallCompleteCallback callback,
                            const std::string& err) {
  PackResult result;
  result.operation_error = err;

  const bool success = err == dlcservice::kErrorNone;
  if (success) {
    result.pack_state = PackResult::NOT_INSTALLED;
  } else {
    result.pack_state = PackResult::UNKNOWN;
  }

  base::UmaHistogramBoolean("ChromeOS.LanguagePacks.UninstallComplete.Success",
                            success);

  std::move(callback).Run(result);
}

void OnGetDlcState(GetPackStateCallback callback,
                   const std::string& err,
                   const dlcservice::DlcState& dlc_state) {
  PackResult result;
  if (err == dlcservice::kErrorNone) {
    result = ConvertDlcStateToPackResult(dlc_state);
  } else {
    result.pack_state = PackResult::UNKNOWN;
  }

  result.operation_error = err;

  std::move(callback).Run(result);
}

}  // namespace

bool LanguagePackManager::IsPackAvailable(const std::string& feature_id,
                                          const std::string& locale) {
  // We search in the static list for the given Pack spec.
  const PackSpecPair spec(feature_id, locale);
  return base::Contains(GetAllLanguagePackDlcIds(), spec);
}

void LanguagePackManager::InstallPack(const std::string& feature_id,
                                      const std::string& locale,
                                      OnInstallCompleteCallback callback) {
  const absl::optional<std::string> dlc_id =
      GetDlcIdForLanguagePack(feature_id, locale);

  // If the given Language Pack doesn't exist, run callback and don't reach the
  // DLC Service.
  if (!dlc_id) {
    PackResult result;
    result.operation_error = dlcservice::kErrorInvalidDlc;
    result.pack_state = PackResult::WRONG_ID;
    std::move(callback).Run(result);
    return;
  }

  dlcservice::InstallRequest install_request;
  install_request.set_id(*dlc_id);
  DlcserviceClient::Get()->Install(
      install_request,
      base::BindOnce(&OnInstallDlcComplete, std::move(callback)),
      base::DoNothing());
}

void LanguagePackManager::GetPackState(const std::string& feature_id,
                                       const std::string& locale,
                                       GetPackStateCallback callback) {
  const absl::optional<std::string> dlc_id =
      GetDlcIdForLanguagePack(feature_id, locale);

  // If the given Language Pack doesn't exist, run callback and don't reach the
  // DLC Service.
  if (!dlc_id) {
    PackResult result;
    result.operation_error = dlcservice::kErrorInvalidDlc;
    result.pack_state = PackResult::WRONG_ID;
    std::move(callback).Run(result);
    return;
  }

  DlcserviceClient::Get()->GetDlcState(
      *dlc_id, base::BindOnce(&OnGetDlcState, std::move(callback)));
}

void LanguagePackManager::RemovePack(const std::string& feature_id,
                                     const std::string& locale,
                                     OnUninstallCompleteCallback callback) {
  const absl::optional<std::string> dlc_id =
      GetDlcIdForLanguagePack(feature_id, locale);

  // If the given Language Pack doesn't exist, run callback and don't reach the
  // DLC Service.
  if (!dlc_id) {
    PackResult result;
    result.operation_error = dlcservice::kErrorInvalidDlc;
    result.pack_state = PackResult::WRONG_ID;
    std::move(callback).Run(result);
    return;
  }

  DlcserviceClient::Get()->Uninstall(
      *dlc_id, base::BindOnce(&OnUninstallDlcComplete, std::move(callback)));
}

void LanguagePackManager::InstallBasePayload(
    const std::string& feature_id,
    OnInstallBasePayloadCompleteCallback callback) {
  const absl::optional<std::string> dlc_id = GetDlcIdForBasePayload(feature_id);

  // If the given |feature_id| doesn't have a Base Payload, run callback and
  // don't reach the DLC Service.
  if (!dlc_id) {
    PackResult result;
    result.operation_error = dlcservice::kErrorInvalidDlc;
    result.pack_state = PackResult::WRONG_ID;
    std::move(callback).Run(result);
    return;
  }

  dlcservice::InstallRequest install_request;
  install_request.set_id(*dlc_id);
  DlcserviceClient::Get()->Install(
      install_request,
      base::BindOnce(&OnInstallDlcComplete, std::move(callback)),
      base::DoNothing());
}

void LanguagePackManager::AddObserver(Observer* const observer) {
  observers_.AddObserver(observer);
}

void LanguagePackManager::RemoveObserver(Observer* const observer) {
  observers_.RemoveObserver(observer);
}

void LanguagePackManager::NotifyPackStateChanged(
    const dlcservice::DlcState& dlc_state) {
  PackResult result = ConvertDlcStateToPackResult(dlc_state);
  for (Observer& observer : observers_)
    observer.OnPackStateChanged(result);
}

void LanguagePackManager::OnDlcStateChanged(
    const dlcservice::DlcState& dlc_state) {
  // As of now, we only have Handwriting as a client.
  // We will check the full list once we have more than one DLC.
  if (dlc_state.id() != kHandwritingFeatureId)
    return;

  NotifyPackStateChanged(dlc_state);
}

LanguagePackManager::LanguagePackManager() = default;
LanguagePackManager::~LanguagePackManager() = default;

void LanguagePackManager::Initialize() {
  DlcserviceClient::Get()->AddObserver(this);
}

void LanguagePackManager::ResetForTesting() {
  observers_.Clear();
}

// static
LanguagePackManager* LanguagePackManager::GetInstance() {
  static base::NoDestructor<LanguagePackManager> instance;
  return instance.get();
}

}  // namespace chromeos::language_packs
