// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/cellular_esim_profile_handler_impl.h"

#include <sstream>
#include <vector>

#include "ash/constants/ash_pref_names.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/values.h"
#include "chromeos/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/network/cellular_utils.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_type_pattern.h"
#include "components/prefs/pref_registry_simple.h"

namespace chromeos {
namespace {

base::flat_set<std::string> GetEuiccPathsFromHermes() {
  base::flat_set<std::string> paths;
  for (const dbus::ObjectPath& euicc_path :
       HermesManagerClient::Get()->GetAvailableEuiccs()) {
    paths.insert(euicc_path.value());
  }
  return paths;
}

bool ContainsProfileWithoutIccid(
    const std::vector<CellularESimProfile>& profiles) {
  auto iter = std::find_if(profiles.begin(), profiles.end(),
                           [](const CellularESimProfile& profile) {
                             return profile.iccid().empty();
                           });
  return iter != profiles.end();
}

}  // namespace

// static
void CellularESimProfileHandlerImpl::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kESimRefreshedEuiccs);
  registry->RegisterListPref(prefs::kESimProfiles);
}

CellularESimProfileHandlerImpl::CellularESimProfileHandlerImpl() = default;

CellularESimProfileHandlerImpl::~CellularESimProfileHandlerImpl() {
  network_state_handler()->RemoveObserver(this, FROM_HERE);
}

void CellularESimProfileHandlerImpl::DeviceListChanged() {
  if (!device_prefs_)
    return;

  AutoRefreshEuiccsIfNecessary();
}

void CellularESimProfileHandlerImpl::InitInternal() {
  network_state_handler()->AddObserver(this, FROM_HERE);
}

std::vector<CellularESimProfile>
CellularESimProfileHandlerImpl::GetESimProfiles() {
  // Profiles are stored in prefs.
  if (!device_prefs_)
    return std::vector<CellularESimProfile>();

  const base::Value* profiles_list =
      device_prefs_->GetList(prefs::kESimProfiles);
  if (!profiles_list) {
    NET_LOG(ERROR) << "eSIM profiles pref is not a list";
    return std::vector<CellularESimProfile>();
  }

  std::vector<CellularESimProfile> profiles;
  for (const base::Value& value : profiles_list->GetList()) {
    const base::DictionaryValue* dict;
    if (!value.GetAsDictionary(&dict)) {
      NET_LOG(ERROR) << "List item from eSIM profiles pref is not a dictionary";
      continue;
    }

    absl::optional<CellularESimProfile> profile =
        CellularESimProfile::FromDictionaryValue(*dict);
    if (!profile) {
      NET_LOG(ERROR) << "Unable to deserialize eSIM profile: " << *dict;
      continue;
    }

    profiles.push_back(*profile);
  }
  return profiles;
}

bool CellularESimProfileHandlerImpl::HasRefreshedProfilesForEuicc(
    const std::string& eid) {
  base::flat_set<std::string> euicc_paths =
      GetAutoRefreshedEuiccPathsFromPrefs();

  for (const auto& path : euicc_paths) {
    HermesEuiccClient::Properties* euicc_properties =
        HermesEuiccClient::Get()->GetProperties(dbus::ObjectPath(path));
    if (!euicc_properties)
      continue;

    if (eid == euicc_properties->eid().value())
      return true;
  }

  return false;
}

void CellularESimProfileHandlerImpl::SetDevicePrefs(PrefService* device_prefs) {
  device_prefs_ = device_prefs;
  OnHermesPropertiesUpdated();
}

void CellularESimProfileHandlerImpl::OnHermesPropertiesUpdated() {
  if (!device_prefs_)
    return;

  AutoRefreshEuiccsIfNecessary();
  UpdateProfilesFromHermes();
}

void CellularESimProfileHandlerImpl::AutoRefreshEuiccsIfNecessary() {
  if (!CellularDeviceExists())
    return;

  base::flat_set<std::string> euicc_paths_from_hermes =
      GetEuiccPathsFromHermes();
  base::flat_set<std::string> auto_refreshed_euicc_paths =
      GetAutoRefreshedEuiccPaths();

  base::flat_set<std::string> paths_needing_auto_refresh;
  for (const auto& euicc_path : euicc_paths_from_hermes) {
    if (!base::Contains(auto_refreshed_euicc_paths, euicc_path))
      paths_needing_auto_refresh.insert(euicc_path);
  }

  // We only need to request profiles if we see a new EUICC from Hermes that we
  // have not yet seen before. If no such EUICCs exist, return early.
  if (paths_needing_auto_refresh.empty())
    return;

  StartAutoRefresh(paths_needing_auto_refresh);
}

void CellularESimProfileHandlerImpl::StartAutoRefresh(
    const base::flat_set<std::string>& euicc_paths) {
  paths_pending_auto_refresh_.insert(euicc_paths.begin(), euicc_paths.end());

  // If there is more than one EUICC, log an error. This configuration is not
  // officially supported, so this log may be helpful in feedback reports.
  if (euicc_paths.size() > 1u)
    NET_LOG(ERROR) << "Attempting to refresh profiles from multiple EUICCs";

  // Refresh profiles from the unknown EUICCs. Note that this will internally
  // start an inhibit operation, temporarily blocking the user from changing
  // cellular settings. This operation is only expected to occur when the device
  // originally boots or after a powerwash.
  for (const auto& path : euicc_paths) {
    NET_LOG(EVENT) << "Found new EUICC whose profiles have not yet been "
                   << "refreshsed. Refreshing profile list for " << path;
    RefreshProfileList(
        dbus::ObjectPath(path),
        base::BindOnce(
            &CellularESimProfileHandlerImpl::OnAutoRefreshEuiccComplete,
            weak_ptr_factory_.GetWeakPtr(), path));
  }
}

base::flat_set<std::string>
CellularESimProfileHandlerImpl::GetAutoRefreshedEuiccPaths() const {
  // Add all paths stored in prefs.
  base::flat_set<std::string> euicc_paths =
      GetAutoRefreshedEuiccPathsFromPrefs();

  // Add paths which are currently pending a refresh.
  euicc_paths.insert(paths_pending_auto_refresh_.begin(),
                     paths_pending_auto_refresh_.end());

  return euicc_paths;
}

base::flat_set<std::string>
CellularESimProfileHandlerImpl::GetAutoRefreshedEuiccPathsFromPrefs() const {
  DCHECK(device_prefs_);

  const base::Value* euicc_paths_from_prefs =
      device_prefs_->GetList(prefs::kESimRefreshedEuiccs);
  if (!euicc_paths_from_prefs) {
    NET_LOG(ERROR) << "Could not fetch refreshed EUICCs pref.";
    return {};
  }

  base::flat_set<std::string> euicc_paths;
  for (const auto& euicc : euicc_paths_from_prefs->GetList()) {
    if (!euicc.is_string()) {
      NET_LOG(ERROR) << "Non-string EUICC path: " << euicc;
      continue;
    }
    euicc_paths.insert(euicc.GetString());
  }
  return euicc_paths;
}

void CellularESimProfileHandlerImpl::OnAutoRefreshEuiccComplete(
    const std::string& path,
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock) {
  paths_pending_auto_refresh_.erase(path);

  if (!inhibit_lock) {
    NET_LOG(ERROR) << "Auto-refresh failed due to inhibit error. Path: "
                   << path;
    return;
  }

  NET_LOG(EVENT) << "Finished auto-refresh for path: " << path;
  AddNewlyRefreshedEuiccPathToPrefs(path);
}

void CellularESimProfileHandlerImpl::AddNewlyRefreshedEuiccPathToPrefs(
    const std::string& new_path) {
  DCHECK(device_prefs_);

  base::flat_set<std::string> auto_refreshed_euicc_paths =
      GetAutoRefreshedEuiccPathsFromPrefs();

  // Keep all paths which were already in prefs.
  base::Value euicc_paths(base::Value::Type::LIST);
  for (const auto& path : auto_refreshed_euicc_paths)
    euicc_paths.Append(path);

  // Add new path.
  euicc_paths.Append(new_path);

  device_prefs_->Set(prefs::kESimRefreshedEuiccs, std::move(euicc_paths));
}

void CellularESimProfileHandlerImpl::UpdateProfilesFromHermes() {
  DCHECK(device_prefs_);

  std::vector<CellularESimProfile> profiles_from_hermes =
      GenerateProfilesFromHermes();

  // Skip updating if there are profiles that haven't received ICCID updates
  // yet. This is required because property updates to eSIM profile objects
  // occur after the profile list has been updated. This state is temporary.
  // This method will be triggered again when ICCID properties are updated.
  if (ContainsProfileWithoutIccid(profiles_from_hermes)) {
    return;
  }

  // When the device starts up, Hermes is expected to return an empty list of
  // profiles if the profiles have not yet been requested. If this occurs,
  // return early and rely on the profiles stored in prefs instead.
  if (profiles_from_hermes.empty() &&
      !has_completed_successful_profile_refresh()) {
    return;
  }

  std::vector<CellularESimProfile> profiles_before_fetch = GetESimProfiles();

  // If nothing has changed since the last update, do not update prefs or notify
  // observers of a change.
  if (profiles_from_hermes == profiles_before_fetch)
    return;

  std::stringstream ss;
  ss << "New set of eSIM profiles have been fetched from Hermes: ";

  // Store the updated list of profiles in prefs.
  base::Value list(base::Value::Type::LIST);
  for (const auto& profile : profiles_from_hermes) {
    list.Append(profile.ToDictionaryValue());
    ss << "{iccid: " << profile.iccid() << ", eid: " << profile.eid() << "}, ";
  }
  device_prefs_->Set(prefs::kESimProfiles, std::move(list));

  if (profiles_from_hermes.empty())
    ss << "<empty>";
  NET_LOG(EVENT) << ss.str();

  network_state_handler()->SyncStubCellularNetworks();
  NotifyESimProfileListUpdated();
}

bool CellularESimProfileHandlerImpl::CellularDeviceExists() const {
  return network_state_handler()->GetDeviceStateByType(
             NetworkTypePattern::Cellular()) != nullptr;
}

void CellularESimProfileHandlerImpl::ResetESimProfileCache() {
  DCHECK(device_prefs_);

  device_prefs_->Set(prefs::kESimProfiles,
                     base::Value(base::Value::Type::LIST));
  device_prefs_->Set(prefs::kESimRefreshedEuiccs,
                     base::Value(base::Value::Type::LIST));

  NET_LOG(EVENT) << "Resetting eSIM profile cache";
  OnHermesPropertiesUpdated();
}

}  // namespace chromeos
