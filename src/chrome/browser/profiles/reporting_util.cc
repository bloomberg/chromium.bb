// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/reporting_util.h"

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/account_id/account_id.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/affiliation.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "components/policy/core/common/policy_loader_lacros.h"
#endif

namespace {

// Returns policy for the given |profile|. If failed to get policy returns
// nullptr.
const enterprise_management::PolicyData* GetPolicyData(Profile* profile) {
  if (!profile)
    return nullptr;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // TODO(crbug.com/1254373): Clean up for Dent V2
  if (profile->IsMainProfile()) {
    const enterprise_management::PolicyData* policy =
        policy::PolicyLoaderLacros::main_user_policy_data();
    if (policy)
      return policy;
  }
#endif

  auto* manager =
#if BUILDFLAG(IS_CHROMEOS_ASH)
      profile->GetUserCloudPolicyManagerAsh();
#else
      profile->GetUserCloudPolicyManager();
#endif
  if (!manager)
    return nullptr;

  policy::CloudPolicyStore* store = manager->core()->store();
  if (!store || !store->has_policy())
    return nullptr;

  return store->policy();
}

// Returns User DMToken for a given |profile| if:
// * |profile| is NOT incognito profile.
// * |profile| is NOT sign-in screen profile
// * user corresponding to a |profile| is managed.
// Otherwise returns empty string. More about DMToken:
// go/dmserver-domain-model#dmtoken.
std::string GetUserDmToken(Profile* profile) {
  if (!profile)
    return std::string();

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (profile->IsMainProfile()) {
    const enterprise_management::PolicyData* policy =
        policy::PolicyLoaderLacros::main_user_policy_data();
    if (policy)
      return policy->request_token();
  }
#endif

  const enterprise_management::PolicyData* policy = GetPolicyData(profile);
  if (!policy || !policy->has_request_token())
    return std::string();

  return policy->request_token();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// A callback which fetches device dm_token based on user affiliation.
using DeviceDMTokenCallback = base::RepeatingCallback<std::string(
    const std::vector<std::string>& user_affiliation_ids)>;

// Returns the Device DMToken for the given |profile| if:
// * |profile| is NOT incognito profile
// * user corresponding to a given |profile| is affiliated.
// Otherwise returns empty string. More about DMToken:
// go/dmserver-domain-model#dmtoken.
std::string GetDeviceDmToken(Profile* profile) {
  if (!profile)
    return std::string();

  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user)
    return std::string();

  DeviceDMTokenCallback device_dm_token_callback =
      ash::GetDeviceDMTokenForUserPolicyGetter(user->GetAccountId());
  if (!device_dm_token_callback)
    return std::string();

  const enterprise_management::PolicyData* policy = GetPolicyData(profile);
  if (!policy)
    return std::string();

  std::vector<std::string> user_affiliation_ids(
      policy->user_affiliation_ids().begin(),
      policy->user_affiliation_ids().end());
  return device_dm_token_callback.Run(user_affiliation_ids);
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

namespace reporting {

base::Value GetContext(Profile* profile) {
  base::Value context(base::Value::Type::DICTIONARY);
  context.SetStringPath("browser.userAgent", embedder_support::GetUserAgent());

  if (!profile)
    return context;

  ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();
  ProfileAttributesEntry* entry =
      storage.GetProfileAttributesWithPath(profile->GetPath());
  if (entry) {
    context.SetStringPath("profile.profileName", entry->GetName());
    context.SetStringPath("profile.gaiaEmail", entry->GetUserName());
  }

  context.SetStringPath("profile.profilePath",
                        profile->GetPath().AsUTF8Unsafe());

  const enterprise_management::PolicyData* policy = GetPolicyData(profile);

  if (policy) {
    if (policy->has_device_id())
      context.SetStringPath("profile.clientId", policy->device_id());

#if BUILDFLAG(IS_CHROMEOS_ASH)
    std::string device_dm_token = GetDeviceDmToken(profile);
    if (!device_dm_token.empty())
      context.SetStringPath("device.dmToken", device_dm_token);
#endif

    std::string user_dm_token = GetUserDmToken(profile);
    if (!user_dm_token.empty())
      context.SetStringPath("profile.dmToken", user_dm_token);
  }

  return context;
}

enterprise_connectors::ClientMetadata GetContextAsClientMetadata(
    Profile* profile) {
  enterprise_connectors::ClientMetadata metadata;
  metadata.mutable_browser()->set_user_agent(embedder_support::GetUserAgent());

  if (!profile)
    return metadata;

  ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();
  ProfileAttributesEntry* entry =
      storage.GetProfileAttributesWithPath(profile->GetPath());
  if (entry) {
    metadata.mutable_profile()->set_profile_name(
        base::UTF16ToUTF8(entry->GetName()));
    metadata.mutable_profile()->set_gaia_email(
        base::UTF16ToUTF8(entry->GetUserName()));
  }

  metadata.mutable_profile()->set_profile_path(
      profile->GetPath().AsUTF8Unsafe());

  const enterprise_management::PolicyData* policy = GetPolicyData(profile);

  if (policy) {
    if (policy->has_device_id())
      metadata.mutable_profile()->set_client_id(policy->device_id());

#if BUILDFLAG(IS_CHROMEOS_ASH)
    std::string device_dm_token = GetDeviceDmToken(profile);
    if (!device_dm_token.empty())
      metadata.mutable_device()->set_dm_token(device_dm_token);
#endif

    std::string user_dm_token = GetUserDmToken(profile);
    if (!user_dm_token.empty())
      metadata.mutable_profile()->set_dm_token(user_dm_token);
  }

  return metadata;
}

}  // namespace reporting
