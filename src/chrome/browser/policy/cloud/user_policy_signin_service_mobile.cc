// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/user_policy_signin_service_mobile.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/account_id_from_account_info.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/cloud/user_policy_signin_service_util.h"
#include "components/policy/core/common/cloud/cloud_policy_client_registration_helper.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/network_change_notifier.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace em = enterprise_management;

namespace policy {

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
      profile_prefs_(profile->GetPrefs()),
      profile_(profile) {
  if (g_browser_process->profile_manager())
    g_browser_process->profile_manager()->AddObserver(this);
}

UserPolicySigninService::~UserPolicySigninService() {}

void UserPolicySigninService::ShutdownUserCloudPolicyManager() {
  CancelPendingRegistration();
  UserPolicySigninServiceBase::ShutdownUserCloudPolicyManager();
}

void UserPolicySigninService::Shutdown() {
  // Don't handle ProfileManager when testing because it is null.
  if (g_browser_process->profile_manager())
    g_browser_process->profile_manager()->RemoveObserver(this);
  if (identity_manager())
    identity_manager()->RemoveObserver(this);
  CancelPendingRegistration();
  UserPolicySigninServiceBase::Shutdown();
}

void UserPolicySigninService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (profile_manager && IsSignoutEvent(event)) {
    UpdateProfileAttributesWhenSignout(profile_, profile_manager);
    ShutdownUserCloudPolicyManager();
  } else if (IsTurnOffSyncEvent(event)) {
    ShutdownUserCloudPolicyManager();
  }
}

void UserPolicySigninService::OnProfileAdded(Profile* profile) {
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

base::TimeDelta UserPolicySigninService::GetTryRegistrationDelay() {
  net::NetworkChangeNotifier::ConnectionType connection_type =
      net::NetworkChangeNotifier::GetConnectionType();
  base::TimeDelta retry_delay = base::Days(3);
  if (connection_type == net::NetworkChangeNotifier::CONNECTION_ETHERNET ||
      connection_type == net::NetworkChangeNotifier::CONNECTION_WIFI) {
    retry_delay = base::Days(1);
  }

  base::Time last_check_time = base::Time::FromInternalValue(
      profile_prefs_->GetInt64(policy_prefs::kLastPolicyCheckTime));
  base::Time next_check_time = last_check_time + retry_delay;

  // Check immediately if no check was ever done before (last_check_time == 0),
  // or if the last check was in the future (?), or if we're already past the
  // next check time. Otherwise, delay checking until the next check time.
  base::Time now = base::Time::Now();
  base::TimeDelta try_registration_delay = base::Seconds(5);
  if (now > last_check_time && now < next_check_time)
    try_registration_delay = next_check_time - now;

  return try_registration_delay;
}

void UserPolicySigninService::UpdateLastPolicyCheckTime() {
  // Persist the current time as the last policy registration attempt time.
  profile_->GetPrefs()->SetInt64(policy_prefs::kLastPolicyCheckTime,
                                 base::Time::Now().ToInternalValue());
}

signin::ConsentLevel UserPolicySigninService::GetConsentLevelForRegistration() {
  return signin::ConsentLevel::kSync;
}

}  // namespace policy
