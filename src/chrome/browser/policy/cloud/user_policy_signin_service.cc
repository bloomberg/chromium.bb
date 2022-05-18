// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/user_policy_signin_service.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_internal.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/account_id_from_account_info.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/cloud/user_policy_signin_service_util.h"
#include "components/policy/core/common/cloud/cloud_policy_client_registration_helper.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_constants.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace policy {
namespace internal {
bool g_force_prohibit_signout_for_tests = false;
}

ProfileManagerObserverBridge::ProfileManagerObserverBridge(
    UserPolicySigninService* user_policy_signin_service)
    : user_policy_signin_service_(user_policy_signin_service) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (profile_manager)
    profile_manager->AddObserver(this);
}

void ProfileManagerObserverBridge::OnProfileAdded(Profile* profile) {
  user_policy_signin_service_->OnProfileReady(profile);
}

ProfileManagerObserverBridge::~ProfileManagerObserverBridge() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (profile_manager)
    profile_manager->RemoveObserver(this);
}

UserPolicySigninService::UserPolicySigninService(
    Profile* profile,
    PrefService* local_state,
    DeviceManagementService* device_management_service,
    UserCloudPolicyManager* policy_manager,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory)
    : UserPolicySigninServiceBase(local_state,
                                  device_management_service,
                                  policy_manager,
                                  identity_manager,
                                  system_url_loader_factory),
      profile_(profile) {
  // IdentityManager should not yet have loaded its tokens since this
  // happens in the background after PKS initialization - so this service
  // should always be created before the oauth token is available.
  DCHECK(!CanApplyPolicies(/*check_for_refresh_token=*/true));
  // Some tests don't have a profile manager.
  if (g_browser_process->profile_manager()) {
    observed_profile_.Observe(
        &g_browser_process->profile_manager()->GetProfileAttributesStorage());
  }
}

UserPolicySigninService::~UserPolicySigninService() {
}

void UserPolicySigninService::PrepareForUserCloudPolicyManagerShutdown() {
  // Stop any pending registration helper activity. We do this here instead of
  // in the destructor because we want to shutdown the registration helper
  // before UserCloudPolicyManager shuts down the CloudPolicyClient.
  registration_helper_.reset();

  UserPolicySigninServiceBase::PrepareForUserCloudPolicyManagerShutdown();
}

void UserPolicySigninService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (profile_manager && IsSignoutEvent(event)) {
    UpdateProfileAttributesWhenSignout(profile_, profile_manager);
    ShutdownUserCloudPolicyManager();
  } else if (IsTurnOffSyncEvent(event) &&
             !CanApplyPolicies(/*check_for_refresh_token=*/true)) {
    ShutdownUserCloudPolicyManager();
  }

  if (!IsAnySigninEvent(event))
    return;

  DCHECK(identity_manager()->HasPrimaryAccount(consent_level()));
  if (!CanApplyPolicies(/*check_for_refresh_token=*/true))
    return;

  // IdentityManager has a refresh token for the primary account, so initialize
  // the UserCloudPolicyManager.
  TryInitializeForSignedInUser();
}

void UserPolicySigninService::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  // Ignore OAuth tokens or those for any account but the primary one.
  if (account_info.account_id !=
          identity_manager()->GetPrimaryAccountId(consent_level()) ||
      !CanApplyPolicies(/*check_for_refresh_token=*/true)) {
    return;
  }

  // ProfileOAuth2TokenService now has a refresh token for the primary account
  // so initialize the UserCloudPolicyManager.
  TryInitializeForSignedInUser();
}

void UserPolicySigninService::TryInitializeForSignedInUser() {
  DCHECK(CanApplyPolicies(/*check_for_refresh_token=*/true));

  // If using a TestingProfile with no UserCloudPolicyManager, skip
  // initialization.
  if (!policy_manager()) {
    DVLOG(1) << "Skipping initialization for tests due to missing components.";
    return;
  }

  observed_profile_.Reset();

  InitializeForSignedInUser(
      AccountIdFromAccountInfo(
          identity_manager()->GetPrimaryAccountInfo(consent_level())),
      profile_->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess());
}

void UserPolicySigninService::InitializeUserCloudPolicyManager(
    const AccountId& account_id,
    std::unique_ptr<CloudPolicyClient> client) {
  UserPolicySigninServiceBase::InitializeUserCloudPolicyManager(
      account_id, std::move(client));
  ProhibitSignoutIfNeeded();
}

void UserPolicySigninService::Shutdown() {
  if (identity_manager())
    identity_manager()->RemoveObserver(this);
  UserPolicySigninServiceBase::Shutdown();
}

void UserPolicySigninService::ShutdownUserCloudPolicyManager() {
  UserCloudPolicyManager* manager = policy_manager();
  // Allow the user to signout again.
  if (manager)
    signin_util::SetUserSignoutAllowedForProfile(profile_, true);

  UserPolicySigninServiceBase::ShutdownUserCloudPolicyManager();
}

void UserPolicySigninService::OnProfileUserManagementAcceptanceChanged(
    const base::FilePath& profile_path) {
  if (CanApplyPolicies(/*check_for_refresh_token=*/true))
    TryInitializeForSignedInUser();
}

void UserPolicySigninService::ProhibitSignoutIfNeeded() {
  if (policy_manager()->IsClientRegistered() ||
      internal::g_force_prohibit_signout_for_tests) {
    DVLOG(1) << "User is registered for policy - prohibiting signout";
    signin_util::SetUserSignoutAllowedForProfile(profile_, false);
  }
}

void UserPolicySigninService::OnProfileReady(Profile* profile) {
  if (profile && profile == profile_)
    InitializeOnProfileReady(profile);
}

void UserPolicySigninService::InitializeOnProfileReady(Profile* profile) {
  DCHECK_EQ(profile, profile_);

  // If using a TestingProfile with no IdentityManager or
  // UserCloudPolicyManager, skip initialization.
  if (!policy_manager() || !identity_manager()) {
    DVLOG(1) << "Skipping initialization for tests due to missing components.";
    return;
  }

  // Shutdown the UserCloudPolicyManager when the user signs out. We start
  // observing the IdentityManager here because we don't want to get signout
  // notifications until after the profile has started initializing
  // (http://crbug.com/316229).
  identity_manager()->AddObserver(this);

  AccountId account_id = AccountIdFromAccountInfo(
      identity_manager()->GetPrimaryAccountInfo(consent_level()));
  if (!CanApplyPolicies(/*check_for_refresh_token=*/false)) {
    ShutdownUserCloudPolicyManager();
  } else {
    InitializeForSignedInUser(account_id,
                              profile->GetDefaultStoragePartition()
                                  ->GetURLLoaderFactoryForBrowserProcess());
  }
}

bool UserPolicySigninService::CanApplyPolicies(bool check_for_refresh_token) {
  if (!CanApplyPoliciesForSignedInUser(check_for_refresh_token, consent_level(),
                                       identity_manager())) {
    return false;
  }

  return (profile_can_be_managed_for_testing_ ||
          chrome::enterprise_util::ProfileCanBeManaged(profile_));
}

}  // namespace policy
