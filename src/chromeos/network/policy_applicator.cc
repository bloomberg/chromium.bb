// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/policy_applicator.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chromeos/components/onc/onc_signature.h"
#include "chromeos/dbus/shill/shill_profile_client.h"
#include "chromeos/network/cellular_policy_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_type_pattern.h"
#include "chromeos/network/network_ui_data.h"
#include "chromeos/network/onc/onc_translator.h"
#include "chromeos/network/policy_util.h"
#include "chromeos/network/shill_property_util.h"
#include "components/onc/onc_constants.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

void LogErrorMessageAndInvokeCallback(base::OnceClosure callback,
                                      const base::Location& from_where,
                                      const std::string& error_name,
                                      const std::string& error_message) {
  LOG(ERROR) << from_where.ToString() << ": " << error_message;
  std::move(callback).Run();
}

const base::Value* GetByGUID(const std::map<std::string, base::Value>& policies,
                             const std::string& guid) {
  auto it = policies.find(guid);
  if (it == policies.end())
    return nullptr;
  return &(it->second);
}

const base::Value* FindMatchingPolicy(
    const std::map<std::string, base::Value>& policies,
    const base::Value& actual_network) {
  for (auto& policy : policies) {
    if (policy_util::IsPolicyMatching(policy.second, actual_network))
      return &(policy.second);
  }
  return nullptr;
}

// Returns the GUID property from |onc_part|, or an empty string if no GUID was
// present.
std::string GetGUIDFromONCPart(const base::Value& onc_part) {
  const base::Value* guid_value = onc_part.FindKeyOfType(
      ::onc::network_config::kGUID, base::Value::Type::STRING);
  if (!guid_value)
    return std::string();
  return guid_value->GetString();
}

bool IsCellularPolicy(const base::Value& onc_config) {
  const std::string* type =
      onc_config.FindStringKey(::onc::network_config::kType);
  return type && *type == ::onc::network_type::kCellular;
}

const std::string* GetSMDPAddressFromONC(const base::Value& onc_config) {
  const std::string* type =
      onc_config.FindStringKey(::onc::network_config::kType);
  const base::Value* cellular_dict =
      onc_config.FindKey(::onc::network_config::kCellular);
  const std::string* smdp_address = nullptr;

  if (type && *type == ::onc::network_type::kCellular && cellular_dict &&
      cellular_dict->is_dict())
    smdp_address = cellular_dict->FindStringKey(::onc::cellular::kSMDPAddress);

  return smdp_address;
}

void CopyStringKey(const base::Value& old_shill_properties,
                   base::Value* new_shill_properties,
                   const std::string& property_key_name) {
  const std::string* value_in_old_entry =
      old_shill_properties.FindStringKey(property_key_name);
  const std::string* value_in_new_entry =
      new_shill_properties->FindStringKey(property_key_name);
  if (value_in_old_entry &&
      (!value_in_new_entry || value_in_new_entry->empty())) {
    NET_LOG(EVENT) << "Copying " << property_key_name
                   << " over to the new Shill entry, value: "
                   << *value_in_old_entry;
    new_shill_properties->SetStringKey(property_key_name, *value_in_old_entry);
  }
}

void CopyRequiredCellularProperies(const base::Value& old_shill_properties,
                                   base::Value* new_shill_properties) {
  const std::string* type =
      old_shill_properties.FindStringKey(shill::kTypeProperty);
  if (!type || *type != shill::kTypeCellular)
    return;

  CopyStringKey(old_shill_properties, new_shill_properties,
                shill::kIccidProperty);
  CopyStringKey(old_shill_properties, new_shill_properties,
                shill::kEidProperty);
}

// Special service name in shill remembering settings across ethernet services.
// Chrome should not attempt to configure / delete this.
const char kEthernetAnyService[] = "ethernet_any";

}  // namespace

PolicyApplicator::PolicyApplicator(
    const NetworkProfile& profile,
    std::map<std::string, base::Value> all_policies,
    base::Value global_network_config,
    ConfigurationHandler* handler,
    CellularPolicyHandler* cellular_policy_handler,
    std::set<std::string>* modified_policy_guids)
    : cellular_policy_handler_(cellular_policy_handler),
      handler_(handler),
      profile_(profile),
      all_policies_(std::move(all_policies)),
      global_network_config_(std::move(global_network_config)) {
  remaining_policy_guids_.swap(*modified_policy_guids);
}

PolicyApplicator::~PolicyApplicator() {
  VLOG(1) << "Destroying PolicyApplicator for " << profile_.userhash;
}

void PolicyApplicator::Run() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ShillProfileClient::Get()->GetProperties(
      dbus::ObjectPath(profile_.path),
      base::BindOnce(&PolicyApplicator::GetProfilePropertiesCallback,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&PolicyApplicator::GetProfilePropertiesError,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PolicyApplicator::GetProfilePropertiesCallback(
    base::Value profile_properties) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(2) << "Received properties for profile " << profile_.ToDebugString();
  const base::Value* entries =
      profile_properties.FindListKey(shill::kEntriesProperty);
  if (!entries) {
    LOG(ERROR) << "Profile " << profile_.ToDebugString()
               << " doesn't contain the property " << shill::kEntriesProperty;
    NotifyConfigurationHandlerAndFinish();
    return;
  }

  for (const auto& it : entries->GetListDeprecated()) {
    if (!it.is_string())
      continue;

    std::string entry_identifier = it.GetString();

    // Skip "ethernet_any", as this is used by shill internally to persist
    // ethernet settings and the policy application logic should not mess with
    // it.
    if (entry_identifier == kEthernetAnyService)
      continue;

    pending_get_entry_calls_.insert(entry_identifier);
    ShillProfileClient::Get()->GetEntry(
        dbus::ObjectPath(profile_.path), entry_identifier,
        base::BindOnce(&PolicyApplicator::GetEntryCallback,
                       weak_ptr_factory_.GetWeakPtr(), entry_identifier),
        base::BindOnce(&PolicyApplicator::GetEntryError,
                       weak_ptr_factory_.GetWeakPtr(), entry_identifier));
  }
  if (pending_get_entry_calls_.empty())
    ApplyRemainingPolicies();
}

void PolicyApplicator::GetProfilePropertiesError(
    const std::string& error_name,
    const std::string& error_message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(ERROR) << "Could not retrieve properties of profile " << profile_.path
             << ": " << error_message;
  NotifyConfigurationHandlerAndFinish();
}

void PolicyApplicator::GetEntryCallback(const std::string& entry_identifier,
                                        base::Value entry_properties) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(2) << "Received properties for entry " << entry_identifier
          << " of profile " << profile_.ToDebugString();

  base::Value onc_part = onc::TranslateShillServiceToONCPart(
      entry_properties, ::onc::ONC_SOURCE_UNKNOWN,
      &onc::kNetworkWithStateSignature, nullptr /* network_state */);

  std::string old_guid = GetGUIDFromONCPart(onc_part);
  std::unique_ptr<NetworkUIData> ui_data =
      shill_property_util::GetUIDataFromProperties(entry_properties);
  if (!ui_data) {
    VLOG(1) << "Entry " << entry_identifier << " of profile "
            << profile_.ToDebugString() << " contains no or no valid UIData.";
    // This might be an entry of an older ChromeOS version. Assume it to be
    // unmanaged. It's an inconsistency if there is a GUID but no UIData, thus
    // clear the GUID just in case.
    old_guid.clear();
  }

  bool was_managed =
      ui_data && (ui_data->onc_source() == ::onc::ONC_SOURCE_DEVICE_POLICY ||
                  ui_data->onc_source() == ::onc::ONC_SOURCE_USER_POLICY);

  const base::Value* new_policy = nullptr;
  if (was_managed) {
    // If we have a GUID that might match a current policy, do a lookup using
    // that GUID at first. In particular this is necessary, as some networks
    // can't be matched to policies by properties (e.g. VPN).
    new_policy = GetByGUID(all_policies_, old_guid);
  }

  if (!new_policy) {
    // If we didn't find a policy by GUID, still a new policy might match.
    new_policy = FindMatchingPolicy(all_policies_, onc_part);
  }

  auto profile_entry_finished_callback =
      base::BindOnce(&PolicyApplicator::ProfileEntryFinished,
                     weak_ptr_factory_.GetWeakPtr(), entry_identifier);
  if (new_policy) {
    std::string new_guid = GetGUIDFromONCPart(*new_policy);
    DCHECK(!new_guid.empty());
    VLOG_IF(1, was_managed && old_guid != new_guid)
        << "Updating configuration previously managed by policy " << old_guid
        << " with new policy " << new_guid << ".";
    VLOG_IF(1, !was_managed)
        << "Applying policy " << new_guid << " to previously unmanaged "
        << "configuration.";

    ApplyNewPolicy(entry_identifier, entry_properties, std::move(ui_data),
                   old_guid, new_guid, *new_policy,
                   std::move(profile_entry_finished_callback));
    return;
  }

  if (was_managed) {
    VLOG(1) << "Removing configuration previously managed by policy "
            << old_guid << ", because the policy was removed.";

    // Remove the entry, because the network was managed but isn't anymore.
    // Note: An alternative might be to preserve the user settings, but it's
    // unclear which values originating the policy should be removed.
    DeleteEntry(entry_identifier, std::move(profile_entry_finished_callback));
    return;
  }

  ApplyGlobalPolicyOnUnmanagedEntry(entry_identifier, entry_properties,
                                    std::move(profile_entry_finished_callback));
}

void PolicyApplicator::GetEntryError(const std::string& entry_identifier,
                                     const std::string& error_name,
                                     const std::string& error_message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(ERROR) << "Could not retrieve entry " << entry_identifier
             << " of profile " << profile_.path << ": " << error_message;
  ProfileEntryFinished(entry_identifier);
}

void PolicyApplicator::ApplyNewPolicy(const std::string& entry_identifier,
                                      const base::Value& entry_properties,
                                      std::unique_ptr<NetworkUIData> ui_data,
                                      const std::string& old_guid,
                                      const std::string& new_guid,
                                      const base::Value& new_policy,
                                      base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (old_guid == new_guid &&
      remaining_policy_guids_.find(new_guid) == remaining_policy_guids_.end()) {
    VLOG(1) << "Not updating existing managed configuration with guid "
            << new_guid << " because the policy didn't change.";
    std::move(callback).Run();
    return;
  }
  remaining_policy_guids_.erase(new_guid);

  DCHECK(new_policy.is_dict());
  DCHECK(entry_properties.is_dict());

  const base::Value* user_settings =
      ui_data ? ui_data->GetUserSettingsDictionary() : nullptr;
  base::Value new_shill_properties = policy_util::CreateShillConfiguration(
      profile_, new_guid, &global_network_config_, &new_policy, user_settings);

  // Copy over the value of ICCID and EID property from old entry to new shill
  // properties since Shill requires ICCID and EID to create or update the
  // existing service.
  CopyRequiredCellularProperies(entry_properties, &new_shill_properties);

  // A new policy has to be applied to this profile entry. In order to keep
  // implicit state of Shill like "connected successfully before", keep the
  // entry if a policy is reapplied (e.g. after reboot) or is updated.
  // However, some Shill properties are used to identify the network and
  // cannot be modified after initial configuration, so we have to delete
  // the profile entry in these cases. Also, keeping Shill's state if the
  // SSID changed might not be a good idea anyways. If the policy GUID
  // changed, or there was no policy before, we delete the entry at first to
  // ensure that no old configuration remains.
  if (old_guid == new_guid && shill_property_util::DoIdentifyingPropertiesMatch(
                                  new_shill_properties, entry_properties)) {
    NET_LOG(EVENT) << "Updating previously managed configuration with the "
                   << "updated policy " << new_guid << ".";
    WriteNewShillConfiguration(std::move(new_shill_properties),
                               new_policy.Clone(), std::move(callback));
  } else {
    NET_LOG(EVENT) << "Deleting profile entry before writing new policy "
                   << new_guid << " because of identifying properties changed.";
    // In general, old entries should at first be deleted before new
    // configurations are written to prevent inconsistencies. Therefore, we
    // delay the writing of the new config here until ~PolicyApplicator.
    // E.g. one problematic case is if a policy { {GUID=X, SSID=Y} } is
    // applied to the profile entries
    // { ENTRY1 = {GUID=X, SSID=X, USER_SETTINGS=X},
    //   ENTRY2 = {SSID=Y, ... } }.
    // At first ENTRY1 and ENTRY2 should be removed, then the new config be
    // written and the result should be:
    // { {GUID=X, SSID=Y, USER_SETTINGS=X} }
    DeleteEntry(entry_identifier,
                base::BindOnce(&PolicyApplicator::WriteNewShillConfiguration,
                               weak_ptr_factory_.GetWeakPtr(),
                               std::move(new_shill_properties),
                               new_policy.Clone(), std::move(callback)));
  }
}

void PolicyApplicator::ApplyGlobalPolicyOnUnmanagedEntry(
    const std::string& entry_identifier,
    const base::Value& entry_properties,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The entry wasn't managed and doesn't match any current policy. Global
  // network settings have to be applied.
  base::Value shill_properties_to_update(base::Value::Type::DICTIONARY);
  policy_util::SetShillPropertiesForGlobalPolicy(
      entry_properties, global_network_config_, &shill_properties_to_update);
  if (shill_properties_to_update.DictEmpty()) {
    VLOG(2) << "Ignore unmanaged entry.";
    // Calling a SetProperties of Shill with an empty dictionary is a no op.
    std::move(callback).Run();
    return;
  }
  NET_LOG(EVENT) << "Apply global network config to unmanaged entry "
                 << entry_identifier << ".";
  handler_->UpdateExistingConfigurationWithPropertiesFromPolicy(
      entry_properties, shill_properties_to_update, std::move(callback));
}

void PolicyApplicator::DeleteEntry(const std::string& entry_identifier,
                                   base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  ShillProfileClient::Get()->DeleteEntry(
      dbus::ObjectPath(profile_.path), entry_identifier,
      std::move(split_callback.first),
      base::BindOnce(&LogErrorMessageAndInvokeCallback,
                     std::move(split_callback.second), FROM_HERE));
}

void PolicyApplicator::WriteNewShillConfiguration(base::Value shill_dictionary,
                                                  base::Value policy,
                                                  base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Ethernet (non EAP) settings, like GUID or UIData, cannot be stored per
  // user. Abort in that case.
  const std::string* type = policy.FindStringKey(::onc::network_config::kType);
  if (type && *type == ::onc::network_type::kEthernet &&
      profile_.type() == NetworkProfile::TYPE_USER) {
    const base::Value* ethernet = policy.FindKeyOfType(
        ::onc::network_config::kEthernet, base::Value::Type::DICTIONARY);
    if (ethernet) {
      const std::string* auth =
          ethernet->FindStringKey(::onc::ethernet::kAuthentication);
      if (auth && *auth == ::onc::ethernet::kAuthenticationNone) {
        std::move(callback).Run();
        return;
      }
    }
  }

  handler_->CreateConfigurationFromPolicy(shill_dictionary,
                                          std::move(callback));
}

void PolicyApplicator::ProfileEntryFinished(
    const std::string& entry_identifier) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto iter = pending_get_entry_calls_.find(entry_identifier);
  DCHECK(iter != pending_get_entry_calls_.end());
  pending_get_entry_calls_.erase(iter);
  if (pending_get_entry_calls_.empty())
    ApplyRemainingPolicies();
}

void PolicyApplicator::ApplyRemainingPolicies() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pending_get_entry_calls_.empty());

  VLOG_IF(2, !remaining_policy_guids_.empty())
      << "Create new managed network configurations in profile"
      << profile_.ToDebugString() << ".";

  if (remaining_policy_guids_.empty()) {
    NotifyConfigurationHandlerAndFinish();
    return;
  }

  // All profile entries were compared to policies. |remaining_policy_guids_|
  // contains all modified policies that didn't match any entry. For these
  // remaining policies, new configurations have to be created.
  for (std::set<std::string>::iterator it = remaining_policy_guids_.begin();
       it != remaining_policy_guids_.end();) {
    const std::string& guid = *it;
    const base::Value* network_policy = GetByGUID(all_policies_, guid);
    DCHECK(network_policy);

    NET_LOG(EVENT) << "Creating new configuration managed by policy " << guid
                   << " in profile " << profile_.ToDebugString() << ".";

    if (IsCellularPolicy(*network_policy)) {
      const std::string* smdp_address = GetSMDPAddressFromONC(*network_policy);
      if (features::IsESimPolicyEnabled() && smdp_address) {
        NET_LOG(EVENT)
            << "Found ONC configuration with SMDP: " << *smdp_address
            << ". Start installing policy eSim profile with ONC config: "
            << *network_policy;
        if (cellular_policy_handler_)
          cellular_policy_handler_->InstallESim(*smdp_address, *network_policy);
        else
          NET_LOG(ERROR) << "Unable to install eSIM. CellularPolicyHandler not "
                            "initialized.";
      } else {
        NET_LOG(EVENT) << "Skip installing policy eSIM either because "
                          "the eSIM policy feature is not enabled or the SMDP "
                          "address is missing from ONC.";
      }

      it = remaining_policy_guids_.erase(it);
      if (remaining_policy_guids_.empty()) {
        NotifyConfigurationHandlerAndFinish();
      }
      continue;
    }

    base::Value shill_dictionary = policy_util::CreateShillConfiguration(
        profile_, guid, &global_network_config_, network_policy,
        nullptr /* no user settings */);

    handler_->CreateConfigurationFromPolicy(
        shill_dictionary,
        base::BindOnce(&PolicyApplicator::RemainingPolicyApplied,
                       weak_ptr_factory_.GetWeakPtr(), guid));
    it++;
  }
}

void PolicyApplicator::RemainingPolicyApplied(const std::string& guid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  remaining_policy_guids_.erase(guid);
  if (remaining_policy_guids_.empty()) {
    NotifyConfigurationHandlerAndFinish();
  }
}

void PolicyApplicator::NotifyConfigurationHandlerAndFinish() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_ptr_factory_.InvalidateWeakPtrs();
  handler_->OnPoliciesApplied(profile_);
}

}  // namespace chromeos
