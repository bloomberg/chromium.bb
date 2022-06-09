// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/flags_ui/pref_service_flags_storage.h"

#include "base/logging.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/flags_ui/flags_ui_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace flags_ui {

PrefServiceFlagsStorage::PrefServiceFlagsStorage(PrefService* prefs)
    : prefs_(prefs) {}

PrefServiceFlagsStorage::~PrefServiceFlagsStorage() {}

std::set<std::string> PrefServiceFlagsStorage::GetFlags() const {
  const base::Value* enabled_experiments =
      prefs_->GetList(prefs::kAboutFlagsEntries);
  std::set<std::string> flags;
  for (const auto& entry : enabled_experiments->GetList()) {
    if (!entry.is_string()) {
      LOG(WARNING) << "Invalid entry in " << prefs::kAboutFlagsEntries;
      continue;
    }
    flags.insert(entry.GetString());
  }
  return flags;
}

bool PrefServiceFlagsStorage::SetFlags(const std::set<std::string>& flags) {
  ListPrefUpdate update(prefs_, prefs::kAboutFlagsEntries);
  base::Value* experiments_list = update.Get();
  DCHECK(experiments_list->is_list());

  experiments_list->ClearList();
  for (const auto& flag : flags)
    experiments_list->Append(flag);

  return true;
}

std::string PrefServiceFlagsStorage::GetOriginListFlag(
    const std::string& internal_entry_name) const {
  const base::Value* origin_lists =
      prefs_->GetDictionary(prefs::kAboutFlagsOriginLists);
  if (!origin_lists)
    return std::string();
  if (const std::string* s = origin_lists->FindStringKey(internal_entry_name))
    return *s;
  return std::string();
}

void PrefServiceFlagsStorage::SetOriginListFlag(
    const std::string& internal_entry_name,
    const std::string& origin_list_value) {
  DictionaryPrefUpdate update(prefs_, prefs::kAboutFlagsOriginLists);
  update->SetString(internal_entry_name, origin_list_value);
}

void PrefServiceFlagsStorage::CommitPendingWrites() {
  prefs_->CommitPendingWrite();
}

// static
void PrefServiceFlagsStorage::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kAboutFlagsEntries);
  registry->RegisterDictionaryPref(prefs::kAboutFlagsOriginLists);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// static
void PrefServiceFlagsStorage::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kAboutFlagsEntries);
  registry->RegisterDictionaryPref(prefs::kAboutFlagsOriginLists);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace flags_ui
