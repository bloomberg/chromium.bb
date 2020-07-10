// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/developer_tools_policy_handler.h"

#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"

namespace policy {

namespace {

using Availability = DeveloperToolsPolicyHandler::Availability;

// The result of checking a policy value.
enum class PolicyCheckResult {
  // The policy is not set.
  kNotSet,
  // The policy is set to an invalid value.
  kInvalid,
  // The policy is set to a valid value.
  kValid
};

// Checks the value of the DeveloperToolsDisabled policy. |errors| may be
// nullptr.
PolicyCheckResult CheckDeveloperToolsDisabled(
    const base::Value* developer_tools_disabled,
    policy::PolicyErrorMap* errors) {
  if (!developer_tools_disabled)
    return PolicyCheckResult::kNotSet;

  if (!developer_tools_disabled->is_bool()) {
    if (errors) {
      errors->AddError(key::kDeveloperToolsDisabled, IDS_POLICY_TYPE_ERROR,
                       base::Value::GetTypeName(base::Value::Type::BOOLEAN));
    }
    return PolicyCheckResult::kInvalid;
  }

  return PolicyCheckResult::kValid;
}

// Returns the target value of the |kDevToolsAvailability| pref derived only
// from the legacy DeveloperToolsDisabled policy. If this policy is not set or
// does not have a valid value, returns |nullopt|.
base::Optional<Availability> GetValueFromDeveloperToolsDisabledPolicy(
    const PolicyMap& policies) {
  const base::Value* developer_tools_disabled =
      policies.GetValue(key::kDeveloperToolsDisabled);

  if (CheckDeveloperToolsDisabled(developer_tools_disabled,
                                  nullptr /*error*/) !=
      PolicyCheckResult::kValid) {
    return base::nullopt;
  }

  return developer_tools_disabled->GetBool() ? Availability::kDisallowed
                                             : Availability::kAllowed;
}

// Returns true if |value| is within the valid range of the
// DeveloperToolsAvailability enum policy.
bool IsValidDeveloperToolsAvailabilityValue(int value) {
  return value >= 0 && value <= static_cast<int>(Availability::kMaxValue);
}

// Checks the value of the DeveloperToolsAvailability policy. |errors| may be
// nullptr.
PolicyCheckResult CheckDeveloperToolsAvailability(
    const base::Value* developer_tools_availability,
    policy::PolicyErrorMap* errors) {
  if (!developer_tools_availability)
    return PolicyCheckResult::kNotSet;

  if (!developer_tools_availability->is_int()) {
    if (errors) {
      errors->AddError(key::kDeveloperToolsAvailability, IDS_POLICY_TYPE_ERROR,
                       base::Value::GetTypeName(base::Value::Type::INTEGER));
    }
    return PolicyCheckResult::kInvalid;
  }

  const int value = developer_tools_availability->GetInt();
  if (!IsValidDeveloperToolsAvailabilityValue(value)) {
    if (errors) {
      errors->AddError(key::kDeveloperToolsAvailability,
                       IDS_POLICY_OUT_OF_RANGE_ERROR,
                       base::NumberToString(value));
    }
    return PolicyCheckResult::kInvalid;
  }
  return PolicyCheckResult::kValid;
}

// Returns the target value of the |kDevToolsAvailability| pref derived only
// from the DeveloperToolsAvailability policy. If this policy is not set or does
// not have a valid value, returns |nullopt|.
base::Optional<Availability> GetValueFromDeveloperToolsAvailabilityPolicy(
    const PolicyMap& policies) {
  const base::Value* developer_tools_availability =
      policies.GetValue(key::kDeveloperToolsAvailability);

  if (CheckDeveloperToolsAvailability(developer_tools_availability,
                                      nullptr /*error*/) !=
      PolicyCheckResult::kValid) {
    return base::nullopt;
  }

  return static_cast<Availability>(developer_tools_availability->GetInt());
}

// Returns the target value of the |kDevToolsAvailability| pref, derived from
// both the DeveloperToolsDisabled policy and the
// DeveloperToolsAvailability policy. If both policies are set,
// DeveloperToolsAvailability wins.
base::Optional<Availability> GetValueFromBothPolicies(
    const PolicyMap& policies) {
  const base::Optional<Availability> developer_tools_availability =
      GetValueFromDeveloperToolsAvailabilityPolicy(policies);

  if (developer_tools_availability.has_value()) {
    // DeveloperToolsAvailability overrides DeveloperToolsDisabled policy.
    return developer_tools_availability;
  }

  return GetValueFromDeveloperToolsDisabledPolicy(policies);
}

}  // namespace

DeveloperToolsPolicyHandler::DeveloperToolsPolicyHandler() {}

DeveloperToolsPolicyHandler::~DeveloperToolsPolicyHandler() {}

bool DeveloperToolsPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  // Deprecated boolean policy DeveloperToolsDisabled.
  const base::Value* developer_tools_disabled =
      policies.GetValue(key::kDeveloperToolsDisabled);
  PolicyCheckResult developer_tools_disabled_result =
      CheckDeveloperToolsDisabled(developer_tools_disabled, errors);

  // Enumerated policy DeveloperToolsAvailability
  const base::Value* developer_tools_availability =
      policies.GetValue(key::kDeveloperToolsAvailability);
  PolicyCheckResult developer_tools_availability_result =
      CheckDeveloperToolsAvailability(developer_tools_availability, errors);

  if (developer_tools_disabled_result == PolicyCheckResult::kValid &&
      developer_tools_availability_result == PolicyCheckResult::kValid) {
    errors->AddError(key::kDeveloperToolsDisabled, IDS_POLICY_OVERRIDDEN,
                     key::kDeveloperToolsAvailability);
  }

  // Always continue to ApplyPolicySettings which can handle invalid policy
  // values.
  return true;
}

void DeveloperToolsPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                      PrefValueMap* prefs) {
  const base::Optional<Availability> value = GetValueFromBothPolicies(policies);

  if (value.has_value()) {
    prefs->SetInteger(prefs::kDevToolsAvailability,
                      static_cast<int>(value.value()));

#if BUILDFLAG(ENABLE_EXTENSIONS)
    if (value.value() == Availability::kDisallowed) {
      // Piggy-back disallowed developer tools to also force-disable
      // kExtensionsUIDeveloperMode.
      prefs->SetValue(prefs::kExtensionsUIDeveloperMode, base::Value(false));
    }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  }
}

// static
void DeveloperToolsPolicyHandler::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // The default for this pref is |kDisallowedForForceInstalledExtensions|, both
  // for managed and for unmanaged users. This is fine for unmanaged users too,
  // because even if they have force-installed extensions (which could happen
  // e.g. through GPO for Chrome on Windows), developer tools should be disabled
  // for these by default.
  registry->RegisterIntegerPref(
      prefs::kDevToolsAvailability,
      static_cast<int>(Availability::kDisallowedForForceInstalledExtensions));
}

// static
DeveloperToolsPolicyHandler::Availability
DeveloperToolsPolicyHandler::GetDevToolsAvailability(
    const PrefService* pref_sevice) {
  int value = pref_sevice->GetInteger(prefs::kDevToolsAvailability);
  if (!IsValidDeveloperToolsAvailabilityValue(value)) {
    // This should never happen, because the |kDevToolsAvailability| pref is
    // only set by DeveloperToolsPolicyHandler which validates the value range.
    // If it is not set, it will have its default value which is also valid, see
    // |RegisterProfilePrefs|.
    NOTREACHED();
    return Availability::kAllowed;
  }

  return static_cast<Availability>(value);
}

// static
bool DeveloperToolsPolicyHandler::IsDevToolsAvailabilitySetByPolicy(
    const PrefService* pref_service) {
  return pref_service->IsManagedPreference(prefs::kDevToolsAvailability);
}

// static
DeveloperToolsPolicyHandler::Availability
DeveloperToolsPolicyHandler::GetMostRestrictiveAvailability(
    Availability availability_1,
    Availability availability_2) {
  if (availability_1 == Availability::kDisallowed ||
      availability_2 == Availability::kDisallowed) {
    return Availability::kDisallowed;
  }
  if (availability_1 == Availability::kDisallowedForForceInstalledExtensions ||
      availability_2 == Availability::kDisallowedForForceInstalledExtensions) {
    return Availability::kDisallowedForForceInstalledExtensions;
  }
  return Availability::kAllowed;
}

}  // namespace policy
