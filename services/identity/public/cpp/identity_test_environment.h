// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_IDENTITY_PUBLIC_CPP_IDENTITY_TEST_ENVIRONMENT_H_
#define SERVICES_IDENTITY_PUBLIC_CPP_IDENTITY_TEST_ENVIRONMENT_H_

#include "base/optional.h"
#include "components/signin/core/browser/account_consistency_method.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/identity/public/cpp/identity_test_utils.h"

class AccountFetcherService;
class AccountTrackerService;
class FakeProfileOAuth2TokenService;
class GaiaCookieManagerService;
class IdentityTestEnvironmentChromeBrowserStateAdaptor;
class IdentityTestEnvironmentProfileAdaptor;
class PrefService;
class SigninManager;
class TestSigninClient;

namespace sync_preferences {
class TestingPrefServiceSyncable;
}

namespace network {
class TestURLLoaderFactory;
}

namespace identity {

class IdentityManagerDependenciesOwner;
class TestIdentityManagerObserver;

// Necessary to declare this test as a temporary friend.
class IdentityAccessorImplTest;

// Class that creates an IdentityManager for use in testing contexts and
// provides facilities for driving that IdentityManager. The IdentityManager
// instance is brought up in an environment where the primary account is
// not available; call MakePrimaryAccountAvailable() as needed.
// NOTE: IdentityTestEnvironment requires that tests have a properly set up
// task environment. If your test doesn't already have one, use a
// base::test::ScopedTaskEnvironment instance variable to fulfill this
// requirement.
class IdentityTestEnvironment : public IdentityManager::DiagnosticsObserver {
 public:
  // Preferred constructor: constructs an IdentityManager object and its
  // dependencies internally. Cannot be used if the client of this class
  // is still interacting directly with those dependencies (e.g., if
  // IdentityTestEnvironment is being introduced to incrementally convert
  // a test). In that case, use the below constructor and switch to this
  // constructor once the conversion is complete.
  //
  // This constructor takes an optional parameter |test_url_loader_factory| to
  // use for cookie-related network requests.
  // Note: the provided |test_url_loader_factory| is expected to outlive
  // IdentityTestEnvironment.
  //
  // This constructor also takes an optional PrefService instance as parameter,
  // which allows tests to move away from referencing IdentityManager's
  // dependencies directly (namely AccountTrackerService, PO2TS, SigninManager
  // and GaiaCookieManagerService), but still be able to tweak preferences on
  // demand.
  //
  // Last, this constructor can take an optional parameter |account_consistency|
  // as parameter, to specify the account consistency policy that will be used.
  IdentityTestEnvironment(
      network::TestURLLoaderFactory* test_url_loader_factory = nullptr,
      sync_preferences::TestingPrefServiceSyncable* pref_service = nullptr,
      signin::AccountConsistencyMethod account_consistency =
          signin::AccountConsistencyMethod::kDisabled,
      TestSigninClient* test_signin_client = nullptr);

  ~IdentityTestEnvironment() override;

  // The IdentityManager instance associated with this instance.
  IdentityManager* identity_manager();

  // Returns the |TestIdentityManagerObserver| watching the IdentityManager.
  TestIdentityManagerObserver* identity_manager_observer();

  // Sets the primary account for the given email address, generating a GAIA ID
  // that corresponds uniquely to that email address. On non-ChromeOS, results
  // in the firing of the IdentityManager and SigninManager callbacks for signin
  // success. Blocks until the primary account is set. Returns the
  // CoreAccountInfo of the newly-set account.
  CoreAccountInfo SetPrimaryAccount(const std::string& email);

  // Sets a refresh token for the primary account (which must already be set).
  // Before updating the refresh token, blocks until refresh tokens are loaded.
  // After updating the token, blocks until the update is processed by
  // |identity_manager|.
  void SetRefreshTokenForPrimaryAccount();

  // Sets a special invalid refresh token for the primary account (which must
  // already be set). Before updating the refresh token, blocks until refresh
  // tokens are loaded. After updating the token, blocks until the update is
  // processed by |identity_manager|.
  void SetInvalidRefreshTokenForPrimaryAccount();

  // Removes any refresh token for the primary account, if present. Blocks until
  // the refresh token is removed.
  void RemoveRefreshTokenForPrimaryAccount();

  // Makes the primary account available for the given email address, generating
  // a GAIA ID and refresh token that correspond uniquely to that email address.
  // On non-ChromeOS platforms, this will also result in the firing of the
  // IdentityManager and SigninManager callbacks for signin success. On all
  // platforms, this method blocks until the primary account is available.
  // Returns the AccountInfo of the newly-available account.
  AccountInfo MakePrimaryAccountAvailable(const std::string& email);

  // Clears the primary account if present, with |policy| used to determine
  // whether to keep or remove all accounts. On non-ChromeOS, results in the
  // firing of the IdentityManager and SigninManager callbacks for signout.
  // Blocks until the primary account is cleared.
  void ClearPrimaryAccount(
      ClearPrimaryAccountPolicy policy = ClearPrimaryAccountPolicy::DEFAULT);

  // Makes an account available for the given email address, generating a GAIA
  // ID and refresh token that correspond uniquely to that email address. Blocks
  // until the account is available. Returns the AccountInfo of the
  // newly-available account.
  AccountInfo MakeAccountAvailable(const std::string& email);

  // Sets a refresh token for the given account (which must already be
  // available). Before updating the refresh token, blocks until refresh tokens
  // are loaded. After updating the token, blocks until the update is processed
  // by |identity_manager|. NOTE: See disclaimer at top of file re: direct
  // usage.
  void SetRefreshTokenForAccount(const std::string& account_id);

  // Sets a special invalid refresh token for the given account (which must
  // already be available). Before updating the refresh token, blocks until
  // refresh tokens are loaded. After updating the token, blocks until the
  // update is processed by |identity_manager|. NOTE: See disclaimer at top of
  // file re: direct usage.
  void SetInvalidRefreshTokenForAccount(const std::string& account_id);

  // Removes any refresh token that is present for the given account. Blocks
  // until the refresh token is removed.
  // NOTE: See disclaimer at top of file re: direct usage.
  void RemoveRefreshTokenForAccount(const std::string& account_id);

  // Updates the persistent auth error set on |account_id| which must be a known
  // account, i.e., an account with a refresh token.
  void UpdatePersistentErrorOfRefreshTokenForAccount(
      const std::string& account_id,
      const GoogleServiceAuthError& auth_error);

  // Puts the given accounts into the Gaia cookie, replacing any previous
  // accounts. Blocks until the accounts have been set.
  void SetCookieAccounts(const std::vector<CookieParams>& cookie_accounts);

  // When this is set, access token requests will be automatically granted with
  // an access token value of "access_token".
  void SetAutomaticIssueOfAccessTokens(bool grant);

  // Issues |token| in response to any access token request that either has (a)
  // already occurred and has not been matched by a previous call to this or
  // other WaitFor... method, or (b) will occur in the future. In the latter
  // case, waits until the access token request occurs.
  // |id_token| is an uncommonly-needed parameter that contains extra
  // information regarding the user's currently-registered services; if this
  // means nothing to you, you don't need to concern yourself with it.
  // NOTE: This method behaves this way to allow IdentityTestEnvironment to be
  // agnostic with respect to whether access token requests are handled
  // synchronously or asynchronously in the production code.
  // NOTE: This version is suitable for use in the common context where access
  // token requests are only being made for one account. If you need to
  // disambiguate requests coming for different accounts, see the version below.
  void WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      const std::string& token,
      const base::Time& expiration,
      const std::string& id_token = std::string());

  // Issues |token| in response to an access token request for |account_id| that
  // either already occurred and has not been matched by a previous call to this
  // or other WaitFor... method , or (b) will occur in the future. In the latter
  // case, waits until the access token request occurs.
  // |id_token| is an uncommonly-needed parameter that contains extra
  // information regarding the user's currently-registered services; if this
  // means nothing to you, you don't need to concern yourself with it.
  // NOTE: This method behaves this way to allow
  // IdentityTestEnvironment to be agnostic with respect to whether access token
  // requests are handled synchronously or asynchronously in the production
  // code.
  void WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      const std::string& account_id,
      const std::string& token,
      const base::Time& expiration,
      const std::string& id_token = std::string());

  // Similar to WaitForAccessTokenRequestIfNecessaryAndRespondWithToken above
  // apart from the fact that it issues tokens for a given set of scopes only,
  // instead of issueing all tokens for all requests (the method variant above).
  void WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
      const std::string& token,
      const base::Time& expiration,
      const std::string& id_token,
      const identity::ScopeSet& scopes);

  // Issues |error| in response to any access token request that either has (a)
  // already occurred and has not been matched by a previous call to this or
  // other WaitFor... method, or (b) will occur in the future. In the latter
  // case, waits until the access token request occurs.
  // NOTE: This method behaves this way to allow IdentityTestEnvironment to be
  // agnostic with respect to whether access token requests are handled
  // synchronously or asynchronously in the production code.
  // NOTE: This version is suitable for use in the common context where access
  // token requests are only being made for one account. If you need to
  // disambiguate requests coming for different accounts, see the version below.
  void WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      const GoogleServiceAuthError& error);

  // Issues |error| in response to an access token request for |account_id| that
  // either has (a) already occurred and has not been matched by a previous call
  // to this or other WaitFor... method, or (b) will occur in the future. In the
  // latter case, waits until the access token request occurs.
  // NOTE: This method behaves this way to allow
  // IdentityTestEnvironment to be agnostic with respect to whether access token
  // requests are handled synchronously or asynchronously in the production
  // code.
  void WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      const std::string& account_id,
      const GoogleServiceAuthError& error);

  // Sets a callback that will be invoked on the next incoming access token
  // request. Note that this can not be combined with the
  // WaitForAccessTokenRequestIfNecessaryAndRespondWith* methods - you must
  // either wait for the callback to get called, or explicitly reset it by
  // passing in a null callback, before the Wait* methods can be used again.
  void SetCallbackForNextAccessTokenRequest(base::OnceClosure callback);

  // Updates the info for |account_info.account_id|, which must be a known
  // account.
  void UpdateAccountInfoForAccount(AccountInfo account_info);

  // Resets to the state where accounts have not yet been loaded from disk.
  void ResetToAccountsNotYetLoadedFromDiskState();

  // Simulates the reloading of the accounts from disk.
  void ReloadAccountsFromDisk();

  // Returns whether there is a access token request pending.
  bool IsAccessTokenRequestPending();

  // Sets whether the list of accounts in Gaia cookie jar is fresh and does not
  // need to be updated.
  void SetFreshnessOfAccountsInGaiaCookie(bool accounts_are_fresh);

  // By default, extended account info removal is disabled in testing
  // contexts. This call enables it for tests that require
  // IdentityManager::Observer::OnExtendedAccountInfoRemoved() to fire as
  // expected. TODO(https://crbug.com/927687): Enable this unconditionally.
  void EnableRemovalOfExtendedAccountInfo();

  // Simulate account fetching using AccountTrackerService without sending
  // network requests.
  void SimulateSuccessfulFetchOfAccountInfo(const std::string& account_id,
                                            const std::string& email,
                                            const std::string& gaia,
                                            const std::string& hosted_domain,
                                            const std::string& full_name,
                                            const std::string& given_name,
                                            const std::string& locale,
                                            const std::string& picture_url);

  // Simulates a merge session failure with |auth_error| as the error.
  void SimulateMergeSessionFailure(const GoogleServiceAuthError& auth_error);

 private:
  friend class ::IdentityTestEnvironmentChromeBrowserStateAdaptor;
  friend class ::IdentityTestEnvironmentProfileAdaptor;

  // TODO(https://crbug.com/933109): Remove friendship after conversion to
  // IdentityManager is complete.
  friend class identity::IdentityAccessorImplTest;

  struct AccessTokenRequestState {
    AccessTokenRequestState();
    ~AccessTokenRequestState();
    AccessTokenRequestState(AccessTokenRequestState&& other);
    AccessTokenRequestState& operator=(AccessTokenRequestState&& other);

    enum {
      kPending,
      kAvailable,
    } state;
    base::Optional<std::string> account_id;
    base::OnceClosure on_available;
  };

  // Constructor that takes in instances of the dependencies of
  // IdentityManager and constructs an IdentityManager instance from those
  // dependencies. For use in contexts where those dependencies are still
  // being used directly by the creator of this object (i.e., while a test is
  // being incrementally converted). Prefer the above constructor, and switch to
  // that constructor once possible (e.g., when an incremental conversion is
  // completed). NOTE: The passed-in objects must all outlive this object.
  IdentityTestEnvironment(
      PrefService* pref_service,
      AccountTrackerService* account_tracker_service,
      AccountFetcherService* account_fetcher_service,
      FakeProfileOAuth2TokenService* token_service,
      SigninManagerBase* signin_manager,
      GaiaCookieManagerService* gaia_cookie_manager_service,
      network::TestURLLoaderFactory* test_url_loader_factory = nullptr);

  // Constructor that takes in an IdentityManager instance as well as instances
  // of the dependencies of that IdentityManager. For use only in contexts where
  // IdentityManager and its dependencies are all unavoidably created by the
  // embedder (e.g., //chrome-level unittests that use the
  // ProfileKeyedServiceFactory infrastructure).
  // When using this constructor, the invoker is responsible for ensuring the
  // following:
  // - That all of these objects outlive this object
  // - That the dependencies being passed in were in fact the objects used to
  //   construct |identity_manager|
  // - That the passed-in dependencies of |identity_manager| outlive it
  // NOTE: This constructor is for usage only in the special case of embedder
  // unittests that must use the IdentityManager instance associated with the
  // Profile/ChromeBrowserState. If you think you have another use case for it,
  // contact blundell@chromium.org.
  IdentityTestEnvironment(
      PrefService* pref_service,
      AccountTrackerService* account_tracker_service,
      AccountFetcherService* account_fetcher_service,
      FakeProfileOAuth2TokenService* token_service,
      SigninManagerBase* signin_manager,
      GaiaCookieManagerService* gaia_cookie_manager_service,
      IdentityManager* identity_manager,
      network::TestURLLoaderFactory* test_url_loader_factory = nullptr);

  // Constructs this object from the supplied
  // dependencies of IdentityManager and potentially IdentityManager itself.
  // The supplied dependencies must be either:
  // (1) non-null instances of the backing classes,
  // (2) a non-null instance of |dependencies_owner|.
  // In the case of (1), |identity_manager| can be non-null, in which case it
  // must point to an object created via these dependencies. In the case of 2,
  // |identity_manager| must be null. If |identity_manager| is non-null, it will
  // be the IdentityManager instance associated with this object. Otherwise,
  // this object will create and own an IdentityManager instance from the
  // supplied dependencies.
  IdentityTestEnvironment(
      PrefService* pref_service,
      AccountTrackerService* account_tracker_service,
      AccountFetcherService* account_fetcher_service,
      FakeProfileOAuth2TokenService* token_service,
      SigninManagerBase* signin_manager,
      GaiaCookieManagerService* gaia_cookie_manager_service,
      network::TestURLLoaderFactory* test_url_loader_factory,
      std::unique_ptr<IdentityManagerDependenciesOwner> dependencies_owner,
      IdentityManager* identity_manager);

  // IdentityManager::DiagnosticsObserver:
  void OnAccessTokenRequested(const std::string& account_id,
                              const std::string& consumer_id,
                              const identity::ScopeSet& scopes) override;

  // Handles the notification that an access token request was received for
  // |account_id|. Invokes |on_access_token_request_callback_| if the latter
  // is non-null *and* either |*pending_access_token_requester_| equals
  // |account_id| or |pending_access_token_requester_| is empty.
  void HandleOnAccessTokenRequested(std::string account_id);

  // If a token request for |account_id| (or any account if nullopt) has already
  // been made and not matched by a different call, returns immediately.
  // Otherwise and runs a nested runloop until a matching access token request
  // is observed.
  void WaitForAccessTokenRequestIfNecessary(
      base::Optional<std::string> account_id);

  // NOTE: This object must be first in the list, as it owns the objects
  // pointed to below in the case where those objects are not passed in via
  // the IdentityTestEnvironment constructor.
  std::unique_ptr<IdentityManagerDependenciesOwner> dependencies_owner_;
  PrefService* pref_service_ = nullptr;
  AccountTrackerService* account_tracker_service_ = nullptr;
  AccountFetcherService* account_fetcher_service_ = nullptr;
  FakeProfileOAuth2TokenService* token_service_ = nullptr;
  SigninManagerBase* signin_manager_ = nullptr;
  GaiaCookieManagerService* gaia_cookie_manager_service_ = nullptr;

  // Used to set fake responses for cookie-related requests.
  // This can be null if no TestURLLoaderFactory was passed via the constructor.
  network::TestURLLoaderFactory* test_url_loader_factory_ = nullptr;

  // Depending on which constructor is used, exactly one of these will be
  // non-null. See the documentation on the constructor wherein IdentityManager
  // is passed in for required lifetime invariants in that case.
  std::unique_ptr<IdentityManager> owned_identity_manager_;
  IdentityManager* raw_identity_manager_ = nullptr;

  std::unique_ptr<TestIdentityManagerObserver> test_identity_manager_observer_;

  base::OnceClosure on_access_token_requested_callback_;
  std::vector<AccessTokenRequestState> requesters_;

  base::WeakPtrFactory<IdentityTestEnvironment> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(IdentityTestEnvironment);
};

}  // namespace identity

#endif  // SERVICES_IDENTITY_PUBLIC_CPP_IDENTITY_TEST_ENVIRONMENT_H_
