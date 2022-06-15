// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_CLOUD_USER_POLICY_SIGNIN_SERVICE_BASE_H_
#define COMPONENTS_POLICY_CORE_BROWSER_CLOUD_USER_POLICY_SIGNIN_SERVICE_BASE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/core_account_id.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

class AccountId;
class PrefService;

namespace policy {

class DeviceManagementService;
class UserCloudPolicyManager;
class CloudPolicyClientRegistrationHelper;
class CloudPolicyClient;

// The UserPolicySigninService is responsible for interacting with the policy
// infrastructure (mainly UserCloudPolicyManager) to load policy for the signed
// in user. This is the base class that contains shared behavior.
//
// At signin time, this class initializes the UserCloudPolicyManager and loads
// policy before any other signed in services are initialized. After each
// restart, this class ensures that the CloudPolicyClient is registered (in case
// the policy server was offline during the initial policy fetch) and if not it
// initiates a fresh registration process.
//
// Finally, if the user signs out, this class is responsible for shutting down
// the policy infrastructure to ensure that any cached policy is cleared.
class POLICY_EXPORT UserPolicySigninServiceBase
    : public KeyedService,
      public CloudPolicyClient::Observer,
      public CloudPolicyService::Observer {
 public:
  // The callback invoked once policy registration is complete. Passed
  // |dm_token| and |client_id| parameters are empty if policy registration
  // failed.
  typedef base::OnceCallback<void(const std::string& dm_token,
                                  const std::string& client_id)>
      PolicyRegistrationCallback;

  // The callback invoked once policy fetch is complete. Passed boolean
  // parameter is set to true if the policy fetch succeeded.
  typedef base::OnceCallback<void(bool)> PolicyFetchCallback;

  // Creates a UserPolicySigninServiceBase associated with the passed |profile|.
  UserPolicySigninServiceBase(
      PrefService* local_state,
      DeviceManagementService* device_management_service,
      UserCloudPolicyManager* policy_manager,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory);
  UserPolicySigninServiceBase(const UserPolicySigninServiceBase&) = delete;
  UserPolicySigninServiceBase& operator=(const UserPolicySigninServiceBase&) =
      delete;
  ~UserPolicySigninServiceBase() override;

  // Initiates a policy fetch as part of user signin, using a |dm_token| and
  // |client_id| fetched via RegisterForPolicyXXX(). |callback| is invoked
  // once the policy fetch is complete, passing true if the policy fetch
  // succeeded.
  // Virtual for testing.
  virtual void FetchPolicyForSignedInUser(
      const AccountId& account_id,
      const std::string& dm_token,
      const std::string& client_id,
      scoped_refptr<network::SharedURLLoaderFactory> profile_url_loader_factory,
      PolicyFetchCallback callback);

  // CloudPolicyService::Observer implementation:
  void OnCloudPolicyServiceInitializationCompleted() override;

  // CloudPolicyClient::Observer implementation:
  void OnPolicyFetched(CloudPolicyClient* client) override;
  void OnRegistrationStateChanged(CloudPolicyClient* client) override;
  void OnClientError(CloudPolicyClient* client) override;

  // KeyedService implementation:
  void Shutdown() override;

  // Registers to DM Server to get a DM token to fetch user policies for that
  // account.
  //
  // Registration goes through the following steps:
  //   1) Request an OAuth2 access token to let the account access the service.
  //   2) Fetch the user info tied to the access token.
  //   3) Register the account to DMServer using the access token and user info
  //      to get a DM token that allows fetching user policies for the
  //      registered account.
  //   4) Invoke |callback| when the DM token is available. Will pass an empty
  //      token when the account isn't managed OR there is an error during the
  //      registration.
  virtual void RegisterForPolicyWithAccountId(
      const std::string& username,
      const CoreAccountId& account_id,
      PolicyRegistrationCallback callback);

 protected:
  // Invoked to initialize the cloud policy service for |account_id|, which is
  // the account associated with the Profile that owns this service. This is
  // invoked from InitializeOnProfileReady() if the Profile already has a
  // signed-in account at startup, or (on the desktop platforms) as soon as the
  // user signs-in and an OAuth2 login refresh token becomes available.
  void InitializeForSignedInUser(const AccountId& account_id,
                                 scoped_refptr<network::SharedURLLoaderFactory>
                                     profile_url_loader_factory);

  // Initializes the cloud policy manager with the passed |client|. This is
  // called from InitializeForSignedInUser() when the Profile already has a
  // signed in account at startup, and from FetchPolicyForSignedInUser() during
  // the initial policy fetch after signing in.
  virtual void InitializeUserCloudPolicyManager(
      const AccountId& account_id,
      std::unique_ptr<CloudPolicyClient> client);

  // Prepares for the UserCloudPolicyManager to be shutdown due to
  // user signout or profile destruction.
  virtual void PrepareForUserCloudPolicyManagerShutdown();

  // Shuts down the UserCloudPolicyManager (for example, after the user signs
  // out) and deletes any cached policy.
  virtual void ShutdownUserCloudPolicyManager();

  // Updates the timestamp of the last policy check. Implemented on mobile
  // platforms for network efficiency.
  virtual void UpdateLastPolicyCheckTime();

  // Gets the sign-in consent level required to perform registration.
  virtual signin::ConsentLevel GetConsentLevelForRegistration();

  // Gets the delay before the next registration.
  virtual base::TimeDelta GetTryRegistrationDelay();

  // Prohibits signout if needed when the account is registered for cloud policy
  // . Might be no-op for some platforms (eg., iOS and Android).
  virtual void ProhibitSignoutIfNeeded();

  // Returns true when policies can be applied for the profile. The profile has
  // to be at least tied to an account.
  virtual bool CanApplyPolicies(bool check_for_refresh_token);

  // Cancels the pending task that does registration. This invalidates the
  // |weak_factory_for_registration_| weak pointers used for registration.
  void CancelPendingRegistration();

  // Convenience helpers to get the associated UserCloudPolicyManager and
  // IdentityManager.
  UserCloudPolicyManager* policy_manager() { return policy_manager_; }
  signin::IdentityManager* identity_manager() { return identity_manager_; }

  signin::ConsentLevel consent_level() const { return consent_level_; }

  scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory() {
    return system_url_loader_factory_;
  }

 private:
  // Returns a CloudPolicyClient to perform a registration with the DM server,
  // or NULL if |username| shouldn't register for policy management.
  std::unique_ptr<CloudPolicyClient> CreateClientForRegistrationOnly(
      const std::string& username);

  // Returns false if cloud policy is disabled or if the passed |email_address|
  // is definitely not from a hosted domain (according to the list in
  // BrowserPolicyConnector::IsNonEnterpriseUser()).
  bool ShouldLoadPolicyForUser(const std::string& email_address);

  // Handler to call the policy registration callback that provides the DM
  // token.
  void CallPolicyRegistrationCallback(std::unique_ptr<CloudPolicyClient> client,
                                      PolicyRegistrationCallback callback);

  // Fetches an OAuth token to allow the cloud policy service to register with
  // the cloud policy server. |oauth_login_token| should contain an OAuth login
  // refresh token that can be downscoped to get an access token for the
  // device_management service.
  void RegisterCloudPolicyService();

  // Callback invoked when policy registration has finished.
  void OnRegistrationComplete();

  // Weak pointer to the UserCloudPolicyManager and IdentityManager this service
  // is associated with.
  raw_ptr<UserCloudPolicyManager> policy_manager_;
  raw_ptr<signin::IdentityManager> identity_manager_;

  raw_ptr<PrefService> local_state_;
  raw_ptr<DeviceManagementService> device_management_service_;
  scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory_;

  signin::ConsentLevel consent_level_ = signin::ConsentLevel::kSignin;

  // Helper for registering the client to DMServer to get a DM token using a
  // cloud policy client. When there is an instance of |registration_helper_|,
  // it means that registration is ongoing. There is no registration when null.
  std::unique_ptr<CloudPolicyClientRegistrationHelper> registration_helper_;

  base::WeakPtrFactory<UserPolicySigninServiceBase> weak_factory_{this};

  // Weak pointer factory used for registration.
  base::WeakPtrFactory<UserPolicySigninServiceBase>
      weak_factory_for_registration_{this};
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_CLOUD_USER_POLICY_SIGNIN_SERVICE_BASE_H_
