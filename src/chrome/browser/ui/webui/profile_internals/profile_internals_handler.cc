// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/profile_internals/profile_internals_handler.h"

#include "base/bind.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/json/values_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/browser/web_ui.h"

namespace {

base::Value CreateProfileEntry(
    const ProfileAttributesEntry* entry,
    const base::flat_set<base::FilePath>& loaded_profile_paths,
    const base::flat_set<base::FilePath>& has_off_the_record_profile) {
  base::Value profile_entry(base::Value::Type::DICTIONARY);
  profile_entry.SetKey("profilePath", base::FilePathToValue(entry->GetPath()));
  profile_entry.SetStringKey("localProfileName", entry->GetLocalProfileName());
  std::string signin_state;
  switch (entry->GetSigninState()) {
    case SigninState::kNotSignedIn:
      signin_state = "Not signed in";
      break;
    case SigninState::kSignedInWithUnconsentedPrimaryAccount:
      signin_state = "Signed in with unconsented primary account";
      break;
    case SigninState::kSignedInWithConsentedPrimaryAccount:
      signin_state = "Signed in with consented primary account";
      break;
  }
  profile_entry.SetStringKey("signinState", signin_state);
  profile_entry.SetBoolKey("signinRequired", entry->IsSigninRequired());
  // GAIA full name/user name can be empty, if the profile is not signed in to
  // chrome.
  profile_entry.SetStringKey("gaiaName", entry->GetGAIAName());
  profile_entry.SetStringKey("gaiaId", entry->GetGAIAId());
  profile_entry.SetStringKey("userName", entry->GetUserName());
  profile_entry.SetStringKey("hostedDomain", entry->GetHostedDomain());
  profile_entry.SetBoolKey("isSupervised", entry->IsSupervised());
  profile_entry.SetBoolKey("isOmitted", entry->IsOmitted());
  profile_entry.SetBoolKey("isEphemeral", entry->IsEphemeral());
  profile_entry.SetBoolKey("userAcceptedAccountManagement",
                           entry->UserAcceptedAccountManagement());

  base::Value keep_alives(base::Value::Type::LIST);
  std::map<ProfileKeepAliveOrigin, int> keep_alives_map =
      g_browser_process->profile_manager()->GetKeepAlivesByPath(
          entry->GetPath());
  for (const auto& pair : keep_alives_map) {
    if (pair.second != 0) {
      std::stringstream ss;
      ss << pair.first;
      base::Value keep_alive_pair(base::Value::Type::DICTIONARY);
      keep_alive_pair.SetStringKey("origin", ss.str());
      keep_alive_pair.SetIntKey("count", pair.second);
      keep_alives.Append(std::move(keep_alive_pair));
    }
  }
  profile_entry.SetKey("keepAlives", std::move(keep_alives));

  base::Value signedAccounts(base::Value::Type::LIST);
  for (const std::string& gaiaId : entry->GetGaiaIds()) {
    signedAccounts.Append(gaiaId);
  }
  profile_entry.SetKey("signedAccounts", std::move(signedAccounts));
  profile_entry.SetBoolKey("isLoaded",
                           loaded_profile_paths.contains(entry->GetPath()));
  profile_entry.SetBoolKey(
      "hasOffTheRecord", has_off_the_record_profile.contains(entry->GetPath()));
  return profile_entry;
}

}  // namespace

ProfileInternalsHandler::ProfileInternalsHandler() = default;

ProfileInternalsHandler::~ProfileInternalsHandler() = default;

void ProfileInternalsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getProfilesList",
      base::BindRepeating(&ProfileInternalsHandler::HandleGetProfilesList,
                          base::Unretained(this)));
}

void ProfileInternalsHandler::HandleGetProfilesList(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(0u, args.size());
  PushProfilesList();
}

void ProfileInternalsHandler::PushProfilesList() {
  DCHECK(IsJavascriptAllowed());
  FireWebUIListener("profiles-list-changed", GetProfilesList());
}

base::Value ProfileInternalsHandler::GetProfilesList() {
  base::Value profiles_list(base::Value::Type::LIST);
  std::vector<ProfileAttributesEntry*> entries =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetAllProfilesAttributesSortedByLocalProfileName();
  std::vector<Profile*> loaded_profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  base::flat_set<base::FilePath> loaded_profile_paths =
      base::MakeFlatSet<base::FilePath>(
          loaded_profiles, {}, [](const auto& it) { return it->GetPath(); });
  base::flat_set<base::FilePath> has_off_the_record_profile;
  for (Profile* profile : loaded_profiles) {
    if (profile->GetAllOffTheRecordProfiles().size() > 0) {
      has_off_the_record_profile.insert(profile->GetPath());
    }
  }
  for (const ProfileAttributesEntry* entry : entries) {
    profiles_list.Append(CreateProfileEntry(entry, loaded_profile_paths,
                                            has_off_the_record_profile));
  }
  return profiles_list;
}
