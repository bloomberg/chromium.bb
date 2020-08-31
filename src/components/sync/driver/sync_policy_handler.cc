// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/sync_policy_handler.h"

#include <string>

#include "base/optional.h"
#include "base/values.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/base/user_selectable_type.h"

#if defined(OS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif  // defined(OS_CHROMEOS)

namespace syncer {
namespace {

void DisableSyncType(const std::string& type_name, PrefValueMap* prefs) {
  base::Optional<UserSelectableType> type =
      GetUserSelectableTypeFromString(type_name);
  if (type.has_value()) {
    const char* pref = SyncPrefs::GetPrefNameForType(*type);
    if (pref)
      prefs->SetValue(pref, base::Value(false));
  }

#if defined(OS_CHROMEOS)
  if (chromeos::features::IsSplitSettingsSyncEnabled()) {
    // Check for OS types. This includes types that used to be browser types,
    // like "apps" and "preferences".
    base::Optional<UserSelectableOsType> os_type =
        GetUserSelectableOsTypeFromString(type_name);
    if (os_type.has_value()) {
      const char* os_pref = SyncPrefs::GetPrefNameForOsType(*os_type);
      if (os_pref)
        prefs->SetValue(os_pref, base::Value(false));
    }
  }
#endif  // defined(OS_CHROMEOS)
}

}  // namespace

SyncPolicyHandler::SyncPolicyHandler()
    : policy::TypeCheckingPolicyHandler(policy::key::kSyncDisabled,
                                        base::Value::Type::BOOLEAN) {}

SyncPolicyHandler::~SyncPolicyHandler() = default;

void SyncPolicyHandler::ApplyPolicySettings(const policy::PolicyMap& policies,
                                            PrefValueMap* prefs) {
  const base::Value* disable_sync_value = policies.GetValue(policy_name());
  bool disable_sync;
  if (disable_sync_value && disable_sync_value->GetAsBoolean(&disable_sync) &&
      disable_sync) {
    prefs->SetValue(prefs::kSyncManaged, disable_sync_value->Clone());
  }

  const base::Value* disabled_sync_types_value =
      policies.GetValue(policy::key::kSyncTypesListDisabled);

  if (disabled_sync_types_value && disabled_sync_types_value->is_list()) {
    auto list = disabled_sync_types_value->GetList();
    for (const base::Value& type_name : list) {
      if (!type_name.is_string())
        continue;
      DisableSyncType(type_name.GetString(), prefs);
    }
  }
}

}  // namespace syncer
