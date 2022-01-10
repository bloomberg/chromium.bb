// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/cellular_utils.h"

#include "ash/constants/ash_features.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/dbus/hermes/hermes_manager_client.h"
#include "chromeos/dbus/hermes/hermes_profile_client.h"
#include "chromeos/network/cellular_esim_profile.h"

namespace chromeos {

namespace {

const char kNonShillCellularNetworkPathPrefix[] = "/non-shill-cellular/";

base::flat_set<dbus::ObjectPath> GetProfilePathsFromEuicc(
    HermesEuiccClient::Properties* euicc_properties) {
  base::flat_set<dbus::ObjectPath> profile_paths;

  for (const dbus::ObjectPath& path :
       euicc_properties->installed_carrier_profiles().value()) {
    profile_paths.insert(path);
  }
  for (const dbus::ObjectPath& path :
       euicc_properties->pending_carrier_profiles().value()) {
    profile_paths.insert(path);
  }

  return profile_paths;
}

CellularESimProfile::State FromProfileState(hermes::profile::State state) {
  switch (state) {
    case hermes::profile::State::kPending:
      return CellularESimProfile::State::kPending;
    case hermes::profile::State::kInactive:
      return CellularESimProfile::State::kInactive;
    case hermes::profile::State::kActive:
      return CellularESimProfile::State::kActive;
    default:
      NOTREACHED() << "Unexpected Hermes profile state: " << state;
      return CellularESimProfile::State::kInactive;
  }
}

std::vector<CellularESimProfile> GenerateProfilesFromEuicc(
    const dbus::ObjectPath& euicc_path) {
  std::vector<CellularESimProfile> profiles;

  HermesEuiccClient::Properties* euicc_properties =
      HermesEuiccClient::Get()->GetProperties(euicc_path);
  std::string eid = euicc_properties->eid().value();

  for (const dbus::ObjectPath& profile_path :
       GetProfilePathsFromEuicc(euicc_properties)) {
    HermesProfileClient::Properties* profile_properties =
        HermesProfileClient::Get()->GetProperties(profile_path);

    // Hermes only exposes eSIM profiles with relevant profile class. e.g.
    // Test profiles are exposed only when Hermes is put into test mode.
    // No additional profile filtering is done on Chrome side.
    profiles.emplace_back(
        FromProfileState(profile_properties->state().value()), profile_path,
        eid, profile_properties->iccid().value(),
        base::UTF8ToUTF16(profile_properties->name().value()),
        base::UTF8ToUTF16(profile_properties->nick_name().value()),
        base::UTF8ToUTF16(profile_properties->service_provider().value()),
        profile_properties->activation_code().value());
  }

  return profiles;
}

const base::flat_map<int32_t, std::string> GetESimSlotToEidMap() {
  base::flat_map<int32_t, std::string> esim_slot_to_eid;
  for (auto& euicc_path : HermesManagerClient::Get()->GetAvailableEuiccs()) {
    HermesEuiccClient::Properties* properties =
        HermesEuiccClient::Get()->GetProperties(euicc_path);
    int32_t slot_id = properties->physical_slot().value();
    std::string eid = properties->eid().value();
    esim_slot_to_eid.emplace(slot_id, eid);
  }
  return esim_slot_to_eid;
}

}  // namespace

std::vector<CellularESimProfile> GenerateProfilesFromHermes() {
  std::vector<CellularESimProfile> profiles;

  for (const dbus::ObjectPath& euicc_path :
       HermesManagerClient::Get()->GetAvailableEuiccs()) {
    std::vector<CellularESimProfile> profiles_from_euicc =
        GenerateProfilesFromEuicc(euicc_path);
    std::copy(profiles_from_euicc.begin(), profiles_from_euicc.end(),
              std::back_inserter(profiles));
  }

  return profiles;
}

const DeviceState::CellularSIMSlotInfos GetSimSlotInfosWithUpdatedEid(
    const DeviceState* device) {
  const base::flat_map<int32_t, std::string> esim_slot_to_eid =
      GetESimSlotToEidMap();

  DeviceState::CellularSIMSlotInfos sim_slot_infos = device->GetSimSlotInfos();
  for (auto& sim_slot_info : sim_slot_infos) {
    const std::string shill_provided_eid = sim_slot_info.eid;

    // If there is no associated |slot_id| in the map, the SIM slot info refers
    // to a pSIM, and the Hermes provided data is irrelevant.
    auto it = esim_slot_to_eid.find(sim_slot_info.slot_id);
    if (it == esim_slot_to_eid.end())
      continue;

    const std::string hermes_provided_eid = it->second;
    if (!shill_provided_eid.empty() &&
        hermes_provided_eid != shill_provided_eid) {
      LOG(ERROR) << "Hermes provided EID of " << hermes_provided_eid
                 << " does not match Shill provided non-empty EID of "
                 << shill_provided_eid << ". Defaulting to Shill provided EID.";
    } else {
      sim_slot_info.eid = hermes_provided_eid;
    }
  }
  return sim_slot_infos;
}

bool IsSimPrimary(const std::string& iccid, const DeviceState* device) {
  for (const auto& sim_slot_info : device->GetSimSlotInfos()) {
    if (sim_slot_info.iccid == iccid && sim_slot_info.primary) {
      return true;
    }
  }
  return false;
}

std::string GenerateStubCellularServicePath(const std::string& iccid) {
  return base::StrCat({kNonShillCellularNetworkPathPrefix, iccid});
}

bool IsStubCellularServicePath(const std::string& service_path) {
  return base::StartsWith(service_path, kNonShillCellularNetworkPathPrefix);
}

absl::optional<dbus::ObjectPath> GetCurrentEuiccPath() {
  bool use_external_euicc = base::FeatureList::IsEnabled(
      chromeos::features::kCellularUseExternalEuicc);
  const std::vector<dbus::ObjectPath>& euicc_paths =
      HermesManagerClient::Get()->GetAvailableEuiccs();
  if (euicc_paths.empty() || (use_external_euicc && euicc_paths.size() < 2))
    return absl::nullopt;

  return use_external_euicc ? euicc_paths[1] : euicc_paths[0];
}

}  // namespace chromeos