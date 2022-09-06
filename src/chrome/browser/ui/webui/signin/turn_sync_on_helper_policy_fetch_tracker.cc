// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/turn_sync_on_helper_policy_fetch_tracker.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/policy/chrome_policy_conversions_client.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/signin/account_id_from_account_info.h"
#include "components/policy/core/browser/policy_conversions.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "content/public/browser/storage_partition.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "components/policy/core/common/policy_loader_lacros.h"
#endif

namespace {
class PolicyFetchTracker
    : public TurnSyncOnHelperPolicyFetchTracker,
      public policy::PolicyService::ProviderUpdateObserver {
 public:
  PolicyFetchTracker(Profile* profile, const AccountInfo& account_info)
      : profile_(profile), account_info_(account_info) {}
  ~PolicyFetchTracker() override = default;

  void SwitchToProfile(Profile* new_profile) override {
    profile_ = new_profile;
  }

  void RegisterForPolicy(base::OnceCallback<void(bool)> callback) override {
    policy::UserPolicySigninService* policy_service =
        policy::UserPolicySigninServiceFactory::GetForProfile(profile_);
    policy_service->RegisterForPolicyWithAccountId(
        account_info_.email, account_info_.account_id,
        base::BindOnce(&PolicyFetchTracker::OnRegisteredForPolicy,
                       weak_pointer_factory_.GetWeakPtr(),
                       std::move(callback)));
  }

  bool FetchPolicy(base::OnceClosure callback) override {
    if (dm_token_.empty() || client_id_.empty()) {
      std::move(callback).Run();
      return false;
    }

    policy::UserPolicySigninService* policy_service =
        policy::UserPolicySigninServiceFactory::GetForProfile(profile_);
    policy_service->FetchPolicyForSignedInUser(
        AccountIdFromAccountInfo(account_info_), dm_token_, client_id_,
        profile_->GetDefaultStoragePartition()
            ->GetURLLoaderFactoryForBrowserProcess(),
        base::BindOnce(&PolicyFetchTracker::OnPolicyFetchComplete,
                       weak_pointer_factory_.GetWeakPtr(),
                       std::move(callback)));
    return true;
  }

  // policy::PolicyService::ProviderUpdateObserver
  void OnProviderUpdatePropagated(
      policy::ConfigurationPolicyProvider* provider) override {
    if (provider != profile_->GetUserCloudPolicyManager())
      return;
    VLOG(2) << "Policies after sign in:";
    VLOG(2) << policy::DictionaryPolicyConversions(
                   std::make_unique<policy::ChromePolicyConversionsClient>(
                       profile_))
                   .ToJSON();
    profile_->GetProfilePolicyConnector()
        ->policy_service()
        ->RemoveProviderUpdateObserver(this);
  }

 private:
  void OnRegisteredForPolicy(base::OnceCallback<void(bool)> callback,
                             const std::string& dm_token,
                             const std::string& client_id) {
    // Indicates that the account isn't managed OR there is an error during the
    // registration
    if (dm_token.empty()) {
      std::move(callback).Run(/*is_managed_account=*/false);
      return;
    }

    DVLOG(1) << "Policy registration succeeded: dm_token=" << dm_token;

    DCHECK(dm_token_.empty());
    DCHECK(client_id_.empty());
    dm_token_ = dm_token;
    client_id_ = client_id;
    std::move(callback).Run(/*is_managed_account=*/true);
  }

  void OnPolicyFetchComplete(base::OnceClosure callback, bool success) {
    // For now, we allow signin to complete even if the policy fetch fails. If
    // we ever want to change this behavior, we could call
    // PrimaryAccountMutator::ClearPrimaryAccount() here instead.
    DLOG_IF(ERROR, !success) << "Error fetching policy for user";
    DVLOG_IF(1, success) << "Policy fetch successful - completing signin";
    if (VLOG_IS_ON(2)) {
      // User cloud policies have been fetched from the server. Dump all policy
      // values into log once these new policies are merged.
      profile_->GetProfilePolicyConnector()
          ->policy_service()
          ->AddProviderUpdateObserver(this);
    }
    std::move(callback).Run();
  }

  raw_ptr<Profile> profile_;
  const AccountInfo account_info_;

  // Policy credentials we keep while determining whether to create
  // a new profile for an enterprise user or not.
  std::string dm_token_;
  std::string client_id_;

  base::WeakPtrFactory<PolicyFetchTracker> weak_pointer_factory_{this};
};

#if BUILDFLAG(IS_CHROMEOS_LACROS)
class LacrosPrimaryProfilePolicyFetchTracker
    : public TurnSyncOnHelperPolicyFetchTracker {
 public:
  ~LacrosPrimaryProfilePolicyFetchTracker() override = default;

  void SwitchToProfile(Profile* new_profile) override {
    // Sign in intercept and syncing with a different account are not supported
    // use cases for the Lacros primary profile.
    NOTREACHED();
  }

  void RegisterForPolicy(
      base::OnceCallback<void(bool is_managed)> registered_callback) override {
    // Policies for the Lacros main profile are provided by Ash on start, so
    // there is no need to register to anything to fetch them. See
    // crsrc.org/c/chromeos/crosapi/mojom/crosapi.mojom?q=device_account_policy.
    std::move(registered_callback).Run(IsManagedProfile());
  }

  bool FetchPolicy(base::OnceClosure callback) override {
    // Policies are populated via Ash at Lacros startup time, nothing to do.
    std::move(callback).Run();
    return IsManagedProfile();
  }

 private:
  bool IsManagedProfile() {
    const enterprise_management::PolicyData* policy =
        policy::PolicyLoaderLacros::main_user_policy_data();
    return policy && policy->has_managed_by();
  }
};
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}  // namespace

std::unique_ptr<TurnSyncOnHelperPolicyFetchTracker>
TurnSyncOnHelperPolicyFetchTracker::CreateInstance(
    Profile* profile,
    const AccountInfo& account_info) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (profile->IsMainProfile()) {
    return std::make_unique<LacrosPrimaryProfilePolicyFetchTracker>();
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  return std::make_unique<PolicyFetchTracker>(profile, account_info);
}
